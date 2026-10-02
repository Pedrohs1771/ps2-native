#!/usr/bin/env python3
"""Propose bounded EE data-operand shapes offline; never authorize execution.

Equal guarded bytes suggest a structure. They do not prove its producer,
relocation/control semantics, fetch identity, entry context or hardware fidelity.
Only fixed ordinary operations may expose typed low-16 data fields. Branch
encodings and instruction/register selection remain fixed.
"""
from __future__ import annotations

import argparse
from collections import deque
import hashlib
import io
import json
import os
from pathlib import Path
import re
import struct
import tempfile
import time

RAM_BYTES = 32 * 1024 * 1024
MAX_CASES = 512
MAX_METADATA_BYTES = 8 * 1024 * 1024
MAX_TOTAL_METADATA_BYTES = 64 * 1024 * 1024
MAX_BINDINGS = 2 * 1024 * 1024
MAX_REGIONS = 262144
MAX_SCANNED_WORDS = 8 * 1024 * 1024
MAX_REGION_WORDS = 128
MAX_FAMILIES = 32768
MAX_CANDIDATES = 65536
MAX_REPORT_BYTES = 64 * 1024 * 1024
MAX_PROVENANCE_SHARD_BYTES = 4 * 1024 * 1024
MAX_PROVENANCE_BYTES = 128 * 1024 * 1024
MAX_PROVENANCE_SHARDS = 64
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


def has_delay_slot(word):
    """Conservative boundary classifier, not a semantic support decision."""
    opcode=word>>26
    return opcode in (1,2,3,4,5,6,7,0x14,0x15,0x16,0x17) or \
        (opcode==0 and (word&63) in (8,9)) or \
        (opcode in (0x10,0x11,0x12,0x13) and (word>>21)&31==8)


def canonical_region(words,start=0):
    """First transfer+slot, else 127 linear words; never a short prefix.

    Reserving the last word for a slot makes supported root-only patterns
    prefix-disjoint by their fixed control bits. A truncated window is unknown.
    """
    for offset in range(min(127,len(words)-start)):
        if has_delay_slot(words[start+offset]):
            if start+offset+1>=len(words):
                return None,'truncated_terminal',offset+1
            return offset+2,'terminal',offset+2
    available=min(127,len(words)-start)
    if available==127:return 127,'linear',127
    return None,'truncated_linear',available


def canonical_successors(words,begin,kind):
    """Known normal entries only; indirect destinations need observed bytes."""
    if kind=='linear':
        return [begin+len(words)*4]
    word=words[-2];opcode=word>>26;pc=begin+(len(words)-2)*4
    after=pc+8
    if opcode in (2,3):
        target=((pc+4)&0xf0000000)|((word&0x03ffffff)<<2)
        return sorted({target,after} if opcode==3 else {target})
    if opcode==0:
        return [after] if word&63==9 and (word>>11)&31 and not word&0x001f07c0 else []
    relative=opcode in (4,5,6,7,0x14,0x15,0x16,0x17) or \
        (opcode==1 and (word>>16)&31 in (0,1,2,3,16,17,18,19)) or \
        (opcode in (0x10,0x11,0x12,0x13) and (word>>21)&31==8 and (word>>16)&31<=3)
    if relative:
        displacement=word&0xffff
        if displacement&0x8000:displacement-=0x10000
        return sorted({after,(pc+4+displacement*4)&0xffffffff})
    return []


def discover(cases, *, minimum_variants=2, operand_policy='observed',
             root_only=False, terminal_only=False,region_policy='metadata'):
    if type(minimum_variants) is not int or minimum_variants not in (1,2) or \
            operand_policy not in ('observed','typed') or type(root_only) is not bool or \
            type(terminal_only) is not bool or region_policy not in ('metadata','canonical-v1') or \
            (region_policy=='canonical-v1' and
             (not root_only or terminal_only or operand_policy!='typed')):
        raise ValueError('invalid bounded discovery policy')
    cases = list(cases)
    if not 1 <= len(cases) <= MAX_CASES:
        raise ValueError('expected a bounded nonempty case list')
    groups = {}
    entry_groups = {}
    counts = {'cases': len(cases), 'bindings': 0, 'regions': 0,
              'scanned_words': 0, 'oversized_regions_skipped': 0,
              'nonroot_bindings_skipped':0,'nonterminal_regions_skipped':0,
              'canonical_linear_regions':0,'canonical_terminal_regions':0,
              'canonical_successor_roots':0,'canonical_external_successors':0,
              'truncated_linear_regions_skipped':0,'truncated_terminal_regions_skipped':0}
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
        image_words=struct.unpack('<'+'I'*(size//4),image) if region_policy=='canonical-v1' else None
        pending=deque(sorted(regions))
        queued_roots={begin for begin,_ in regions}
        if image_words is not None and entry not in queued_roots:
            pending.append((entry,0));queued_roots.add(entry)
        while pending:
            begin,span=pending.popleft()
            counts['regions'] += 1
            if counts['regions'] > MAX_REGIONS:
                raise ValueError('aggregate region budget exceeded')
            if image_words is not None:
                extent,kind,scanned=canonical_region(image_words,(begin-base)//4)
                counts['scanned_words']+=scanned
                if counts['scanned_words']>MAX_SCANNED_WORDS:
                    raise ValueError('aggregate word budget exceeded')
                if extent is None:
                    counts[kind+'_regions_skipped']+=1
                    continue
                counts['canonical_'+kind+'_regions']+=1
                span=extent*4
                for successor in canonical_successors(
                        image_words[(begin-base)//4:(begin-base)//4+extent],begin,kind):
                    if not base<=successor<base+size:
                        counts['canonical_external_successors']+=1
                    elif successor not in queued_roots:
                        queued_roots.add(successor);pending.append((successor,0))
                        counts['canonical_successor_roots']+=1
            if span // 4 > MAX_REGION_WORDS:
                counts['oversized_regions_skipped'] += 1
                continue
            if image_words is None:counts['scanned_words'] += span // 4
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
            entry_groups.setdefault((normalized,begin,raw),set()).update(
                {0} if image_words is not None else regions[(begin,span)])

    families = []
    for normalized, group in groups.items():
        if len({raw for _, raw in group}) < minimum_variants:
            continue
        if len(families) >= MAX_CANDIDATES:
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
                                'root_only':root_only,'terminal_only':terminal_only,'region_policy':region_policy},
            'analyzer_sha256': hashlib.sha256(Path(__file__).read_bytes()).hexdigest(),
            'strict_approval': False, 'closure_proved': False, 'producer_invariant_proved': False,
            'native_execution_validated': False,
            'scope': 'observed bounded dependency-byte shapes; no execution or universal parameter domain',
            'counts': counts, 'families': sorted(families, key=lambda family: family['shape_sha256'])}


def encoded_json(value,limit,*,compact=False):
    encoded=io.StringIO();total=1
    encoder=json.JSONEncoder(separators=(',',':')) if compact else json.JSONEncoder(indent=2)
    for chunk in encoder.iterencode(value):
        total+=len(chunk)
        if total>limit:raise ValueError('candidate report exceeds its publication budget')
        encoded.write(chunk)
    encoded.write('\n')
    return encoded.getvalue().encode('ascii')


def publish_bytes(path,data):
    """Publish a complete ordinary file without overwriting another owner."""
    path=ordinary_path(path)
    with tempfile.NamedTemporaryFile(dir=path.parent,delete=False) as stream:
        temporary=Path(stream.name)
        try:
            stream.write(data);stream.flush()
            os.link(temporary,path)
        finally:
            temporary.unlink(missing_ok=True)


def compact_report(report):
    """Separate executable proposals from hash-identified observation details."""
    if not report['families']:
        raise ValueError('canonical publication has no candidate families')
    origins=sorted({(row['image_sha256'],row['metadata_sha256'],row['base'])
        for family in report['families'] for observation in family['observations']
        for row in observation['origins']})
    origin_index={origin:index for index,origin in enumerate(origins)}
    families=[];shards=[];rows=[];total=0
    prefix=b'{"schema_version":1,"status":"OBSERVED_LABORATORY","strict_approval":false,"records":['
    suffix=b']}\n';size=len(prefix)+len(suffix)
    def flush():
        nonlocal rows,size,total
        if not rows:return
        data=prefix+b','.join(rows)+suffix;total+=len(data)
        if total>MAX_PROVENANCE_BYTES or len(shards)>=MAX_PROVENANCE_SHARDS:
            raise ValueError('candidate provenance exceeds its publication budget')
        digest=hashlib.sha256(data).hexdigest()
        shards.append(({'name':'ee-candidate-provenance-'+digest+'.json','sha256':digest,
                        'bytes':len(data),'record_count':len(rows)},data))
        rows=[];size=len(prefix)+len(suffix)
    for family_index,family in enumerate(report['families']):
        families.append({**{key:value for key,value in family.items()
                            if key not in ('parameters','observations','word_count')},
                         'observation_count':len(family['observations'])})
        for observation in family['observations']:
            record={**{key:value for key,value in observation.items() if key!='origins'},
                    'family_index':family_index,'origin_ids':[origin_index[(row['image_sha256'],
                        row['metadata_sha256'],row['base'])] for row in observation['origins']]}
            data=json.dumps(record,separators=(',',':')).encode('ascii')
            if size+len(data)+(1 if rows else 0)>MAX_PROVENANCE_SHARD_BYTES:
                flush()
            if size+len(data)>MAX_PROVENANCE_SHARD_BYTES:
                raise ValueError('one candidate observation exceeds its shard budget')
            size+=len(data)+(1 if rows else 0);rows.append(data)
    flush()
    summary={**report,'schema_version':2,'families':families,
        'provenance':{'origins':[{'image_sha256':image,'metadata_sha256':metadata,'base':base}
            for image,metadata,base in origins],'shards':[row for row,_ in shards],
            'total_bytes':total,'record_count':sum(row['record_count'] for row,_ in shards)}}
    return summary,shards


def read_report(path,*,observations=False,expected_sha256=None):
    """Read v1 or validate bounded v2 provenance before any conversion work."""
    path=ordinary_path(path);data=bounded_bytes(path,MAX_REPORT_BYTES)
    if expected_sha256 is not None and (not isinstance(expected_sha256,str) or
            not re.fullmatch('[0-9a-f]{64}',expected_sha256) or
            hashlib.sha256(data).hexdigest()!=expected_sha256):
        raise ValueError('candidate report identity differs')
    report=json.loads(data)
    if not isinstance(report,dict):raise ValueError('candidate report must be an object')
    if type(report.get('schema_version')) is not int or report['schema_version'] not in (1,2):
        raise ValueError('unsupported candidate schema')
    if report['schema_version']==1:return report
    families=report.get('families');provenance=report.get('provenance')
    if report.get('status')!='CANDIDATES_LABORATORY' or report.get('strict_approval') is not False or \
            report.get('closure_proved') is not False or not isinstance(families,list) or \
            not 1<=len(families)<=MAX_CANDIDATES or not isinstance(provenance,dict):
        raise ValueError('invalid compact candidate publication')
    origins=provenance.get('origins');shards=provenance.get('shards')
    if not isinstance(origins,list) or not 1<=len(origins)<=MAX_CASES or \
            not isinstance(shards,list) or not 1<=len(shards)<=MAX_PROVENANCE_SHARDS:
        raise ValueError('invalid candidate provenance dimensions')
    for origin in origins:
        if not isinstance(origin,dict) or set(origin)!=set(('image_sha256','metadata_sha256','base')) or \
                any(not isinstance(origin[field],str) or not re.fullmatch('[0-9a-f]{64}',origin[field])
                    for field in ('image_sha256','metadata_sha256')) or \
                not integer(origin['base']) or origin['base']&3 or not 0<=origin['base']<RAM_BYTES:
            raise ValueError('invalid candidate origin identity')
    observed=[[] for _ in families] if observations else None
    counts=[0]*len(families);total=0;records=0;names=set();parameters=[]
    for family in families:
        if not isinstance(family,dict) or not integer(family.get('observation_count')) or \
                not 1<=family['observation_count']<=MAX_REGIONS or \
                not isinstance(family.get('guard_words'),list) or not isinstance(family.get('guard_masks'),list) or \
                not 1<=len(family['guard_words'])<=128 or len(family['guard_words'])!=len(family['guard_masks']) or \
                ('word_count' in family and (not integer(family['word_count']) or
                 family['word_count']!=len(family['guard_words']))) or \
                any(not integer(value) or not 0<=value<=0xffffffff
                    for value in family['guard_words']+family['guard_masks']):
            raise ValueError('invalid compact family dimensions')
        family['word_count']=len(family['guard_words'])
        parameters.append([{'word_index':index,'kind':parameter_kind(word),'bits':16}
            for index,(word,mask) in enumerate(zip(family['guard_words'],family['guard_masks']))
            if mask==0xffff0000])
    for shard in shards:
        if not isinstance(shard,dict):raise ValueError('invalid provenance shard')
        digest=shard.get('sha256');name=shard.get('name');size=shard.get('bytes')
        if not isinstance(digest,str) or not re.fullmatch('[0-9a-f]{64}',digest) or \
                name!='ee-candidate-provenance-'+digest+'.json' or name in names or \
                not integer(size) or not 0<size<=MAX_PROVENANCE_SHARD_BYTES:
            raise ValueError('invalid provenance shard identity')
        total+=size
        if total>MAX_PROVENANCE_BYTES:raise ValueError('aggregate provenance budget exceeded')
        data=bounded_bytes(path.parent/name,MAX_PROVENANCE_SHARD_BYTES)
        if len(data)!=size or hashlib.sha256(data).hexdigest()!=digest:
            raise ValueError('candidate provenance bytes changed')
        detail=json.loads(data);items=detail.get('records') if isinstance(detail,dict) else None
        if not isinstance(detail,dict) or type(detail.get('schema_version')) is not int or \
                detail['schema_version']!=1 or detail.get('status')!='OBSERVED_LABORATORY' or \
                detail.get('strict_approval') is not False or not isinstance(items,list) or \
                not integer(shard.get('record_count')) or len(items)!=shard['record_count'] or not items:
            raise ValueError('invalid provenance shard records')
        records+=len(items)
        if records>MAX_REGIONS:raise ValueError('aggregate observation budget exceeded')
        for item in items:
            index=item.get('family_index') if isinstance(item,dict) else None
            if not integer(index) or not 0<=index<len(families):
                raise ValueError('invalid observed family index')
            pc=item.get('pc');values=item.get('parameters');ids=item.get('origin_ids')
            word_digest=item.get('word_sha256')
            if not integer(pc) or pc&3 or not 0<=pc<RAM_BYTES or \
                    not isinstance(word_digest,str) or not re.fullmatch('[0-9a-f]{64}',word_digest) or \
                    item.get('normal_entry_offsets')!=[0] or not isinstance(values,list) or \
                    len(values)!=len(parameters[index]) or \
                    any(not integer(value) or not -32768<=value<=65535 for value in values) or \
                    not isinstance(ids,list) or not ids or len(ids)>MAX_CASES or \
                    any(not integer(value) or not 0<=value<len(origins) for value in ids) or \
                    len(set(ids))!=len(ids):
                raise ValueError('invalid candidate observation')
            counts[index]+=1
            if observations:
                observed[index].append({**{key:value for key,value in item.items()
                    if key not in ('family_index','origin_ids')},'origins':[origins[value] for value in ids]})
        names.add(name)
    if not integer(provenance.get('total_bytes')) or total!=provenance['total_bytes'] or \
            not integer(provenance.get('record_count')) or records!=provenance['record_count'] or \
            any(count!=family['observation_count'] for count,family in zip(counts,families)):
        raise ValueError('candidate provenance totals differ')
    if observations:
        report['families']=[{**{key:value for key,value in family.items() if key!='observation_count'},
            'parameters':parameters[index],'observations':observed[index]}
            for index,family in enumerate(families)]
    return report


def write_report(cases, output, **policy):
    output = ordinary_path(output)
    if output.exists():
        raise FileExistsError('output must be fresh')
    report = discover(cases,**policy)
    canonical=policy.get('region_policy')=='canonical-v1'
    shards=[]
    if canonical:report,shards=compact_report(report)
    encoded=encoded_json(report,MAX_REPORT_BYTES,compact=canonical)
    for row,data in shards:
        path=ordinary_path(output.parent/row['name'])
        if path.exists():
            if bounded_bytes(path,MAX_PROVENANCE_SHARD_BYTES)!=data:
                raise ValueError('existing provenance identity differs')
        else:publish_bytes(path,data)
    publish_bytes(output,encoded)
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
    parser.add_argument('--region-policy',choices=['metadata','canonical-v1'],default='metadata')
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
                              terminal_only=args.terminal_only,region_policy=args.region_policy)
    except (ValueError, OSError, KeyError, TypeError, RecursionError) as error:
        parser.error(str(error))
    print(json.dumps({'status': report['status'], 'family_candidates': len(report['families']),
                      'counts': report['counts'], 'seconds': time.monotonic() - started,
                      'strict_approval': False}))


if __name__ == '__main__':
    main()
