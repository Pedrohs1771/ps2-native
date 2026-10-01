#!/usr/bin/env python3
"""Propose bounded EE data-operand shapes offline; never authorize execution.

Equal guarded bytes suggest a structure. They do not prove its producer,
relocation/control semantics, fetch identity, entry context or hardware fidelity.
Only LUI unsigned immediates and SW signed offsets are candidate parameters.
"""
from __future__ import annotations

import argparse
import hashlib
import json
from pathlib import Path
import struct
import time

RAM_BYTES = 32 * 1024 * 1024
MAX_CASES = 512
MAX_METADATA_BYTES = 8 * 1024 * 1024
MAX_TOTAL_METADATA_BYTES = 64 * 1024 * 1024
MAX_BINDINGS = 2 * 1024 * 1024
MAX_REGIONS = 131072
MAX_SCANNED_WORDS = 4 * 1024 * 1024
MAX_REGION_WORDS = 128


def ordinary_path(path):
    path = Path(path).absolute()
    if any(parent.is_symlink() for parent in (path, *path.parents)):
        raise ValueError('linked paths are not accepted')
    return path


def bounded_bytes(path, limit):
    path = ordinary_path(path)
    if not path.is_file() or path.stat().st_size > limit:
        raise ValueError('missing or oversized input')
    with path.open('rb') as stream:
        result = stream.read(limit + 1)
    if len(result) > limit:
        raise ValueError('oversized input')
    return result


def integer(value):
    return type(value) is int


def parameter_kind(word):
    opcode = word >> 26
    if opcode == 0x0F and (word >> 21) & 31 == 0:
        return 'lui-u16'
    if opcode == 0x2B:
        return 'sw-s16'
    return None


def words_bytes(words):
    return struct.pack('<' + 'I' * len(words), *words)


def discover(cases):
    cases = list(cases)
    if not 1 <= len(cases) <= MAX_CASES:
        raise ValueError('expected a bounded nonempty case list')
    groups = {}
    counts = {'cases': len(cases), 'bindings': 0, 'regions': 0,
              'scanned_words': 0, 'oversized_regions_skipped': 0}
    metadata_bytes = 0
    for case in cases:
        case = ordinary_path(case)
        # Charge the aggregate before allocating another metadata document.
        metadata_path = ordinary_path(case / 'bank.json')
        metadata_bytes += metadata_path.stat().st_size
        if metadata_bytes > MAX_TOTAL_METADATA_BYTES:
            raise ValueError('aggregate metadata budget exceeded')
        encoded = bounded_bytes(metadata_path, MAX_METADATA_BYTES)
        metadata = json.loads(encoded)
        if not isinstance(metadata, dict) or type(metadata.get('schema_version')) is not int or metadata['schema_version'] != 1:
            raise ValueError('unsupported case schema')
        base, entry, size = (metadata.get(key) for key in ('base', 'entry', 'image_bytes'))
        if not all(integer(value) for value in (base, entry, size)) or base & 3 or entry & 3 or \
                not 0 < size <= 65536 or size & 3 or not 0 <= base <= RAM_BYTES - size or \
                not base <= entry < base + size:
            raise ValueError('invalid case address or image size')
        image = bounded_bytes(case / 'snapshot.bin', 65536)
        digest = hashlib.sha256(image).hexdigest()
        if len(image) != size or digest != metadata.get('image_sha256'):
            raise ValueError('snapshot identity mismatch')
        bindings = metadata.get('bindings')
        if not isinstance(bindings, list) or not 1 <= len(bindings) <= 32768:
            raise ValueError('invalid binding count')
        counts['bindings'] += len(bindings)
        if counts['bindings'] > MAX_BINDINGS:
            raise ValueError('aggregate binding budget exceeded')
        regions, previous, entry_seen = set(), -1, False
        for row in bindings:
            if not isinstance(row, dict):
                raise ValueError('invalid binding')
            pc, begin, span = (row.get(key) for key in ('address', 'source_begin', 'source_bytes'))
            if not all(integer(value) for value in (pc, begin, span)) or (pc | begin | span) & 3 or \
                    not base <= begin <= pc < begin + span <= base + size or pc <= previous:
                raise ValueError('invalid binding range or order')
            previous = pc
            entry_seen |= pc == entry
            regions.add((begin, span))
        if not entry_seen:
            raise ValueError('requested entry lacks a binding')
        origin = (digest, hashlib.sha256(encoded).hexdigest(), base)
        for begin, span in sorted(regions):
            counts['regions'] += 1
            if counts['regions'] > MAX_REGIONS:
                raise ValueError('aggregate region budget exceeded')
            if span // 4 > MAX_REGION_WORDS:
                counts['oversized_regions_skipped'] += 1
                continue
            counts['scanned_words'] += span // 4
            if counts['scanned_words'] > MAX_SCANNED_WORDS:
                raise ValueError('aggregate word budget exceeded')
            raw = image[begin - base:begin - base + span]
            words = struct.unpack('<' + 'I' * (span // 4), raw)
            normalized = words_bytes([word & 0xFFFF0000 if parameter_kind(word) else word for word in words])
            # Full bytes are the grouping key; a digest collision cannot merge shapes.
            group = groups.setdefault(normalized, {})
            group.setdefault((begin, raw), set()).add(origin)

    families = []
    for normalized, group in groups.items():
        if len({raw for _, raw in group}) < 2:
            continue
        words = list(struct.unpack('<' + 'I' * (len(normalized) // 4), normalized))
        first = next(iter(group))[1]
        masks, parameters = [], []
        for index, word in enumerate(words):
            offset = index * 4
            differing = len({raw[offset:offset + 4] for _, raw in group}) > 1
            kind = parameter_kind(word)
            if differing and kind:
                masks.append(0xFFFF0000)
                parameters.append({'word_index': index, 'kind': kind, 'bits': 16})
            else:
                masks.append(0xFFFFFFFF)
                words[index] = struct.unpack_from('<I', first, offset)[0]
        observations = []
        for (pc, raw), origins in sorted(group.items()):
            values = []
            for parameter in parameters:
                value = struct.unpack_from('<H', raw, parameter['word_index'] * 4)[0]
                if parameter['kind'] == 'sw-s16' and value >= 0x8000:
                    value -= 0x10000
                values.append(value)
            observations.append({'pc': pc, 'word_sha256': hashlib.sha256(raw).hexdigest(),
                                 'parameters': values,
                                 'origins': [{'image_sha256': image_hash, 'metadata_sha256': metadata_hash,
                                              'base': base} for image_hash, metadata_hash, base in sorted(origins)]})
        identity = b'ee-data-shape-lui-sw-v1\0' + words_bytes(words) + words_bytes(masks)
        families.append({'shape_sha256': hashlib.sha256(identity).hexdigest(),
                         'word_count': len(words), 'guard_words': words, 'guard_masks': masks,
                         'parameters': parameters, 'observations': observations})
    return {'schema_version': 1, 'status': 'CANDIDATES_LABORATORY',
            'analyzer_sha256': hashlib.sha256(Path(__file__).read_bytes()).hexdigest(),
            'strict_approval': False, 'closure_proved': False, 'producer_invariant_proved': False,
            'native_execution_validated': False,
            'scope': 'observed bounded dependency-byte shapes; no execution or universal parameter domain',
            'counts': counts, 'families': sorted(families, key=lambda family: family['shape_sha256'])}


def write_report(cases, output):
    output = ordinary_path(output)
    if output.exists():
        raise FileExistsError('output must be fresh')
    report = discover(cases)
    with output.open('x') as stream:
        stream.write(json.dumps(report, indent=2) + '\n')
    return report


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    inputs = parser.add_mutually_exclusive_group(required=True)
    inputs.add_argument('--case', action='append', type=Path)
    inputs.add_argument('--catalog', type=Path)
    parser.add_argument('--output', required=True, type=Path)
    args = parser.parse_args()
    started = time.monotonic()
    try:
        cases = args.case
        if args.catalog is not None:
            import generate_ee_bank_catalog as catalog_tool
            catalog = ordinary_path(args.catalog)
            ordinary_path(catalog / 'catalog.json')
            cases = catalog_tool.owned_cases(catalog)
        report = write_report(cases, args.output)
    except (ValueError, OSError, KeyError, TypeError, RecursionError) as error:
        parser.error(str(error))
    print(json.dumps({'status': report['status'], 'family_candidates': len(report['families']),
                      'counts': report['counts'], 'seconds': time.monotonic() - started,
                      'strict_approval': False}))


if __name__ == '__main__':
    main()
