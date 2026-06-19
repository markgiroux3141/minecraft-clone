#pragma once

#include <glm/glm.hpp>

namespace vc {

class World;

// RS1 redstone power core (lever -> dust -> lamp). When a component (or a cell
// next to one) changes, recomputes the connected wire network's power and
// refreshes adjacent consumers (lamps). Structurally this is the LightEngine
// again — BFS from sources, attenuate one per step — but run synchronously on
// the main thread over the LIVE world's local network rather than a worker
// snapshot (vanilla runs redstone synchronously too, and the touched region is
// small). Nothing to persist: wire power rides the per-cell meta and lamp
// on/off is a block id, so a disturbed network just re-derives via block
// updates. Holds a World& back-reference (like EntityManager) to keep this
// logic off the World god object.
class RedstoneEngine {
public:
    explicit RedstoneEngine(World& world) : m_world(world) {}

    // `pos` just changed and is, or neighbors, a redstone component. Called
    // from World::ProcessBlockUpdate. Recomputes the wire network reachable
    // from pos and relights the lamps it (or pos) touches.
    void Update(const glm::ivec3& pos);

private:
    World& m_world;
};

} // namespace vc
