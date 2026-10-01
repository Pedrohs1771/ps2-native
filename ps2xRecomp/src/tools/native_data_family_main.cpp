#include "ps2recomp/native_data_family.h"

#include <filesystem>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <vector>

namespace
{
    std::vector<uint32_t> readWords(const char *path)
    {
        std::ifstream input(path,std::ios::binary | std::ios::ate);
        if (!input) throw std::invalid_argument("cannot read bounded family input");
        const auto length=input.tellg();
        if (length<=0 || length>512 || length%4) throw std::invalid_argument("invalid family input size");
        input.seekg(0);
        std::vector<uint32_t> words;
        for(std::streamoff offset=0;offset<length;offset+=4)
        {
            unsigned char bytes[4]{};
            input.read(reinterpret_cast<char*>(bytes),4);
            if(!input) throw std::invalid_argument("truncated family input");
            words.push_back(uint32_t(bytes[0]) | (uint32_t(bytes[1])<<8) |
                            (uint32_t(bytes[2])<<16) | (uint32_t(bytes[3])<<24));
        }
        return words;
    }
}

int main(int argc,char **argv)
{
    if(argc!=4)
    {
        std::cerr << "usage: ps2_native_data_family words.bin masks.bin fresh-output.cpp\n";
        return 2;
    }
    try
    {
        const auto words=readWords(argv[1]),masks=readWords(argv[2]);
        const auto code=ps2recomp::generateNativeDataFamily(words,masks);
        if(std::filesystem::exists(argv[3]) || std::filesystem::is_symlink(argv[3]))
            throw std::invalid_argument("family output must be fresh");
        std::ofstream output(argv[3]);
        output << code;
        if(!output) throw std::invalid_argument("cannot write native family source");
        return 0;
    }
    catch(const std::exception &error) { std::cerr << error.what() << '\n';return 1; }
}
