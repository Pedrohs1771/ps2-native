#!/usr/bin/env python3
"""Generate finite static EE banks from validated recovered snapshots, offline."""
import argparse
import hashlib
import json
from pathlib import Path
import re
import subprocess
import tempfile


def generate(cases, generator, output, root=None):
    if output.exists():
        raise ValueError('catalog output already exists')
    if not 1 <= len(cases) <= 512:
        raise ValueError('EE bank count outside 1..512')
    records = []
    total_bindings = 0
    generator_hash = hashlib.sha256(generator.read_bytes()).hexdigest()
    for case in cases:
        with (case / 'bank.json').open('rb') as stream:
            raw_metadata = stream.read(8 * 1024 * 1024 + 1)
        if len(raw_metadata) > 8 * 1024 * 1024:
            raise ValueError('EE bank metadata exceeds 8 MiB')
        metadata = json.loads(raw_metadata)
        if not isinstance(metadata, dict):
            raise ValueError('EE bank metadata must be an object')
        if 'generator_sha256' in metadata and metadata['generator_sha256'] != generator_hash:
            raise ValueError('captured EE case was prepared with a different generator')
        with (case / 'snapshot.bin').open('rb') as stream:
            image = stream.read(65537)
        base, entry = metadata['base'], metadata['entry']
        if any(type(metadata[field]) is not int for field in ('schema_version', 'base', 'entry', 'image_bytes')) or \
                metadata['schema_version'] != 1 or metadata['image_bytes'] != len(image) or \
                hashlib.sha256(image).hexdigest() != metadata['image_sha256'] or \
                not image or len(image) > 65536 or len(image) % 4 or base % 4 or entry % 4 or \
                base < 0 or base >= 32 * 1024 * 1024 or len(image) > 32 * 1024 * 1024 - base or \
                not base <= entry < base + len(image):
            raise ValueError('invalid EE snapshot identity or dimensions')
        key = hashlib.sha256(base.to_bytes(4, 'little') + entry.to_bytes(4, 'little') + image).hexdigest()
        if not isinstance(metadata['bindings'], list) or not 1 <= len(metadata['bindings']) <= 32768:
            raise ValueError('invalid recovered EE bindings')
        if any(not isinstance(row, dict) or set(row) != {'address', 'source_begin', 'source_bytes'} or
               any(type(value) is not int for value in row.values()) for row in metadata['bindings']):
            raise ValueError('invalid recovered EE binding fields')
        total_bindings += len(metadata['bindings'])
        if total_bindings > 2 * 1024 * 1024:
            raise ValueError('EE catalog binding budget exceeded')
        records.append((image, metadata, key))
    if not 1 <= len(records) <= 512 or len({key for _, _, key in records}) != len(records):
        raise ValueError('invalid, duplicate or excessive EE banks')
    records.sort(key=lambda record: record[2])
    output.mkdir()
    files = []
    bank_functions = []
    for image, metadata, key in records:
        name = 'ee_bank_' + key
        with tempfile.TemporaryDirectory() as temporary:
            code_file = Path(temporary) / 'bank.cpp'
            snapshot = Path(temporary) / 'snapshot.bin'
            snapshot.write_bytes(image) # Execute the generator against the admitted immutable bytes.
            result = subprocess.run([str(generator), str(snapshot), str(metadata['base']),
                                     str(metadata['entry']), str(code_file)], capture_output=True,
                                    text=True, timeout=30)
            if result.returncode:
                raise ValueError('EE bank generator failed: ' + result.stderr)
            with code_file.open('rb') as stream:
                code_bytes = stream.read(64 * 1024 * 1024 + 1)
            if len(code_bytes) > 64 * 1024 * 1024:
                raise ValueError('generated EE bank source exceeds 64 MiB')
            code = code_bytes.decode()
        getter = 'extern "C" const PS2NativeOverlayBinding *ps2xOverlayGetBindings'
        if code.count('ps2xOverlayGetBindings') != 1 or code.count(getter) != 1:
            raise ValueError('unrecognized generated overlay getter')
        rows = [{'address': int(address, 16), 'source_begin': int(begin, 16), 'source_bytes': int(size, 16)}
                for address, begin, size, offset in re.findall(
                    r'\{(0x[0-9a-f]+)u,ps2native_block_[0-9a-f]+,(0x[0-9a-f]+)u,'
                    r'(0x[0-9a-f]+)u,snapshot\+(0x[0-9a-f]+)u\}', code)
                if int(begin, 16) == metadata['base'] + int(offset, 16)]
        if rows != metadata['bindings']:
            raise ValueError('regenerated EE bindings differ from recovered identity')
        includes = [line for line in code.splitlines() if line.startswith('#include')]
        body = '\n'.join(line for line in code.splitlines() if not line.startswith('#include'))
        body = body.replace(getter,
                            'const PS2NativeOverlayBinding *getBindings')
        code = '\n'.join(includes) + '\n#include "ps2_ee_aot_bank.h"\nnamespace ' + name + ' {\n' + body + '\n}\n'
        function = name + '_descriptor'
        code += ('ps2native::ee_aot::Bank ' + function + '() { return {' + str(metadata['base']) +
                 'u,std::span(' + name + '::snapshot),std::span(' + name + '::bindings)}; }\n')
        source = output / (name + '.cpp')
        source.write_text(code)
        files.append(source.name)
        bank_functions.append(function)
    index = '#include "ps2_ee_aot_bank.h"\n#include <array>\n'
    index += ''.join('ps2native::ee_aot::Bank ' + name + '();\n' for name in bank_functions)
    index += 'const ps2native::ee_aot::Program &compiledEeProgram() {\n'
    index += 'static const std::array<ps2native::ee_aot::Bank,' + str(len(bank_functions)) + '> banks{{\n'
    index += ''.join(name + '(),\n' for name in bank_functions) + '}};\n'
    index += 'static const ps2native::ee_aot::Program program{banks};return program; }\n'
    (output / 'ee_catalog.cpp').write_text(index)
    files.append('ee_catalog.cpp')
    if hashlib.sha256(generator.read_bytes()).hexdigest() != generator_hash:
        raise ValueError('EE generator changed during catalog production')
    manifest = {'schema_version': 1, 'symbol': 'compiledEeProgram', 'sources': files,
                'sha256': {name: hashlib.sha256((output / name).read_bytes()).hexdigest() for name in files},
                'generator_sha256': generator_hash,
                'bank_generator_sha256': hashlib.sha256(Path(__file__).read_bytes()).hexdigest(),
                'banks': [{'base': m['base'], 'entry': m['entry'], 'image_sha256': m['image_sha256']}
                          for _, m, _ in records],
                'scope': 'finite observed EE RAM-block banks; publication, hidden state, closure and fidelity unqualified'}
    (output / 'catalog.json').write_text(json.dumps(manifest, indent=2) + '\n')
    return manifest


def extend_catalog(cases, generator, output):
    """Extend an exclusively owned offline catalog, preserving unchanged sources.

    All existing identities are validated before publication. The manifest is
    replaced last. An interrupted update must pass hash validation before reuse.
    This directory must not have concurrent builders or producers.
    """
    if output.is_symlink() or not output.is_dir() or (output / 'catalog.json').is_symlink():
        raise ValueError('extension requires an existing owned catalog directory')
    with (output / 'catalog.json').open('rb') as stream:
        raw = stream.read(1024 * 1024 + 1)
    if len(raw) > 1024 * 1024:
        raise ValueError('existing EE catalog manifest exceeds its bound')
    old = json.loads(raw)
    if not isinstance(old, dict) or type(old.get('schema_version')) is not int or \
            old['schema_version'] != 1 or old.get('symbol') != 'compiledEeProgram' or \
            old.get('generator_sha256') != hashlib.sha256(generator.read_bytes()).hexdigest() or \
            not isinstance(old.get('sources'), list) or not 2 <= len(old['sources']) <= 513 or \
            any(type(name) is not str for name in old['sources']) or \
            old['sources'].count('ee_catalog.cpp') != 1 or \
            len(set(old['sources'])) != len(old['sources']) or \
            not isinstance(old.get('sha256'), dict) or set(old['sha256']) != set(old['sources']) or \
            not isinstance(old.get('banks'), list) or len(old['banks']) != len(old['sources']) - 1:
        raise ValueError('existing EE catalog identity is invalid')
    for name in old['sources']:
        if not isinstance(name, str) or not re.fullmatch(r'ee_bank_[0-9a-f]{64}\.cpp|ee_catalog\.cpp', name):
            raise ValueError('existing EE catalog source name is invalid')
        path = output / name
        if path.is_symlink():
            raise ValueError('existing EE catalog source is a symlink')
        with path.open('rb') as stream:
            source = stream.read(64 * 1024 * 1024 + 1)
        if len(source) > 64 * 1024 * 1024 or \
                hashlib.sha256(source).hexdigest() != old['sha256'][name]:
            raise ValueError('existing EE catalog source identity changed')
    with tempfile.TemporaryDirectory(prefix='.ee-catalog-stage-', dir=output.parent) as temporary:
        staged = Path(temporary) / 'catalog'
        new = generate(cases, generator, staged)
        if not set(old['sources']).issubset(new['sources']) or \
                any(bank not in new['banks'] for bank in old['banks']) or \
                any(new['sha256'][name] != old['sha256'][name]
                    for name in old['sources'] if name != 'ee_catalog.cpp'):
            raise ValueError('extension cannot remove or rewrite existing EE banks')
        for name in new['sources']:
            if name not in old['sources'] and ((output / name).exists() or (output / name).is_symlink()):
                raise ValueError('new EE source conflicts with an unrecorded artifact')
        # Existing bank files are byte-identical, so do not touch their mtimes.
        for name in new['sources']:
            if name not in old['sources'] or new['sha256'][name] != old['sha256'][name]:
                (staged / name).replace(output / name)
        if (staged / 'catalog.json').read_bytes() != raw:
            (staged / 'catalog.json').replace(output / 'catalog.json')
    return new


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('cases', type=Path, nargs='+')
    parser.add_argument('--generator', type=Path, required=True)
    parser.add_argument('--output', type=Path, required=True)
    parser.add_argument('--extend', action='store_true',
                        help='Extend a verified catalog while preserving unchanged source timestamps')
    args = parser.parse_args()
    try:
        if args.extend:
            manifest = extend_catalog(args.cases, args.generator.resolve(), args.output)
        else:
            manifest = generate(args.cases, args.generator.resolve(), args.output)
        print(json.dumps({'banks': len(manifest['banks']), 'sources': len(manifest['sources'])}))
    except (ValueError, OSError, KeyError, TypeError, subprocess.TimeoutExpired) as error:
        parser.error(str(error))


if __name__ == '__main__':
    main()
