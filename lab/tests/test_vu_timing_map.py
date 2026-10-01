import importlib.util
from pathlib import Path
import struct
import unittest
import zlib

SCRIPT=Path(__file__).resolve().parents[1]/"compare_vu_issue_traces.py"
spec=importlib.util.spec_from_file_location("vu_timing_map",SCRIPT)
mapper=importlib.util.module_from_spec(spec)
spec.loader.exec_module(mapper)

def envelope(variant,rows):
    payload=struct.pack('<I',len(rows))+b''.join(struct.pack('<Q3I',*r) for r in rows)
    b=bytearray(b'NEXOVPI\0'+struct.pack('<4I',1,variant,len(payload),0)+payload)
    struct.pack_into('<I',b,20,zlib.crc32(b[:20]+b[24:]));return bytes(b)

class TimingMapTests(unittest.TestCase):
    def test_raw_clock_phase_is_identified_not_fitted(self):
        model=[(10,0,7,9),(11,8,8,10)]
        reference=[(21,0,7,9),(22,8,8,10)]
        result=mapper.compare_call(model,reference,10,20,13,23)
        self.assertTrue(result['instruction_sequence_equal'])
        self.assertIsNone(result['first_issue_timing_difference'])
        self.assertEqual(result['issue_clock_delta_changes'],[])

    def test_reports_first_stall_change_and_end_drain_separately(self):
        model=[(0,0,1,2),(4,8,3,4),(5,16,5,6)]
        reference=[(1,0,1,2),(3,8,3,4),(4,16,5,6)]
        result=mapper.compare_call(model,reference,0,0,10,6)
        self.assertEqual(result['first_issue_timing_difference']['index'],1)
        self.assertEqual(result['issue_clock_delta_changes'][0]['delta_change'],2)
        self.assertEqual(result['model_tail_cycles'],5)
        self.assertEqual(result['reference_tail_cycles'],3)
        self.assertEqual(result['elapsed_delta'],4)
        self.assertEqual(result['last_issue_delta']+result['tail_delta'],4)

    def test_pc_or_code_difference_stops_timing_alignment(self):
        result=mapper.compare_call([(0,0,1,2)],[(1,8,1,2)],0,0,2,2)
        self.assertFalse(result['instruction_sequence_equal'])
        self.assertEqual(result['first_order_difference']['index'],0)
        self.assertIsNone(result['issue_clock_delta_changes'])

    def test_checksum_count_variant_and_clock_corruption_are_rejected(self):
        good=envelope(1,[(0,0,1,2),(1,8,3,4)])
        self.assertEqual(len(mapper.decode_issues(good,1)),2)
        corrupt=bytearray(good);corrupt[-1]^=1
        for data,variant in ((bytes(corrupt),1),(good,2),(good[:-1],1),
                             (envelope(1,[(2,0,1,2),(1,8,3,4)]),1),
                             (envelope(1,[(0,1,1,2)]),1)):
            with self.assertRaises(ValueError):mapper.decode_issues(data,variant)

    def test_reference_leading_tick_cannot_underflow(self):
        with self.assertRaises(ValueError):
            mapper.compare_call([(0,0,1,2)],[(0,0,1,2)],0,0,1,1)

    def test_issue_must_fit_its_original_callback_horizon(self):
        with self.assertRaises(ValueError):
            mapper.compare_call([(3,0,1,2)],[(1,0,1,2)],0,0,2,2)

    def test_extra_pair_cannot_be_hidden_by_zip_alignment(self):
        result=mapper.compare_call([(0,0,1,2),(1,8,3,4)],[(1,0,1,2)],0,0,2,2)
        self.assertFalse(result['instruction_sequence_equal'])
        self.assertEqual(result['first_order_difference']['index'],1)
        self.assertNotIn('last_issue_delta',result)

if __name__=='__main__':unittest.main()
