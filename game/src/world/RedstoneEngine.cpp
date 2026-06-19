#include "world/RedstoneEngine.h"

#include <algorithm>
#include <unordered_map>
#include <vector>

#include "world/Block.h"
#include "world/World.h"

namespace vc {

namespace {

// Wire spreads/connects across the four horizontal neighbours only (RS1 is
// flat — no climbing the side of a taller block; deferred refine). The full
// six dirs are used when scanning for what POWERS a wire / lamp, since a
// source or wire directly below/above a consumer still feeds it.
constexpr glm::ivec3 kHoriz[4] = {{1, 0, 0}, {-1, 0, 0}, {0, 0, 1}, {0, 0, -1}};
constexpr glm::ivec3 kFaces[6] = {
    {1, 0, 0}, {-1, 0, 0}, {0, 1, 0}, {0, -1, 0}, {0, 0, 1}, {0, 0, -1},
};

struct IVec3Hash {
    size_t operator()(const glm::ivec3& v) const {
        return static_cast<size_t>(v.x) * 73856093u ^ static_cast<size_t>(v.y) * 19349663u ^
               static_cast<size_t>(v.z) * 83492791u;
    }
};

bool IsWire(const World& w, const glm::ivec3& p) {
    return w.GetBlock(p.x, p.y, p.z) == blocks::RedstoneWire;
}

bool IsLamp(BlockId id) {
    return id == blocks::RedstoneLampOff || id == blocks::RedstoneLampOn;
}

// Power a source at `p` injects into an adjacent wire (0 if it isn't an
// on-source). RS2 will add the redstone torch + repeater here.
int SourcePower(const World& w, const glm::ivec3& p) {
    const BlockId id = w.GetBlock(p.x, p.y, p.z);
    if (id == blocks::RedstoneBlock) {
        return 15;
    }
    if (id == blocks::Lever) {
        return facing::LeverOn(w.GetMeta(p.x, p.y, p.z)) ? 15 : 0;
    }
    return 0;
}

// Does the cell at `p`, seen by an adjacent lamp, count as "powered"? An
// on-source, or a wire carrying any power. (RS1 skips the strong/weak
// distinction — see the milestone notes.)
bool EmitsToConsumer(const World& w, const glm::ivec3& p) {
    if (SourcePower(w, p) > 0) {
        return true;
    }
    return IsWire(w, p) && facing::WirePower(w.GetMeta(p.x, p.y, p.z)) > 0;
}

} // namespace

void RedstoneEngine::Update(const glm::ivec3& pos) {
    World& w = m_world;
    constexpr size_t kMaxNet = 4096; // backstop against a pathological run

    // 1. Flood-fill the connected wire network. Seed from pos if it's wire,
    //    else from any wire face-neighbour (so toggling/placing a source or
    //    breaking a support block next to a run still recomputes it).
    std::vector<glm::ivec3> net;
    std::unordered_map<glm::ivec3, int, IVec3Hash> index;
    std::vector<glm::ivec3> stack;
    auto consider = [&](const glm::ivec3& p) {
        if (!IsWire(w, p) || index.count(p) != 0 || net.size() >= kMaxNet) {
            return;
        }
        index.emplace(p, static_cast<int>(net.size()));
        net.push_back(p);
        stack.push_back(p);
    };
    if (IsWire(w, pos)) {
        consider(pos);
    } else {
        for (const auto& d : kFaces) {
            consider(pos + d);
        }
    }
    while (!stack.empty()) {
        const glm::ivec3 c = stack.back();
        stack.pop_back();
        for (const auto& d : kHoriz) {
            consider(c + d);
        }
    }

    // 2/3. Power per wire = max adjacent source, then BFS-relax across wire
    //      neighbours dropping 1 per step (LightEngine's block-light spread).
    std::vector<int> power(net.size(), 0);
    std::vector<int> queue;
    for (size_t i = 0; i < net.size(); ++i) {
        int src = 0;
        for (const auto& d : kFaces) {
            src = std::max(src, SourcePower(w, net[i] + d));
        }
        power[i] = src;
        if (src > 0) {
            queue.push_back(static_cast<int>(i));
        }
    }
    for (size_t h = 0; h < queue.size(); ++h) {
        const int i = queue[h];
        const int p = power[i];
        if (p <= 1) {
            continue;
        }
        for (const auto& d : kHoriz) {
            const auto it = index.find(net[i] + d);
            if (it != index.end() && power[it->second] < p - 1) {
                power[it->second] = p - 1;
                queue.push_back(it->second);
            }
        }
    }

    // 4. Write back changed wire power. SetBlock bumps dataVersion (remesh ->
    //    the brighter tile) and wakes neighbours; the network is now stable, so
    //    those re-updates recompute the same values and no-op — it converges.
    for (size_t i = 0; i < net.size(); ++i) {
        const uint8_t want = facing::WireMeta(power[i]);
        if (w.GetMeta(net[i].x, net[i].y, net[i].z) != want) {
            w.SetBlock(net[i], blocks::RedstoneWire, want);
        }
    }

    // 5. Refresh the lamps pos / the network touch (lit iff a face-neighbour
    //    emits power). Reads the wire metas just written above.
    auto refreshLamp = [&](const glm::ivec3& lp) {
        const BlockId id = w.GetBlock(lp.x, lp.y, lp.z);
        if (!IsLamp(id)) {
            return;
        }
        bool lit = false;
        for (const auto& d : kFaces) {
            if (EmitsToConsumer(w, lp + d)) {
                lit = true;
                break;
            }
        }
        const BlockId want = lit ? blocks::RedstoneLampOn : blocks::RedstoneLampOff;
        if (id != want) {
            w.SetBlock(lp, want, w.GetMeta(lp.x, lp.y, lp.z));
        }
    };
    refreshLamp(pos); // pos itself might be a lamp that just (un)powered
    for (const auto& d : kFaces) {
        refreshLamp(pos + d); // a source/wire at pos lights adjacent lamps
    }
    for (const auto& c : net) {
        for (const auto& d : kFaces) {
            refreshLamp(c + d);
        }
    }
}

} // namespace vc
