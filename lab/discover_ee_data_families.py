#!/usr/bin/env python3
"""Propose bounded EE data-operand shapes offline; never authorize execution.

Equal guarded bytes suggest a structure. They do not prove its producer,
relocation/control semantics, fetch identity, entry context or hardware fidelity.
Only fixed ordinary operations may expose typed low-16 data fields. Branch
encodings and instruction/register selection remain fixed.
"""
from __future__ import annotations

import argparse
import hashlib
import io
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
MAX_FAMILIES = 32768
MAX_REPORT_BYTES = 64 * 1024 * 1024
DATA_FIELDS = {0x09:'addiu-s16',0x0a:'slti-s16',0x0b:'sltiu-s16',0x0c:'andi-u16',
               0x0d:'ori-u16',0x0e:'xori-u16',0x20:'lb-s16',0x21:'lh-s16',0x23:'lw-s16',
               0x24:'lbu-s16',0x25:'lhu-s16',0x27:'lwu-s16',0x37:'ld-s16',0x1e:'lq-s16',
               0x28:'sb-s16',0x29:'sh-s16',0x2b:'sw-s16',0x3f:'sd-s16',0x1f:'sq-s16'}


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
    return DATA_FIELDS.get(opcode)


def words_bytes(words):
    return struct.pack('<' + 'I' * len(words), *words)


def discover(cases, *, minimum_variants=2, operand_policy='observed',
             root_only=False, terminal_only=False):
    if type(minimum_variants) is not int or minimum_variants not in (1,2) or \
            operand_policy not in ('observed','typed') or type(root_only) is not bool or \
            type(terminal_only) is not bool:
        raise ValueError('invalid bounded discovery policy')
    cases = list(cases)
    if not 1 <= len(cases) <= MAX_CASES:
        raise ValueError('expected a bounded nonempty case list')
    groups = {}
    entry_groups = {}
    counts = {'cases': len(cases), 'bindings': 0, 'regions': 0,
              'scanned_words': 0, 'oversized_regions_skipped': 0,
              'nonroot_bindings_skipped':0,'nonterminal_regions_skipped':0}
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
        regions, previous, entry_seen = {}, -1, False
        for row in bindings:
            if not isinstance(row, dict):
                raise ValueError('invalid binding')
            pc, begin, span = (row.get(key) for key in ('address', 'source_begin', 'source_bytes'))
            if not all(integer(value) for value in (pc, begin, span)) or (pc | begin | span) & 3 or \
                    not base <= begin <= pc < begin + span <= base + size or pc <= previous:
                raise ValueError('invalid binding range or order')
            previous = pc
            entry_seen |= pc == entry
            if root_only and pc != begin:
                counts['nonroot_bindings_skipped'] += 1
            else:
                regions.setdefault((begin, span), set()).add(pc-begin)
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
            if terminal_only:
                branch = words[-2] if len(words)>=2 else 0
                opcode = branch>>26
                if not (opcode in (1,2,3,4,5,6,7,0x14,0x15,0x16,0x17) or
                        (opcode==0 and branch&63 in (8,9))):
                    counts['nonterminal_regions_skipped'] += 1
                    continue
            normalized = words_bytes([word & 0xFFFF0000 if parameter_kind(word) else word for word in words])
            # Full bytes are the grouping key; a digest collision cannot merge shapes.
            group = groups.setdefault(normalized, {})
            group.setdefault((begin, raw), set()).add(origin)
            entry_groups.setdefault((normalized,begin,raw),set()).update(regions[(begin,span)])

    families = []
    for normalized, group in groups.items():
        if len({raw for _, raw in group}) < minimum_variants:
            continue
        if len(families) >= MAX_FAMILIES:
            raise ValueError('candidate family budget exceeded')
        words = list(struct.unpack('<' + 'I' * (len(normalized) // 4), normalized))
        first = next(iter(group))[1]
        masks, parameters = [], []
        for index, word in enumerate(words):
            offset = index * 4
            differing = len({raw[offset:offset + 4] for _, raw in group}) > 1
            kind = parameter_kind(word)
            if kind and (differing or operand_policy=='typed'):
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
                if parameter['kind'].endswith('-s16') and value >= 0x8000:
                    value -= 0x10000
                values.append(value)
            observations.append({'pc': pc, 'word_sha256': hashlib.sha256(raw).hexdigest(),
                                 'parameters': values,
                                 'normal_entry_offsets': sorted(entry_groups[(normalized,pc,raw)]),
                                 'origins': [{'image_sha256': image_hash, 'metadata_sha256': metadata_hash,
                                              'base': base} for image_hash, metadata_hash, base in sorted(origins)]})
        identity = b'ee-data-shape-typed-v2\0' + words_bytes(words) + words_bytes(masks)
        families.append({'shape_sha256': hashlib.sha256(identity).hexdigest(),
                         'word_count': len(words), 'guard_words': words, 'guard_masks': masks,
                         'normal_entry_offsets': sorted({offset for observation in observations
                                                         for offset in observation['normal_entry_offsets']}),
                         'parameters': parameters, 'observations': observations})
    return {'schema_version': 1, 'data_operand_profile': 2, 'status': 'CANDIDATES_LABORATORY',
            'discovery_policy':{'minimum_variants':minimum_variants,'operand_policy':operand_policy,
                                'root_only':root_only,'terminal_only':terminal_only},
            'analyzer_sha256': hashlib.sha256(Path(__file__).read_bytes()).hexdigest(),
            'strict_approval': False, 'closure_proved': False, 'producer_invariant_proved': False,
            'native_execution_validated': False,
            'scope': 'observed bounded dependency-byte shapes; no execution or universal parameter domain',
            'counts': counts, 'families': sorted(families, key=lambda family: family['shape_sha256'])}


def write_report(cases, output, **policy):
    output = ordinary_path(output)
    if output.exists():
        raise FileExistsError('output must be fresh')
    report = discover(cases,**policy)
    # Charge serialization before creating the publication path. JSON uses
    # escaped ASCII, so character count equals its encoded byte count.
    encoded=io.StringIO();total=1
    for chunk in json.JSONEncoder(indent=2).iterencode(report):
        total+=len(chunk)
        if total>MAX_REPORT_BYTES:
            raise ValueError('candidate report exceeds its publication budget')
        encoded.write(chunk)
    encoded.write('\n')
    with output.open('x') as stream:
        stream.write(encoded.getvalue())
    return report


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    inputs = parser.add_mutually_exclusive_group(required=True)
    inputs.add_argument('--case', action='append', type=Path)
    inputs.add_argument('--catalog', type=Path)
    parser.add_argument('--output', required=True, type=Path)
    parser.add_argument('--minimum-variants',type=int,choices=[1,2],default=2)
    parser.add_argument('--operand-policy',choices=['observed','typed'],default='observed')
    parser.add_argument('--root-only',action='store_true')
    parser.add_argument('--terminal-only',action='store_true')
    args = parser.parse_args()
    started = time.monotonic()
    try:
        cases = args.case
        if args.catalog is not None:
            import generate_ee_bank_catalog as catalog_tool
            catalog = ordinary_path(args.catalog)
            ordinary_path(catalog / 'catalog.json')
            cases = catalog_tool.owned_cases(catalog)
        report = write_report(cases, args.output,minimum_variants=args.minimum_variants,
                              operand_policy=args.operand_policy,root_only=args.root_only,
                              terminal_only=args.terminal_only)
    except (ValueError, OSError, KeyError, TypeError, RecursionError) as error:
        parser.error(str(error))
    print(json.dumps({'status': report['status'], 'family_candidates': len(report['families']),
                      'counts': report['counts'], 'seconds': time.monotonic() - started,
                      'strict_approval': False}))


if __name__ == '__main__':
    main()
