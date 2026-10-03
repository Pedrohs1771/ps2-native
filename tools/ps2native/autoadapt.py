"""Bounded, local EE recovery during conversion; never a game-time compiler."""
from __future__ import annotations

import hashlib
import json
import os
from pathlib import Path, PurePosixPath
import re
import shutil
import sys
from typing import Callable

from .pipeline import PipelineError, _find_tool, _run, _write_json, sha256_file


def _read_json(path: Path, limit: int = 1024 * 1024) -> dict:
    if path.is_symlink() or not path.is_file() or path.stat().st_size > limit:
        raise PipelineError(f"missing or oversized adaptation record: {path}")
    value = json.loads(path.read_text(encoding="utf-8"))
    if not isinstance(value, dict):
        raise PipelineError(f"adaptation record must be an object: {path}")
    return value


def _miss_key(capture: Path) -> str:
    from lab.prepare_ee_miss import RAM_BYTES, ram_offset
    request = _read_json(capture / 'request.json')
    target, base = request.get('target_pc'), request.get('window_base')
    if any(type(value) is not int or ram_offset(value) >= RAM_BYTES or value % 4
           for value in (target, base)):
        raise PipelineError('invalid captured EE address')
    image = capture / 'snapshot.bin'
    if image.is_symlink() or not image.is_file() or not 0 < image.stat().st_size <= 65536:
        raise PipelineError('missing or oversized captured EE image')
    return hashlib.sha256(base.to_bytes(4, 'little') + target.to_bytes(4, 'little') +
                          image.read_bytes()).hexdigest()


_STARTUP_OPTION = re.compile(r'-{1,2}[A-Za-z][A-Za-z0-9_-]{0,31}')


def read_startup_requirement(log: Path) -> dict | None:
    """Admit only an explicit guest request for a PS2 working directory."""
    options = set()
    pattern = re.compile(r'(?:PS2 printf: )?Missing Command Line Option: '
                         r'(-{1,2}[A-Za-z][A-Za-z0-9_-]{0,31}) \(working directory\)')
    with log.open(errors='replace') as stream:
        while line := stream.readline(8192):
            match = pattern.fullmatch(line.strip())
            if match:
                options.add(match[1])
    if len(options) == 1:
        return {'kind': 'startup_requirement', 'purpose': 'working_directory', 'option': options.pop()}
    return None


def _valid_startup_arguments(arguments, program: str) -> bool:
    return isinstance(arguments, list) and 3 <= len(arguments) <= 15 and len(arguments) % 2 == 1 and \
        arguments[0] == program and \
        all(type(option) is str and _STARTUP_OPTION.fullmatch(option) and value == 'cdrom0:\\'
            for option, value in zip(arguments[1::2], arguments[2::2])) and \
        len(set(arguments[1::2])) == len(arguments[1::2])


def run_adaptation(output: Path, *, probe: Callable, recover: Callable,
                   rebuild: Callable, max_rounds: int, configure_startup: Callable | None = None) -> dict:
    if type(max_rounds) is not int or not 1 <= max_rounds <= 64:
        raise ValueError('adaptation round budget must be in 1..64')
    output.mkdir(parents=True, exist_ok=False)
    receipt = {'schema_version': 1, 'status': 'probing', 'recovery_rounds': 0,
               'configuration_rounds': 0,
               'menu_approved': False, 'gameplay_approved': False,
               'native_execution_qualified': False, 'closure_proved': False,
               'strict_approval': False, 'steps': [],
               'scope': 'local observed EE recovery; IOP/VU/GS/audio/menu fidelity unqualified'}
    seen = set()
    path = output / 'adaptation.json'

    def step(stage, directory, action):
        record = {'stage': stage, 'directory': str(directory), 'status': 'running'}
        receipt['steps'].append(record)
        _write_json(path, receipt)
        try:
            result = action()
            record.update(status='complete', result=result)
            return result
        except BaseException:
            record['status'] = 'failed'
            raise
        finally:
            _write_json(path, receipt)

    try:
        for number in range(max_rounds + 1):
            directory = output / f'round-{number:03d}'
            directory.mkdir()
            result = step('probe', directory, lambda: probe(directory))
            if result.get('kind') == 'no_code_miss':
                receipt['status'] = 'replayed_unverified'
                return receipt
            kind = result.get('kind')
            if kind not in ('code_miss', 'startup_requirement'):
                raise PipelineError(result.get('reason', 'execution failed without a recoverable EE miss'))
            if kind == 'startup_requirement':
                if configure_startup is None:
                    raise PipelineError('guest requires an unsupported startup configuration')
                key = 'startup:' + json.dumps([result.get('purpose'), result.get('option')])
            else:
                capture = Path(result['capture'])
                key = _miss_key(capture)
            if key in seen:
                receipt['status'] = 'no_progress'
                raise PipelineError('offline recovery made no progress; an identical requirement recurred')
            if number == max_rounds:
                receipt['status'] = 'budget_exhausted'
                raise PipelineError('offline recovery budget exhausted; captures and compiled catalog are preserved')
            seen.add(key)
            if kind == 'startup_requirement':
                step('configure_startup', directory, lambda: configure_startup(result, directory))
                receipt['configuration_rounds'] += 1
                continue
            recovered = step('generate', directory, lambda: recover(capture, directory))
            step('relink', directory, lambda: rebuild(recovered, directory))
            receipt['recovery_rounds'] += 1
        raise AssertionError('bounded adaptation did not terminate')
    except BaseException as error:
        if receipt['status'] not in ('no_progress', 'budget_exhausted'):
            receipt['status'] = 'failed'
        receipt['error'] = str(error)
        raise
    finally:
        _write_json(path, receipt)


def validate_catalog(directory: Path, generator: Path) -> list[Path]:
    from lab import generate_ee_bank_catalog as catalog_tool
    if directory.is_symlink() or not directory.is_dir():
        raise PipelineError('EE catalog directory cannot be missing or a symlink')
    manifest = _read_json(directory / 'catalog.json')
    if manifest.get('generator_sha256') != sha256_file(generator):
        raise PipelineError('cached EE catalog was produced by a different generator')
    cases = catalog_tool.owned_cases(directory, manifest)
    for name in manifest['sources']:
        path = directory / name
        if path.is_symlink() or not path.is_file() or path.stat().st_size > 64 * 1024 * 1024 or \
                manifest.get('sha256', {}).get(name) != sha256_file(path):
            raise PipelineError('cached EE source differs from its catalog')
    return cases


class DesktopAdaptation:
    """One conversion owns its build, capture directories and locked memory key."""

    def __init__(self, root: Path, workspace: Path, package: Path, iso: Path,
                 args, manifest: dict):
        self.root, self.workspace, self.package, self.iso = root, workspace, package, iso
        self.args, self.manifest = args, manifest
        self.catalog = workspace / 'offline-ee-catalog'
        self.cases = []
        self.lock = None
        self.memory = None
        self.startup_arguments = []
        self.startup_file = package/'game/boot-args.txt'

    def __enter__(self):
        # This probe is a conversion tool on Linux, not a packaged dependency.
        if sys.platform != 'linux':
            raise PipelineError('automatic probing currently requires Linux; --no-adapt builds an unverified package')
        for name in ('xvfb-run', 'Xvfb', 'xdotool', 'import', 'pactl'):
            if not shutil.which(name):
                raise PipelineError(f'automatic probing requires {name}')
        self.generator = _find_tool(getattr(self.args, 'overlay_generator', None),
                                    'PS2NATIVE_OVERLAY_GENERATOR', ('ps2_native_overlay',), self.root)
        self.cmake = _find_tool(self.args.cmake, 'PS2NATIVE_CMAKE', ('cmake',), self.root)
        memory_root = Path(self.args.memory_root or self.root/'build/ps2native-memory').expanduser().absolute()
        if any(path.is_symlink() for path in (memory_root, *memory_root.parents)):
            raise PipelineError('adaptation memory paths cannot contain symlinks')
        from .pipeline import _ensure_output_outside_sources
        _ensure_output_outside_sources(memory_root, self.root)
        if memory_root.is_relative_to(self.workspace) or self.workspace.is_relative_to(memory_root):
            raise PipelineError('adaptation memory overlaps the conversion workspace')
        identity = {'iso_sha256': self.manifest['source']['iso_sha256'],
                    'generator_sha256': sha256_file(self.generator),
                    'source_tree_sha256': self.manifest['build_inputs']['source_tree']['sha256']}
        key = hashlib.sha256(json.dumps(identity, sort_keys=True).encode()).hexdigest()
        self.memory = memory_root/key
        if self.memory.is_symlink():
            raise PipelineError('adaptation memory key cannot be a symlink')
        memory_root.mkdir(parents=True, exist_ok=True, mode=0o700)
        self.memory.mkdir(exist_ok=True, mode=0o700)
        import fcntl
        lock_path = self.memory/'conversion.lock'
        if lock_path.is_symlink():
            raise PipelineError('adaptation memory lock cannot be a symlink')
        self.lock = os.fdopen(os.open(lock_path, os.O_CREAT | os.O_RDWR | os.O_NOFOLLOW, 0o600), 'a')
        try:
            fcntl.flock(self.lock, fcntl.LOCK_EX | fcntl.LOCK_NB)
            prior = self.memory/'catalog'
            if prior.is_symlink():
                raise PipelineError('adaptation memory catalog cannot be a symlink')
            if prior.exists():
                validate_catalog(prior, self.generator)
                shutil.copytree(prior, self.catalog)
                self.cases = validate_catalog(self.catalog, self.generator)
            startup = self.memory/'startup.json'
            if startup.exists() or startup.is_symlink():
                remembered = _read_json(startup, 8192)
                if remembered.get('schema_version') != 1 or remembered.get('identity') != identity or \
                        remembered.get('startup_probe_confirmed') is not True or \
                        not _valid_startup_arguments(remembered.get('guest_arguments'), self.program_argument()):
                    raise PipelineError('cached startup configuration is invalid for this conversion')
                self.startup_arguments = remembered['guest_arguments']
            settings = {'enabled': True, 'identity': identity, 'memory_key': key,
                        'memory_directory': str(self.memory), 'reused_cases': len(self.cases),
                        'reused_startup_arguments': len(self.startup_arguments),
                        'catalog_manifest': str(self.catalog/'catalog.json') if self.cases else '',
                        'max_rounds': self.args.adapt_rounds, 'probe_seconds': self.args.probe_timeout,
                        'live_overlay_compiler_enabled': False, 'menu_approved': False,
                        'native_execution_qualified': False}
            self.manifest['automatic_adaptation'] = settings
            return self
        except BaseException:
            self.__exit__(None, None, None)
            raise

    def __exit__(self, *_):
        if self.lock:
            self.lock.close()
            self.lock = None

    def probe(self, directory: Path) -> dict:
        env = os.environ.copy()
        env.pop('PS2X_NATIVE_OVERLAY_DRIVER', None)
        env['PS2X_EE_MISS_CAPTURE_DIR'] = str(directory/'captures')
        env['PS2X_FRAME_TIME_CSV'] = str(directory/'frames.csv')
        runner = Path(self.manifest['desktop']['runner'])
        command = [sys.executable, '-m', 'tools.ps2native.headless_native_test', 'launch',
                   '--package', str(self.package), '--iso', str(self.iso), '--runner', str(runner),
                   '--state', str(directory/'state.json'), '--log', str(directory/'runner.log'),
                   '--duration', str(self.args.probe_timeout), '--disable-overlay-driver']
        command += ['--guest-arg='+argument for argument in self.startup_arguments]
        failure = None
        try:
            _run(command, directory/'probe.log', self.root, self.args.probe_timeout + 45, env=env)
        except PipelineError as error:
            failure = error
        records = []
        for line in (directory/'probe.log').read_text().splitlines():
            try:
                value = json.loads(line)
                if isinstance(value, dict): records.append(value)
            except json.JSONDecodeError:
                pass
        terminal = records[-1] if records else {}
        captures = sorted((directory/'captures').glob('ee-miss-*/request.json'))
        if captures:
            if len(captures) != 1 or terminal.get('status') != 'runner_failed' or terminal.get('exit_code') != 73:
                raise PipelineError('EE capture was not followed by the strict runner exit')
            return {'kind': 'code_miss', 'capture': str(captures[0].parent),
                    'runner_sha256': sha256_file(runner), 'execution': terminal}
        if failure:
            raise failure
        log = directory/'runner.log'
        with log.open(errors='replace') as stream:
            while line := stream.readline(8192):
                if '[EE:UNSEEN_CODE]' in line or '[guest-branch:missing-target]' in line:
                    raise PipelineError('uncovered EE code stopped without a complete recovery capture')
        if terminal.get('status') not in ('exited', 'duration_reached') or not terminal.get('window_ready'):
            raise PipelineError('conversion probe did not produce a valid execution receipt')
        requirement = read_startup_requirement(log)
        if requirement:
            return {**requirement, 'execution': terminal}
        return {'kind': 'no_code_miss', 'runner_sha256': sha256_file(runner),
                'execution': terminal, 'menu_approved': False}

    def _write_startup_arguments(self, previous: list[str] | None = None):
        self.startup_file.parent.mkdir(parents=True, exist_ok=True)
        text = '\n'.join(self.startup_arguments)+'\n'
        if self.startup_file.exists() or self.startup_file.is_symlink():
            prior_text = '\n'.join(previous)+'\n' if previous else text
            if self.startup_file.is_symlink() or self.startup_file.stat().st_size > 8192 or \
                    self.startup_file.read_text() not in (text, prior_text):
                raise PipelineError('refusing to overwrite an existing guest startup configuration')
        self.startup_file.write_text(text)

    def program_argument(self) -> str:
        path = self.manifest.get('boot_elf', {}).get('iso_path')
        if path is None:
            return 'ps2native'  # Synthetic contract fixtures have no ISO filename.
        if type(path) is not str or not path.startswith('/') or len(path) > 4096 or \
                any(character in path for character in ('\0', '\r', '\n', '\\')) or '..' in PurePosixPath(path).parts:
            raise PipelineError('invalid guest program path in ISO metadata')
        return 'cdrom0:' + path.replace('/', '\\')

    def configure_startup(self, requirement: dict, directory: Path) -> dict:
        arguments = [*(self.startup_arguments or [self.program_argument()]), requirement.get('option'), 'cdrom0:\\']
        if requirement.get('purpose') != 'working_directory' or not _valid_startup_arguments(arguments, self.program_argument()):
            raise PipelineError('unsupported or repeated guest startup configuration')
        previous = self.startup_arguments
        self.startup_arguments = arguments
        try:
            self._write_startup_arguments(previous)
        except BaseException:
            self.startup_arguments = previous
            raise
        result = {'schema_version': 1, 'identity': self.manifest['automatic_adaptation']['identity'],
                  'guest_arguments': arguments, 'origin': 'observed guest working-directory diagnostic',
                  'menu_approved': False, 'native_execution_qualified': False}
        return result

    def recover(self, capture: Path, directory: Path) -> dict:
        from lab import generate_ee_bank_catalog as catalog_tool
        case = directory/'case'
        _run([sys.executable, str(self.root/'lab/prepare_ee_miss.py'), str(capture),
              '--generator', str(self.generator), '--output', str(case)],
             directory/'prepare.log', self.root, self.args.timeout)
        metadata = _read_json(case/'bank.json', 8*1024*1024)
        self.cases.append(case)
        if self.catalog.exists():
            catalog_tool.extend_catalog(self.cases, self.generator, self.catalog)
        else:
            catalog_tool.generate(self.cases, self.generator, self.catalog)
        self.cases = validate_catalog(self.catalog, self.generator)
        return {'catalog_manifest': str(self.catalog/'catalog.json'),
                'catalog_sha256': sha256_file(self.catalog/'catalog.json'),
                'case_count': len(self.cases), 'target_pc': metadata['entry']}

    def rebuild(self, recovered: dict, directory: Path) -> dict:
        build = self.workspace/'desktop-project/build'
        project = build.parent
        if sha256_file(Path(recovered['catalog_manifest'])) != recovered['catalog_sha256']:
            raise PipelineError('offline EE catalog changed before compilation')
        _run([str(self.cmake), '-S', str(project), '-B', str(build),
              '-DNEXO_EE_BANK_MANIFEST:FILEPATH='+recovered['catalog_manifest']],
             directory/'configure.log', self.root, self.args.timeout)
        _run([str(self.cmake), '--build', str(build), '--target', 'ps2EntryRunner',
              '--parallel', str(self.args.build_jobs or 4)],
             directory/'build.log', self.root, self.args.timeout)
        _run([str(self.cmake), '--install', str(build), '--prefix', str(self.package), '--config', 'Release'],
             directory/'install.log', self.root, self.args.timeout)
        runner = Path(self.manifest['desktop']['runner'])
        if not runner.is_file():
            raise PipelineError('offline relink did not produce the desktop runner')
        self.remember()
        return {'runner_sha256': sha256_file(runner), 'catalog_sha256': recovered['catalog_sha256']}

    def remember(self):
        # The memory is private and keyed by ISO, generator and source version.
        # Only manifests whose compiled sources passed validation are retained.
        from lab import generate_ee_bank_catalog as catalog_tool
        prior = self.memory/'catalog'
        if prior.exists():
            catalog_tool.extend_catalog(self.cases, self.generator, prior)
        else:
            staged = self.memory/'catalog.staging'
            if staged.exists():
                raise PipelineError('unfinished adaptation memory publication; preserve and inspect the staging directory')
            shutil.copytree(self.catalog, staged)
            validate_catalog(staged, self.generator)
            staged.rename(prior)

    def run(self) -> dict:
        settings = dict(self.manifest['automatic_adaptation'])
        if self.startup_arguments:
            self._write_startup_arguments()
        result = run_adaptation(self.workspace/'adaptation', probe=self.probe, recover=self.recover,
                                rebuild=self.rebuild, max_rounds=self.args.adapt_rounds,
                                configure_startup=self.configure_startup)
        if self.startup_arguments:
            # A failed/repeated diagnostic must not teach a later conversion
            # that its candidate arguments worked. The completed probe only
            # confirms startup progress, never menu or hardware fidelity.
            _write_json(self.memory/'startup.json', {
                'schema_version': 1, 'identity': settings['identity'],
                'guest_arguments': self.startup_arguments,
                'origin': 'observed guest working-directory diagnostic',
                'startup_probe_confirmed': True,
                'menu_approved': False, 'native_execution_qualified': False})
        return {**settings, **result, 'catalog_manifest': str(self.catalog/'catalog.json') if self.cases else '',
                'guest_startup_arguments': self.startup_arguments,
                'receipt': str(self.workspace/'adaptation/adaptation.json')}
