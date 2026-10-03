#!/usr/bin/env python3
"""Prepare an observed EE miss for offline bank generation, never runtime compilation."""
import argparse
import hashlib
import json
from pathlib import Path
import re
import struct
import subprocess
import tempfile
import zlib

RAM_BYTES = 32 * 1024 * 1024


def ram_offset(address):
    """Physical RAM for a physical PC or a direct-mapped KSEG0/KSEG1 PC."""
    if type(address) is not int or not 0 <= address <= 0xFFFFFFFF:
        return RAM_BYTES
    if address < RAM_BYTES:
        return address
    if (address & 0xE0000000) not in (0x80000000, 0xA0000000):
        return RAM_BYTES
    physical = address & 0x1FFFFFFF
    return physical if physical < RAM_BYTES else RAM_BYTES


def bounded_bytes(path, bound):
    with path.open('rb') as stream:
        data = stream.read(bound + 1)
    if len(data) > bound:
        raise ValueError('EE miss artifact exceeds its bound')
    return data


def prepare(capture, generator, output):
    if output.exists():
        raise ValueError('EE case output already exists')
    request_bytes = bounded_bytes(capture / 'request.json', 1024 * 1024)
    request = json.loads(request_bytes)
    if not isinstance(request, dict):
        raise ValueError('EE miss request must be an object')
    base, entry, size = request['window_base'], request['target_pc'], request['window_bytes']
    physical_base = ram_offset(base)
    if any(type(request[field]) is not int for field in
           ('schema_version', 'window_base', 'target_pc', 'window_bytes', 'ee_model_profile')) or \
            request['schema_version'] != 1 or request['processor'] != 'EE' or \
            request['ee_model_profile'] != 1 or request['runtime_admission'] != 'missing' or \
            request['complete'] is not True or request['ram_captured'] is not True or \
            type(request['module_owns_address']) is not bool or \
            (request['module_owns_address'] and
             (request.get('entry_binding_missing') is not True or
              not isinstance(request.get('module_key'), str) or
              not 1 <= len(request['module_key']) <= 4096)) or \
            type(request['context_captured']) is not bool or \
            request['complete_machine_checkpoint'] is not False or request['quiescence_qualified'] is not False or \
            request['overlay_lookup_status'] not in ('MissingEntry', 'CodeChanged') or \
            not 0 < size <= 65536 or size % 4 or base < 0 or base % 4 or \
            physical_base >= RAM_BYTES or size > RAM_BYTES - physical_base or entry % 4 or not base <= entry < base + size:
        raise ValueError('EE miss identity/context is not an admitted byte-generation case')
    ram = bounded_bytes(capture / 'ee-ram.bin', RAM_BYTES)
    image = bounded_bytes(capture / 'snapshot.bin', 65536)
    if len(ram) != RAM_BYTES or len(image) != size or image != ram[physical_base:physical_base + size]:
        raise ValueError('EE snapshot differs from the captured full RAM')
    cpu_hash = None
    if request['context_captured']:
        cpu = bounded_bytes(capture / 'ee-context.bin', 8192)
        if len(cpu) != 1643 or cpu[:8] != b'NEXOEE\0\0' or \
                struct.unpack_from('<III', cpu, 8) != (1, 1, 1619) or \
                struct.unpack_from('<I', cpu, 20)[0] != zlib.crc32(cpu[:20] + cpu[24:]) or \
                cpu[24 + 1350] > 1:
            raise ValueError('invalid canonical EE model context')
        if struct.unpack_from('<I', cpu, 24 + 512)[0] != entry or cpu[24 + 1350]:
            raise ValueError('captured PC/delay-slot context requires a separate entry contract')
        cpu_hash = hashlib.sha256(cpu).hexdigest()
    generator_hash = hashlib.sha256(generator.read_bytes()).hexdigest()
    with tempfile.TemporaryDirectory() as temporary:
        snapshot, code_file = Path(temporary) / 'snapshot.bin', Path(temporary) / 'bank.cpp'
        snapshot.write_bytes(image)
        result = subprocess.run([str(generator.resolve()), str(snapshot), str(base), str(entry), str(code_file)],
                                capture_output=True, text=True, timeout=30)
        if result.returncode:
            raise ValueError('offline EE generator failed: ' + result.stderr)
        code = bounded_bytes(code_file, 64 * 1024 * 1024).decode()
    rows = []
    for address, begin, count, offset in re.findall(
            r'\{(0x[0-9a-f]+)u,ps2native_block_[0-9a-f]+,(0x[0-9a-f]+)u,'
            r'(0x[0-9a-f]+)u,snapshot\+(0x[0-9a-f]+)u\}', code):
        address, begin, count, offset = map(lambda word: int(word, 16), (address, begin, count, offset))
        if begin != base + offset or offset % 4 or offset >= size or count <= 0 or count % 4 or \
                count > size - offset or address % 4 or not begin <= address < begin + count:
            raise ValueError('offline generator emitted an invalid dependency')
        rows.append({'address': address, 'source_begin': begin, 'source_bytes': count})
    if not rows or len(rows) > 32768 or len({row['address'] for row in rows}) != len(rows) or \
            not any(row['address'] == entry for row in rows) or \
            hashlib.sha256(generator.read_bytes()).hexdigest() != generator_hash:
        raise ValueError('offline EE generator identity/root/bindings were not admitted')
    metadata = {'schema_version': 1, 'origin': 'observed-ee-miss', 'base': base, 'entry': entry,
                'dependency_contract': 'normal-entry-v1',
                'image_bytes': size, 'image_sha256': hashlib.sha256(image).hexdigest(), 'bindings': rows,
                'capture_request_sha256': hashlib.sha256(request_bytes).hexdigest(),
                'ee_ram_sha256': hashlib.sha256(ram).hexdigest(), 'cpu_snapshot_sha256': cpu_hash,
                'context_captured': request['context_captured'], 'generator_sha256': generator_hash,
                'module_owns_address': request['module_owns_address'],
                'module_key': request.get('module_key', ''),
                'preparer_sha256': hashlib.sha256(Path(__file__).read_bytes()).hexdigest(),
                'complete_machine_checkpoint': False,
                'scope': 'observed RAM byte case; entry-context/fetch/closure/fidelity and full machine replay unqualified'}
    output.mkdir()
    (output / 'snapshot.bin').write_bytes(image)
    (output / 'bank.json').write_text(json.dumps(metadata, indent=2) + '\n')
    return metadata


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('capture', type=Path)
    parser.add_argument('--generator', type=Path, required=True)
    parser.add_argument('--output', type=Path, required=True)
    args = parser.parse_args()
    try:
        metadata = prepare(args.capture, args.generator, args.output)
        print(json.dumps({'bindings': len(metadata['bindings']), 'base': metadata['base'], 'entry': metadata['entry']}))
    except (ValueError, OSError, KeyError, TypeError, subprocess.TimeoutExpired) as error:
        parser.error(str(error))


if __name__ == '__main__':
    main()
