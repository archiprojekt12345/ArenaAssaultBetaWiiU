#pragma once

#include <cstdint>
#include "math.hpp"

namespace aa {

enum class MissionPhase : std::uint32_t {
    Infiltrate = 0,
    ActivateTerminal,
    DefendTerminal,
    EliminateMiniboss,
    Evacuate,
    Complete,
    Count
};

struct MissionState {
    MissionPhase phase{MissionPhase::Infiltrate};
    Vec3 terminal{0.0f, 0.0f, -15.3f};
    Vec3 extraction{0.0f, 0.0f, 17.2f};
    float phaseTime{0.0f};
    float actionProgress{0.0f};
    float defendRemaining{28.0f};
    float spawnCooldown{0.0f};
    int defenseSquads{0};
    int bossEnemyIndex{-1};
};

constexpr float kTerminalInteractRadius = 2.35f;
constexpr float kTerminalInteractTime = 2.20f;
constexpr float kDefenseRadius = 8.5f;
constexpr float kDefenseDuration = 28.0f;
constexpr float kExtractionRadius = 2.45f;

inline int missionStep(MissionPhase phase) {
    switch (phase) {
        case MissionPhase::Infiltrate: return 0;
        case MissionPhase::ActivateTerminal: return 1;
        case MissionPhase::DefendTerminal: return 2;
        case MissionPhase::EliminateMiniboss: return 3;
        case MissionPhase::Evacuate: return 4;
        case MissionPhase::Complete: return 5;
        case MissionPhase::Count: break;
    }
    return 0;
}

} // namespace aa
