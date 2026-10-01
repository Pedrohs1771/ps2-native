#!/usr/bin/env python3
"""Bounded offline admission of captured EE cases; never game-time compilation."""
import argparse
import hashlib
import importlib.util
import json
from pathlib import Path
import subprocess
import time


def module(name):
    spec = importlib.util.spec_from_file_location(name, Path(__file__).with_name(name + '.py'))
    result = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(result)
    return result


catalog_tool = module('generate_ee_bank_catalog')
preparer = module('prepare_ee_miss')


def case_key(case):
    image = preparer.bounded_bytes(case / 'snapshot.bin', 65536)
    metadata = json.loads(preparer.bounded_bytes(case / 'bank.json', 8 * 1024 * 1024))
    if not isinstance(metadata, dict) or any(type(metadata.get(field)) is not int or
            not 0 <= metadata[field] < 32 * 1024 * 1024 or metadata[field] % 4
            for field in ('base', 'entry')) or not image:
        raise ValueError('invalid EE case key')
    return hashlib.sha256(metadata['base'].to_bytes(4, 'little') +
                          metadata['entry'].to_bytes(4, 'little') + image).hexdigest()


def admit(captures, generator, catalog, output, *, initial_cases=()):
    captures = [Path(path) for path in captures]
    if Path(output).is_symlink():
        raise ValueError('EE batch output cannot be a symlink')
    generator, catalog, output = Path(generator).resolve(), Path(catalog).resolve(), Path(output).resolve()
    if not 1 <= len(captures) <= 16 or len(initial_cases) > 512:
        raise ValueError('EE batch budget exceeded or empty')
    if output.exists() or output.is_symlink() or output == catalog or \
            output.is_relative_to(catalog) or catalog.is_relative_to(output):
        raise ValueError('EE batch output must be fresh and separate from its catalog')
    output.mkdir(parents=True)
    report = {'schema_version': 1, 'status': 'PREPARING_LABORATORY', 'strict_approval': False,
              'closure_proved': False, 'complete_machine_checkpoint': False,
              'scope': 'offline finite observed EE cases; no semantics/fetch/publication/closure/game qualification',
              'records': [], 'new_banks': 0, 'prepared_new_banks': 0,
              'duplicate_records': 0, 'manifest_published': False,
              'admitter_sha256': hashlib.sha256(Path(__file__).read_bytes()).hexdigest()}
    started = time.monotonic()
    stage = 'catalog-inputs'
    try:
        before = preparer.bounded_bytes(catalog / 'catalog.json', 1024 * 1024)
        manifest = json.loads(before)
        report['catalog_before_sha256'] = hashlib.sha256(before).hexdigest()
        report['generator_sha256'] = hashlib.sha256(generator.read_bytes()).hexdigest()
        cases = [Path(path) for path in initial_cases] if initial_cases else catalog_tool.owned_cases(catalog, manifest)
        unique = {}
        for case in cases:
            key = case_key(case)
            if key in unique: raise ValueError('duplicate initial EE bank cases')
            unique[key] = case
        if not isinstance(manifest, dict) or not isinstance(manifest.get('sources'), list) or \
                {'ee_bank_' + key + '.cpp' for key in unique} != set(manifest['sources']) - {'ee_catalog.cpp'}:
            raise ValueError('initial EE cases must describe exactly the previous catalog')
        (output / 'cases').mkdir()
        stage = 'capture-preparation'
        for number, capture in enumerate(captures):
            prepared = output / 'cases' / str(number)
            metadata = preparer.prepare(capture, generator, prepared)
            key = case_key(prepared)
            duplicate = key in unique
            report['records'].append({'case_key': key, 'duplicate': duplicate,
                                      'request_sha256': metadata['capture_request_sha256'],
                                      'image_sha256': metadata['image_sha256'],
                                      'entry': metadata['entry'], 'base': metadata['base'],
                                      'context_captured': metadata['context_captured']})
            if duplicate:
                report['duplicate_records'] += 1
            else:
                unique[key] = prepared
                report['prepared_new_banks'] += 1
        stage = 'catalog-extension'
        # All preparations must succeed before the exclusive offline publisher
        # validates and modifies the catalog. It refuses all old bank rewrites.
        after = catalog_tool.extend_catalog(list(unique.values()), generator, catalog)
        report['total_banks'] = len(after['banks'])
        report['catalog_after_sha256'] = hashlib.sha256((catalog / 'catalog.json').read_bytes()).hexdigest()
        report['new_banks'] = report['prepared_new_banks']
        report['manifest_published'] = True
        report['status'] = 'EXTENDED_LABORATORY' if report['new_banks'] else 'NO_NEW_BANKS_LABORATORY'
        return report
    except Exception as error:
        report['status'] = 'FAILED'
        report['failed_step'] = stage
        report['error_type'] = type(error).__name__
        raise
    finally:
        report['seconds'] = time.monotonic() - started
        temporary = output / 'report.json.tmp'
        temporary.write_text(json.dumps(report, indent=2) + '\n')
        temporary.replace(output / 'report.json')


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--catalog', type=Path, required=True)
    parser.add_argument('--capture', type=Path, action='append', required=True)
    parser.add_argument('--generator', type=Path, required=True)
    parser.add_argument('--output', type=Path, required=True)
    parser.add_argument('--case', type=Path, action='append', default=[],
                        help='Bootstrap older catalogs with all previous cases once')
    args = parser.parse_args()
    try:
        report = admit(args.capture, args.generator, args.catalog, args.output, initial_cases=args.case)
        print(json.dumps({key: report[key] for key in ('status', 'new_banks', 'duplicate_records', 'total_banks', 'seconds')}))
    except (ValueError, OSError, KeyError, TypeError, subprocess.TimeoutExpired) as error:
        parser.error('EE batch failed (' + type(error).__name__ + '); inspect the fresh job report and inputs')


if __name__ == '__main__':
    main()
