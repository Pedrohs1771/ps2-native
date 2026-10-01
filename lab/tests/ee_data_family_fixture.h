#pragma once
#include "ps2_runtime.h"
#include <span>

struct EeDataFamilyFixture
{
    uint32_t shape;
    uint32_t base;
    std::span<const uint32_t> words;
};
std::span<const EeDataFamilyFixture> eeDataFamilyFixtures();
void runEeDataFamily(unsigned shape,uint8_t *ram,R5900Context *context,PS2Runtime *runtime,uint32_t base);
void runEeDataReference(unsigned fixture,uint8_t *ram,R5900Context *context,PS2Runtime *runtime);
