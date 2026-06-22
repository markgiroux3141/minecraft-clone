#include "world/RedstoneEngine.h"

#include <algorithm>
#include <unordered_map>
#include <unordered_set>
#include <vector>

#include "world/Block.h"
#include "world/World.h"

namespace vc {

namespace {

// Wire spreads/connects across the four horizontal neighbours only (flat — no
// climbing the side of a taller block; deferred refine). The six faces are used
// when scanning for what POWERS a cell.
constexpr glm::ivec3 kHoriz[4] = {{1, 0, 0}, {-1, 0, 0}, {0, 0, 1}, {0, 0, -1}};
constexpr glm::ivec3 kFaces[6] = {
    {1, 0, 0}, {-1, 0, 0}, {0, 1, 0}, {0, -1, 0}, {0, 0, 1}, {0, 0, -1},
};
constexpr glm::ivec3 kUp{0, 1, 0};
constexpr glm::ivec3 kDown{0, -1, 0};

struct IVec3Hash {
    size_t operator()(const glm::ivec3& v) const {
        return static_cast<size_t>(v.x) * 73856093u ^ static_cast<size_t>(v.y) * 19349663u ^
               static_cast<size_t>(v.z) * 83492791u;
    }
};

BlockId Id(const World& w, const glm::ivec3& p) { return w.GetBlock(p.x, p.y, p.z); }
uint8_t Meta(const World& w, const glm::ivec3& p) { return w.GetMeta(p.x, p.y, p.z); }

bool IsWire(const World& w, const glm::ivec3& p) { return Id(w, p) == blocks::RedstoneWire; }
bool IsLamp(BlockId id) {
    return id == blocks::RedstoneLampOff || id == blocks::RedstoneLampOn;
}
bool IsTorch(BlockId id) {
    return id == blocks::RedstoneTorchOn || id == blocks::RedstoneTorchOff;
}

// A full opaque solid cube conducts (re-emits) the strong power it receives —
// vanilla isNormalCube. Slabs/stairs/liquids/cutouts/plants are not full cubes.
bool IsNormalCube(const World& w, const glm::ivec3& p) {
    const BlockDef& def = BlockRegistry::Get().Def(Id(w, p));
    return def.opaque && def.solid && !def.cutout && !def.slab && !def.stairs && !def.liquid;
}

// The direction a redstone torch points (its FACING): up for a floor torch, the
// horizontal away-from-wall direction for a wall torch. Reused for both states.
glm::ivec3 TorchPointDir(uint8_t meta) {
    return facing::TorchIsWall(meta) ? facing::Dir(facing::TorchWallFacing(meta)) : kUp;
}

// Does the wire at `p` logically connect toward horizontal dir `hd`? (Flat-only:
// a wire or any power-providing component sitting in that direction.) Drives the
// vanilla directional weak-power rule.
bool WireConnects(const World& w, const glm::ivec3& p, const glm::ivec3& hd) {
    const BlockId id = Id(w, p + hd);
    return id == blocks::RedstoneWire || id == blocks::Lever || id == blocks::RedstoneBlock ||
           IsTorch(id);
}

// Weak power a redstone wire emits toward `side` (= the unit dir from the
// querying cell toward the wire), per BlockRedstoneWire.getWeakPower:
//   - UP face -> full power (so dust on a block powers the block it sits on),
//   - DOWN face -> 0 (never powers the block above it),
//   - horizontal -> full only at a "tip" (connected that way, not crossing).
int WireWeakPower(const World& w, const glm::ivec3& p, const glm::ivec3& side) {
    const int i = facing::WirePower(Meta(w, p));
    if (i == 0) {
        return 0;
    }
    if (side == kUp) {
        return i;
    }
    if (side == kDown) {
        return 0;
    }
    // Horizontal side: collect the directions this wire connects toward.
    bool connect[4];
    bool any = false;
    for (int d = 0; d < 4; ++d) {
        connect[d] = WireConnects(w, p, kHoriz[d]);
        any = any || connect[d];
    }
    if (!any) {
        return i; // isolated dust powers all four horizontal neighbours
    }
    // Find which horizontal index `side` is, plus its two perpendiculars.
    int s = -1;
    for (int d = 0; d < 4; ++d) {
        if (side == kHoriz[d]) {
            s = d;
        }
    }
    if (s < 0) {
        return 0;
    }
    // kHoriz = {+X,-X,+Z,-Z}; the X pair (0,1) is perpendicular to the Z pair
    // (2,3). Power flows out `side` only if connected that way and NOT turning.
    const bool perp = (s < 2) ? (connect[2] || connect[3]) : (connect[0] || connect[1]);
    return (connect[s] && !perp) ? i : 0;
}

// Weak power the component at `p` emits toward `side` (= dir from the querying
// cell toward p). `excludeWire` makes wires contribute nothing (vanilla's
// canProvidePower=false while a wire recomputes its own value).
int WeakPower(const World& w, const glm::ivec3& p, const glm::ivec3& side, bool excludeWire) {
    const BlockId id = Id(w, p);
    if (id == blocks::RedstoneBlock) {
        return 15;
    }
    if (id == blocks::Lever) {
        return facing::LeverOn(Meta(w, p)) ? 15 : 0;
    }
    if (id == blocks::RedstoneTorchOn) {
        // Weak 15 to every face except the one it points along (its FACING).
        return side == TorchPointDir(Meta(w, p)) ? 0 : 15;
    }
    if (id == blocks::RedstoneWire) {
        return excludeWire ? 0 : WireWeakPower(w, p, side);
    }
    return 0;
}

// Strong power the component at `p` emits toward `side` (= dir from the querying
// cell toward p). A torch strong-powers the block above it; a floor lever the
// block below it; wire's strong power equals its weak power.
int StrongPower(const World& w, const glm::ivec3& p, const glm::ivec3& side, bool excludeWire) {
    const BlockId id = Id(w, p);
    if (id == blocks::RedstoneTorchOn) {
        return side == kDown ? 15 : 0; // torch below the queried cell -> powers it
    }
    if (id == blocks::Lever) {
        // Floor lever (v1): mounted on the block below it, so it strong-powers
        // that support — queried from below, side == UP.
        return (facing::LeverOn(Meta(w, p)) && side == kUp) ? 15 : 0;
    }
    if (id == blocks::RedstoneWire) {
        return excludeWire ? 0 : WireWeakPower(w, p, side);
    }
    return 0; // redstone block: weak source only, conducts no strong power
}

// Strong power a (normal-cube) block at `p` receives from its neighbours — the
// value it then re-emits indirectly. Vanilla World.getStrongPower(pos).
int StrongPowerReceived(const World& w, const glm::ivec3& p, bool excludeWire) {
    int best = 0;
    for (const auto& d : kFaces) {
        best = std::max(best, StrongPower(w, p + d, d, excludeWire));
    }
    return best;
}

// Power the cell at `p` presents to a neighbour querying from direction `side`:
// a normal cube hands back the strong power it receives; anything else its own
// weak power. Vanilla World.getRedstonePower.
int RedstonePower(const World& w, const glm::ivec3& p, const glm::ivec3& side, bool excludeWire) {
    return IsNormalCube(w, p) ? StrongPowerReceived(w, p, excludeWire)
                              : WeakPower(w, p, side, excludeWire);
}

// Max power reaching `pos` from any of its six neighbours. Vanilla
// World.isBlockIndirectlyGettingPowered. `excludeWire` is used when computing a
// wire's own injected power (other wires are handled by the BFS relaxation).
int IndirectPower(const World& w, const glm::ivec3& pos, bool excludeWire) {
    int best = 0;
    for (const auto& d : kFaces) {
        best = std::max(best, RedstonePower(w, pos + d, d, excludeWire));
    }
    return best;
}

} // namespace

int RedstoneEngine::RecentToggleCount(const glm::ivec3& pos, bool record) {
    const uint64_t now = m_world.SimTick();
    while (!m_toggles.empty() && now - m_toggles.front().second > 60) {
        m_toggles.pop_front();
    }
    if (record) {
        m_toggles.emplace_back(pos, now);
    }
    int count = 0;
    for (const auto& t : m_toggles) {
        if (t.first == pos) {
            ++count;
        }
    }
    return count;
}

void RedstoneEngine::RefreshLamp(const glm::ivec3& lampPos) {
    const BlockId id = Id(m_world, lampPos);
    if (!IsLamp(id)) {
        return;
    }
    const bool lit = IndirectPower(m_world, lampPos, false) > 0;
    const BlockId want = lit ? blocks::RedstoneLampOn : blocks::RedstoneLampOff;
    if (id != want) {
        m_world.SetBlock(lampPos, want, Meta(m_world, lampPos));
    }
}

void RedstoneEngine::RefreshTorch(const glm::ivec3& torchPos) {
    const BlockId id = Id(m_world, torchPos);
    if (!IsTorch(id)) {
        return;
    }
    const uint8_t meta = Meta(m_world, torchPos);
    // The torch inverts the power of its mount block: ON iff that block is NOT
    // powered (vanilla shouldBeOff = isSidePowered(attachment, -FACING)).
    const glm::ivec3 point = TorchPointDir(meta);
    const glm::ivec3 attach = torchPos - point;
    const bool shouldBeOff = RedstonePower(m_world, attach, -point, false) > 0;
    const bool isOn = (id == blocks::RedstoneTorchOn);

    if (isOn && shouldBeOff) {
        m_world.SetBlock(torchPos, blocks::RedstoneTorchOff, meta);
        if (RecentToggleCount(torchPos, true) >= 8) {
            // Burned out: stay off and re-check well after the toggle window
            // clears (vanilla 160 ticks) — caps a self-feeding torch oscillator.
            m_world.ScheduleBlockUpdate(torchPos, 160);
        }
    } else if (!isOn && !shouldBeOff) {
        if (RecentToggleCount(torchPos, false) < 8) {
            m_world.SetBlock(torchPos, blocks::RedstoneTorchOn, meta);
        }
    }
}

void RedstoneEngine::Update(const glm::ivec3& pos) {
    const World& w = m_world;
    constexpr size_t kMaxNet = 4096; // backstop against a pathological run

    // 1. Flood-fill the connected wire network. Seed from pos if it's wire, else
    //    from any wire face-neighbour (so toggling/placing a source or breaking a
    //    support block next to a run still recomputes it).
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

    // 2/3. Wire power = max indirect power injected (EXCLUDING other wires —
    //      those propagate via the relaxation), then BFS-relax across wire
    //      neighbours dropping 1 per step (LightEngine's block-light spread).
    std::vector<int> power(net.size(), 0);
    std::vector<int> queue;
    for (size_t i = 0; i < net.size(); ++i) {
        const int inject = IndirectPower(w, net[i], true);
        power[i] = inject;
        if (inject > 0) {
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

    // 4. Write back changed wire power. SetBlock bumps dataVersion (remesh -> the
    //    brighter tile) and wakes neighbours; the network is now stable, so those
    //    re-updates recompute the same values and no-op — it converges.
    for (size_t i = 0; i < net.size(); ++i) {
        const uint8_t want = facing::WireMeta(power[i]);
        if (Meta(w, net[i]) != want) {
            m_world.SetBlock(net[i], blocks::RedstoneWire, want);
        }
    }

    // 5. Refresh consumers (lamps + redstone torches) in the affected region.
    //    Reach two face-rings out from pos and every network cell: a consumer
    //    can sit one block past a normal cube that a wire/source powers (the cube
    //    conducts), so one ring is not enough. Each consumer is handled once.
    std::unordered_set<glm::ivec3, IVec3Hash> seen;
    auto visit = [&](const glm::ivec3& c) {
        if (!seen.insert(c).second) {
            return;
        }
        const BlockId id = Id(w, c);
        if (IsLamp(id)) {
            RefreshLamp(c);
        } else if (IsTorch(id)) {
            RefreshTorch(c);
        }
    };
    auto sweep = [&](const glm::ivec3& origin) {
        visit(origin);
        for (const auto& d1 : kFaces) {
            const glm::ivec3 a = origin + d1;
            visit(a);
            for (const auto& d2 : kFaces) {
                visit(a + d2);
            }
        }
    };
    sweep(pos);
    for (const auto& c : net) {
        sweep(c);
    }
}

} // namespace vc
