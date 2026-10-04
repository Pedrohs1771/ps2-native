"""Reject private payloads in the tracked source snapshot, independent of ignores."""
from pathlib import Path, PurePosixPath
import subprocess
import sys

ROOT = Path(__file__).resolve().parents[1]
PRIVATE = ('fixtures/', 'test-data/isos/', 'ps2xRuntime/vita/module/',
           'ps2xStudio/external/Font/')
PAYLOAD = {'.iso', '.chd', '.cso', '.zso', '.elf', '.irx', '.suprx', '.vpk', '.apk'}


def violation(name, header=b'', disc_signature=b''):
    if name == 'test-data/isos/README.md':
        return None
    if name.startswith(PRIVATE) or PurePosixPath(name).suffix.lower() in PAYLOAD:
        return 'private input or binary payload path'
    if header.startswith(b'\x7fELF'):
        return 'ELF executable content'
    if disc_signature == b'CD001':
        return 'ISO9660 disc content'
    return None


def main():
    tracked = subprocess.check_output(['git', 'ls-files', '-z'], cwd=ROOT).decode().split('\0')
    errors = []
    for name in filter(None, tracked):
        path = ROOT / name
        with path.open('rb') as stream:
            header = stream.read(4)
            stream.seek(32769)
            signature = stream.read(5)
        reason = violation(name, header, signature)
        if reason:
            errors.append(f'{name}: {reason}')
    if errors:
        print('\n'.join(errors), file=sys.stderr)
        return 1
    print('Tracked source snapshot: no recognized private payloads.')
    return 0


if __name__ == '__main__':
    sys.exit(main())
