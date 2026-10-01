#!/usr/bin/env python3
"""Publish a finite experimental native family catalog offline, never a certificate."""
import argparse
import hashlib
import json
from pathlib import Path
import re
import struct
import subprocess
import tempfile

from discover_ee_data_families import bounded_bytes, ordinary_path


def uint(value):
    return type(value) is int and 0 <= value <= 0xffffffff


def encode(words):
    return struct.pack('<' + 'I' * len(words), *words)


def root_shape(case):
    case = ordinary_path(case)
    metadata_bytes = bounded_bytes(case / 'bank.json', 8 * 1024 * 1024)
    metadata = json.loads(metadata_bytes)
    image = bounded_bytes(case / 'snapshot.bin', 65536)
    if not isinstance(metadata, dict):
        raise ValueError('root case metadata must be an object')
    base, entry = metadata.get('base'), metadata.get('entry')
    if metadata.get('schema_version') != 1 or type(metadata['schema_version']) is not int or \
            not uint(base) or not uint(entry) or (base | entry) & 3 or \
            not base <= entry < base + len(image) <= 32 * 1024 * 1024 or \
            not uint(metadata.get('image_bytes')) or len(image) != metadata['image_bytes'] or len(image) % 4 or \
            hashlib.sha256(image).hexdigest() != metadata.get('image_sha256'):
        raise ValueError('invalid root case identity')
    bindings = metadata.get('bindings')
    if not isinstance(bindings, list) or not 1 <= len(bindings) <= 32768:
        raise ValueError('invalid root case bindings')
    roots = [row for row in bindings if isinstance(row, dict) and
             uint(row.get('address')) and row['address'] == entry]
    if len(roots) != 1:
        raise ValueError('root case must have one requested entry')
    begin, size = roots[0].get('source_begin'), roots[0].get('source_bytes')
    if not uint(begin) or not uint(size) or (begin | size) & 3 or \
            not base <= begin <= entry < begin + size <= base + len(image) or not 0 < size <= 512:
        raise ValueError('invalid bounded root dependency')
    words = list(struct.unpack('<' + 'I' * (size // 4), image[begin-base:begin-base+size]))
    ledger = {'case': str(case.resolve()), 'metadata_sha256': hashlib.sha256(metadata_bytes).hexdigest(),
              'image_sha256': hashlib.sha256(image).hexdigest(), 'image_bytes': len(image),
              'base': base, 'entry': entry, 'source_begin': begin, 'source_bytes': size}
    return words, [0xffffffff] * len(words), [entry-begin], ledger


def generate(report_path, generator, output, *, cases=(), only_shapes=()):
    output = ordinary_path(output)
    if output.exists():
        raise ValueError('family catalog output must be fresh')
    generator = ordinary_path(generator)
    report_bytes = bounded_bytes(report_path, 16 * 1024 * 1024)
    report = json.loads(report_bytes)
    if not isinstance(report, dict) or report.get('schema_version') != 1 or \
            type(report['schema_version']) is not int or report.get('status') != 'CANDIDATES_LABORATORY' or \
            report.get('strict_approval') is not False:
        raise ValueError('expected a laboratory candidate report')
    families = report.get('families')
    if not isinstance(families, list) or not 1 <= len(families) <= 512 or len(cases) > 16:
        raise ValueError('family input budget exceeded')
    selected = set(only_shapes)
    if any(not isinstance(key, str) or not re.fullmatch('[0-9a-f]{64}', key) for key in selected):
        raise ValueError('invalid requested shape identity')
    shapes = {}

    def add(words, masks, offsets):
        if not 1 <= len(words) <= 128 or len(words) != len(masks) or \
                any(not uint(word) for word in words + masks) or \
                any(mask not in (0xffffffff, 0xffff0000) for mask in masks) or \
                any(not uint(offset) or offset & 3 or offset >= len(words) * 4 for offset in offsets):
            raise ValueError('invalid bounded candidate structure')
        words = [word & mask for word, mask in zip(words, masks)]
        key = hashlib.sha256(b'ee-native-family-catalog-v0\0' + encode(words) + encode(masks)).hexdigest()
        if key in shapes:
            shapes[key][2].update(offsets)
        else:
            shapes[key] = [words, masks, set(offsets)]

    found = set()
    for family in families:
        if not isinstance(family, dict):
            raise ValueError('invalid family proposal')
        key = family.get('shape_sha256')
        if not isinstance(key, str) or not re.fullmatch('[0-9a-f]{64}', key):
            raise ValueError('invalid candidate shape identity')
        if selected and key not in selected:
            continue
        found.add(key)
        words, masks = family.get('guard_words'), family.get('guard_masks')
        if not isinstance(words, list) or not isinstance(masks, list) or \
                type(family.get('word_count')) is not int or family['word_count'] != len(words):
            raise ValueError('invalid candidate word/mask dimensions')
        add(words, masks, list(range(0, len(words) * 4, 4)))
    if selected != found and selected:
        raise ValueError('requested candidate shape missing')
    root_cases = []
    for case in cases:
        words, masks, offsets, ledger = root_shape(case)
        add(words, masks, offsets)
        root_cases.append(ledger)
    if not shapes or len(shapes) > 512:
        raise ValueError('combined family budget exceeded')
    generator_hash = hashlib.sha256(bounded_bytes(generator, 64 * 1024 * 1024)).hexdigest()
    sources, descriptors, rejected = {}, [], []
    with tempfile.TemporaryDirectory() as temporary:
        work = Path(temporary)
        for key, (words, masks, offsets) in sorted(shapes.items()):
            (work / 'words.bin').write_bytes(encode(words))
            (work / 'masks.bin').write_bytes(encode(masks))
            cpp = work / 'family.cpp'
            if cpp.exists():
                cpp.unlink()
            result = subprocess.run([str(generator), str(work/'words.bin'), str(work/'masks.bin'), str(cpp)],
                                    capture_output=True, text=True, timeout=30)
            if result.returncode:
                rejected.append({'key': key, 'reason': result.stderr[:4096]})
                continue
            code = bounded_bytes(cpp, 1024 * 1024).decode()
            if code.count('void ps2native_data_family(') != 1:
                raise ValueError('unrecognized family generator output')
            name = 'ee_family_' + key
            includes = '\n'.join(line for line in code.splitlines() if line.startswith('#include'))
            body = '\n'.join(line for line in code.splitlines() if not line.startswith('#include'))
            source = includes + '\n#include "ps2_ee_data_family.h"\nnamespace ' + name + ' {\n' + body + '\n'
            for field, values in [('words', words), ('masks', masks), ('entries', sorted(offsets))]:
                source += 'static const uint32_t ' + field + '[]={' + ','.join(str(value)+'u' for value in values) + '};\n'
            source += '}\nps2native::ee_family::Family ' + name + '_descriptor(){return {' + name + '::ps2native_data_family,'
            source += name+'::words,'+name+'::masks,'+name+'::entries};}\n'
            sources[name + '.cpp'] = source
            descriptors.append(name + '_descriptor')
    if hashlib.sha256(bounded_bytes(generator, 64 * 1024 * 1024)).hexdigest() != generator_hash or not descriptors:
        raise ValueError('family generator changed or produced no supported structure')
    index = '#include "ps2_ee_data_family.h"\n#include <array>\n'
    index += ''.join('ps2native::ee_family::Family ' + name + '();\n' for name in descriptors)
    index += 'const ps2native::ee_family::Program &compiledEeFamilyProgram(){\nstatic const std::array<ps2native::ee_family::Family,' + str(len(descriptors)) + '> families{{\n'
    index += ''.join(name+'(),\n' for name in descriptors) + '}};\nstatic const ps2native::ee_family::Program program{families};return program;}\n'
    sources['ee_family_catalog.cpp'] = index
    manifest = {'schema_version': 1, 'symbol': 'compiledEeFamilyProgram', 'sources': sorted(sources),
                'sha256': {name: hashlib.sha256(code.encode()).hexdigest() for name, code in sources.items()},
                'family_count': len(descriptors), 'generator_sha256': generator_hash,
                'publisher_sha256': hashlib.sha256(Path(__file__).read_bytes()).hexdigest(),
                'candidate_report_sha256': hashlib.sha256(report_bytes).hexdigest(), 'rejected': rejected,
                'selected_shapes': sorted(selected), 'root_cases': root_cases,
                'strict_approval': False, 'closure_proved': False,
                'scope': 'finite experimental precompiled EE structures; no producer/fetch/fidelity/game qualification'}
    output.mkdir()
    for name, code in sources.items():
        (output / name).write_text(code)
    (output / 'catalog.json').write_text(json.dumps(manifest, indent=2)+'\n')
    return manifest


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--candidates', required=True, type=Path)
    parser.add_argument('--generator', required=True, type=Path)
    parser.add_argument('--output', required=True, type=Path)
    parser.add_argument('--case', action='append', default=[], type=Path)
    parser.add_argument('--shape', action='append', default=[])
    args = parser.parse_args()
    try:
        result = generate(args.candidates, args.generator, args.output, cases=args.case, only_shapes=args.shape)
        print(json.dumps({'families': result['family_count'], 'rejected': len(result['rejected']), 'strict_approval': False}))
    except (ValueError, OSError, KeyError, TypeError, RecursionError, subprocess.TimeoutExpired) as error:
        parser.error(str(error))


if __name__ == '__main__':
    main()
