#pragma once

#include <array>
#include <cstdint>

#include "math.hpp"

namespace aa {

class Renderer;

struct Particle {
    Vec3 pos{};
    Vec3 vel{};
    Color color{1,1,1,1};
    float life{};
    float maxLife{};
    float size{0.03f};
    float gravity{};
    float drag{1.0f};
    float emissive{2.0f};
    bool active{};
};

class ParticleSystem {
public:
    static constexpr std::size_t kMaxParticles = 192;

    void reset();
    void update(float dt);
    void render(Renderer& renderer) const;

    void spawnMuzzle(const Vec3& pos, const Vec3& dir, const Color& color);
    void spawnImpact(const Vec3& pos, const Vec3& normalHint, const Color& color);
    void spawnDeath(const Vec3& pos, const Color& color);

private:
    Particle& allocate();
    float rand01();
    Vec3 randomUnitish();

    std::array<Particle, kMaxParticles> particles_{};
    std::uint32_t cursor_{};
    std::uint32_t rng_{0x31415926u};
};

} // namespace aa
