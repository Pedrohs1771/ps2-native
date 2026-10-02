"""CLI boundary tests. Native admission cases require a configured lab catalog."""
import json
from pathlib import Path
import struct
import subprocess
import sys
import tempfile
import unittest

TOOL=Path(sys.argv.pop(1))

class ProbeTests(unittest.TestCase):
    def setUp(self):
        self.temporary=tempfile.TemporaryDirectory()
        self.root=Path(self.temporary.name)
        result=subprocess.run([str(TOOL),'--describe'],capture_output=True,text=True)
        self.assertEqual(result.returncode,0,result.stderr)
        self.profile=json.loads(result.stdout)

    def tearDown(self):
        self.temporary.cleanup()

    def run_probe(self,ram,entries,output):
        return subprocess.run([str(TOOL),str(ram),str(entries),str(output)],capture_output=True,text=True)

    def test_profile_and_invalid_arguments_do_not_execute_guest_code(self):
        self.assertFalse(self.profile['guest_execution'])
        self.assertFalse(self.profile['strict_approval'])
        self.assertIs(type(self.profile['available']),bool)
        result=subprocess.run([str(TOOL)],capture_output=True,text=True)
        self.assertEqual(result.returncode,2)

    def test_bounds_links_and_existing_evidence_are_rejected(self):
        if not self.profile['available']:
            self.skipTest('configured native family catalog is required for input admission')
        ram=self.root/'ram.bin';ram.write_bytes(bytes(32*1024*1024))
        entries=self.root/'pcs.bin';entries.write_bytes(struct.pack('<III',0,2,32*1024*1024))
        out=self.root/'report.json'
        first=self.run_probe(ram,entries,out)
        self.assertEqual(first.returncode,0,first.stderr)
        report=json.loads(out.read_text())
        self.assertEqual([row['status'] for row in report['rows']],['MissingEntry','MisalignedPc','OutsideRam'])
        self.assertFalse(report['guest_execution'])
        before=out.read_bytes()
        self.assertEqual(self.run_probe(ram,entries,out).returncode,2)
        self.assertEqual(out.read_bytes(),before)
        linked=self.root/'linked.bin';linked.symlink_to(ram)
        self.assertEqual(self.run_probe(linked,entries,self.root/'linked-report.json').returncode,2)
        entries.write_bytes(b'\0')
        self.assertEqual(self.run_probe(ram,entries,self.root/'partial.json').returncode,2)
        ram.write_bytes(b'\0')
        entries.write_bytes(struct.pack('<I',0))
        self.assertEqual(self.run_probe(ram,entries,self.root/'short-ram.json').returncode,2)

if __name__=='__main__':
    unittest.main()
