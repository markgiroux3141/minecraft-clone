#include "world/WorldSave.h"

#include <cstring>
#include <format>
#include <fstream>
#include <iomanip>
#include <string>

#include "vox/core/Log.h"

namespace vc {

namespace {

constexpr uint32_t kMagic = 0x31525856; // "VXR1"
constexpr uint8_t kFormatRle = 0;
constexpr uint8_t kFormatRaw = 1;
constexpr size_t kRawBytes = Chunk::kVolume * sizeof(BlockId);
constexpr auto kFlushInterval = std::chrono::seconds(3);
constexpr int kManifestVersion = 1;

void AppendU16(std::vector<uint8_t>& out, uint16_t v) {
    out.push_back(static_cast<uint8_t>(v & 0xFF));
    out.push_back(static_cast<uint8_t>(v >> 8));
}

void AppendU32(std::vector<uint8_t>& out, uint32_t v) {
    AppendU16(out, static_cast<uint16_t>(v & 0xFFFF));
    AppendU16(out, static_cast<uint16_t>(v >> 16));
}

// Bounds-checked little-endian cursor over a loaded file.
struct Reader {
    const uint8_t* data;
    size_t size;
    size_t pos = 0;

    bool ReadU32(uint32_t& v) {
        if (size - pos < 4) {
            return false;
        }
        v = static_cast<uint32_t>(data[pos]) | static_cast<uint32_t>(data[pos + 1]) << 8 |
            static_cast<uint32_t>(data[pos + 2]) << 16 | static_cast<uint32_t>(data[pos + 3]) << 24;
        pos += 4;
        return true;
    }
};

std::vector<uint8_t> Encode(const Chunk& chunk) {
    const auto& raw = chunk.Raw();
    std::vector<uint8_t> blob;
    blob.push_back(kFormatRle);
    size_t i = 0;
    while (i < raw.size()) {
        const BlockId id = raw[i];
        size_t run = 1;
        while (i + run < raw.size() && raw[i + run] == id && run < 0xFFFF) {
            ++run;
        }
        AppendU16(blob, id);
        AppendU16(blob, static_cast<uint16_t>(run));
        i += run;
    }
    if (blob.size() > 1 + kRawBytes) { // pathological pattern: store raw instead
        blob.assign(1, kFormatRaw);
        blob.resize(1 + kRawBytes);
        std::memcpy(blob.data() + 1, raw.data(), kRawBytes);
    }
    return blob;
}

} // namespace

WorldSave::WorldSave(std::filesystem::path dir, int defaultSeed)
    : m_dir(std::move(dir)), m_lastFlush(std::chrono::steady_clock::now()) {
    std::error_code ec;
    std::filesystem::create_directories(m_dir, ec);
    if (ec) {
        GAME_ERROR("Save: cannot create {} ({}); world will not persist", m_dir.string(),
                   ec.message());
    }
    ReadManifest(defaultSeed);

    for (const auto& entry : std::filesystem::directory_iterator(m_dir, ec)) {
        if (entry.is_regular_file() && entry.path().extension() == ".vxr") {
            ReadRegionFile(entry.path());
        }
    }
    GAME_INFO("Save: {} (seed {}, {} saved chunks in {} regions)", m_dir.string(), m_seed,
              m_count, m_regions.size());
}

void WorldSave::ReadManifest(int defaultSeed) {
    m_seed = defaultSeed;
    const auto path = m_dir / "level.dat";
    if (std::ifstream in{path}) {
        std::string tag;
        int version = 0;
        long long seed = 0;
        if (in >> tag >> version >> tag >> seed && tag == "seed") {
            m_seed = static_cast<int>(seed);
        } else {
            GAME_ERROR("Save: malformed {}; using seed {}", path.string(), defaultSeed);
            return;
        }
        // Optional tagged lines, any order; unknown tags end parsing.
        while (in >> tag) {
            if (tag == "player") {
                PlayerState player;
                int fly = 0;
                if (in >> player.position.x >> player.position.y >> player.position.z >>
                    player.yaw >> player.pitch >> fly) {
                    player.fly = fly != 0;
                    m_player = player;
                }
            } else if (tag == "time") {
                int64_t ticks = 0;
                if (in >> ticks) {
                    m_worldTime = ticks;
                }
            } else if (tag == "inventory" || tag == "inventory2") {
                const bool withDamage = tag == "inventory2";
                size_t n = 0;
                if (in >> n) {
                    std::vector<InventorySlot> slots;
                    slots.reserve(n);
                    InventorySlot s;
                    while (slots.size() < n && in >> s.slot >> s.id >> s.count &&
                           (!withDamage || in >> s.damage)) {
                        slots.push_back(s);
                    }
                    if (slots.size() == n) {
                        m_inventory = std::move(slots);
                    }
                }
            } else {
                break;
            }
        }
        return;
    }
    WriteManifest();
}

void WorldSave::WriteManifest() const {
    std::ofstream out{m_dir / "level.dat"};
    out << "voxcraft-save " << kManifestVersion << "\nseed " << m_seed << '\n';
    if (m_worldTime) {
        out << "time " << *m_worldTime << '\n';
    }
    if (m_player) {
        out << std::setprecision(9) << "player " << m_player->position.x << ' '
            << m_player->position.y << ' ' << m_player->position.z << ' ' << m_player->yaw << ' '
            << m_player->pitch << ' ' << (m_player->fly ? 1 : 0) << '\n';
    }
    if (m_inventory) {
        out << "inventory2 " << m_inventory->size();
        for (const InventorySlot& s : *m_inventory) {
            out << ' ' << s.slot << ' ' << s.id << ' ' << s.count << ' ' << s.damage;
        }
        out << '\n';
    }
}

void WorldSave::SetPlayerState(const PlayerState& state) {
    m_player = state;
    WriteManifest();
}

void WorldSave::SetWorldTime(int64_t ticks) {
    m_worldTime = ticks;
    WriteManifest();
}

void WorldSave::SetInventory(std::vector<InventorySlot> slots) {
    m_inventory = std::move(slots);
    WriteManifest();
}

void WorldSave::ReadRegionFile(const std::filesystem::path& path) {
    std::ifstream in{path, std::ios::binary};
    std::vector<uint8_t> file{std::istreambuf_iterator<char>(in),
                              std::istreambuf_iterator<char>()};
    Reader r{file.data(), file.size()};

    uint32_t magic = 0;
    uint32_t count = 0;
    if (!in || !r.ReadU32(magic) || magic != kMagic || !r.ReadU32(count)) {
        GAME_ERROR("Save: {} is not a region file; skipping", path.string());
        return;
    }
    struct Entry {
        glm::ivec3 coord;
        uint32_t size;
    };
    std::vector<Entry> index;
    index.reserve(count);
    for (uint32_t i = 0; i < count; ++i) {
        uint32_t x, y, z, size;
        if (!r.ReadU32(x) || !r.ReadU32(y) || !r.ReadU32(z) || !r.ReadU32(size)) {
            GAME_ERROR("Save: {} index truncated; skipping", path.string());
            return;
        }
        index.push_back({{static_cast<int>(x), static_cast<int>(y), static_cast<int>(z)}, size});
    }
    for (const Entry& e : index) {
        if (file.size() - r.pos < e.size) {
            GAME_ERROR("Save: {} data truncated; later chunks dropped", path.string());
            return;
        }
        std::vector<uint8_t> blob(file.begin() + static_cast<ptrdiff_t>(r.pos),
                                  file.begin() + static_cast<ptrdiff_t>(r.pos + e.size));
        r.pos += e.size;
        auto [it, inserted] = m_regions[RegionOf(e.coord)].insert_or_assign(e.coord,
                                                                            std::move(blob));
        if (inserted) {
            ++m_count;
        }
    }
}

const std::vector<uint8_t>* WorldSave::FindBlob(const glm::ivec3& chunkCoord) const {
    const auto regionIt = m_regions.find(RegionOf(chunkCoord));
    if (regionIt == m_regions.end()) {
        return nullptr;
    }
    const auto it = regionIt->second.find(chunkCoord);
    return it != regionIt->second.end() ? &it->second : nullptr;
}

bool WorldSave::Decode(const std::vector<uint8_t>& blob, Chunk& out) {
    auto& raw = out.Raw();
    if (blob.size() == 1 + kRawBytes && blob[0] == kFormatRaw) {
        std::memcpy(raw.data(), blob.data() + 1, kRawBytes);
        return true;
    }
    if (!blob.empty() && blob[0] == kFormatRle && (blob.size() - 1) % 4 == 0) {
        size_t cell = 0;
        for (size_t pos = 1; pos < blob.size(); pos += 4) {
            const auto id = static_cast<BlockId>(blob[pos] | blob[pos + 1] << 8);
            const size_t run = static_cast<size_t>(blob[pos + 2] | blob[pos + 3] << 8);
            if (run == 0 || cell + run > raw.size()) {
                break;
            }
            std::fill_n(raw.begin() + static_cast<ptrdiff_t>(cell), run, id);
            cell += run;
        }
        if (cell == raw.size()) {
            return true;
        }
    }
    out = Chunk{}; // don't leave a partial decode behind
    return false;
}

void WorldSave::Put(const glm::ivec3& chunkCoord, const Chunk& chunk) {
    auto [it, inserted] =
        m_regions[RegionOf(chunkCoord)].insert_or_assign(chunkCoord, Encode(chunk));
    if (inserted) {
        ++m_count;
    }
    m_dirtyRegions.insert(RegionOf(chunkCoord));
}

void WorldSave::Flush(bool force) {
    if (m_dirtyRegions.empty()) {
        return;
    }
    const auto now = std::chrono::steady_clock::now();
    if (!force && now - m_lastFlush < kFlushInterval) {
        return;
    }
    m_lastFlush = now;
    for (const auto& region : m_dirtyRegions) {
        WriteRegionFile(region);
    }
    GAME_INFO("Save: wrote {} region file(s)", m_dirtyRegions.size());
    m_dirtyRegions.clear();
}

void WorldSave::WriteRegionFile(const glm::ivec2& region) const {
    const RegionChunks& chunks = m_regions.at(region);
    std::vector<uint8_t> file;
    AppendU32(file, kMagic);
    AppendU32(file, static_cast<uint32_t>(chunks.size()));
    for (const auto& [coord, blob] : chunks) {
        AppendU32(file, static_cast<uint32_t>(coord.x));
        AppendU32(file, static_cast<uint32_t>(coord.y));
        AppendU32(file, static_cast<uint32_t>(coord.z));
        AppendU32(file, static_cast<uint32_t>(blob.size()));
    }
    for (const auto& [coord, blob] : chunks) {
        file.insert(file.end(), blob.begin(), blob.end());
    }

    const auto path = m_dir / std::format("r.{}.{}.vxr", region.x, region.y);
    const auto tmp = m_dir / std::format("r.{}.{}.vxr.tmp", region.x, region.y);
    {
        std::ofstream out{tmp, std::ios::binary | std::ios::trunc};
        out.write(reinterpret_cast<const char*>(file.data()),
                  static_cast<std::streamsize>(file.size()));
        if (!out) {
            GAME_ERROR("Save: failed writing {}", tmp.string());
            return;
        }
    }
    std::error_code ec;
    std::filesystem::rename(tmp, path, ec);
    if (ec) {
        GAME_ERROR("Save: failed replacing {} ({})", path.string(), ec.message());
    }
}

} // namespace vc
