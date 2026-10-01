#!/usr/bin/env python3
"""Recover identified ABI-1 Linux x86-64 EE snapshots without loading a DSO.

This decodes the artifact layout of the diagnostic producer. It is not an ELF
execution engine, an ABI proof, or evidence of closure or hardware fidelity.
"""
from __future__ import annotations

import argparse
import hashlib
import json
from pathlib import Path
import struct

RAM_BYTES = 32 * 1024 * 1024
MAX_FILE_BYTES = 128 * 1024 * 1024


class Artifact:
    def __init__(self, data: bytes):
        self.data = data
        if len(data) > MAX_FILE_BYTES or len(data) < 64 or data[:7] != b'\x7fELF\x02\x01\x01':
            raise ValueError('expected a bounded ELF64 little-endian artifact')
        header = self.unpack('<HHIQQQIHHHHHH', 16)
        if header[0:3] != (3, 62, 1) or header[7] != 64 or header[10] != 64:
            raise ValueError('expected the identified Linux x86-64 ET_DYN layout')
        offset, count = header[5], header[11]
        if not 1 <= count <= 4096:
            raise ValueError('unsupported or unbounded section table')
        self.bytes(offset, count * 64)
        self.sections = [self.unpack('<IIQQQQIIQQ', offset + index * 64) for index in range(count)]
        self.symbol_tables = {}
        for index, section in enumerate(self.sections):
            if section[1] not in (2, 11):
                continue
            if section[9] != 24 or section[5] % 24 or section[6] >= count:
                raise ValueError('invalid symbol table')
            strings = self.sections[section[6]]
            if strings[1] != 3:
                raise ValueError('symbol names lack a string table')
            names = self.section_bytes(strings)
            symbols = []
            table = self.section_bytes(section)
            for position in range(0, len(table), 24):
                name, info, other, owner, address, size = struct.unpack_from('<IBBHQQ', table, position)
                symbols.append({'name': self.string(names, name), 'type': info & 15,
                                'owner': owner, 'address': address, 'size': size})
            self.symbol_tables[index] = symbols
        self.functions = {symbol['address'] for symbols in self.symbol_tables.values() for symbol in symbols
                          if symbol['type'] == 2 and 'ps2native_block_' in symbol['name'] and
                          0 < symbol['owner'] < len(self.sections) and
                          self.sections[symbol['owner']][2] & 4 and
                          self.function_is_file_backed(symbol)}
        self.relocations = {}
        for section in self.sections:
            if section[1] != 4:  # SHT_RELA
                continue
            if section[9] != 24 or section[5] % 24 or section[6] not in self.symbol_tables:
                raise ValueError('invalid relocation table')
            table = self.section_bytes(section)
            for position in range(0, len(table), 24):
                destination, info, addend = struct.unpack_from('<QQq', table, position)
                if destination in self.relocations:
                    raise ValueError('overlapping pointer relocations')
                self.relocations[destination] = (info & 0xFFFFFFFF, info >> 32, addend, section[6])

    def bytes(self, offset, size):
        if offset < 0 or size < 0 or offset > len(self.data) or size > len(self.data) - offset:
            raise ValueError('artifact range outside file')
        return self.data[offset:offset + size]

    def unpack(self, format, offset):
        return struct.unpack(format, self.bytes(offset, struct.calcsize(format)))

    @staticmethod
    def string(table, offset):
        if offset >= len(table):
            raise ValueError('string offset outside table')
        end = table.find(b'\0', offset)
        if end < 0 or end - offset > 4096:
            raise ValueError('unterminated or unbounded artifact string')
        return table[offset:end].decode('ascii')

    def section_bytes(self, section):
        if section[1] == 8:
            raise ValueError('required bytes are not file-backed')
        return self.bytes(section[4], section[5])

    def function_is_file_backed(self, symbol):
        section = self.sections[symbol['owner']]
        offset = symbol['address'] - section[3]
        if section[1] == 8 or offset < 0 or offset >= section[5] or \
                symbol['size'] <= 0 or symbol['size'] > section[5] - offset:
            return False
        self.bytes(section[4] + offset, symbol['size'])
        return True

    def object(self, name):
        matches = [symbol for index, symbols in self.symbol_tables.items()
                   if self.sections[index][1] == 2 for symbol in symbols if symbol['name'] == name]
        if len(matches) != 1 or matches[0]['type'] != 1:
            raise ValueError(f'missing or ambiguous identified object {name}')
        symbol = matches[0]
        if not 0 < symbol['owner'] < len(self.sections):
            raise ValueError('object lacks an ordinary section')
        section = self.sections[symbol['owner']]
        displacement = symbol['address'] - section[3]
        if displacement < 0 or displacement > section[5] or symbol['size'] > section[5] - displacement:
            raise ValueError('object outside its section')
        if section[1] == 8:
            raise ValueError('object is not file-backed')
        return symbol, self.bytes(section[4] + displacement, symbol['size'])

    def pointer(self, address, function=False):
        if address not in self.relocations:
            raise ValueError('required pointer lacks an identified relocation')
        kind, index, addend, table = self.relocations[address]
        symbols = self.symbol_tables[table]
        if kind == 8 and index == 0:  # R_X86_64_RELATIVE
            value = addend
        elif kind == 1 and index < len(symbols):  # R_X86_64_64
            symbol = symbols[index]
            if not 0 < symbol['owner'] < len(self.sections):
                raise ValueError('pointer references an external/undefined symbol')
            value = symbol['address'] + addend
        else:
            raise ValueError('unsupported artifact pointer relocation')
        if value < 0:
            raise ValueError('negative relocated pointer')
        if function:
            if value not in self.functions:
                raise ValueError('binding callback is not an identified native block')
        return value


def extract(data: bytes, entry: int):
    artifact = Artifact(data)
    snapshot, image = artifact.object('_ZL8snapshot')
    table, bindings = artifact.object('_ZL8bindings')
    if not image or len(image) > 65536 or len(image) % 4 or not bindings or len(bindings) % 32:
        raise ValueError('snapshot/binding dimensions differ from the identified producer')
    if len(bindings) // 32 > 32768:
        raise ValueError('too many overlay bindings')
    base = None
    addresses = set()
    rows = []
    for offset in range(0, len(bindings), 32):
        address = struct.unpack_from('<I', bindings, offset)[0]
        begin, size = struct.unpack_from('<II', bindings, offset + 16)
        artifact.pointer(table['address'] + offset + 8, function=True)
        pointer = artifact.pointer(table['address'] + offset + 24)
        displacement = pointer - snapshot['address']
        candidate_base = begin - displacement
        if displacement < 0 or size == 0 or size % 4 or displacement % 4 or size > len(image) - displacement:
            raise ValueError('binding source range outside recovered snapshot')
        if candidate_base < 0 or candidate_base % 4 or candidate_base >= RAM_BYTES or len(image) > RAM_BYTES - candidate_base:
            raise ValueError('recovered guest base outside physical EE RAM')
        if address % 4 or address < begin or address - begin >= size or address in addresses:
            raise ValueError('invalid or duplicate binding address')
        if base is not None and base != candidate_base:
            raise ValueError('inconsistent snapshot base across bindings')
        base = candidate_base
        addresses.add(address)
        rows.append({'address': address, 'source_begin': begin, 'source_bytes': size})
    if entry not in addresses:
        raise ValueError('requested root is absent from the recovered bindings')
    return image, {'schema_version': 1, 'base': base, 'entry': entry,
                   'image_bytes': len(image), 'image_sha256': hashlib.sha256(image).hexdigest(),
                   'artifact_sha256': hashlib.sha256(data).hexdigest(), 'bindings': rows,
                   'identified_layout': 'Linux x86-64 diagnostic overlay ABI 1',
                   'live_abi_independently_verified': False,
                   'scope': 'artifact recovery only; no execution, closure or hardware fidelity qualification'}


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('library', type=Path)
    parser.add_argument('output', type=Path)
    parser.add_argument('--entry', type=lambda value: int(value, 0), required=True)
    args = parser.parse_args()
    if args.output.exists():
        parser.error('output already exists')
    try:
        with args.library.open('rb') as stream:
            data = stream.read(MAX_FILE_BYTES + 1)
        image, metadata = extract(data, args.entry)
        args.output.mkdir()
        (args.output / 'snapshot.bin').write_bytes(image)
        (args.output / 'bank.json').write_text(json.dumps(metadata, indent=2) + '\n')
    except (ValueError, OSError, UnicodeError, struct.error) as error:
        parser.error(str(error))


if __name__ == '__main__':
    main()
