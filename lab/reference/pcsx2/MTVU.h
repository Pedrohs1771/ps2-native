#pragma once
#include "Vif.h"
// The unmodified source retains a disabled MTVU branch; its register layout is
// named here solely to compile that branch. THREAD_VU1 is fixed to zero.
struct ReferenceVuThread { VIFregisters vifRegs; };
inline ReferenceVuThread vu1Thread;
