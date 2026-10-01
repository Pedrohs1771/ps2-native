#!/usr/bin/env python3
"""Generate finite static EE banks from validated recovered snapshots, offline."""
import argparse
import hashlib
import json
from pathlib import Path
import re
import subprocess
import tempfile


def dependency_runs(rows):
    """Losslessly encode consecutive entries with one dependency-end/form."""
    runs = []
    for row in rows:
        address, begin = row['address'], row['source_begin']
        stop = begin + row['source_bytes']
        fixed = 0xFFFFFFFF if begin == address else begin
        if runs and runs[-1][1] == address and runs[-1][2:] == [fixed, stop]:
            runs[-1][1] += 4
        else:
            runs.append([address, address + 4, fixed, stop])
    return runs


def emitted_bindings(code, base, image_bytes):
    rows, functions = [], []
    for address, function, begin, size, offset in re.findall(
            r'\{(0x[0-9a-f]+)u,(ps2native_block_[0-9a-f]+),(0x[0-9a-f]+)u,'
            r'(0x[0-9a-f]+)u,snapshot\+(0x[0-9a-f]+)u\}', code):
        address, begin, size, offset = (int(word, 16) for word in (address, begin, size, offset))
        if begin != base + offset or offset % 4 or not 0 <= offset < image_bytes or \
                size <= 0 or size % 4 or size > image_bytes - offset or address % 4 or \
                not begin <= address < begin + size:
            raise ValueError('invalid generated EE entry dependency')
        rows.append({'address': address, 'source_begin': begin, 'source_bytes': size})
        functions.append(function)
    if not rows or len(rows) > 32768 or any(a['address'] >= b['address'] for a, b in zip(rows, rows[1:])):
        raise ValueError('invalid generated EE entry order/count')
    return rows, functions


def emitted_kernel(code):
    marker = 'static const PS2NativeOverlayBinding bindings[] = {\n'
    footer = '};\nextern "C" const PS2NativeOverlayBinding *ps2xOverlayGetBindings'
    if code.count(marker) != 1 or code.count(footer) != 1:
        raise ValueError('unrecognized generated EE binding table')
    head, table = code.split(marker)
    _, tail = table.split(footer)
    return head + marker + footer + tail


def catalog_index(bank_functions, plans):
    index = '#include "ps2_ee_aot_bank.h"\n#include <array>\n'
    index += ''.join('ps2native::ee_aot::Bank ' + name + '();\n' for name in bank_functions)
    if plans is None:
        index += 'const ps2native::ee_aot::Program &compiledEeProgram() {\n'
        index += 'static const std::array<ps2native::ee_aot::Bank,' + str(len(bank_functions)) + '> banks{{\n'
        index += ''.join(name + '(),\n' for name in bank_functions) + '}};\n'
        return index + 'static const ps2native::ee_aot::Program program{banks};return program; }\n'
    index += '#include <vector>\n#include <stdexcept>\nnamespace {\n'
    index += 'struct DependencyRun { uint32_t first,last,begin,end; };\n'
    for number, runs in enumerate(plans):
        index += 'const std::array<DependencyRun,' + str(len(runs)) + '> plan_' + str(number) + '{{\n'
        index += ''.join('{' + ','.join(str(value) + 'u' for value in run) + '},\n' for run in runs)
        index += '}};\n'
    index += '''void applyDependencies(ps2native::ee_aot::Bank &bank,
  std::vector<PS2NativeOverlayBinding> &entries,std::span<const DependencyRun> plan) {
  entries.assign(bank.bindings.begin(),bank.bindings.end());
  size_t run=0;
  for(auto &entry:entries) {
    while(run<plan.size() && entry.address>=plan[run].last) ++run;
    if(run==plan.size() || entry.address<plan[run].first)
      throw std::invalid_argument("EE dependency plan entry mismatch");
    const auto &range=plan[run];
    const uint32_t begin=range.begin==0xFFFFFFFFu ? entry.address : range.begin;
    if(begin<entry.sourceBegin || begin>entry.address || range.end<=entry.address ||
       range.end-entry.sourceBegin>entry.sourceSize || (begin&3u) || (range.end&3u))
      throw std::invalid_argument("EE dependency plan exceeds original footprint");
    entry.sourceBytes+=begin-entry.sourceBegin;
    entry.sourceBegin=begin;entry.sourceSize=range.end-begin;
  }
  bank.bindings=entries;
}
'''
    count = str(len(bank_functions))
    index += ('struct Catalog {\nstd::array<ps2native::ee_aot::Bank,' + count + '> banks{{\n' +
              ''.join(name + '(),\n' for name in bank_functions) + '}};\n' +
              'std::array<std::vector<PS2NativeOverlayBinding>,' + count + '> entries;\n' +
              'ps2native::ee_aot::Program program{banks};\nCatalog() {\n')
    index += ''.join('applyDependencies(banks[' + str(number) + '],entries[' + str(number) + '],plan_' +
                     str(number) + ');\n' for number in range(len(plans)))
    index += '}};\n}\nconst ps2native::ee_aot::Program &compiledEeProgram() {\n'
    return index + 'static const Catalog catalog;return catalog.program; }\n'


def owned_cases(output, manifest=None):
    """Read bounded immutable catalog inputs; never follow ledger paths outside it."""
    if manifest is None:
        with (output / 'catalog.json').open('rb') as stream: raw = stream.read(1024 * 1024 + 1)
        if len(raw) > 1024 * 1024: raise ValueError('EE manifest exceeds 1 MiB')
        manifest = json.loads(raw)
    if not isinstance(manifest, dict): raise ValueError('EE case ledger manifest must be an object')
    ledger, sources = manifest.get('case_inputs'), manifest.get('sources')
    if not isinstance(ledger, dict) or not 1 <= len(ledger) <= 512 or \
            not isinstance(sources, list) or any(type(name) is not str for name in sources) or \
            set(ledger) != set(sources) - {'ee_catalog.cpp'}:
        raise ValueError('EE catalog needs a complete owned case input ledger')
    if (output / 'ee_cases').is_symlink(): raise ValueError('EE case input parent cannot be a symlink')
    cases = []
    for name, item in sorted(ledger.items()):
        if not re.fullmatch(r'ee_bank_[0-9a-f]{64}\.cpp', name) or not isinstance(item, dict) or \
                set(item) != {'directory', 'image_sha256', 'metadata_sha256'} or \
                any(type(item[field]) is not str or not re.fullmatch('[0-9a-f]{64}', item[field])
                    for field in ('image_sha256', 'metadata_sha256')):
            raise ValueError('invalid owned EE case identity')
        expected = 'ee_cases/' + name[8:-4] + '-' + item['metadata_sha256']
        if item['directory'] != expected: raise ValueError('owned EE case path differs from its identity')
        case = output / expected
        if case.is_symlink() or not case.is_dir(): raise ValueError('invalid owned EE case directory')
        for filename, limit, digest in [('snapshot.bin',65536,item['image_sha256']),
                                        ('bank.json',8*1024*1024,item['metadata_sha256'])]:
            path = case / filename
            if path.is_symlink() or not path.is_file(): raise ValueError('invalid owned EE case file')
            with path.open('rb') as stream: data = stream.read(limit + 1)
            if not data or len(data) > limit or hashlib.sha256(data).hexdigest() != digest:
                raise ValueError('owned EE case bytes differ from the ledger')
        cases.append(case)
    return cases


def generate(cases, generator, output, root=None, *, normal_entries=True, _accepted_producers=None):
    if output.exists():
        raise ValueError('catalog output already exists')
    if not 1 <= len(cases) <= 512:
        raise ValueError('EE bank count outside 1..512')
    records = []
    metadata_bytes = {}
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
        producer = metadata.get('generator_sha256')
        if producer is not None and (type(producer) is not str or not re.fullmatch('[0-9a-f]{64}', producer)):
            raise ValueError('invalid captured EE case producer identity')
        if producer is not None and producer != generator_hash:
            if _accepted_producers is None or producer not in _accepted_producers.get('ee_bank_' + key + '.cpp', ()):
                raise ValueError('captured EE case was prepared with a different generator')
        if metadata.get('dependency_contract', 'whole-block-v0') not in ('whole-block-v0','normal-entry-v1'):
            raise ValueError('unknown EE case dependency contract')
        if not isinstance(metadata['bindings'], list) or not 1 <= len(metadata['bindings']) <= 32768:
            raise ValueError('invalid recovered EE bindings')
        if any(not isinstance(row, dict) or set(row) != {'address', 'source_begin', 'source_bytes'} or
               any(type(value) is not int for value in row.values()) for row in metadata['bindings']):
            raise ValueError('invalid recovered EE binding fields')
        total_bindings += len(metadata['bindings'])
        if total_bindings > 2 * 1024 * 1024:
            raise ValueError('EE catalog binding budget exceeded')
        records.append((image, metadata, key))
        metadata_bytes[key] = raw_metadata
    if not 1 <= len(records) <= 512 or len({key for _, _, key in records}) != len(records):
        raise ValueError('invalid, duplicate or excessive EE banks')
    records.sort(key=lambda record: record[2])
    output.mkdir()
    files = []
    bank_functions = []
    plans = []
    case_inputs = {}
    for image, metadata, key in records:
        name = 'ee_bank_' + key
        with tempfile.TemporaryDirectory() as temporary:
            code_file = Path(temporary) / 'bank.cpp'
            snapshot = Path(temporary) / 'snapshot.bin'
            snapshot.write_bytes(image) # Execute the generator against the admitted immutable bytes.
            def emit(legacy):
                command = [str(generator), str(snapshot), str(metadata['base']),str(metadata['entry']),str(code_file)]
                if legacy: command.append('--legacy-footprints')
                result = subprocess.run(command,capture_output=True,text=True,timeout=30)
                if result.returncode: raise ValueError('EE bank generator failed: ' + result.stderr)
                with code_file.open('rb') as stream: code_bytes = stream.read(64 * 1024 * 1024 + 1)
                if len(code_bytes) > 64 * 1024 * 1024: raise ValueError('generated EE bank source exceeds 64 MiB')
                return code_bytes.decode()
            code = emit(True)
            old_rows, old_functions = emitted_bindings(code, metadata['base'], len(image))
            normal_code = emit(False) if normal_entries or metadata.get('dependency_contract') == 'normal-entry-v1' else None
        if normal_code is not None:
            normal_rows, normal_functions = emitted_bindings(normal_code,metadata['base'],len(image))
            if emitted_kernel(normal_code) != emitted_kernel(code) or normal_functions != old_functions or \
                    len(normal_rows) != len(old_rows):
                raise ValueError('entry dependency refinement changed compiled callback identity')
            for old, new in zip(old_rows,normal_rows):
                if old['address'] != new['address'] or new['source_begin'] < old['source_begin'] or \
                        new['source_begin'] + new['source_bytes'] > old['source_begin'] + old['source_bytes']:
                    raise ValueError('normal entry dependency is not a refinement of the original callback')
            plans.append(dependency_runs(normal_rows))
        getter = 'extern "C" const PS2NativeOverlayBinding *ps2xOverlayGetBindings'
        if code.count('ps2xOverlayGetBindings') != 1 or code.count(getter) != 1:
            raise ValueError('unrecognized generated overlay getter')
        rows = normal_rows if metadata.get('dependency_contract') == 'normal-entry-v1' else old_rows
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
        metadata_hash = hashlib.sha256(metadata_bytes[key]).hexdigest()
        directory = 'ee_cases/' + key + '-' + metadata_hash
        case = output / directory
        case.mkdir(parents=True)
        (case / 'snapshot.bin').write_bytes(image)
        (case / 'bank.json').write_bytes(metadata_bytes[key])
        case_inputs[source.name] = {'directory':directory, 'image_sha256':metadata['image_sha256'],
                                   'metadata_sha256':metadata_hash}
    index = catalog_index(bank_functions,plans if normal_entries else None)
    (output / 'ee_catalog.cpp').write_text(index)
    files.append('ee_catalog.cpp')
    plan_file = 'ee_entry_dependencies.json' if normal_entries else None
    plan_bytes = (json.dumps({'schema_version':1,'dependency_contract':'normal-entry-v1',
                              'banks':[{'source':name+'.cpp','runs':plan}
                                       for name,plan in zip((function.removesuffix('_descriptor')
                                                             for function in bank_functions),plans)]},
                             separators=(',',':'))+'\n').encode() if normal_entries else None
    if plan_bytes is not None:
        if len(plan_bytes)>64*1024*1024: raise ValueError('EE dependency plan exceeds 64 MiB')
        (output / plan_file).write_bytes(plan_bytes)
    if hashlib.sha256(generator.read_bytes()).hexdigest() != generator_hash:
        raise ValueError('EE generator changed during catalog production')
    manifest = {'schema_version': 1, 'symbol': 'compiledEeProgram', 'sources': files,
                'sha256': {name: hashlib.sha256((output / name).read_bytes()).hexdigest() for name in files},
                'generator_sha256': generator_hash,
                'bank_generator_sha256': hashlib.sha256(Path(__file__).read_bytes()).hexdigest(),
                'case_producers': {'ee_bank_' + key + '.cpp': metadata['generator_sha256']
                                   for _, metadata, key in records if metadata.get('generator_sha256') is not None},
                'case_inputs': case_inputs,
                'dependency_contract': 'normal-entry-v1' if normal_entries else 'whole-block-v0',
                'dependency_plan_file':plan_file,
                'dependency_plan_sha256': hashlib.sha256(plan_bytes).hexdigest() if normal_entries else None,
                'dependency_runs': sum(len(plan) for plan in plans) if normal_entries else 0,
                'banks': [{'base': m['base'], 'entry': m['entry'], 'image_sha256': m['image_sha256']}
                          for _, m, _ in records],
                'scope': 'finite observed EE RAM-block banks; publication, hidden state, closure and fidelity unqualified'}
    (output / 'catalog.json').write_text(json.dumps(manifest, indent=2) + '\n')
    return manifest


def extend_catalog(cases, generator, output, *, migrate_entry_guards=False):
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
            not isinstance(old.get('generator_sha256'),str) or \
            not re.fullmatch('[0-9a-f]{64}',old['generator_sha256']) or \
            (not migrate_entry_guards and old['generator_sha256'] != hashlib.sha256(generator.read_bytes()).hexdigest()) or \
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
    case_producers = old.get('case_producers', {})
    if not isinstance(case_producers, dict) or len(case_producers) > 512 or \
            any(name not in old['sources'] or name == 'ee_catalog.cpp' or type(producer) is not str or
                not re.fullmatch('[0-9a-f]{64}', producer) for name, producer in case_producers.items()):
        raise ValueError('invalid existing EE case producer ledger')
    accepted_producers = {name: {old['generator_sha256']} for name in old['sources'] if name != 'ee_catalog.cpp'}
    for name, producer in case_producers.items():
        accepted_producers[name].add(producer)
    # Bootstrap catalogs produced before the per-bank ledger existed. The prior
    # migration verified every retained bank source; equality is checked again
    # below before any staged file can replace an existing artifact.
    previous = old.get('previous_generator_sha256')
    if previous is not None:
        if type(previous) is not str or not re.fullmatch('[0-9a-f]{64}', previous):
            raise ValueError('invalid prior EE generator identity')
        for producers in accepted_producers.values():
            producers.add(previous)
    old_plan = old.get('dependency_plan_file')
    if 'case_inputs' in old: owned_cases(output, old)
    if old.get('dependency_contract') == 'normal-entry-v1':
        if old_plan != 'ee_entry_dependencies.json' or (output / old_plan).is_symlink():
            raise ValueError('existing EE dependency plan identity is invalid')
        with (output / old_plan).open('rb') as stream: old_plan_bytes=stream.read(64*1024*1024+1)
        if len(old_plan_bytes)>64*1024*1024 or hashlib.sha256(old_plan_bytes).hexdigest()!=old.get('dependency_plan_sha256'):
            raise ValueError('existing EE dependency plan changed')
    with tempfile.TemporaryDirectory(prefix='.ee-catalog-stage-', dir=output.parent) as temporary:
        staged = Path(temporary) / 'catalog'
        old_contract = old.get('dependency_contract','whole-block-v0')
        if old_contract not in ('whole-block-v0','normal-entry-v1'):
            raise ValueError('unknown existing EE dependency contract')
        new = generate(cases, generator, staged,normal_entries=migrate_entry_guards or old_contract=='normal-entry-v1',
                       _accepted_producers=accepted_producers)
        if migrate_entry_guards:
            new['previous_generator_sha256'] = old['generator_sha256']
            new['previous_catalog_sha256'] = hashlib.sha256(raw).hexdigest()
            (staged/'catalog.json').write_text(json.dumps(new,indent=2)+'\n')
        if not set(old['sources']).issubset(new['sources']) or \
                any(bank not in new['banks'] for bank in old['banks']) or \
                any(new['sha256'][name] != old['sha256'][name]
                    for name in old['sources'] if name != 'ee_catalog.cpp'):
            raise ValueError('extension cannot remove or rewrite existing EE banks')
        for name in new['sources']:
            if name not in old['sources'] and ((output / name).exists() or (output / name).is_symlink()):
                raise ValueError('new EE source conflicts with an unrecorded artifact')
        old_inputs = {item['directory'] for item in old.get('case_inputs', {}).values()}
        parent = output / 'ee_cases'
        if parent.is_symlink() or (parent.exists() and not parent.is_dir()):
            raise ValueError('EE case input parent must be an owned directory')
        for item in new['case_inputs'].values():
            target = output / item['directory']
            if item['directory'] not in old_inputs and (target.exists() or target.is_symlink()):
                raise ValueError('new EE input case conflicts with an unrecorded artifact')
        plan_file = new.get('dependency_plan_file')
        if plan_file:
            target = output / plan_file
            if old_plan != plan_file and (target.exists() or target.is_symlink()):
                raise ValueError('EE dependency plan conflicts with an unrecorded artifact')
            if not target.exists() or (staged / plan_file).read_bytes() != target.read_bytes():
                (staged / plan_file).replace(target)
        for item in new['case_inputs'].values():
            if item['directory'] not in old_inputs:
                target = output / item['directory']
                target.parent.mkdir(exist_ok=True)
                (staged / item['directory']).replace(target)
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
    parser.add_argument('--migrate-entry-guards', action='store_true',
                        help='Migrate verified callback sources to normal-entry guards without rewriting existing banks')
    args = parser.parse_args()
    try:
        if args.extend:
            manifest = extend_catalog(args.cases, args.generator.resolve(), args.output,
                                      migrate_entry_guards=args.migrate_entry_guards)
        else:
            if args.migrate_entry_guards: raise ValueError('guard migration requires --extend')
            manifest = generate(args.cases, args.generator.resolve(), args.output)
        print(json.dumps({'banks': len(manifest['banks']), 'sources': len(manifest['sources'])}))
    except (ValueError, OSError, KeyError, TypeError, subprocess.TimeoutExpired) as error:
        parser.error(str(error))


if __name__ == '__main__':
    main()
