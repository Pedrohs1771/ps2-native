#pragma once

#include "nexo/vu_native.h"

class PS2Runtime;

namespace ps2native::nexo
{
// Laboratory integration for an initialized runtime. Installs strict native
// MSCAL/MSCNT callbacks into the existing memory session. Bank metadata and
// identities are copied; their compiled function pointers must remain loaded.
// Unknown code throws UNSEEN_CODE. Reinitializing/rebinding memory requires a
// new installation; this does not qualify a final interpreter-free package.
void bindNativeVu1(PS2Runtime &runtime, std::span<const VuNativeProgram> programs);
}
