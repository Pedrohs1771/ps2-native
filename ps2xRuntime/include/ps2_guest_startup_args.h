#pragma once

#include <string>
#include <vector>

namespace ps2_guest_startup_args
{
    void set(std::vector<std::string> arguments);
    std::vector<std::string> get();
}
