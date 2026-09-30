#pragma once

#include <cstdint>
#include <span>
#include <vector>

class PS2Memory;
class GifArbiter;
class GS;

namespace ps2native::nexo
{
// Execution and concurrent writers must be paused. Formats are explicit,
// bounded, little-endian and transactional; no host pointers are serialized.
class Vif1SnapshotCodec
{
public:
    static std::vector<uint8_t> encode(const PS2Memory &memory);
    static void restore(PS2Memory &memory, std::span<const uint8_t> bytes);
};

class GifSnapshotCodec
{
public:
    static std::vector<uint8_t> encode(const GifArbiter &arbiter);
    static void restore(GifArbiter &arbiter, std::span<const uint8_t> bytes);
};

class GsSnapshotCodec
{
public:
    static std::vector<uint8_t> encode(const GS &gs);
    static void restore(GS &gs, std::span<const uint8_t> bytes);
private:
    struct State;
    template <typename A, typename G> static void frontendFields(A &a, G &gs);
    template <typename A> static void stateFields(A &a, State &state);
    static void validate(const State &state);
};
}
