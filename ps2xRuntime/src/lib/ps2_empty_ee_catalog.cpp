#include "ps2_ee_aot_bank.h"

// Bootstrap for conversion: every unseen entry is a strict miss. Recovery
// replaces this empty program with offline compiled, byte-guarded banks.
const ps2native::ee_aot::Program &compiledEeProgram()
{
    static const ps2native::ee_aot::Program program{};
    return program;
}
