#include "audio.hpp"

#include <algorithm>
#include <cmath>
#include <cstring>

#include <coreinit/cache.h>
#include <sndcore2/core.h>
#include <sndcore2/device.h>
#include <sndcore2/voice.h>
#include <whb/log.h>

namespace aa {
namespace {

constexpr int kSampleRate = 32000;
constexpr float kTwoPi = 6.2831853071795864769f;

std::uint32_t sNoise = 0xC001D00Du;
float noise01() {
    sNoise = sNoise * 1664525u + 1013904223u;
    return float((sNoise >> 8) & 0x00FFFFFFu) / float(0x01000000u);
}

std::uint16_t pcm(float v) {
    v = clampf(v,-1.0f,1.0f);
    const std::int16_t s = static_cast<std::int16_t>(v * 32767.0f);
    return static_cast<std::uint16_t>(s);
}

void resizeSeconds(std::vector<std::uint16_t>& out, float seconds) {
    out.assign(static_cast<std::size_t>(std::max(1.0f, seconds*kSampleRate)), 0);
}

float envExp(float t, float k) {
    return std::exp(-t*k);
}

} // namespace

bool AudioSystem::init() {
    if (initialized_) return true;

    buildSynthSounds();
    for (auto& s : samples_) {
        if (!s.empty()) DCFlushRange(s.data(), s.size()*sizeof(s[0]));
    }

    if (!AXIsInit()) {
        AXInitParams params{};
        params.renderer = AX_INIT_RENDERER_32KHZ;
        params.pipeline = AX_INIT_PIPELINE_SINGLE;
        AXInitWithParams(&params);
        ownsAX_ = true;
    }

    if (!AXIsInit()) {
        WHBLogPrintf("ArenaAssault: AX/sndcore2 initialization failed");
        return false;
    }

    std::size_t acquired = 0;
    for (std::size_t i=0;i<voices_.size();++i) {
        AXVoice* v = AXAcquireVoice(28, nullptr, nullptr);
        voices_[i] = v;
        if (v) ++acquired;
    }
    initialized_ = acquired > 0;
    WHBLogPrintf("ArenaAssault: sndcore2 audio active, voices=%u",
                 static_cast<unsigned>(acquired));
    return initialized_;
}

void AudioSystem::shutdown() {
    if (!initialized_ && !ownsAX_) return;
    for (void*& opaque : voices_) {
        AXVoice* v = static_cast<AXVoice*>(opaque);
        if (v) {
            AXSetVoiceState(v, AX_VOICE_STATE_STOPPED);
            AXFreeVoice(v);
            opaque = nullptr;
        }
    }
    if (ownsAX_ && AXIsInit()) AXQuit();
    ownsAX_ = false;
    initialized_ = false;
}

void AudioSystem::update(float dt) {
    (void)dt;
}

void AudioSystem::setListener(const Vec3& position, float yaw) {
    listenerPos_ = position;
    listenerYaw_ = yaw;
}

float AudioSystem::eventGain(AudioEvent event) const {
    switch (event) {
        case AudioEvent::PlayerShot:   return 1.00f;
        case AudioEvent::PlayerReload: return 0.62f;
        case AudioEvent::PlayerHit:    return 0.78f;
        case AudioEvent::EnemyShot:    return 0.82f;
        case AudioEvent::EnemyDeath:   return 0.92f;
        case AudioEvent::WaveStart:    return 0.72f;
        case AudioEvent::Count:        break;
    }
    return 0.7f;
}

void AudioSystem::play(AudioEvent event, const Vec3& position) {
    if (!initialized_) return;
    const std::size_t eidx = static_cast<std::size_t>(event);
    if (eidx >= samples_.size() || samples_[eidx].empty()) return;

    std::size_t chosen = voices_.size();
    for (std::size_t i=0;i<voices_.size();++i) {
        AXVoice* v = static_cast<AXVoice*>(voices_[i]);
        if (v && !AXIsVoiceRunning(v)) { chosen = i; break; }
    }
    if (chosen == voices_.size()) {
        for (std::size_t n=0;n<voices_.size();++n) {
            const std::size_t i = (stealCursor_ + n) % voices_.size();
            if (voices_[i]) { chosen = i; break; }
        }
    }
    if (chosen == voices_.size()) return;
    stealCursor_ = (chosen + 1) % voices_.size();

    const Vec3 delta = position - listenerPos_;
    const float dist = std::sqrt(delta.x*delta.x + delta.z*delta.z);
    float attenuation = 1.0f / (1.0f + 0.045f*dist*dist);
    float pan = 0.0f;
    if (event != AudioEvent::WaveStart && dist > 0.001f) {
        const Vec3 right{std::cos(listenerYaw_),0,std::sin(listenerYaw_)};
        pan = clampf(dot(normalize(Vec3{delta.x,0,delta.z}),right),-1.0f,1.0f);
    } else if (event == AudioEvent::WaveStart) {
        attenuation = 1.0f;
    }
    const float gain = clampf(eventGain(event)*attenuation,0.05f,1.0f);
    setupAndPlay(chosen,event,position,gain,pan);
}

void AudioSystem::setupAndPlay(std::size_t voiceIndex, AudioEvent event,
                               const Vec3& position, float gain, float pan) {
    (void)position;
    AXVoice* voice = static_cast<AXVoice*>(voices_[voiceIndex]);
    if (!voice) return;
    auto& samples = samples_[static_cast<std::size_t>(event)];

    AXSetVoiceState(voice, AX_VOICE_STATE_STOPPED);
    AXVoiceBegin(voice);
    AXSetVoiceType(voice, AX_VOICE_TYPE_UNKNOWN);
    AXSetVoiceSrcType(voice, AX_VOICE_SRC_TYPE_NONE);

    AXVoiceVeData ve{};
    ve.volume = static_cast<std::uint16_t>(clampf(gain,0.0f,1.0f) * 0xD800u);
    ve.delta = 0;
    AXSetVoiceVe(voice,&ve);

    AXVoiceDeviceMixData mix[6]{};
    const float left = std::sqrt(clampf((1.0f-pan)*0.5f,0.0f,1.0f));
    const float right = std::sqrt(clampf((1.0f+pan)*0.5f,0.0f,1.0f));
    mix[0].bus[0].volume = static_cast<std::uint16_t>(left * 0xC000u);
    mix[1].bus[0].volume = static_cast<std::uint16_t>(right * 0xC000u);
    AXSetVoiceDeviceMix(voice, AX_DEVICE_TYPE_TV, 0, mix);
    AXSetVoiceDeviceMix(voice, AX_DEVICE_TYPE_DRC, 0, mix);

    AXVoiceOffsets offsets{};
    offsets.dataType = AX_VOICE_FORMAT_LPCM16;
    offsets.loopingEnabled = AX_VOICE_LOOP_DISABLED;
    offsets.loopOffset = 0;
    offsets.endOffset = static_cast<std::uint32_t>(samples.size());
    offsets.currentOffset = 0;
    offsets.data = samples.data();
    AXSetVoiceOffsets(voice,&offsets);
    AXVoiceEnd(voice);

    AXSetVoiceCurrentOffset(voice,0);
    AXSetVoiceState(voice,AX_VOICE_STATE_PLAYING);
}

void AudioSystem::buildSynthSounds() {
    // Player shot: short low crack + noise transient.
    {
        auto& out = samples_[static_cast<std::size_t>(AudioEvent::PlayerShot)];
        resizeSeconds(out,0.115f);
        for (std::size_t i=0;i<out.size();++i) {
            const float t=float(i)/kSampleRate;
            const float env=envExp(t,30.0f);
            const float n=noise01()*2.0f-1.0f;
            const float tone=std::sin(kTwoPi*(110.0f+180.0f*env)*t);
            out[i]=pcm((n*0.63f+tone*0.37f)*env);
        }
    }
    // Reload: three mechanical clicks.
    {
        auto& out = samples_[static_cast<std::size_t>(AudioEvent::PlayerReload)];
        resizeSeconds(out,0.42f);
        for (std::size_t i=0;i<out.size();++i) {
            const float t=float(i)/kSampleRate;
            float v=0.0f;
            const float times[3]={0.01f,0.16f,0.31f};
            for (float start:times) {
                const float x=t-start;
                if (x>=0.0f && x<0.055f)
                    v += (noise01()*2.0f-1.0f)*envExp(x,55.0f)*0.7f +
                         std::sin(kTwoPi*680.0f*x)*envExp(x,45.0f)*0.28f;
            }
            out[i]=pcm(v);
        }
    }
    // Player hit: bass thud.
    {
        auto& out = samples_[static_cast<std::size_t>(AudioEvent::PlayerHit)];
        resizeSeconds(out,0.18f);
        for (std::size_t i=0;i<out.size();++i) {
            const float t=float(i)/kSampleRate;
            const float env=envExp(t,17.0f);
            out[i]=pcm((std::sin(kTwoPi*74.0f*t)*0.70f + (noise01()*2-1)*0.22f)*env);
        }
    }
    // Enemy shot: synthetic descending plasma pulse.
    {
        auto& out = samples_[static_cast<std::size_t>(AudioEvent::EnemyShot)];
        resizeSeconds(out,0.13f);
        float phase=0.0f;
        for (std::size_t i=0;i<out.size();++i) {
            const float t=float(i)/kSampleRate;
            const float f=1180.0f-690.0f*(t/0.13f);
            phase += kTwoPi*f/kSampleRate;
            const float env=envExp(t,20.0f);
            out[i]=pcm((std::sin(phase)*0.72f + std::sin(phase*0.5f)*0.22f)*env);
        }
    }
    // Enemy death: metallic burst + downward sweep.
    {
        auto& out = samples_[static_cast<std::size_t>(AudioEvent::EnemyDeath)];
        resizeSeconds(out,0.48f);
        float phase=0.0f;
        for (std::size_t i=0;i<out.size();++i) {
            const float t=float(i)/kSampleRate;
            const float f=340.0f-240.0f*(t/0.48f);
            phase += kTwoPi*f/kSampleRate;
            const float env=envExp(t,6.2f);
            const float n=(noise01()*2-1)*envExp(t,10.0f);
            out[i]=pcm((std::sin(phase)*0.55f+n*0.42f)*env);
        }
    }
    // Wave start: three clean confirmation beeps.
    {
        auto& out = samples_[static_cast<std::size_t>(AudioEvent::WaveStart)];
        resizeSeconds(out,0.52f);
        for (std::size_t i=0;i<out.size();++i) {
            const float t=float(i)/kSampleRate;
            float v=0.0f;
            const float starts[3]={0.00f,0.16f,0.32f};
            const float freqs[3]={520.0f,660.0f,880.0f};
            for (int j=0;j<3;++j) {
                const float x=t-starts[j];
                if (x>=0 && x<0.11f) {
                    const float e=std::sin(PI*clampf(x/0.11f,0,1));
                    v += std::sin(kTwoPi*freqs[j]*x)*e*0.52f;
                }
            }
            out[i]=pcm(v);
        }
    }
}

} // namespace aa
