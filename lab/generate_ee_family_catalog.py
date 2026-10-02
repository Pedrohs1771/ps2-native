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
from concurrent.futures import ThreadPoolExecutor

from discover_ee_data_families import (MAX_FAMILIES, MAX_CANDIDATES, bounded_bytes,
    ordinary_path, parameter_kind, canonical_region, read_report)


def uint(value):
    return type(value) is int and 0 <= value <= 0xffffffff


def encode(words):
    return struct.pack('<' + 'I' * len(words), *words)


def reusable_body_sources(path,bodies,families_per_source):
    """Reuse complete units only when their current emitted bytes are exact."""
    path=ordinary_path(path);raw=bounded_bytes(path,16*1024*1024);previous=json.loads(raw)
    if not isinstance(previous,dict) or type(previous.get('schema_version')) is not int or \
            previous['schema_version']!=2 or previous.get('symbol')!='compiledEeFamilyProgram' or \
            previous.get('strict_approval') is not False or previous.get('closure_proved') is not False:
        raise ValueError('invalid previous family catalog')
    names=previous.get('sources');hashes=previous.get('sha256');count=previous.get('source_count')
    families=previous.get('family_count');records=previous.get('families')
    if type(families) is not int or not 1<=families<=MAX_FAMILIES or \
            not isinstance(records,list) or len(records)!=families or \
            type(count) is not int or not 2<=count<=families+1 or not isinstance(names,list) or \
            len(names)!=count or any(not isinstance(name,str) for name in names) or \
            len(set(names))!=count or names.count('ee_family_catalog.cpp')!=1 or \
            not isinstance(hashes,dict) or set(hashes)!=set(names):
        raise ValueError('invalid previous family source dimensions')
    available=dict(bodies);sources={};retained=set()
    for name in names:
        index=name=='ee_family_catalog.cpp'
        if not index and not re.fullmatch(r'ee_family_[0-9a-f]{64}\.cpp',name):
            raise ValueError('invalid previous family source name')
        data=bounded_bytes(path.parent/name,8*1024*1024 if index else 1024*1024)
        digest=hashlib.sha256(data).hexdigest()
        if hashes[name]!=digest or (not index and name!='ee_family_'+digest+'.cpp'):
            raise ValueError('previous family source identity differs')
        if index:continue
        code=data.decode();keys=re.findall(r'^namespace ee_family_([0-9a-f]{64}) \{$',code,re.M)
        if not keys or len(keys)>32 or len(set(keys))!=len(keys):
            raise ValueError('invalid previous family body structure')
        if len(keys)>families_per_source:continue
        if any(key not in available for key in keys):continue
        if code!='\n'.join(available[key] for key in keys):continue
        if retained.intersection(keys):
            raise ValueError('duplicate previous family body ownership')
        sources[name]=code;retained.update(keys)
    return sources,retained,hashlib.sha256(raw).hexdigest()


def root_shape(case, *, typed_data=False):
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
              'base': base, 'entry': entry, 'source_begin': begin, 'source_bytes': size,
              'operand_policy':'typed-data' if typed_data else 'exact'}
    masks=[0xffff0000 if typed_data and parameter_kind(word) else 0xffffffff for word in words]
    return words, masks, [entry-begin], ledger


def generate(report_path, generator, output, *, cases=(), only_shapes=(), entry_policy='observed',
             terminal_only=False, families_per_source=16, root_data_parameters=False,
             source_buckets=128, workers=1, previous_catalog=None):
    output = ordinary_path(output)
    if output.exists():
        raise ValueError('family catalog output must be fresh')
    generator = ordinary_path(generator)
    report_bytes = bounded_bytes(report_path, 64 * 1024 * 1024)
    report = read_report(report_path,expected_sha256=hashlib.sha256(report_bytes).hexdigest())
    if not isinstance(report, dict) or report.get('schema_version') not in (1,2) or \
            type(report['schema_version']) is not int or report.get('status') != 'CANDIDATES_LABORATORY' or \
            report.get('strict_approval') is not False:
        raise ValueError('expected a laboratory candidate report')
    families = report.get('families')
    if not isinstance(families, list) or not 1 <= len(families) <= MAX_CANDIDATES or len(cases) > 16 or \
            type(families_per_source) is not int or not 1 <= families_per_source <= 32 or type(terminal_only) is not bool or \
            type(root_data_parameters) is not bool or type(source_buckets) is not int or not 1 <= source_buckets <= 256 or \
            type(workers) is not int or not 1 <= workers <= 16:
        raise ValueError('family input budget exceeded')
    selected = set(only_shapes)
    if entry_policy not in ('observed','root-only'):
        raise ValueError('invalid family entry policy')
    policy=report.get('discovery_policy',{})
    if not isinstance(policy,dict):raise ValueError('invalid discovery policy')
    region_policy=policy.get('region_policy','metadata')
    canonical=region_policy=='canonical-v1'
    if region_policy not in ('metadata','canonical-v1') or (canonical and
        (entry_policy!='root-only' or terminal_only or cases or policy.get('root_only') is not True or
         policy.get('terminal_only') is not False or policy.get('operand_policy')!='typed')):
        raise ValueError('canonical root regions cannot mix other entry or operand policies')
    if any(not isinstance(key, str) or not re.fullmatch('[0-9a-f]{64}', key) for key in selected):
        raise ValueError('invalid requested shape identity')
    shapes = {}

    def add(words, masks, offsets):
        if not 1 <= len(words) <= 128 or len(words) != len(masks) or \
                any(not uint(word) for word in words + masks) or \
                any(mask not in (0xffffffff, 0xffff0000) for mask in masks) or \
                not isinstance(offsets,list) or not 1 <= len(offsets) <= len(words) or \
                any(not uint(offset) or offset & 3 or offset >= len(words) * 4 for offset in offsets) or \
                len(set(offsets)) != len(offsets):
            raise ValueError('invalid bounded candidate structure')
        words = [word & mask for word, mask in zip(words, masks)]
        if canonical and (offsets!=[0] or canonical_region(words)[0]!=len(words) or
                masks!=[0xffff0000 if parameter_kind(word) else 0xffffffff for word in words]):
            raise ValueError('candidate does not satisfy the canonical region contract')
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
        offsets = family.get('normal_entry_offsets',list(range(0,len(words)*4,4)))
        if not isinstance(offsets,list) or not 1 <= len(offsets) <= len(words) or \
                any(not uint(offset) or offset & 3 or offset >= len(words)*4 for offset in offsets) or \
                len(set(offsets))!=len(offsets):
            raise ValueError('invalid declared family normal entries')
        if entry_policy == 'root-only':
            if canonical and offsets!=[0]:
                raise ValueError('canonical regions require exactly the root normal entry')
            if not isinstance(offsets,list) or 0 not in offsets:
                continue
            offsets = [0]
        add(words, masks, offsets)
    if selected != found and selected:
        raise ValueError('requested candidate shape missing')
    root_cases = []
    for case in cases:
        words, masks, offsets, ledger = root_shape(case,typed_data=root_data_parameters)
        add(words, masks, offsets)
        root_cases.append(ledger)
    if not shapes or len(shapes) > MAX_CANDIDATES:
        raise ValueError('combined family budget exceeded')
    generator_hash = hashlib.sha256(bounded_bytes(generator, 64 * 1024 * 1024)).hexdigest()
    sources, descriptors, rejected, bodies, admitted = {}, [], [], [], []
    with tempfile.TemporaryDirectory() as temporary:
        def emit_shape(item):
            key, (words, masks, offsets) = item
            if terminal_only:
                branch=words[-2] if len(words)>=2 else 0
                opcode=branch>>26
                terminal=opcode in (1,2,3,4,5,6,7,0x14,0x15,0x16,0x17) or \
                    (opcode==0 and (branch&63) in (8,9))
                if not terminal:
                    return key, None, {'key':key,'reason':'batch policy requires a complete terminal transfer'}
            work = Path(temporary) / key
            work.mkdir()
            (work / 'words.bin').write_bytes(encode(words))
            (work / 'masks.bin').write_bytes(encode(masks))
            cpp = work / 'family.cpp'
            result = subprocess.run([str(generator), str(work/'words.bin'), str(work/'masks.bin'), str(cpp)],
                                    capture_output=True, text=True, timeout=30)
            if result.returncode:
                return key, None, {'key': key, 'reason': result.stderr[:4096]}
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
            return key, source, None

        ordered = sorted(shapes.items())
        # At most workers subprocesses and futures exist at once. Result order
        # and emitted bytes are independent of scheduling or worker count.
        with ThreadPoolExecutor(max_workers=workers) as executor:
            for start in range(0,len(ordered),workers):
                for key, source, failure in executor.map(emit_shape,ordered[start:start+workers]):
                    if failure is not None:
                        rejected.append(failure)
                        continue
                    words, masks, offsets = shapes[key]
                    if len(descriptors)>=MAX_FAMILIES:
                        raise ValueError('admitted family budget exceeded; no structures were discarded')
                    bodies.append((key,source))
                    descriptors.append('ee_family_' + key + '_descriptor')
                    admitted.append({'key':key,'words':words,'masks':masks,'normal_entry_offsets':sorted(offsets)})
    if hashlib.sha256(bounded_bytes(generator, 64 * 1024 * 1024)).hexdigest() != generator_hash or not descriptors:
        raise ValueError('family generator changed or produced no supported structure')
    retained=set();previous_digest=None
    if previous_catalog is not None:
        sources,retained,previous_digest=reusable_body_sources(previous_catalog,bodies,families_per_source)
    reused_source_count=len(sources)
    buckets={}
    for key,body in bodies:
        if key in retained:continue
        size=len(body.encode())
        if size>1024*1024:
            raise ValueError('single family source exceeds its bound')
        buckets.setdefault(int(key[:8],16)%source_buckets,[]).append((key,body,size))
    def publish_rows(rows):
        if rows:
            code='\n'.join(rows)
            name='ee_family_'+hashlib.sha256(code.encode()).hexdigest()+'.cpp'
            sources[name]=code
    def publish_leaf(rows,depth=0):
        if not rows:return
        size=sum(row[2] for row in rows)+len(rows)-1
        if len(rows)<=families_per_source and size<=1024*1024:
            publish_rows([row[1] for row in rows])
            return
        if depth>=256:
            raise ValueError('distinct family identities cannot share an oversized leaf')
        # Fixed hash prefixes preserve every unrelated leaf when a new body is
        # inserted. Positional chunks shifted all later bodies in a bucket.
        children=[[],[]]
        for row in rows:children[(int(row[0],16)>>(255-depth))&1].append(row)
        for child in children:publish_leaf(child,depth+1)
    for bucket in sorted(buckets):publish_leaf(buckets[bucket])
    index = '#include "ps2_ee_data_family.h"\n#include <array>\n'
    index += ''.join('ps2native::ee_family::Family ' + name + '();\n' for name in descriptors)
    index += 'const ps2native::ee_family::Program &compiledEeFamilyProgram(){\nstatic const std::array<ps2native::ee_family::Family,' + str(len(descriptors)) + '> families{{\n'
    index += ''.join(name+'(),\n' for name in descriptors) + '}};\nstatic const ps2native::ee_family::Program program{families};return program;}\n'
    if len(index.encode())>8*1024*1024:
        raise ValueError('family index source exceeds its bound')
    sources['ee_family_catalog.cpp'] = index
    manifest = {'schema_version': 2, 'symbol': 'compiledEeFamilyProgram', 'sources': sorted(sources),
                'sha256': {name: hashlib.sha256(code.encode()).hexdigest() for name, code in sources.items()},
                'family_count': len(descriptors), 'generator_sha256': generator_hash,
                'source_count':len(sources),'families_per_source':families_per_source,'families':admitted,
                'source_buckets':source_buckets,'root_data_parameters':root_data_parameters,
                'source_partition':'preserved-units-v1' if previous_catalog is not None else 'hash-trie-v1',
                'previous_catalog_sha256':previous_digest,
                'reused_body_sources':reused_source_count,'reused_families':len(retained),
                'generation_workers':workers,
                'publisher_sha256': hashlib.sha256(Path(__file__).read_bytes()).hexdigest(),
                'candidate_report_sha256': hashlib.sha256(report_bytes).hexdigest(), 'rejected': rejected,
                'selected_shapes': sorted(selected), 'root_cases': root_cases,
                'entry_policy': entry_policy,
                'region_policy':region_policy,
                'terminal_only':terminal_only,'data_operand_profile':2,
                'strict_approval': False, 'closure_proved': False,
                'scope': 'finite experimental precompiled EE structures; no producer/fetch/fidelity/game qualification'}
    encoded_manifest=(json.dumps(manifest,separators=(',',':')) if canonical else
                      json.dumps(manifest,indent=2))+'\n'
    if len(encoded_manifest.encode())>16*1024*1024:
        raise ValueError('family manifest exceeds its bound')
    output.mkdir()
    for name, code in sources.items():
        (output / name).write_text(code)
    (output / 'catalog.json').write_text(encoded_manifest)
    return manifest


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--candidates', required=True, type=Path)
    parser.add_argument('--generator', required=True, type=Path)
    parser.add_argument('--output', required=True, type=Path)
    parser.add_argument('--case', action='append', default=[], type=Path)
    parser.add_argument('--shape', action='append', default=[])
    parser.add_argument('--entry-policy', choices=['observed','root-only'],default='observed')
    parser.add_argument('--terminal-only', action='store_true')
    parser.add_argument('--families-per-source',type=int,default=16)
    parser.add_argument('--source-buckets',type=int,default=128)
    parser.add_argument('--root-data-parameters',action='store_true')
    parser.add_argument('--workers',type=int,default=1)
    parser.add_argument('--previous-catalog',type=Path,
                        help='Preserve prior source units with exactly matching current emitted bodies')
    args = parser.parse_args()
    try:
        result = generate(args.candidates, args.generator, args.output, cases=args.case,
                          only_shapes=args.shape,entry_policy=args.entry_policy,
                          terminal_only=args.terminal_only,families_per_source=args.families_per_source,
                          root_data_parameters=args.root_data_parameters,source_buckets=args.source_buckets,
                          workers=args.workers,previous_catalog=args.previous_catalog)
        print(json.dumps({'families': result['family_count'], 'rejected': len(result['rejected']), 'strict_approval': False}))
    except (ValueError, OSError, KeyError, TypeError, RecursionError, subprocess.TimeoutExpired) as error:
        parser.error(str(error))


if __name__ == '__main__':
    main()
