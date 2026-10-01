#!/usr/bin/env python3
"""Map instruction issue and callback-tail time for one original VIF replay.

PCSX2's diagnostic point is after its leading tick and stall checks. The model
records just before issue after stalls. The fixed one-tick coordinate change
is part of this identified adapter, never estimated from the compared data.
It does not assert equality of both hidden pipeline state representations.
"""
import argparse
import hashlib
import json
from pathlib import Path
import struct
import zlib

def bounded(path,minimum,maximum):
    if not minimum<=path.stat().st_size<=maximum:raise ValueError('invalid artifact extent: '+str(path))
    data=path.read_bytes()
    if not minimum<=len(data)<=maximum:raise ValueError('artifact changed during read')
    return data

def payload(data,magic,variant,maximum):
    if not 24<=len(data)<=maximum or data[:8]!=magic:raise ValueError('invalid canonical header')
    version,actual,size,crc=struct.unpack_from('<4I',data,8)
    if version!=1 or actual!=variant or size!=len(data)-24 or crc!=zlib.crc32(data[:20]+data[24:]):
        raise ValueError('invalid canonical version, variant, extent or checksum')
    return memoryview(data)[24:]

def decode_issues(data,variant):
    body=payload(data,b'NEXOVPI\0',variant,6*1024*1024)
    if len(body)<4:raise ValueError('missing issue count')
    count=struct.unpack_from('<I',body)[0]
    if count>262144 or len(body)!=4+count*20:raise ValueError('invalid issue count or trailing fields')
    rows=list(struct.iter_unpack('<Q3I',body[4:]))
    for i,(cycle,pc,lower,upper) in enumerate(rows):
        if pc>=16384 or pc&7 or (i and cycle<=rows[i-1][0]):raise ValueError('invalid issue PC or nonincreasing clock')
    return rows

def vu_clock(data):
    payload(data,b'NEXOVU\0\0',1,80*1024)
    if len(data)!=70249:raise ValueError('unidentified model VU schema extent')
    return struct.unpack_from('<Q',data,648)[0]

def composite_vu(data):
    body=payload(data,b'NEXOVCS\0',1,272*1024*1024)
    offset=16;components=[]
    for _ in range(6):
        if offset+4>len(body):raise ValueError('truncated VIF component length')
        size=struct.unpack_from('<I',body,offset)[0];offset+=4
        if size>len(body)-offset:raise ValueError('truncated VIF component')
        components.append(bytes(body[offset:offset+size]));offset+=size
    if offset!=len(body):raise ValueError('trailing VIF component fields')
    return components[3]

def callback_events(data):
    body=payload(data,b'NEXOVTR\0',1,64*1024*1024)
    if len(body)<4:raise ValueError('missing event count')
    count=struct.unpack_from('<I',body)[0];offset=4;calls=[];previous=0
    if count>262144:raise ValueError('event count exceeds bound')
    for _ in range(count):
        if offset+33>len(body):raise ValueError('truncated event')
        kind,cycle,op,pc,top,itop,bank,size=struct.unpack_from('<BQ6I',body,offset);offset+=33
        if size>len(body)-offset or cycle<previous:raise ValueError('invalid event extent or clock')
        offset+=size;previous=cycle
        if kind==1:
            if size or op not in (0x14,0x15,0x17):raise ValueError('invalid VU callback event')
            calls.append({'cycle':cycle,'opcode':op,'pc':pc,'top':top,'itop':itop,'bank':bank})
        elif kind not in (2,3):raise ValueError('unknown event kind')
    if offset!=len(body) or not 1<=len(calls)<=256:raise ValueError('invalid callback inventory')
    return calls

def compare_call(model,reference,model_start,reference_start,model_end,reference_end):
    if not model or not reference:raise ValueError('missing callback issue history')
    if model_end<model_start or reference_end<reference_start:raise ValueError('invalid callback clock horizon')
    for rows,start,end,phase in ((model,model_start,model_end,0),(reference,reference_start,reference_end,1)):
        if any(row[0]<start+phase or row[0]>end for row in rows):raise ValueError('issue clock outside its callback')
    first_order=next((i for i,(a,b) in enumerate(zip(model,reference)) if a[1:]!=b[1:]),None)
    sequence=first_order is None and len(model)==len(reference)
    if first_order is None and not sequence:first_order=min(len(model),len(reference))
    result={'model_issues':len(model),'reference_issues':len(reference),'instruction_sequence_equal':sequence,
            'first_order_difference':None if sequence else {'index':first_order},
            'model_elapsed':model_end-model_start,'reference_elapsed':reference_end-reference_start,
            'elapsed_delta':(model_end-model_start)-(reference_end-reference_start),
            'first_issue_timing_difference':None,'issue_clock_delta_changes':None,
            'model_tail_cycles':model_end-model[-1][0],
            'reference_tail_cycles':reference_end-(reference[-1][0]-1)}
    if not sequence:return result
    changes=[];previous=0
    for i,(a,b) in enumerate(zip(model,reference)):
        mc=a[0]-model_start;rc=b[0]-1-reference_start;delta=mc-rc
        row={'index':i,'pc':a[1],'lower':a[2],'upper':a[3],
             'model_relative_issue':mc,'reference_relative_issue':rc,'cumulative_delta':delta}
        if delta and result['first_issue_timing_difference'] is None:result['first_issue_timing_difference']=dict(row)
        if delta!=previous:changes.append(dict(row,delta_change=delta-previous))
        previous=delta
    result['issue_clock_delta_changes']=changes
    result['last_issue_delta']=previous
    result['tail_delta']=result['model_tail_cycles']-result['reference_tail_cycles']
    if result['elapsed_delta']!=previous+result['tail_delta']:
        raise ValueError('issue and tail clock decomposition is inconsistent')
    return result

def map_case(case,model_directory,reference_directory):
    for directory in (case,model_directory,reference_directory):
        if bounded(directory/'.complete',1,1)!=b'\x01':raise ValueError('incomplete comparison input')
    model_receipt=json.loads(bounded(model_directory/'report.json',1,1024*1024))
    reference_receipt=json.loads(bounded(reference_directory/'report.json',1,1024*1024))
    if model_receipt.get('schema')!='nexo.vif.model.issue.trace.v1' or not model_receipt.get('matches_original_state_and_events'):
        raise ValueError('model trace did not retain its original full boundary')
    if reference_receipt.get('schema')!='nexo.vu.independent.reference.v1' or not reference_receipt.get('issue_trace',{}).get('enabled'):
        raise ValueError('independent reference has no full issue history')
    model_events=bounded(case/'events.nexo',24,64*1024*1024)
    reference_events=bounded(reference_directory/'reference-events.nexo',24,64*1024*1024)
    calls=callback_events(model_events);rcalls=callback_events(reference_events)
    if model_receipt.get('expected_callbacks')!=len(calls) or model_receipt.get('completed_traces')!=len(calls):
        raise ValueError('model trace receipt count differs from original callbacks')
    if len(calls)!=len(rcalls) or any({k:v for k,v in a.items() if k!='cycle'}!={k:v for k,v in b.items() if k!='cycle'} for a,b in zip(calls,rcalls)):
        raise ValueError('original callback arguments or bank identities changed')
    ref_issues=decode_issues(bounded(reference_directory/'reference-issues.nexo',28,6*1024*1024),2)
    starts=reference_receipt['issue_trace']['callback_starts']
    if len(starts)!=len(calls) or starts[0]!=0 or any(type(i)!=int for i in starts) or any(b<=a for a,b in zip(starts,starts[1:])) or starts[-1]>=len(ref_issues):
        raise ValueError('invalid reference callback issue partition')
    outputs=reference_receipt.get('callback_outputs',[])
    if len(outputs)!=len(calls) or reference_receipt['issue_trace'].get('count')!=len(ref_issues):
        raise ValueError('reference issue or callback receipt count changed')
    for index,output in enumerate(outputs):
        clock=output.get('cycles')
        if output.get('index')!=index or type(clock)!=int or not rcalls[index]['cycle']<clock<2**64:
            raise ValueError('invalid reference callback output clock')
        if index+1<len(rcalls) and clock!=rcalls[index+1]['cycle']:
            raise ValueError('reference callback output differs from its next event')
    directories=[p for p in model_directory.iterdir() if p.is_dir()]
    if any(not p.name.startswith('vu-input-') or not p.name[9:].isdigit() for p in directories):
        raise ValueError('unidentified model callback directory')
    directories.sort(key=lambda p:int(p.name[9:]))
    if len(directories)!=len(calls):raise ValueError('model callback history is missing or has extra entries')
    last_model=vu_clock(composite_vu(bounded(case/'output-state.nexo',24,272*1024*1024)))
    last_reference=outputs[-1]['cycles']
    results=[];artifacts=[]
    for index,(directory,call,rcall) in enumerate(zip(directories,calls,rcalls)):
        if bounded(directory/'.issue-complete',1,1)!=b'\x01':raise ValueError('model issue capture incomplete')
        state=bounded(directory/'input-state.nexo',24,80*1024)
        if vu_clock(state)!=call['cycle']:raise ValueError('model callback clocks differ from original events')
        pc=struct.unpack_from('<I',state,632)[0];top,itop=struct.unpack_from('<2I',state,662)
        if top!=call['top'] or itop!=call['itop'] or (call['opcode']!=0x17 and pc!=call['pc']):raise ValueError('normalized model callback arguments changed')
        code=bounded(directory/'code.bin',16384,16384)
        expected=bounded(case/('bank-'+str(call['bank'])+'.bin'),16384,16384)
        if code!=expected:raise ValueError('model trace executed a different bank')
        issues_path=directory/'issues.nexo';raw=bounded(issues_path,28,6*1024*1024);model=decode_issues(raw,1)
        end=vu_clock(bounded(directory/'output-state.nexo',24,80*1024))
        next_cycle=calls[index+1]['cycle'] if index+1<len(calls) else last_model
        if end!=next_cycle:raise ValueError('model callback output clock changed')
        ref_end=rcalls[index+1]['cycle'] if index+1<len(rcalls) else last_reference
        part=ref_issues[starts[index]:starts[index+1] if index+1<len(starts) else len(ref_issues)]
        result=compare_call(model,part,call['cycle'],rcall['cycle'],end,ref_end)
        result.update(index=index,bank=call['bank'],top=call['top'],itop=call['itop'])
        results.append(result)
        for path in (issues_path,directory/'input-state.nexo',directory/'output-state.nexo',directory/'code.bin',case/('bank-'+str(call['bank'])+'.bin')):
            artifacts.append({'path':str(path),'sha256':hashlib.sha256(path.read_bytes()).hexdigest()})
    for p in (case/'input-state.nexo',case/'output-state.nexo',case/'vif-input.bin',case/'events.nexo',reference_directory/'reference-events.nexo',reference_directory/'reference-issues.nexo',model_directory/'report.json',reference_directory/'report.json'):
        artifacts.append({'path':str(p),'sha256':hashlib.sha256(p.read_bytes()).hexdigest()})
    equal=all(r['instruction_sequence_equal'] for r in results)
    decomposition=None
    if equal:
        decomposition={'elapsed_delta':sum(r['elapsed_delta'] for r in results),
                       'issue_delta':sum(r['last_issue_delta'] for r in results),
                       'tail_delta':sum(r['tail_delta'] for r in results)}
    return {'schema':'nexo.vu.issue.timing.map.v1','assurance':'tested_only',
            'reference_revision':reference_receipt['commit'],'coordinate_relation':'model_issue = upstream_dispatch_clock - 1',
            'coordinate_relation_scope':'identified logging points; hidden-state relation remains unqualified',
            'instruction_sequence_equal':equal,'clock_decomposition':decomposition,
            'callback_count':len(calls),'model_total_cycles':last_model,'reference_total_cycles':last_reference,
            'callbacks':results,'artifacts':artifacts,
            'full_reference_qualified':False,'timing_accuracy_qualified':False,'whole_gameplay_qualified':False}

def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--case',type=Path,required=True)
    parser.add_argument('--model',type=Path,required=True)
    parser.add_argument('--reference',type=Path,required=True)
    parser.add_argument('--output',type=Path,required=True)
    args=parser.parse_args();result=map_case(args.case,args.model,args.reference)
    with args.output.open('x') as f:json.dump(result,f,indent=2);f.write('\n')
    print(json.dumps({'output':str(args.output),'callback_count':result['callback_count'],
                      'instruction_sequence_equal':result['instruction_sequence_equal'],
                      'first_differences':[r['first_issue_timing_difference'] for r in result['callbacks']]}))

if __name__=='__main__':main()
