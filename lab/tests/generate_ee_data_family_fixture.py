"""Build-time synthetic differential fixtures using the actual offline frontends."""
from pathlib import Path
import argparse
import json
import struct
import subprocess
import sys
import tempfile

parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument('family_tool', type=Path)
parser.add_argument('overlay_tool', type=Path)
parser.add_argument('output', type=Path)
parser.add_argument('--candidate-json', type=Path)
parser.add_argument('--shape')
args = parser.parse_args()
family_tool, overlay_tool, output = args.family_tool, args.overlay_tool, args.output
# Shapes, not per-title execution adapters. Includes a generic saved-frame tail.
shapes = [([0x3C010000, 0xACA20000, 0x03E00008, 0], [0xFFFF0000, 0xFFFF0000, 0xFFFFFFFF, 0xFFFFFFFF],
           [[0, 0], [0xFFFF, 0xFFFF], [0x8000, 0x8000], [0x7FFF, 0x7FFF]]),
          ([0x3C010000, 0xAC220000, 0x03C0E82D, 0xDFBF0020, 0xDFBE0010, 0x27BD0030, 0x03E00008, 0],
           [0xFFFF0000, 0xFFFF0000] + [0xFFFFFFFF] * 6,
           [[0x10F, 0xBAAC], [0x172, 0x526C], [0x185, 0xF8C4], [0x186, 0xE32C]]),
          ([0x0080F809, 0xACBF0000], [0xFFFFFFFF, 0xFFFF0000],
           [[0], [0xFFFF], [0x8000], [0x7FFF]])]
observed_bases = None
if args.candidate_json is None:
    # Exhaustive supported data-opcode classes, with signed/unsigned boundaries.
    for opcode in [0x09,0x0a,0x0b,0x0c,0x0d,0x0e,0x0f,0x20,0x21,0x23,0x24,0x25,
                   0x27,0x37,0x1e,0x28,0x29,0x2b,0x3f,0x1f]:
        word=(opcode<<26)|((0 if opcode==0x0f else 5)<<21)|(2<<16)
        shapes.append(([word,0x03e00008,0],[0xffff0000,0xffffffff,0xffffffff],
                       [[0],[0xffff],[0x8000],[0x7fff]]))
    # Fixed branch encodings; both outcomes, branch-likely slots and link slots
    # are exercised by the execution harness at every normal resume label.
    branches=[(opcode<<26)|(2<<21)|5 for opcode in [4,5,6,7,0x14,0x15,0x16,0x17]]
    branches += [(1<<26)|(2<<21)|(kind<<16)|0xfff9 for kind in [0,1,2,3,16,17,18,19]]
    for word in branches:
        slot=0xacbf0000 if word>>26==1 else 0x24630001
        shapes.append(([word,slot],[0xffffffff,0xffffffff],[[]]))
    shapes.append(([0x1440ffff,0x2442ffff],[0xffffffff,0xffffffff],[[]]))
    # Canonical zero operands must not erase the concrete emitter's slot
    # metadata or accidentally enable/disable its guarded countdown shortcut.
    shapes.append(([0x0080f809,0x24000000],[0xffffffff,0xffff0000],[[0],[1],[0xffff]]))
    shapes.append(([0x24420000,0x1440fffe,0x24000000],[0xffff0000,0xffffffff,0xffff0000],
                   [[0xffff,0],[0xffff,1],[0xfffe,0],[0xfffe,1]]))
    shapes.append(([0x24420000,0x24000000,0x1440fffd,0],
                   [0xffff0000,0xffff0000,0xffffffff,0xffffffff],
                   [[0xffff,0],[0xffff,1],[0xfffe,0],[0xfffe,1]]))
if args.candidate_json is not None:
    if not args.shape or args.candidate_json.stat().st_size > 16 * 1024 * 1024:
        parser.error('candidate fixture needs a bounded report and exact shape')
    report = json.loads(args.candidate_json.read_bytes())
    matches = [family for family in report['families'] if family['shape_sha256'] == args.shape]
    if len(matches) != 1:
        parser.error('candidate shape must be uniquely identified')
    family = matches[0]
    words, masks = family['guard_words'], family['guard_masks']
    observations = family['observations']
    if not 1 <= len(words) <= 128 or len(words) != len(masks) or not 1 <= len(observations) <= 64:
        parser.error('candidate fixture exceeds word/observation bounds')
    parameter_count = masks.count(0xFFFF0000)
    values = [observation['parameters'] for observation in observations]
    if any(len(row) != parameter_count for row in values):
        parser.error('candidate parameter count mismatch')
    shapes = [(words, masks, [[value & 0xFFFF for value in row] for row in values])]
    observed_bases = [observation['pc'] for observation in observations]


def encode(words):
    return struct.pack('<' + 'I' * len(words), *words)


includes = set()
bodies = []
fixtures = []


def wrap(code, name):
    lines = code.splitlines()
    includes.update(line for line in lines if line.startswith('#include'))
    body = '\n'.join(line for line in lines if not line.startswith('#include'))
    # Reference getter becomes C++ linkage inside an isolated fixture namespace.
    return 'namespace ' + name + ' {\n' + body.replace('extern "C" ', '') + '\n}\n'


with tempfile.TemporaryDirectory() as temporary:
    directory = Path(temporary)
    for shape, (words, masks, variants) in enumerate(shapes):
        (directory / 'words.bin').write_bytes(encode(words))
        (directory / 'masks.bin').write_bytes(encode(masks))
        cpp = directory / ('family-' + str(shape) + '.cpp')
        subprocess.run([str(family_tool), str(directory / 'words.bin'), str(directory / 'masks.bin'), str(cpp)], check=True)
        bodies.append(wrap(cpp.read_text(), 'family_' + str(shape)))
        for variant_index, values in enumerate(variants):
            concrete = list(words)
            position = 0
            for index, mask in enumerate(masks):
                if mask == 0xFFFF0000:
                    concrete[index] |= values[position]
                    position += 1
            bases = [observed_bases[variant_index]] if observed_bases is not None else [0x10000, 0x210000, 0x2000000 - len(words) * 4]
            for base in bases:
                number = len(fixtures)
                (directory / 'words.bin').write_bytes(encode(concrete))
                cpp = directory / ('reference-' + str(number) + '.cpp')
                subprocess.run([str(overlay_tool), str(directory / 'words.bin'), str(base), str(base), str(cpp)], check=True)
                bodies.append(wrap(cpp.read_text(), 'reference_' + str(number)))
                fixtures.append((shape, base, concrete))

code = '\n'.join(sorted(includes)) + '\n#include "ee_data_family_fixture.h"\n'
code += ''.join(bodies)
for number, (_, _, words) in enumerate(fixtures):
    code += 'static constexpr uint32_t words_' + str(number) + '[]={' + ','.join(str(word) + 'u' for word in words) + '};\n'
code += 'static const EeDataFamilyFixture fixtures[]={\n'
code += ''.join('{' + str(shape) + 'u,' + str(base) + 'u,words_' + str(number) + '},\n'
                for number, (shape, base, _) in enumerate(fixtures)) + '};\n'
code += 'std::span<const EeDataFamilyFixture> eeDataFamilyFixtures(){return fixtures;}\n'
code += 'void runEeDataFamily(unsigned shape,uint8_t *ram,R5900Context *ctx,PS2Runtime *runtime,uint32_t base){switch(shape){\n'
code += ''.join('case ' + str(shape) + ':family_' + str(shape) + '::ps2native_data_family(ram,ctx,runtime,base);return;\n'
                for shape in range(len(shapes)))
code += 'default:throw std::invalid_argument("unknown fixture shape");}}\n'
code += 'void runEeDataReference(unsigned fixture,uint8_t *ram,R5900Context *ctx,PS2Runtime *runtime){\n'
code += 'const PS2NativeOverlayBinding *bindings=nullptr;size_t count=0;uint32_t abi=0;switch(fixture){\n'
code += ''.join('case ' + str(number) + ':bindings=reference_' + str(number) + '::ps2xOverlayGetBindings(&count,&abi);break;\n'
                for number in range(len(fixtures)))
code += 'default:throw std::invalid_argument("unknown reference fixture");}\n'
code += 'for(size_t i=0;i<count;++i) if(bindings[i].address==ctx->pc){bindings[i].function(ram,ctx,runtime);return;}\n'
code += 'throw std::invalid_argument("unknown reference entry");}\n'
output.write_text(code)
