"""Exercise automatic recovery across existing private builds, without title patches.

This laboratory bridge reuses generated guest objects and replaces the host
runtime. It is an integration experiment, not a fresh conversion qualification.
Run after building ps2x_tests and nexo_ee_autoadapt_fixture with the empty catalog.
"""
from __future__ import annotations

import argparse
from concurrent.futures import ThreadPoolExecutor
import json
from pathlib import Path
import shlex

from tools.ps2native.autoadapt import DesktopAdaptation, _read_json, validate_catalog
from tools.ps2native.pipeline import (PipelineError, _run, _write_json, repo_root,
                                    sha256_file, source_tree_snapshot, verify_package)


def link_inputs(argv: list[str], cwd: Path) -> tuple[list[str], list[Path]]:
    objects, result = [], []
    runtimes = mains = 0
    for value in argv:
        if value.startswith('@'):
            response = cwd / value[1:]
            if not response.is_file() or response.stat().st_size > 8*1024*1024:
                raise PipelineError('missing or oversized CMake object response')
            entries = shlex.split(response.read_text())
        elif value.endswith('.o'):
            entries = [value]
        else:
            entries = []
            path = cwd / value
            if path.name == 'libps2_runtime.a':
                runtimes += 1
            elif path.name == 'libps2_iop.a':
                pass  # The current shared IOP library is linked with the runtime.
            else:
                result.append(str(path.resolve()) if path.is_file() else value)
        for entry in entries:
            path = (cwd/entry).resolve()
            if not path.is_file() or path.suffix != '.o':
                raise PipelineError('missing CMake guest object')
            if path.name == 'main.cpp.o':
                mains += 1
            else:
                objects.append(path)
    if runtimes != 1 or mains != 1 or not objects or len(objects) > 32768 or result.count('-o') != 1:
        raise PipelineError('CMake inputs must contain one runtime, one host main and bounded guest objects')
    return result, objects


class CorpusAdaptation(DesktopAdaptation):
    def prepare_link(self, baseline: dict):
        # Laboratory probes pass these arguments explicitly; the immutable
        # original package must keep its original game assets and hashes.
        self.startup_file = self.workspace/'boot-args.txt'
        self.bank_objects = {}
        build = Path(baseline['desktop']['cmake_build_dir'])
        records = _read_json_list(build/'compile_commands.json')
        main = [record for record in records if record['file'].endswith('/ps2xRuntime/src/main.cpp')]
        if len(main) != 1:
            raise PipelineError('baseline must identify the host main compile command')
        cwd = Path(main[0]['directory'])
        self.link_argv, self.guest_objects = link_inputs(
            shlex.split((cwd/'CMakeFiles/ps2EntryRunner.dir/link.txt').read_text()), cwd)
        self.link_cwd = cwd
        self.host_main = self.workspace/'main.cpp.o'
        command = shlex.split(main[0]['command'])
        while '-include' in command:
            index = command.index('-include'); del command[index:index+2]
        command = [word for word in command if word != '-Winvalid-pch']
        command[command.index('-o')+1] = str(self.host_main)
        command[command.index('-c')+1] = str(self.root/'ps2xRuntime/src/main.cpp')
        _run(command, self.workspace/'main-build.log', cwd, self.args.timeout)
        fixture_objects = self.root/'build/lab/CMakeFiles/nexo_ee_autoadapt_fixture.dir/__/ps2xRuntime/src/lib'
        self.backend = fixture_objects/'ps2_ee_overlay_backend.cpp.o'
        self.bootstrap = fixture_objects/'ps2_empty_ee_catalog.cpp.o'
        self.runtime = self.root/'build/ps2xRuntime/libps2_runtime.a'
        self.iop = self.root/'build/ps2xIOP/libps2_iop.a'
        self.inputs = self.guest_objects + [self.host_main, self.backend, self.bootstrap, self.runtime, self.iop]
        self.input_hashes = {str(path): sha256_file(path) for path in self.inputs}
        _write_json(self.workspace/'link-inputs.json', {
            'baseline_package_manifest_sha256': sha256_file(self.package/'manifest.json'),
            'inputs_sha256': self.input_hashes, 'guest_objects_reused': len(self.guest_objects),
            'guest_generation': 'baseline build; current generated-code discovery not qualified',
            'strict_approval': False, 'closure_proved': False, 'menu_approved': False})

    def relink(self, banks: list[Path], directory: Path) -> dict:
        for path in self.inputs:
            if self.input_hashes[str(path)] != sha256_file(path):
                raise PipelineError('a baseline native link input changed during adaptation')
        response = directory/'objects.rsp'
        objects = [self.host_main, self.backend, *banks, *self.guest_objects]
        response.write_text('\n'.join(json.dumps(str(path)) for path in objects)+'\n')
        argv = list(self.link_argv)
        runner = directory/'ps2EntryRunner'
        argv[argv.index('-o')+1] = str(runner)
        argv = ['-Wl,--dependency-file='+str(directory/'link.d')
                if word.startswith('-Wl,--dependency-file=') else word for word in argv]
        argv.insert(1, '@'+str(response))
        argv += [str(self.runtime), str(self.iop)]
        # Runtime references appear after its baseline dependency libraries.
        # Repeat those libraries for the static linker's left-to-right lookup.
        argv += [word for word in self.link_argv if word.endswith('.a') or word.endswith('.so') or word.startswith('-l')]
        _run(argv, directory/'link.log', self.link_cwd, self.args.timeout)
        self.manifest['desktop'] = {'runner': str(runner)}
        result = {'runner': str(runner), 'runner_sha256': sha256_file(runner),
                  'live_overlay_compiler_enabled': False, 'menu_approved': False}
        _write_json(directory/'relink.json', {**result, 'command': argv})
        return result

    def compile_bank(self, source: Path, directory: Path) -> tuple[Path, bool]:
        digest = sha256_file(source)
        prior = self.bank_objects.get(digest)
        if prior:
            path, object_digest = prior
            if not path.is_file() or sha256_file(path) != object_digest:
                raise PipelineError('compiled native bank object changed before reuse')
            return path, True
        output = directory/(source.name+'.o')
        _run(['/usr/bin/c++', '-std=c++20', '-O0', '-fno-lto', '-msse4.1', '-ffp-contract=off',
              '-I'+str(self.root/'ps2xRuntime/include'), '-I'+str(self.root/'ps2xIOP/include'),
              '-I'+str(self.root/'ps2xRuntime/src/lib'), '-I'+str(self.root/'ps2xRuntime/src/lib/Kernel'),
              '-c', str(source), '-o', str(output)],
             directory/(source.name+'.log'), self.root, self.args.timeout)
        self.bank_objects[digest] = (output, sha256_file(output))
        return output, False

    def rebuild(self, recovered: dict, directory: Path) -> dict:
        catalog = Path(recovered['catalog_manifest']).parent
        if sha256_file(catalog/'catalog.json') != recovered['catalog_sha256']:
            raise PipelineError('catalog changed before laboratory compilation')
        validate_catalog(catalog, self.generator)
        sources = _read_json(catalog/'catalog.json')['sources']
        with ThreadPoolExecutor(max_workers=self.args.build_jobs) as executor:
            objects = list(executor.map(lambda name: self.compile_bank(catalog/name, directory), sources))
        result = self.relink([path for path, _ in objects], directory)
        result.update(compiled_sources=sum(not reused for _, reused in objects),
                      reused_sources=sum(reused for _, reused in objects))
        self.remember()
        return result

    def initial_link(self):
        directory = self.workspace/'initial-link'; directory.mkdir()
        if self.cases:
            self.rebuild({'catalog_manifest': str(self.catalog/'catalog.json'),
                          'catalog_sha256': sha256_file(self.catalog/'catalog.json')}, directory)
        else:
            self.relink([self.bootstrap], directory)


def _read_json_list(path: Path) -> list:
    if path.is_symlink() or not path.is_file() or path.stat().st_size > 128*1024*1024:
        raise PipelineError('missing or oversized CMake compile commands')
    result = json.loads(path.read_text())
    if not isinstance(result, list):
        raise PipelineError('CMake compile commands must be an array')
    return result


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--corpus', required=True, type=Path)
    parser.add_argument('--out', required=True, type=Path)
    parser.add_argument('--memory-root', type=Path)
    parser.add_argument('--overlay-generator')
    parser.add_argument('--adapt-rounds', type=int, default=8)
    parser.add_argument('--probe-timeout', type=int, default=30)
    parser.add_argument('--timeout', type=int, default=300)
    parser.add_argument('--build-jobs', type=int, default=4)
    args = parser.parse_args()
    if not 1 <= args.build_jobs <= 4 or not 1 <= args.adapt_rounds <= 64 or not 1 <= args.probe_timeout <= 3600:
        parser.error('invalid bounded adaptation settings')
    args.cmake = None
    root = repo_root()
    source = source_tree_snapshot(root)
    corpus = _read_json(args.corpus)
    out = args.out.expanduser().absolute(); out.mkdir(parents=True, exist_ok=False)
    receipt = {'status': 'running', 'source_tree_sha256': source['sha256'], 'images': [],
               'menu_approved': False, 'native_execution_qualified': False,
               'scope': 'generic automatic recovery with reused baseline guest objects; menus/IOP/VU unqualified'}
    try:
        for row in corpus['images']:
            item = {'iso_sha256': row['iso_sha256'], 'iso_path': row['iso_path'], 'status': 'running'}
            receipt['images'].append(item); _write_json(out/'corpus-adaptation.json', receipt)
            try:
                if source_tree_snapshot(root)['sha256'] != source['sha256']:
                    raise PipelineError('source tree changed during corpus adaptation')
                package = Path(row['artifact']); iso = Path(row['iso_path'])
                if verify_package(package)['status'] != 'verified' or sha256_file(iso) != row['iso_sha256']:
                    raise PipelineError('baseline package or ISO differs from the corpus')
                baseline = _read_json(package/'manifest.json', 64*1024*1024)
                # The distributable manifest deliberately hides build paths.
                # Link inputs belong to the private workspace receipt.
                baseline = _read_json(Path(baseline['workspace'])/'manifest.json', 64*1024*1024)
                if baseline['source']['iso_sha256'] != row['iso_sha256']:
                    raise PipelineError('private build receipt belongs to a different ISO')
                workspace = out/row['iso_sha256']; workspace.mkdir()
                manifest = {'source': baseline['source'], 'boot_elf': baseline['boot_elf'],
                            'build_inputs': {'source_tree': source}}
                with CorpusAdaptation(root, workspace, package, iso, args, manifest) as adapter:
                    adapter.prepare_link(baseline)
                    adapter.initial_link()
                    item.update(adapter.run())
            except (PipelineError, OSError, ValueError) as error:
                item.update(status='failed', error=str(error))
            _write_json(out/'corpus-adaptation.json', receipt)
        receipt['status'] = 'finished_unverified'
    finally:
        _write_json(out/'corpus-adaptation.json', receipt)
    print(json.dumps({'receipt': str(out/'corpus-adaptation.json'),
                      'images': [{key: item.get(key) for key in ('iso_path','status','recovery_rounds','error')}
                                 for item in receipt['images']]}))
    return 0 if all(item['status'] == 'replayed_unverified' for item in receipt['images']) else 1


if __name__ == '__main__':
    raise SystemExit(main())
