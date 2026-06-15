#include "world/WorldSave.h"

#include <algorithm>
#include <cstring>
#include <format>
#include <fstream>
#include <iomanip>
#include <string>
#include <type_traits>

#include "vox/core/Log.h"

namespace vc {

namespace {

constexpr uint32_t kMagic = 0x31525856; // "VXR1"
constexpr uint8_t kFormatRle = 0;       // id RLE only (meta all 0)
constexpr uint8_t kFormatRaw = 1;       // raw ids only (meta all 0)
constexpr uint8_t kFormatRleMeta = 2;   // M24: id RLE, then a meta RLE stream
constexpr uint8_t kFormatRawMeta = 3;   // M24: raw ids, then a meta RLE stream
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

// RLE a run-length stream of values onto blob (no header): {value, run}
// pairs, both little-endian u16. Meta is u8 but stored in a u16 cell so the
// stream stays 4-byte aligned like the id stream (meta blobs are usually a
// single 0-run, so the wasted byte is free).
template <typename T>
void AppendRle(std::vector<uint8_t>& blob, const T* data, size_t n) {
    size_t i = 0;
    while (i < n) {
        const auto v = data[i];
        size_t run = 1;
        while (i + run < n && data[i + run] == v && run < 0xFFFF) {
            ++run;
        }
        AppendU16(blob, static_cast<uint16_t>(v));
        AppendU16(blob, static_cast<uint16_t>(run));
        i += run;
    }
}

std::vector<uint8_t> Encode(const Chunk& chunk) {
    const auto& raw = chunk.Raw();
    const auto& meta = chunk.RawMeta();
    const bool hasMeta = std::any_of(meta.begin(), meta.end(), [](uint8_t m) { return m != 0; });

    // Build the id payload first to choose RLE vs raw (raw only when a
    // pathological pattern makes RLE larger than the flat array).
    std::vector<uint8_t> idRle;
    AppendRle(idRle, raw.data(), raw.size());
    const bool useRaw = idRle.size() > kRawBytes;

    std::vector<uint8_t> blob;
    if (useRaw) {
        blob.assign(1, hasMeta ? kFormatRawMeta : kFormatRaw);
        blob.resize(1 + kRawBytes);
        std::memcpy(blob.data() + 1, raw.data(), kRawBytes);
    } else {
        blob.push_back(hasMeta ? kFormatRleMeta : kFormatRle);
        blob.insert(blob.end(), idRle.begin(), idRle.end());
    }
    // M24: append the meta RLE stream only when something is oriented, so
    // the common all-zero case stays bit-identical to a pre-M24 blob.
    if (hasMeta) {
        AppendRle(blob, meta.data(), meta.size());
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
    ReadFurnaces();

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
            } else if (tag == "vitals") {
                Vitals v;
                if (in >> v.health >> v.foodLevel >> v.saturation >> v.exhaustion >> v.air) {
                    m_vitals = v;
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
    if (m_vitals) {
        out << std::setprecision(9) << "vitals " << m_vitals->health << ' ' << m_vitals->foodLevel
            << ' ' << m_vitals->saturation << ' ' << m_vitals->exhaustion << ' ' << m_vitals->air
            << '\n';
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

void WorldSave::SetVitals(const Vitals& vitals) {
    m_vitals = vitals;
    WriteManifest();
}

void WorldSave::ReadFurnaces() {
    std::ifstream in{m_dir / "furnaces.dat"};
    if (!in) {
        return;
    }
    std::string tag;
    while (in >> tag && tag == "furnace") {
        FurnaceRecord r;
        if (!(in >> r.pos.x >> r.pos.y >> r.pos.z >> r.id[0] >> r.count[0] >> r.damage[0] >>
              r.id[1] >> r.count[1] >> r.damage[1] >> r.id[2] >> r.count[2] >> r.damage[2] >>
              r.burnTicks >> r.burnTotal >> r.cookTicks)) {
            GAME_ERROR("Save: malformed furnaces.dat line; later furnaces dropped");
            return;
        }
        m_furnaces.push_back(r);
    }
}

void WorldSave::WriteFurnaces() const {
    const auto path = m_dir / "furnaces.dat";
    if (m_furnaces.empty()) {
        std::error_code ec;
        std::filesystem::remove(path, ec);
        return;
    }
    const auto tmp = m_dir / "furnaces.dat.tmp";
    {
        std::ofstream out{tmp, std::ios::trunc};
        for (const FurnaceRecord& r : m_furnaces) {
            out << "furnace " << r.pos.x << ' ' << r.pos.y << ' ' << r.pos.z;
            for (int slot = 0; slot < 3; ++slot) {
                out << ' ' << r.id[slot] << ' ' << r.count[slot] << ' ' << r.damage[slot];
            }
            out << ' ' << r.burnTicks << ' ' << r.burnTotal << ' ' << r.cookTicks << '\n';
        }
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

void WorldSave::SetFurnaces(std::vector<FurnaceRecord> furnaces) {
    m_furnaces = std::move(furnaces);
    m_furnacesDirty = true;
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
    out = Chunk{}; // reset ids + meta; legacy formats leave meta all-air-0
    if (blob.empty()) {
        return false;
    }
    const uint8_t format = blob[0];
    const bool raw = format == kFormatRaw || format == kFormatRawMeta;
    const bool rle = format == kFormatRle || format == kFormatRleMeta;
    const bool withMeta = format == kFormatRleMeta || format == kFormatRawMeta;
    size_t pos = 1;

    // Decode one RLE stream of `count` cells from pos into dst[], advancing
    // pos. False on truncation or a bad run.
    const auto decodeRle = [&](auto* dst, size_t count) {
        size_t cell = 0;
        while (cell < count) {
            if (blob.size() - pos < 4) {
                return false;
            }
            const auto value =
                static_cast<std::remove_reference_t<decltype(*dst)>>(blob[pos] | blob[pos + 1] << 8);
            const size_t run = static_cast<size_t>(blob[pos + 2] | blob[pos + 3] << 8);
            pos += 4;
            if (run == 0 || cell + run > count) {
                return false;
            }
            std::fill_n(dst + cell, run, value);
            cell += run;
        }
        return true;
    };

    auto& ids = out.Raw();
    if (raw) {
        if (blob.size() < 1 + kRawBytes) {
            out = Chunk{};
            return false;
        }
        std::memcpy(ids.data(), blob.data() + 1, kRawBytes);
        pos = 1 + kRawBytes;
    } else if (rle) {
        if (!decodeRle(ids.data(), ids.size())) {
            out = Chunk{};
            return false;
        }
    } else {
        out = Chunk{};
        return false;
    }

    if (withMeta && !decodeRle(out.RawMeta().data(), Chunk::kVolume)) {
        out = Chunk{};
        return false;
    }
    return true;
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
    if (m_dirtyRegions.empty() && !m_furnacesDirty) {
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
    if (!m_dirtyRegions.empty()) {
        GAME_INFO("Save: wrote {} region file(s)", m_dirtyRegions.size());
    }
    m_dirtyRegions.clear();
    if (m_furnacesDirty) {
        WriteFurnaces();
        m_furnacesDirty = false;
    }
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
