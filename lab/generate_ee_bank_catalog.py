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
    for case in cases:
        with (case / 'bank.json').open('rb') as stream:
            raw_metadata = stream.read(8 * 1024 * 1024 + 1)
        if len(raw_metadata) > 8 * 1024 * 1024:
            raise ValueError('EE bank metadata exceeds 8 MiB')
        metadata = json.loads(raw_metadata)
        if not isinstance(metadata, dict):
            raise ValueError('EE bank metadata must be an object')
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
    generator_hash = hashlib.sha256(generator.read_bytes()).hexdigest()
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
            code = code_file.read_text()
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
        code = '\n'.join(includes) + '\n#include "ps2_ee_aot.h"\nnamespace ' + name + ' {\n' + body + '\n}\n'
        function = name + '_descriptor'
        code += ('ps2native::ee_aot::Bank ' + function + '() { return {' + str(metadata['base']) +
                 'u,std::span(' + name + '::snapshot),std::span(' + name + '::bindings)}; }\n')
        source = output / (name + '.cpp')
        source.write_text(code)
        files.append(source.name)
        bank_functions.append(function)
    index = '#include "ps2_ee_aot.h"\n#include <array>\n'
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


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('cases', type=Path, nargs='+')
    parser.add_argument('--generator', type=Path, required=True)
    parser.add_argument('--output', type=Path, required=True)
    args = parser.parse_args()
    try:
        manifest = generate(args.cases, args.generator.resolve(), args.output, Path(__file__).resolve().parents[1])
        print(json.dumps({'banks': len(manifest['banks']), 'sources': len(manifest['sources'])}))
    except (ValueError, OSError, KeyError, TypeError, subprocess.TimeoutExpired) as error:
        parser.error(str(error))


if __name__ == '__main__':
    main()
