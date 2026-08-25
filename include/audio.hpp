#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <vector>

#include "math.hpp"

namespace aa {

enum class AudioEvent : std::uint32_t {
    PlayerShot = 0,
    PlayerReload,
    PlayerHit,
    EnemyShot,
    EnemyDeath,
    WaveStart,
    Count
};

class AudioSystem {
public:
    static constexpr std::size_t kVoiceCount = 12;
    static constexpr std::size_t kEventCount = static_cast<std::size_t>(AudioEvent::Count);

    bool init();
    void shutdown();
    void update(float dt);
    void setListener(const Vec3& position, float yaw);
    void play(AudioEvent event, const Vec3& position = {});

    bool initialized() const { return initialized_; }

private:
    void buildSynthSounds();
    void setupAndPlay(std::size_t voiceIndex, AudioEvent event,
                      const Vec3& position, float gain, float pan);
    float eventGain(AudioEvent event) const;

    bool initialized_{false};
    bool ownsAX_{false};
    std::array<void*, kVoiceCount> voices_{}; // AXVoice*, kept opaque in header
    std::array<std::vector<std::uint16_t>, kEventCount> samples_{};
    Vec3 listenerPos_{};
    float listenerYaw_{};
    std::size_t stealCursor_{};
};

} // namespace aa
