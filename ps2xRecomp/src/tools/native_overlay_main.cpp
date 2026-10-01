#include "ps2recomp/native_overlay.h"
#include <fstream>
#include <iostream>
#include <iterator>
#include <vector>

int main(int argc, char **argv)
{
    if (argc != 5 && (argc != 6 || std::string(argv[5]) != "--legacy-footprints"))
    { std::cerr << "usage: ps2_native_overlay snapshot.bin base entry output.cpp [--legacy-footprints]\n"; return 2; }
    try
    {
        std::ifstream input(argv[1], std::ios::binary);
        if (!input) throw std::runtime_error("cannot read overlay snapshot");
        const std::vector<uint8_t> bytes((std::istreambuf_iterator<char>(input)), {});
        const auto code = ps2recomp::generateNativeOverlay(bytes, std::stoul(argv[2], nullptr, 0),
                                                          std::stoul(argv[3], nullptr, 0),
                                                          argc == 6 ? ps2recomp::OverlayDependencyContract::LegacyWholeBlock
                                                                    : ps2recomp::OverlayDependencyContract::NormalEntry);
        std::ofstream output(argv[4]);
        output << code;
        if (!output) throw std::runtime_error("cannot write native overlay C++");
        return 0;
    }
    catch (const std::exception &error) { std::cerr << error.what() << '\n'; return 1; }
}
