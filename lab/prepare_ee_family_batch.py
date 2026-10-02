#!/usr/bin/env python3
"""Own captured EE cases and synthesize a bounded offline laboratory batch.

This tool never launches a game, edits title configuration, or approves closure.
Its output can seed the next conversion job without copying addresses by hand.
"""
import argparse
import hashlib
import json
from pathlib import Path
import re
import subprocess
import time

from discover_ee_data_families import (MAX_CASES, MAX_TOTAL_METADATA_BYTES,
    bounded_bytes, ordinary_path, write_report)
from generate_ee_bank_catalog import owned_cases
from generate_ee_family_catalog import generate
from prepare_ee_miss import prepare


def digest(data):
    return hashlib.sha256(data).hexdigest()


def case_identity(metadata,image):
    return digest(b'nexo-ee-family-case-v1\0'+len(metadata).to_bytes(8,'little')+metadata+
                  len(image).to_bytes(8,'little')+image)


def owned_batch_cases(directory):
    directory=ordinary_path(directory)
    report=json.loads(bounded_bytes(directory/'report.json',8*1024*1024))
    if not isinstance(report,dict) or type(report.get('schema_version')) is not int or \
            report['schema_version']!=1 or report.get('status')!='PUBLISHED_LABORATORY' or \
            report.get('strict_approval') is not False or report.get('closure_proved') is not False or \
            report.get('manifest_published') is not True:
        raise ValueError('previous batch is not a complete laboratory publication')
    records=report.get('case_records')
    count=report.get('owned_cases')
    if type(count) is not int or not 1<=count<=MAX_CASES or \
            not isinstance(records,list) or len(records)!=count or \
            digest(bounded_bytes(directory/'catalog/catalog.json',16*1024*1024))!=report.get('family_catalog_sha256'):
        raise ValueError('previous batch dimensions or manifest identity differ')
    cases=[];seen=set();metadata_bytes=0
    for row in records:
        key=row.get('key') if isinstance(row,dict) else None
        if not isinstance(key,str) or not re.fullmatch('[0-9a-f]{64}',key) or key in seen:
            raise ValueError('invalid previous batch case identity')
        case=directory/'cases'/key
        metadata_bytes+=ordinary_path(case/'bank.json').stat().st_size
        if metadata_bytes>MAX_TOTAL_METADATA_BYTES:
            raise ValueError('previous batch metadata budget exceeded')
        metadata=bounded_bytes(case/'bank.json',8*1024*1024)
        image=bounded_bytes(case/'snapshot.bin',65536)
        if digest(metadata)!=row.get('metadata_sha256') or digest(image)!=row.get('image_sha256') or \
                case_identity(metadata,image)!=key:
            raise ValueError('previous batch case bytes changed')
        seen.add(key);cases.append(case)
    return cases


def prepare_batch(output,family_generator,*,cases=(),captures=(),catalog=None,
                  previous_batch=None,overlay_generator=None,workers=1,
                  families_per_source=32,source_buckets=64):
    output=ordinary_path(output);family_generator=ordinary_path(family_generator)
    cases=list(cases);captures=list(captures)
    input_roots=[]
    if type(workers) is not int or not 1<=workers<=16 or len(captures)>16 or \
            type(families_per_source) is not int or not 1<=families_per_source<=32 or \
            type(source_buckets) is not int or not 1<=source_buckets<=256:
        raise ValueError('invalid offline batch budgets')
    if catalog is not None:
        catalog=ordinary_path(catalog);input_roots.append(catalog)
        cases+=owned_cases(catalog)
    if previous_batch is not None:
        previous_batch=ordinary_path(previous_batch);input_roots.append(previous_batch)
        cases+=owned_batch_cases(previous_batch)
    cases=[ordinary_path(case) for case in cases]
    captures=[ordinary_path(capture) for capture in captures]
    if not cases and not captures or len(cases)+len(captures)>MAX_CASES:
        raise ValueError('empty or oversized case batch')
    if output.exists() or any(output.is_relative_to(source) or source.is_relative_to(output)
                              for source in cases+captures+input_roots):
        raise ValueError('batch output must be fresh and separate from its inputs')
    family_hash=digest(bounded_bytes(family_generator,64*1024*1024))
    overlay_hash=None
    if captures:
        if overlay_generator is None:raise ValueError('captures require an offline overlay generator')
        overlay_generator=ordinary_path(overlay_generator)
        overlay_hash=digest(bounded_bytes(overlay_generator,64*1024*1024))
        for capture in captures:
            for name in ['request.json','snapshot.bin','ee-ram.bin','ee-context.bin']:
                ordinary_path(capture/name)
    output.mkdir()
    started=time.monotonic();stage='capture-preparation'
    report={'schema_version':1,'status':'PREPARING_LABORATORY','strict_approval':False,
            'closure_proved':False,'complete_machine_checkpoint':False,
            'scope':'offline finite captured byte structures; producer/fetch/alias/timing/gameplay unqualified',
            'manifest_published':False,'prepared_captures':0,'owned_cases':0,'duplicate_cases':0,
            'case_records':[],'family_generator_sha256':family_hash,
            'overlay_generator_sha256':overlay_hash,'orchestrator_sha256':digest(Path(__file__).read_bytes())}
    try:
        captured=output/'captured';captured.mkdir()
        for number,capture in enumerate(captures):
            case=captured/str(number)
            prepare(capture,overlay_generator,case)
            cases.append(case);report['prepared_captures']+=1
        stage='case-ownership'
        case_directory=output/'cases';case_directory.mkdir()
        unique={};metadata_bytes=0
        for case in cases:
            metadata_path=ordinary_path(case/'bank.json')
            metadata_bytes+=metadata_path.stat().st_size
            if metadata_bytes>MAX_TOTAL_METADATA_BYTES:
                raise ValueError('aggregate case metadata budget exceeded')
            metadata=bounded_bytes(metadata_path,8*1024*1024)
            image=bounded_bytes(case/'snapshot.bin',65536)
            key=case_identity(metadata,image)
            if key in unique:
                report['duplicate_cases']+=1
                continue
            owned=case_directory/key;owned.mkdir()
            (owned/'bank.json').write_bytes(metadata);(owned/'snapshot.bin').write_bytes(image)
            unique[key]=owned
            report['case_records'].append({'key':key,'source':str(case),
                'metadata_sha256':digest(metadata),'image_sha256':digest(image)})
        report['owned_cases']=len(unique)
        stage='structure-discovery'
        proposals=write_report(unique.values(),output/'candidates.json',minimum_variants=1,
                               operand_policy='typed',root_only=True,terminal_only=True)
        report['candidate_count']=len(proposals['families'])
        report['discovery_policy']=proposals['discovery_policy']
        report['discovery_counts']=proposals['counts']
        stage='catalog-generation'
        manifest=generate(output/'candidates.json',family_generator,output/'catalog',
            entry_policy='root-only',terminal_only=True,workers=workers,
            families_per_source=families_per_source,source_buckets=source_buckets)
        if digest(bounded_bytes(family_generator,64*1024*1024))!=family_hash or \
                (overlay_hash is not None and
                 digest(bounded_bytes(overlay_generator,64*1024*1024))!=overlay_hash):
            raise ValueError('offline frontend changed during the batch')
        report['family_count']=manifest['family_count'];report['source_count']=manifest['source_count']
        report['declined_structures']=len(manifest['rejected'])
        report['family_catalog_sha256']=digest((output/'catalog/catalog.json').read_bytes())
        report['manifest_published']=True;report['status']='PUBLISHED_LABORATORY'
        return report
    except Exception as error:
        report['status']='FAILED';report['failed_step']=stage
        report['error_type']=type(error).__name__
        raise
    finally:
        report['seconds']=time.monotonic()-started
        temporary=output/'report.json.tmp'
        temporary.write_text(json.dumps(report,indent=2)+'\n');temporary.replace(output/'report.json')


def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--case',type=Path,action='append',default=[])
    parser.add_argument('--capture',type=Path,action='append',default=[])
    parser.add_argument('--catalog',type=Path)
    parser.add_argument('--previous-batch',type=Path)
    parser.add_argument('--family-generator',type=Path,required=True)
    parser.add_argument('--overlay-generator',type=Path)
    parser.add_argument('--output',type=Path,required=True)
    parser.add_argument('--workers',type=int,default=1)
    parser.add_argument('--families-per-source',type=int,default=32)
    parser.add_argument('--source-buckets',type=int,default=64)
    args=parser.parse_args()
    try:
        report=prepare_batch(args.output,args.family_generator,cases=args.case,captures=args.capture,
            catalog=args.catalog,previous_batch=args.previous_batch,overlay_generator=args.overlay_generator,
            workers=args.workers,families_per_source=args.families_per_source,source_buckets=args.source_buckets)
        print(json.dumps({key:report[key] for key in ['status','owned_cases','candidate_count',
            'family_count','declined_structures','seconds']}))
    except (ValueError,OSError,KeyError,TypeError,RecursionError,subprocess.TimeoutExpired) as error:
        parser.error('offline batch failed ('+type(error).__name__+'); inspect its fresh receipt')


if __name__=='__main__':main()
