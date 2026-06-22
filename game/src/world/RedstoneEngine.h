#pragma once

#include <cstdint>
#include <deque>
#include <utility>

#include <glm/glm.hpp>

namespace vc {

class World;

// Redstone power core. When a component (or a cell next to one) changes,
// recomputes the connected wire network's power and refreshes the consumers it
// touches (lamps, redstone torches). Structurally the wire spread is the
// LightEngine again — BFS from sources, attenuate one per step — but run
// synchronously on the LIVE world rather than a worker snapshot (vanilla runs
// redstone synchronously too, and the touched region is small).
//
// RS2a added the vanilla STRONG/WEAK powering model (ported from 1.12
// World.getRedstonePower / isBlockIndirectlyGettingPowered):
//   - A component emits WEAK power to adjacent cells and STRONG power to
//     specific cells (a redstone torch strong-powers the block above it; a
//     floor lever strong-powers the block below it; redstone wire strong-powers
//     the block it sits on).
//   - A full opaque solid cube (`IsNormalCube`) re-emits the STRONG power it
//     receives to all its neighbours ("indirect" powering). This is what lets a
//     torch under a block, or dust on top of a block, power things on the far
//     side — and what turns a redstone torch off when its mount block is
//     powered (the NOT gate).
//   - Wire reads the indirect power feeding it (excluding other wires, which the
//     BFS relaxation handles) and the strongest neighbour wire minus one.
//
// Nothing here is persisted: wire power rides the per-cell meta and component
// on/off is a block id, so a disturbed network re-derives via block updates.
// The only state is the transient torch-burnout history (matches vanilla — the
// toggle list isn't saved). Holds a World& back-reference (like EntityManager)
// to keep this logic off the World god object.
class RedstoneEngine {
public:
    explicit RedstoneEngine(World& world) : m_world(world) {}

    // `pos` just changed and is, or neighbors, a redstone component. Called
    // from World::ProcessBlockUpdate. Recomputes the wire network reachable
    // from pos and refreshes the lamps + redstone torches it touches.
    void Update(const glm::ivec3& pos);

private:
    // Refresh one consumer cell (no-op if it isn't a lamp / redstone torch).
    void RefreshLamp(const glm::ivec3& lampPos);
    void RefreshTorch(const glm::ivec3& torchPos);
    // Burnout bookkeeping: count this position's turn-off toggles in the last
    // 60 ticks (vanilla BlockRedstoneTorch). `record` adds the current toggle.
    int RecentToggleCount(const glm::ivec3& pos, bool record);

    World& m_world;
    // (pos, tick) of recent redstone-torch turn-OFF events, pruned to a 60-tick
    // window. >= 8 at one position => that torch burns out (forced off, 160-tick
    // lockout) — caps runaway torch oscillators.
    std::deque<std::pair<glm::ivec3, uint64_t>> m_toggles;
};

} // namespace vc
