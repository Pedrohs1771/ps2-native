"""Require the explicit mutation inventory to cover each declared EE model cell."""
from pathlib import Path
import re
import unittest

ROOT = Path(__file__).resolve().parents[2]


def declared_cells(source):
    body = source.split('struct alignas(16) R5900Context', 1)[1].split('R5900Context()', 1)[0]
    body = re.sub(r'//[^\n]*', '', body).strip().removeprefix('{').strip()
    cells = []
    for declaration in body.split(';'):
        if not declaration.strip():
            continue
        match = re.fullmatch(r'\s*(__m128i|__m128|uint16_t|uint32_t|uint64_t|float|bool)\s+(.+)\s*',
                             declaration, re.S)
        if not match:
            raise ValueError('unreviewed EE model declaration')
        kind, names = match.groups()
        for name in names.split(','):
            field = re.fullmatch(r'\s*(\w+)(?:\[(\d+)\])?\s*', name)
            if not field:
                raise ValueError('unreviewed EE model cell')
            scalar, count = field.groups()
            for index in range(int(count) if count else 1):
                cell = f'{scalar}[{index}]' if count else scalar
                if kind.startswith('__m128'):
                    cells.extend(f'{cell}.lane{lane}' for lane in range(4))
                else:
                    cells.append(cell)
    return cells


class EeContextInventoryTests(unittest.TestCase):
    def test_every_declared_cell_has_an_independent_mutation(self):
        expected = declared_cells((ROOT / 'ps2xRuntime/include/ps2_runtime.h').read_text())
        actual = re.findall(r'check\("([^\"]+)"',
                            (ROOT / 'lab/tests/ee_context_field_inventory.inc').read_text())
        self.assertEqual(len(expected), 409)
        self.assertCountEqual(actual, expected)
        self.assertEqual(len(set(actual)), len(actual))

    def test_added_fields_require_a_new_inventory_entry(self):
        source = 'struct alignas(16) R5900Context { uint32_t added[2]; R5900Context()'
        self.assertEqual(declared_cells(source), ['added[0]', 'added[1]'])

    def test_unreviewed_declaration_syntax_is_rejected(self):
        with self.assertRaises(ValueError):
            declared_cells('struct alignas(16) R5900Context { void *pointer; R5900Context()')


if __name__ == '__main__':
    unittest.main()
