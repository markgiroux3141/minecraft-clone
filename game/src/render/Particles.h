#pragma once

#include <memory>
#include <vector>

#include <glm/glm.hpp>

#include "vox/renderer/Camera.h"
#include "vox/renderer/Shader.h"
#include "vox/renderer/VertexArray.h"

#include "world/Block.h"

namespace vc {

class World;

// Block-break particles (M20), a port of vanilla's ParticleDigging:
// chips textured with a random quarter of the block's tile, scattered
// with the EntityItem-style velocity shaping, gravity 0.04 b/tick^2,
// drag x0.98 (ground friction x0.7), lifetime 4/(rand*0.9+0.1) ticks.
// Tick-simulated, render-interpolated; drawn as one streamed batch of
// camera-facing quads (alpha-tested, depth-tested, no depth writes).
class ParticleSystem {
public:
    ParticleSystem(); // builds the shader + streaming buffers

    // 4x4x4 burst filling the broken block (addBlockDestroyEffects).
    void SpawnBlockDestroy(const World& world, const glm::ivec3& cell, BlockId id);
    // One chip at the face being dug (addBlockHitEffects): slower, smaller.
    void SpawnBlockHit(const World& world, const glm::ivec3& cell, const glm::ivec3& faceNormal,
                       BlockId id);

    // M35 explosion debris: a radial burst of chips flung from the blast center,
    // textured from the ground beneath it (the terrain it tore up); count scales
    // with the blast size. The boom sound is GameApp's; this is the visual puff.
    void SpawnExplosion(const World& world, const glm::vec3& center, float size);

    // M37 eat crumbs: a small burst falling from the player's mouth while eating
    // (vanilla ParticleItemPie), textured from the food's sprite tile (an atlas
    // layer, not a block) — so it takes the tile directly rather than a BlockId.
    void SpawnEatCrumbs(const World& world, const glm::vec3& pos, uint16_t tile);

    void Tick(const World& world); // 20 TPS, alongside the entity ticks

    void Render(const vox::PerspectiveCamera& camera, double alpha, float sunLight,
                const glm::vec3& skyTint);

    size_t Count() const { return m_particles.size(); }

private:
    struct Particle {
        glm::vec3 pos, prevPos; // center
        glm::vec3 vel;          // blocks/s
        float size;             // quad half-extent (vanilla 0.1 * scale)
        float u0, v0;           // quarter-tile UV origin (tile-local)
        uint16_t tile;
        glm::vec2 light; // sky/block 0..1, sampled at spawn
        int age = 0;
        int maxAge;
        bool onGround = false;
    };

    void Spawn(const World& world, const glm::vec3& pos, const glm::vec3& velBTick, float scale,
               BlockId id, const glm::ivec3& lightCell);

    std::vector<Particle> m_particles;
    std::shared_ptr<vox::Shader> m_shader;
    std::shared_ptr<vox::VertexArray> m_vertexArray;
    std::shared_ptr<vox::VertexBuffer> m_vertexBuffer;
    std::vector<float> m_scratch; // CPU vertex build, reused
};

} // namespace vc
