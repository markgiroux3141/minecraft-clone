#pragma once

#include <cstdint>

namespace vc {

// Exact clone of java.util.Random's LCG. The cave carver's tunnel shapes
// and the ore veins' ellipsoid sweeps live in this generator's draw
// sequence, so faithful ports need the real thing, not a stand-in PRNG.
class JavaRandom {
public:
    explicit JavaRandom(int64_t seed) { SetSeed(seed); }

    void SetSeed(int64_t seed) {
        m_state = (static_cast<uint64_t>(seed) ^ 0x5DEECE66DULL) & kMask;
    }

    int32_t NextInt(int32_t bound) {
        if ((bound & -bound) == bound) { // power of two
            return static_cast<int32_t>(
                (static_cast<int64_t>(bound) * Next(31)) >> 31);
        }
        while (true) {
            const int32_t bits = static_cast<int32_t>(Next(31));
            const int32_t value = bits % bound;
            // Java's int-overflow rejection test, done in 64-bit.
            if (static_cast<int64_t>(bits) - value + (bound - 1) <= INT32_MAX) {
                return value;
            }
        }
    }

    int64_t NextLong() {
        const auto hi = static_cast<int64_t>(static_cast<int32_t>(Next(32)));
        return (hi << 32) + static_cast<int32_t>(Next(32));
    }

    float NextFloat() { return static_cast<float>(Next(24)) / static_cast<float>(1 << 24); }

    double NextDouble() {
        const auto hi = static_cast<int64_t>(Next(26));
        return static_cast<double>((hi << 27) + Next(27)) * 0x1.0p-53;
    }

private:
    static constexpr uint64_t kMask = (1ULL << 48) - 1;

    uint32_t Next(int bits) {
        m_state = (m_state * 0x5DEECE66DULL + 0xBULL) & kMask;
        return static_cast<uint32_t>(m_state >> (48 - bits));
    }

    uint64_t m_state = 0;
};

} // namespace vc
