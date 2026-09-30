#pragma once
#include <cstdint>
#include <vector>
class PS2Memory;

namespace ps2native::nexo
{
struct Vif1ParserCheckpoint
{
    uint32_t remainingBytes = 0;
    bool directHl = false;
    std::vector<uint8_t> payload;
    std::vector<uint8_t> pendingCommand;
};
// Laboratory-only bridge to the existing ABI-stable sidecar. All execution
// and writers must be paused. Replacement allocation precedes publication.
Vif1ParserCheckpoint snapshotVif1Parser(const PS2Memory &memory);
void restoreVif1Parser(PS2Memory &memory, Vif1ParserCheckpoint state);
}
