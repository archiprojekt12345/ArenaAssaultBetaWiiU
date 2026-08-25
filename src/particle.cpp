#include "particle.hpp"

#include <algorithm>
#include <cmath>

#include "material.hpp"
#include "renderer.hpp"

namespace aa {

void ParticleSystem::reset() {
    for (auto& p : particles_) p = Particle{};
    cursor_ = 0;
}

Particle& ParticleSystem::allocate() {
    for (std::size_t i=0;i<particles_.size();++i) {
        const std::size_t idx = (cursor_ + i) % particles_.size();
        if (!particles_[idx].active) {
            cursor_ = static_cast<std::uint32_t>((idx + 1) % particles_.size());
            particles_[idx] = Particle{};
            particles_[idx].active = true;
            return particles_[idx];
        }
    }
    Particle& p = particles_[cursor_ % particles_.size()];
    cursor_ = (cursor_ + 1) % particles_.size();
    p = Particle{};
    p.active = true;
    return p;
}

float ParticleSystem::rand01() {
    rng_ = rng_ * 1664525u + 1013904223u;
    return float(rng_ & 0x00FFFFFFu) / float(0x01000000u);
}

Vec3 ParticleSystem::randomUnitish() {
    Vec3 v{rand01()*2.0f-1.0f, rand01()*2.0f-1.0f, rand01()*2.0f-1.0f};
    if (lengthSq(v) < 0.001f) v = {0,1,0};
    return normalize(v);
}

void ParticleSystem::update(float dt) {
    for (auto& p : particles_) {
        if (!p.active) continue;
        p.life -= dt;
        if (p.life <= 0.0f) {
            p.active = false;
            continue;
        }
        p.vel.y -= p.gravity * dt;
        const float damp = std::pow(clampf(p.drag,0.0f,1.0f), dt*60.0f);
        p.vel = p.vel * damp;
        p.pos += p.vel * dt;
    }
}

void ParticleSystem::spawnMuzzle(const Vec3& pos, const Vec3& dir, const Color& color) {
    for (int i=0;i<8;++i) {
        Particle& p = allocate();
        p.pos = pos + randomUnitish()*0.035f;
        p.vel = normalize(dir + randomUnitish()*0.55f) * (2.0f + rand01()*3.0f);
        p.color = color;
        p.life = p.maxLife = 0.035f + rand01()*0.055f;
        p.size = 0.025f + rand01()*0.045f;
        p.drag = 0.80f;
        p.emissive = 4.5f;
    }
    Particle& flash = allocate();
    flash.pos = pos + dir*0.05f;
    flash.vel = {0,0,0};
    flash.color = color;
    flash.life = flash.maxLife = 0.045f;
    flash.size = 0.11f;
    flash.drag = 1.0f;
    flash.emissive = 6.0f;
}

void ParticleSystem::spawnImpact(const Vec3& pos, const Vec3& normalHint, const Color& color) {
    const Vec3 base = lengthSq(normalHint) > 0.001f ? normalize(normalHint) : Vec3{0,1,0};
    for (int i=0;i<12;++i) {
        Particle& p = allocate();
        Vec3 d = normalize(base*0.65f + randomUnitish()*0.95f);
        p.pos = pos + d*0.02f;
        p.vel = d * (1.8f + rand01()*4.2f);
        p.color = color;
        p.life = p.maxLife = 0.12f + rand01()*0.25f;
        p.size = 0.012f + rand01()*0.022f;
        p.gravity = 4.8f;
        p.drag = 0.93f;
        p.emissive = 3.7f;
    }
}

void ParticleSystem::spawnDeath(const Vec3& pos, const Color& color) {
    for (int i=0;i<28;++i) {
        Particle& p = allocate();
        Vec3 d = randomUnitish();
        d.y = std::fabs(d.y) + 0.20f;
        d = normalize(d);
        p.pos = pos + Vec3{0,1.1f,0} + randomUnitish()*0.25f;
        p.vel = d * (1.2f + rand01()*4.6f);
        p.color = (i&1) ? color : Color{1.0f,0.34f,0.04f,1.0f};
        p.life = p.maxLife = 0.28f + rand01()*0.55f;
        p.size = 0.018f + rand01()*0.045f;
        p.gravity = 3.2f;
        p.drag = 0.965f;
        p.emissive = 3.1f;
    }
}

void ParticleSystem::render(Renderer& renderer) const {
    for (const auto& p : particles_) {
        if (!p.active) continue;
        const float alpha = clampf(p.life / std::max(p.maxLife,0.0001f),0.0f,1.0f);
        Color c = p.color;
        c.a *= alpha;
        Material m = materials::emissive(c, p.emissive * (0.35f + 0.65f*alpha));
        m.diffuse = mulColor(c, 0.75f);
        m.textureMix = 0.0f;
        const float s = p.size * (0.65f + 0.35f*alpha);
        renderer.submitBox(p.pos,{s,s,s},0.0f,m);
    }
}

} // namespace aa
