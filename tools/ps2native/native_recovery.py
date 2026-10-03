"""Recover captured EE code and build its incremental native family catalog."""
from pathlib import Path
import json
import sys
import time

from .pipeline import (PipelineError, _ensure_output_outside_sources, _find_tool,
                       _run, _write_json, repo_root, sha256_file)


def ordinary(path):
    path=Path(path).expanduser().absolute()
    if any(parent.is_symlink() for parent in (path,*path.parents)):
        raise PipelineError('recovery paths cannot contain symlinks')
    return path.resolve()


def cmake_source(build):
    cache=ordinary(build/'CMakeCache.txt')
    if not cache.is_file() or cache.stat().st_size>2*1024*1024:
        raise PipelineError('native build requires an existing bounded CMake cache')
    values=[line.removeprefix('CMAKE_HOME_DIRECTORY:INTERNAL=')
            for line in cache.read_text().splitlines()
            if line.startswith('CMAKE_HOME_DIRECTORY:INTERNAL=')]
    if len(values)!=1:
        raise PipelineError('native build cache has no unique source directory')
    source=ordinary(values[0])
    if not (source/'CMakeLists.txt').is_file():
        raise PipelineError('native build source project is unavailable')
    return source


def run_recovery(args):
    root=repo_root();out=ordinary(args.out);captures=ordinary(args.capture_root)
    previous=ordinary(args.previous_batch)
    if type(args.workers) is not int or not 1<=args.workers<=16 or \
            type(args.timeout) is not int or not 1<=args.timeout<=86400:
        raise ValueError('invalid recovery worker/timeout budget')
    if out.exists() or not captures.is_dir() or not previous.is_dir():
        raise PipelineError('recovery requires existing inputs and a fresh output directory')
    _ensure_output_outside_sources(out,root)
    if any(out.is_relative_to(p) or p.is_relative_to(out) for p in (captures,previous)):
        raise PipelineError('recovery output overlaps an input directory')
    build=ordinary(args.native_build) if args.native_build else None
    source=cmake_source(build) if build else None
    family=_find_tool(args.family_generator,'PS2NATIVE_FAMILY_GENERATOR',
                      ['ps2_native_data_family'],root)
    overlay=_find_tool(args.overlay_generator,'PS2NATIVE_OVERLAY_GENERATOR',
                       ['ps2_native_overlay'],root)
    cmake=_find_tool(args.cmake,'PS2NATIVE_CMAKE',['cmake'],root) if build else None
    compiled_catalog=ordinary(args.previous_family_catalog) if args.previous_family_catalog else None
    if compiled_catalog and (not compiled_catalog.is_file() or out.is_relative_to(compiled_catalog.parent)):
        raise PipelineError('previous family catalog is unavailable or overlaps output')
    out.mkdir(parents=True)
    receipt={'schema_version':1,'status':'preparing','strict_approval':False,
             'closure_proved':False,'gameplay_approved':False,'capture_root':str(captures),
             'previous_batch':str(previous),'steps':[],
             'scope':'offline EE recovery and incremental catalog build; no gameplay or full native approval'}

    def execute(name,command):
        step={'name':name,'command':[str(x) for x in command],'status':'running'}
        receipt['steps'].append(step);_write_json(out/'recovery.json',receipt)
        started=time.monotonic()
        try:
            _run(step['command'],out/(name+'.log'),root,args.timeout)
            step['status']='complete'
        except Exception:
            step['status']='failed'
            raise
        finally:
            step['seconds']=time.monotonic()-started
            _write_json(out/'recovery.json',receipt)

    try:
        command=[sys.executable,root/'lab/prepare_ee_family_batch.py',
                 '--capture-root',captures,'--previous-batch',previous,
                 '--family-generator',family,'--overlay-generator',overlay,
                 '--output',out/'batch','--workers',str(args.workers)]
        if compiled_catalog:
            command+=['--previous-family-catalog',compiled_catalog]
        execute('prepare',command)
        report_path=out/'batch/report.json'
        if not report_path.is_file() or report_path.stat().st_size>8*1024*1024:
            raise PipelineError('EE recovery has no bounded batch receipt')
        report=json.loads(report_path.read_text());manifest=out/'batch/catalog/catalog.json'
        if not isinstance(report,dict) or report.get('status')!='PUBLISHED_LABORATORY' or \
                report.get('manifest_published') is not True or report.get('strict_approval') is not False or \
                report.get('closure_proved') is not False or not manifest.is_file() or \
                report.get('family_catalog_sha256')!=sha256_file(manifest) or \
                any(type(report.get(name)) is not int or report[name]<minimum
                    for name,minimum in [('owned_cases',1),('family_count',1),('reused_body_sources',0)]):
            raise PipelineError('EE recovery did not publish an identified laboratory catalog')
        receipt.update(status='generated',family_manifest=str(manifest),
                       family_manifest_sha256=sha256_file(manifest),
                       owned_cases=report['owned_cases'],family_count=report['family_count'],
                       reused_body_sources=report['reused_body_sources'])
        if build:
            execute('configure',[cmake,'-S',source,'-B',build,
                                '-DNEXO_EE_FAMILY_MANIFEST='+str(manifest)])
            execute('build',[cmake,'--build',build,'--target','ps2_ee_compiled_families',
                            '--parallel',str(args.workers)])
            receipt['status']='built';receipt['native_build']=str(build)
        return receipt
    except Exception as error:
        receipt['status']='failed';receipt['error_type']=type(error).__name__
        raise
    finally:
        _write_json(out/'recovery.json',receipt)
