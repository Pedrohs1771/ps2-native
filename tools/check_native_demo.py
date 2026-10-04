"""Run the owned EE integration fixture without accepting skipped tests."""
from pathlib import Path
import sys
import unittest

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT))


def main():
    required = (ROOT / 'build/lab/nexo_ee_autoadapt_fixture',
                ROOT / 'build/ps2xRecomp/ps2_native_overlay')
    if not all(path.is_file() for path in required):
        print('Build the native EE fixture first; see docs/REVIEWER_GUIDE.md.', file=sys.stderr)
        return 1
    suite = unittest.defaultTestLoader.loadTestsFromName('lab.tests.test_autoadaptation_execution')
    result = unittest.TextTestRunner(verbosity=2).run(suite)
    return 0 if result.wasSuccessful() and result.testsRun > 0 and not result.skipped else 1


if __name__ == '__main__':
    sys.exit(main())
