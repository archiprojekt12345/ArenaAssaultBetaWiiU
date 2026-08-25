#pragma once
#include "math.hpp"

namespace aa {

enum class EnemyState { Patrol, Chase, Search, Attack, Dead };
enum class EnemyClass { Scout, Soldier, Heavy };

struct Enemy {
    Vec3 pos{};
    Vec3 home{};
    Vec3 lastSeen{};
    float yaw{};
    float hp{100.0f};
    float maxHp{100.0f};
    float fireCooldown{};
    float searchTimer{};
    float patrolPhase{};
    float anim{};
    float strafeSign{1.0f};

    float moveSpeed{2.35f};
    float preferredMin{4.4f};
    float preferredMax{6.4f};
    float attackRange{7.6f};
    float detectionRange{18.5f};
    float damage{7.0f};
    float accuracy{0.62f};
    float fireInterval{0.86f};
    float fireJitter{0.55f};
    float strafeWeight{1.0f};
    float radius{0.44f};
    float scale{1.0f};

    EnemyState state{EnemyState::Patrol};
    EnemyClass archetype{EnemyClass::Soldier};
    bool elite{false};
};

} // namespace aa
