#include "render/Particles.h"

#include <algorithm>
#include <cmath>

#include "vox/renderer/Renderer.h"

#include "world/Light.h"
#include "world/World.h"

namespace vc {

namespace {

constexpr size_t kMaxParticles = 2048; // soft cap; excess spawns are dropped
constexpr float kTickDt = 0.05f;
constexpr float kGravity = 16.0f; // vanilla 0.04 b/tick^2
constexpr float kHalfBox = 0.1f;  // collision box (vanilla 0.2 cube)

float Rand01() {
    static uint32_t s = 0x2545F491u;
    s ^= s << 13;
    s ^= s >> 17;
    s ^= s << 5;
    return static_cast<float>(s & 0xFFFFFFu) / 16777215.0f;
}

} // namespace

ParticleSystem::ParticleSystem() {
    m_shader = vox::Shader::FromFiles("shaders/particle.vert", "shaders/particle.frag");
    // 8 floats per vertex, 4 verts per particle; shared quad index pattern.
    m_vertexBuffer = std::make_shared<vox::VertexBuffer>(
        static_cast<uint32_t>(kMaxParticles * 4 * 8 * sizeof(float)));
    m_vertexBuffer->SetLayout({{vox::ShaderDataType::Float3, "a_position"},
                               {vox::ShaderDataType::Float3, "a_uvw"},
                               {vox::ShaderDataType::Float2, "a_light"}});
    std::vector<uint32_t> indices;
    indices.reserve(kMaxParticles * 6);
    for (uint32_t i = 0; i < kMaxParticles; ++i) {
        const uint32_t base = i * 4;
        indices.insert(indices.end(),
                       {base, base + 1, base + 2, base + 2, base + 3, base});
    }
    m_vertexArray = std::make_shared<vox::VertexArray>();
    m_vertexArray->AddVertexBuffer(m_vertexBuffer);
    m_vertexArray->SetIndexBuffer(std::make_shared<vox::IndexBuffer>(
        indices.data(), static_cast<uint32_t>(indices.size())));
}

void ParticleSystem::Spawn(const World& world, const glm::vec3& pos, const glm::vec3& velBTick,
                           float scale, BlockId id, const glm::ivec3& lightCell) {
    if (m_particles.size() >= kMaxParticles) {
        return;
    }
    Particle p;
    p.pos = pos;
    p.prevPos = pos;
    p.vel = velBTick * 20.0f; // vanilla works in blocks/tick
    p.size = 0.1f * scale;
    // Random quarter of the block's side tile (vanilla's texture jitter).
    p.u0 = Rand01() * 0.75f;
    p.v0 = Rand01() * 0.75f;
    p.tile = BlockRegistry::Get().Def(id).faceTiles[static_cast<size_t>(BlockFace::PosX)];
    p.maxAge = static_cast<int>(4.0f / (Rand01() * 0.9f + 0.1f));
    const uint8_t packed = world.PackedLightAt(lightCell);
    p.light = {static_cast<float>(ChunkLight::Sky(packed)) / 15.0f,
               static_cast<float>(ChunkLight::Block(packed)) / 15.0f};
    m_particles.push_back(p);
}

void ParticleSystem::SpawnBlockDestroy(const World& world, const glm::ivec3& cell, BlockId id) {
    // Vanilla addBlockDestroyEffects: a 4x4x4 grid of chips, each pushed
    // outward from the block center; Particle's constructor then
    // randomizes direction and rescales speed.
    for (int i = 0; i < 4; ++i) {
        for (int j = 0; j < 4; ++j) {
            for (int k = 0; k < 4; ++k) {
                const glm::vec3 off{(static_cast<float>(i) + 0.5f) / 4.0f,
                                    (static_cast<float>(j) + 0.5f) / 4.0f,
                                    (static_cast<float>(k) + 0.5f) / 4.0f};
                glm::vec3 vel = off - 0.5f;
                vel += glm::vec3{Rand01() * 2.0f - 1.0f, Rand01() * 2.0f - 1.0f,
                                 Rand01() * 2.0f - 1.0f} *
                       0.4f;
                const float speed = (Rand01() + Rand01() + 1.0f) * 0.15f;
                vel = glm::normalize(vel) * speed * 0.4f + glm::vec3{0.0f, 0.1f, 0.0f};
                // Digging chips halve the base scale (rand*0.5+0.5)*2.
                Spawn(world, glm::vec3(cell) + off, vel, Rand01() * 0.5f + 0.5f, id, cell);
            }
        }
    }
}

void ParticleSystem::SpawnBlockHit(const World& world, const glm::ivec3& cell,
                                   const glm::ivec3& faceNormal, BlockId id) {
    // Vanilla addBlockHitEffects: a random point on the dug face, 0.1
    // outside it; velocity x0.2, scale x0.6 of a destroy chip.
    glm::vec3 pos{static_cast<float>(cell.x) + 0.1f + Rand01() * 0.8f,
                  static_cast<float>(cell.y) + 0.1f + Rand01() * 0.8f,
                  static_cast<float>(cell.z) + 0.1f + Rand01() * 0.8f};
    for (int axis = 0; axis < 3; ++axis) {
        if (faceNormal[axis] > 0) {
            pos[axis] = static_cast<float>(cell[axis]) + 1.1f;
        } else if (faceNormal[axis] < 0) {
            pos[axis] = static_cast<float>(cell[axis]) - 0.1f;
        }
    }
    glm::vec3 vel{Rand01() * 2.0f - 1.0f, Rand01() * 2.0f - 1.0f, Rand01() * 2.0f - 1.0f};
    vel *= 0.4f;
    const float speed = (Rand01() + Rand01() + 1.0f) * 0.15f;
    vel = (glm::normalize(vel) * speed * 0.4f + glm::vec3{0.0f, 0.1f, 0.0f}) * 0.2f;
    Spawn(world, pos, vel, (Rand01() * 0.5f + 0.5f) * 0.6f, id, cell + faceNormal);
}

void ParticleSystem::SpawnExplosion(const World& world, const glm::vec3& center, float size) {
    // Texture the debris from the ground just under the blast (the cell at the
    // center is air now — the blast cleared it). No solid ground -> skip the
    // visual (the boom sound still plays).
    const int cx = static_cast<int>(std::floor(center.x));
    const int cz = static_cast<int>(std::floor(center.z));
    int cy = static_cast<int>(std::floor(center.y));
    BlockId id = blocks::Air;
    for (int dy = 0; dy <= 3 && id == blocks::Air; ++dy) {
        id = world.GetBlock(cx, cy - dy, cz);
    }
    if (id == blocks::Air) {
        return;
    }
    const glm::ivec3 lightCell{cx, cy, cz};
    const int count = std::min(static_cast<int>(size * 16.0f), 96);
    for (int i = 0; i < count; ++i) {
        glm::vec3 dir{Rand01() * 2.0f - 1.0f, Rand01() * 2.0f - 1.0f, Rand01() * 2.0f - 1.0f};
        const float len = glm::length(dir);
        dir = len > 1e-4f ? dir / len : glm::vec3{0.0f, 1.0f, 0.0f};
        const float speed = (Rand01() + Rand01() + 0.5f) * size * 0.12f; // b/tick
        const glm::vec3 vel = dir * speed + glm::vec3{0.0f, 0.1f, 0.0f};
        const glm::vec3 pos = center + dir * (Rand01() * size * 0.5f);
        Spawn(world, pos, vel, Rand01() + 0.6f, id, lightCell);
    }
}

void ParticleSystem::SpawnEatCrumbs(const World& world, const glm::vec3& pos, uint16_t tile) {
    // Vanilla ItemRenderer.spawnItemParticles per bite: a handful of crumbs
    // pushed out then nudged downward, textured from the food's sprite tile.
    const glm::ivec3 lightCell{static_cast<int>(std::floor(pos.x)),
                               static_cast<int>(std::floor(pos.y)),
                               static_cast<int>(std::floor(pos.z))};
    const uint8_t packed = world.PackedLightAt(lightCell);
    const glm::vec2 light{static_cast<float>(ChunkLight::Sky(packed)) / 15.0f,
                          static_cast<float>(ChunkLight::Block(packed)) / 15.0f};
    for (int i = 0; i < 5; ++i) {
        if (m_particles.size() >= kMaxParticles) {
            return;
        }
        Particle p;
        p.pos = pos;
        p.prevPos = pos;
        // Small scatter, biased downward so crumbs drop off the chin.
        p.vel = glm::vec3{(Rand01() * 2.0f - 1.0f) * 1.5f, Rand01() * 1.0f - 1.5f,
                          (Rand01() * 2.0f - 1.0f) * 1.5f};
        p.size = 0.1f * (Rand01() * 0.5f + 0.5f);
        p.u0 = Rand01() * 0.75f;
        p.v0 = Rand01() * 0.75f;
        p.tile = tile;
        p.maxAge = static_cast<int>(4.0f / (Rand01() * 0.9f + 0.1f));
        p.light = light;
        m_particles.push_back(p);
    }
}

void ParticleSystem::Tick(const World& world) {
    const auto moveAxis = [&](Particle& p, int axis, float delta) {
        if (delta == 0.0f) {
            return false;
        }
        p.pos[axis] += delta;
        const glm::vec3 boxMin = p.pos - glm::vec3{kHalfBox};
        const glm::vec3 boxMax = p.pos + glm::vec3{kHalfBox};
        const glm::ivec3 lo{glm::floor(boxMin + 1e-5f)};
        const glm::ivec3 hi{glm::floor(boxMax - 1e-5f)};
        bool collided = false;
        float resolved = p.pos[axis];
        for (int by = lo.y; by <= hi.y; ++by) {
            for (int bz = lo.z; bz <= hi.z; ++bz) {
                for (int bx = lo.x; bx <= hi.x; ++bx) {
                    if (!world.IsSolid(bx, by, bz)) {
                        continue;
                    }
                    collided = true;
                    const int cell = (axis == 0) ? bx : (axis == 1) ? by : bz;
                    if (delta > 0.0f) {
                        resolved =
                            std::min(resolved, static_cast<float>(cell) - kHalfBox - 0.001f);
                    } else {
                        resolved =
                            std::max(resolved, static_cast<float>(cell + 1) + kHalfBox + 0.001f);
                    }
                }
            }
        }
        if (collided) {
            p.pos[axis] = resolved;
            p.vel[axis] = 0.0f;
        }
        return collided;
    };

    for (size_t i = 0; i < m_particles.size();) {
        Particle& p = m_particles[i];
        p.prevPos = p.pos;
        if (++p.age >= p.maxAge) {
            m_particles.erase(m_particles.begin() + static_cast<ptrdiff_t>(i));
            continue;
        }
        p.vel.y -= kGravity * kTickDt;
        if (moveAxis(p, 1, p.vel.y * kTickDt) && p.prevPos.y >= p.pos.y) {
            p.onGround = true;
        }
        moveAxis(p, 0, p.vel.x * kTickDt);
        moveAxis(p, 2, p.vel.z * kTickDt);
        p.vel *= 0.98f;
        if (p.onGround) {
            p.vel.x *= 0.7f;
            p.vel.z *= 0.7f;
        }
        ++i;
    }
}

void ParticleSystem::Render(const vox::PerspectiveCamera& camera, double alpha, float sunLight,
                            const glm::vec3& skyTint) {
    if (m_particles.empty()) {
        return;
    }
    // Billboard corners from the camera basis, built CPU-side per frame.
    const glm::mat4 view = camera.View();
    const glm::vec3 right{view[0][0], view[1][0], view[2][0]};
    const glm::vec3 up{view[0][1], view[1][1], view[2][1]};
    const float a = static_cast<float>(alpha);

    m_scratch.clear();
    for (const Particle& p : m_particles) {
        const glm::vec3 pos = glm::mix(p.prevPos, p.pos, a);
        const glm::vec3 r = right * p.size;
        const glm::vec3 u = up * p.size;
        constexpr float kSpan = 0.2497f; // quarter tile minus a bleed guard
        const glm::vec3 corners[4] = {pos - r - u, pos + r - u, pos + r + u, pos - r + u};
        const glm::vec2 uvs[4] = {{p.u0, p.v0},
                                  {p.u0 + kSpan, p.v0},
                                  {p.u0 + kSpan, p.v0 + kSpan},
                                  {p.u0, p.v0 + kSpan}};
        for (int c = 0; c < 4; ++c) {
            m_scratch.insert(m_scratch.end(),
                             {corners[c].x, corners[c].y, corners[c].z, uvs[c].x, uvs[c].y,
                              static_cast<float>(p.tile), p.light.x, p.light.y});
        }
    }
    m_vertexBuffer->SetData(m_scratch.data(),
                            static_cast<uint32_t>(m_scratch.size() * sizeof(float)));

    m_shader->Bind();
    m_shader->SetMat4("u_viewProj", camera.ViewProjection());
    m_shader->SetInt("u_atlas", 0);
    m_shader->SetFloat("u_sunLight", sunLight);
    m_shader->SetFloat3("u_skyTint", skyTint);
    vox::Renderer::SetDepthWrite(false);
    vox::Renderer::SetCullFace(false);
    vox::Renderer::DrawIndexed(*m_vertexArray,
                               static_cast<uint32_t>(m_particles.size() * 6));
    vox::Renderer::SetDepthWrite(true);
    vox::Renderer::SetCullFace(true);
}

} // namespace vc
