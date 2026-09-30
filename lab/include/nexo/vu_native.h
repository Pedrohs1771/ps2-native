#pragma once

#include "runtime/ps2_vu1.h"

#include <span>
#include <vector>

namespace ps2native::nexo
{
struct VuNativeProgram;

// Laboratory V0: a finite bank of operations specialized before compilation.
// Native execution takes data memory and a compiled bank, never guest code.
class VuNativeAccess
{
public:
    using DecodedPair = VU1Interpreter::DecodedInstructionPair;
    using InstructionUsage = VU1Interpreter::InstructionUsage;
    using Pipeline = VU1Interpreter::Pipeline;
    using VfAccess = VU1Interpreter::VfAccess;
    using Unit = VU1Interpreter::Unit;

    struct Entry
    {
        const DecodedPair *decoded;
        void (*upper)(VU1Interpreter &);
        void (*lower)(VU1Interpreter &, uint8_t *, uint32_t, GS &, PS2Memory *);
    };

    // Converter-only frontend. It lives in a separate library from the native
    // scheduler and must not be called by the delivered native runner.
    static std::vector<DecodedPair> inspectMicrocode(std::span<const uint8_t> code, Unit unit);

    static void resume(VU1Interpreter &vu, const VuNativeProgram &program,
        uint8_t *data, uint32_t dataSize, GS &gs, PS2Memory *memory,
        uint32_t top, uint32_t itop, uint32_t budget);

    static void execute(VU1Interpreter &vu, const VuNativeProgram &program,
        uint8_t *data, uint32_t dataSize, GS &gs, PS2Memory *memory,
        uint32_t startPC, uint32_t top, uint32_t itop, uint32_t budget);

    template <uint32_t Instruction> static void upper(VU1Interpreter &vu);
    template <uint32_t Instruction> static void lower(VU1Interpreter &vu,
        uint8_t *vuData, uint32_t dataSize, GS &gs, PS2Memory *memory);

private:
    static void validateBinding(const VU1Interpreter &vu, const VuNativeProgram &program,
        const uint8_t *data, uint32_t dataSize, uint32_t budget);
    static constexpr uint32_t kAccForwardLatency = VU1Interpreter::kAccForwardLatency;
    static constexpr Pipeline PipelineBranch = VU1Interpreter::PipelineBranch;
    template <uint32_t Instruction> static bool calculateFmacExactResult(
        VU1Interpreter &vu, uint32_t component, long double &result);
    template <uint32_t Instruction> static uint32_t calculateFmacProductSticky(
        VU1Interpreter &vu, uint8_t dest);
    template <uint32_t Instruction> static void normalizeFmacResult(
        VU1Interpreter &vu, float *result, uint8_t dest, uint8_t laneFlags[4]);
    template <uint32_t Instruction> static void applyFmacDest(
        VU1Interpreter &vu, float *dst, float *result, uint8_t dest);
    template <uint32_t Instruction> static void applyFmacDestAcc(
        VU1Interpreter &vu, float *result, uint8_t dest);
};

struct VuNativeProgram
{
    VU1Interpreter::Unit unit;
    std::span<const VuNativeAccess::Entry> entries;
    // Identity guard only. The scheduler does not read/decode these bytes.
    std::span<const uint8_t> codeIdentity;
};
}
