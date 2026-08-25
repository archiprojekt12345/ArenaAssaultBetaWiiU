#pragma once

#include <array>
#include <string>
#include <vpad/input.h>

#include "audio.hpp"
#include "camera.hpp"
#include "enemy.hpp"
#include "map.hpp"
#include "mesh.hpp"
#include "mission.hpp"
#include "player.hpp"
#include "particle.hpp"
#include "renderer.hpp"

namespace aa {

class Game {
public:
    static constexpr int kMaxEnemies = 16;

    Game();
    bool init(const char* assetRoot);
    void shutdown();
    void reset();
    void update(const VPADStatus& pad, float dt);
    void renderTV(Renderer& renderer) const;
    void renderMap(Renderer& renderer) const;

private:
    Player player_{};
    std::array<Enemy, kMaxEnemies> enemies_{};
    int activeEnemies_{kMaxEnemies};
    int kills_{0};
    int spawnCursor_{0};
    float time_{0.0f};

    MissionState mission_{};
    ArenaMap map_{};
    SkinnedMesh enemySkinnedMesh_{};
    Mesh enemyBodyMesh_{};
    ParticleSystem particles_{};
    AudioSystem audio_{};
    std::string assetRoot_{};

    Camera makeCamera() const;
    void startReload();
    void updatePlayer(const VPADStatus& pad, float dt);
    void firePlayerWeapon();
    void updateEnemies(float dt);
    void updateMission(const VPADStatus& pad, float dt);
    void beginMissionPhase(MissionPhase phase);

    int spawnEnemy(EnemyClass archetype, const Vec3& position,
                   bool elite = false, EnemyState initialState = EnemyState::Patrol);
    void spawnInitialPatrol();
    void spawnDefenseSquad();
    void spawnBossEncounter();
    Vec3 nextSpawnPoint();
    void clearEnemies();
    bool bossAlive() const;
    int aliveCount() const;

    void renderMissionWorld(Renderer& renderer) const;
    void renderMissionHUD(Renderer& renderer) const;
    void renderEnemy(Renderer& renderer, const Enemy& enemy) const;
    void renderWeapon(Renderer& renderer, const Camera& camera) const;

    static float mapX(float x);
    static float mapY(float z);
    static void drawThinLine(Renderer& r, float x0, float y0, float x1, float y1,
                             float width, const Color& c);
};

} // namespace aa
