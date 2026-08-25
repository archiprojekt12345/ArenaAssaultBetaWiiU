#include "game.hpp"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstdio>

#include <gx2/registers.h>
#include <whb/log.h>

#include "animation.hpp"
#include "material.hpp"

namespace aa {
namespace {

std::uint32_t gRngState = 0x8D12F3A7u;
std::uint32_t nextRand() {
    gRngState = gRngState * 1664525u + 1013904223u;
    return gRngState;
}
float rand01() {
    return float(nextRand() & 0x00FFFFFFu) / float(0x01000000u);
}

const FloatKeyframe kWalkKeys[] = {
    {0.0f, 0.0f},
    {PI*0.5f, 1.0f},
    {PI, 0.0f},
    {PI*1.5f, -1.0f},
    {PI*2.0f, 0.0f}
};

const Vec3 kSpawnPoints[Game::kMaxEnemies] = {
    {-17,0,-16},{17,0,-16},{-17,0,16},{17,0,16},
    {-16,0,-7},{16,0,7},{-7,0,-16},{7,0,16},
    {-15,0,11},{15,0,-11},{-11,0,15},{11,0,-15},
    {-18,0,2},{18,0,-2},{2,0,-18},{-2,0,18}
};

float planarDistance(const Vec3& a, const Vec3& b) {
    const float dx = a.x-b.x;
    const float dz = a.z-b.z;
    return std::sqrt(dx*dx+dz*dz);
}

Color classColor(EnemyClass c) {
    switch (c) {
        case EnemyClass::Scout:   return {0.08f,0.78f,0.90f,1.0f};
        case EnemyClass::Soldier: return {0.94f,0.31f,0.08f,1.0f};
        case EnemyClass::Heavy:   return {0.74f,0.12f,0.88f,1.0f};
    }
    return {1,1,1,1};
}

} // namespace

Game::Game() {
    reset();
}

bool Game::init(const char* assetRoot) {
    assetRoot_ = assetRoot ? assetRoot : "";
    audio_.init();

    if (!assetRoot_.empty()) {
        char path[512]{};
        std::snprintf(path,sizeof(path),"%s/assets/meshes/enemy_body.aam2",assetRoot_.c_str());
        if (enemySkinnedMesh_.loadFromFile(path)) {
            WHBLogPrintf("ArenaAssault: loaded AAM2 skinned enemy mesh: %s", path);
        } else {
            std::snprintf(path,sizeof(path),"%s/assets/meshes/enemy_body.aam",assetRoot_.c_str());
            if (enemyBodyMesh_.loadFromFile(path)) {
                WHBLogPrintf("ArenaAssault: AAM2 missing, loaded AAM1 fallback: %s", path);
            } else {
                WHBLogPrintf("ArenaAssault: enemy mesh missing, procedural fallback active");
            }
        }
    }
    return true;
}

void Game::shutdown() {
    enemySkinnedMesh_.clear();
    enemyBodyMesh_.clear();
    audio_.shutdown();
}

void Game::clearEnemies() {
    for (auto& e : enemies_) {
        e = Enemy{};
        e.state = EnemyState::Dead;
    }
    activeEnemies_ = kMaxEnemies;
}

void Game::reset() {
    player_ = Player{};
    player_.pos = {0,0,14};
    player_.yaw = PI;
    kills_ = 0;
    spawnCursor_ = 0;
    time_ = 0.0f;
    mission_ = MissionState{};
    particles_.reset();
    clearEnemies();
    spawnInitialPatrol();
}

void Game::update(const VPADStatus& pad, float dt) {
    time_ += dt;
    particles_.update(dt);
    audio_.setListener(player_.pos,player_.yaw);
    audio_.update(dt);

    if (player_.hp <= 0.0f) {
        player_.damageFlash = std::max(0.0f, player_.damageFlash - dt);
        if (pad.trigger & VPAD_BUTTON_A) reset();
        return;
    }

    if (mission_.phase == MissionPhase::Complete) {
        updatePlayer(pad,dt);
        if (pad.trigger & VPAD_BUTTON_A) reset();
        player_.damageFlash = std::max(0.0f, player_.damageFlash - dt*2.0f);
        return;
    }

    updatePlayer(pad, dt);
    updateEnemies(dt);
    updateMission(pad, dt);
    player_.damageFlash = std::max(0.0f, player_.damageFlash - dt*2.0f);
}

Camera Game::makeCamera() const {
    Camera cam;
    cam.pos = player_.pos + Vec3{0,1.62f,0};
    cam.yaw = player_.yaw;
    cam.pitch = player_.pitch - player_.weapon.recoil*0.012f;
    cam.fov = (player_.aiming ? 54.0f : 72.0f) * PI/180.0f;
    cam.aspect = 16.0f/9.0f;
    return cam;
}

void Game::startReload() {
    Weapon& w = player_.weapon;
    if (w.reloadTimer <= 0.0f && w.ammo < w.magazineSize) {
        w.reloadTimer = w.reloadDuration;
        audio_.play(AudioEvent::PlayerReload, player_.pos);
    }
}

void Game::updatePlayer(const VPADStatus& pad, float dt) {
    Weapon& w = player_.weapon;
    w.fireCooldown = std::max(0.0f, w.fireCooldown-dt);
    w.recoil = std::max(0.0f, w.recoil-dt*5.5f);

    player_.aiming = (pad.hold & VPAD_BUTTON_ZL) != 0;
    const float lookScale = player_.aiming ? 1.45f : 2.35f;
    const float rx = std::fabs(pad.rightStick.x) > 0.08f ? pad.rightStick.x : 0.0f;
    const float ry = std::fabs(pad.rightStick.y) > 0.08f ? pad.rightStick.y : 0.0f;
    player_.yaw += rx * lookScale * dt;
    player_.pitch = clampf(player_.pitch + ry*lookScale*dt, -1.10f, 1.10f);

    const float lx = std::fabs(pad.leftStick.x) > 0.10f ? pad.leftStick.x : 0.0f;
    const float ly = std::fabs(pad.leftStick.y) > 0.10f ? pad.leftStick.y : 0.0f;
    const Vec3 forward{std::sin(player_.yaw),0,-std::cos(player_.yaw)};
    const Vec3 right{std::cos(player_.yaw),0,std::sin(player_.yaw)};
    Vec3 move = right*lx + forward*ly;
    if (lengthSq(move) > 1.0f) move = normalize(move);

    const float speed = (pad.hold & VPAD_BUTTON_L) ? 6.4f : 4.4f;
    const Vec3 old = player_.pos;
    const Vec3 candidate = old + move*speed*dt;
    const Vec3 tryX{candidate.x,0,old.z};
    const Vec3 tryZ{old.x,0,candidate.z};
    if (map_.validPosition(tryX,0.38f)) player_.pos.x = tryX.x;
    if (map_.validPosition({player_.pos.x,0,tryZ.z},0.38f)) player_.pos.z = tryZ.z;

    if (pad.trigger & VPAD_BUTTON_X) startReload();

    if (w.reloadTimer > 0.0f) {
        w.reloadTimer -= dt;
        if (w.reloadTimer <= 0.0f) {
            w.reloadTimer = 0.0f;
            w.ammo = w.magazineSize;
        }
    }

    if ((pad.hold & VPAD_BUTTON_ZR) && w.fireCooldown <= 0.0f && !w.reloading()) {
        if (w.ammo > 0) {
            firePlayerWeapon();
            audio_.play(AudioEvent::PlayerShot, player_.pos);
            --w.ammo;
            w.fireCooldown = 1.0f / w.shotsPerSecond;
            w.recoil = std::min(1.0f, w.recoil + 0.34f);
            if (w.ammo == 0) startReload();
        } else {
            startReload();
        }
    }
}

void Game::firePlayerWeapon() {
    const Camera cam = makeCamera();
    const Vec3 origin = cam.pos;
    const Vec3 dir = normalize(cameraForward(cam));
    particles_.spawnMuzzle(origin + dir*0.78f,dir,{0.12f,0.72f,1.0f,1.0f});
    const float wallT = map_.nearestObstacleDistance(origin,dir);

    int bestIndex = -1;
    float bestT = wallT;
    for (int i=0;i<activeEnemies_;++i) {
        Enemy& e = enemies_[i];
        if (e.state == EnemyState::Dead) continue;
        const float bodyY = 1.15f*e.scale;
        const float bodyRadius = 0.66f*e.scale;
        const float t = raySphereDistance(origin,dir,e.pos+Vec3{0,bodyY,0},bodyRadius);
        if (t >= 0.0f && t < bestT) { bestT=t; bestIndex=i; }
    }

    if (bestIndex >= 0) {
        Enemy& e = enemies_[bestIndex];
        const float headT = raySphereDistance(origin,dir,e.pos+Vec3{0,1.78f*e.scale,0},0.29f*e.scale);
        const bool headshot = headT >= 0.0f && headT <= bestT+0.28f*e.scale;
        float damage = headshot ? 68.0f : 34.0f;
        if (e.archetype == EnemyClass::Scout) damage *= 1.08f;
        if (e.elite) damage *= 0.92f;
        e.hp -= damage;
        e.state = EnemyState::Chase;
        e.lastSeen = player_.pos;
        const Vec3 hitPos = origin + dir*bestT;
        particles_.spawnImpact(hitPos,-1.0f*dir,classColor(e.archetype));
        if (e.hp <= 0.0f) {
            e.hp = 0.0f;
            e.state = EnemyState::Dead;
            ++kills_;
            particles_.spawnDeath(e.pos,classColor(e.archetype));
            audio_.play(AudioEvent::EnemyDeath,e.pos);
        }
    } else if (wallT < 999.0f) {
        const Vec3 hitPos = origin + dir*wallT;
        particles_.spawnImpact(hitPos,-1.0f*dir,{0.18f,0.65f,1.0f,1.0f});
    }
}

void Game::updateEnemies(float dt) {
    const Vec3 playerEye = player_.pos + Vec3{0,1.55f,0};

    for (int i=0;i<activeEnemies_;++i) {
        Enemy& e = enemies_[i];
        if (e.state == EnemyState::Dead) continue;

        e.fireCooldown = std::max(0.0f,e.fireCooldown-dt);
        e.anim += dt * (e.archetype==EnemyClass::Scout ? 7.2f : (e.archetype==EnemyClass::Heavy ? 3.8f : 5.0f));

        const Vec3 enemyEye = e.pos + Vec3{0,1.45f*e.scale,0};
        const Vec3 toPlayer = player_.pos - e.pos;
        const float dist = length(toPlayer);
        const bool canSee = dist < e.detectionRange && map_.hasLineOfSight(enemyEye,playerEye);

        if (canSee) {
            e.lastSeen = player_.pos;
            e.searchTimer = 2.5f + (e.archetype==EnemyClass::Scout ? 0.6f : 0.0f);
            e.state = (dist < e.attackRange) ? EnemyState::Attack : EnemyState::Chase;
        } else if (e.state == EnemyState::Attack || e.state == EnemyState::Chase) {
            e.state = EnemyState::Search;
        }

        Vec3 desired{};
        float speed = e.moveSpeed;

        switch (e.state) {
            case EnemyState::Patrol: {
                e.patrolPhase += dt*(e.archetype==EnemyClass::Scout ? 0.82f : 0.55f);
                const float orbit = e.archetype==EnemyClass::Scout ? 3.4f : 2.4f;
                const Vec3 target = e.home + Vec3{
                    std::cos(e.patrolPhase)*orbit,0,
                    std::sin(e.patrolPhase*0.82f)*orbit};
                desired = normalize(target-e.pos);
                speed *= 0.50f;
                break;
            }
            case EnemyState::Chase:
                desired = normalize(e.lastSeen-e.pos);
                break;
            case EnemyState::Search:
                e.searchTimer -= dt;
                desired = normalize(e.lastSeen-e.pos);
                speed *= 0.72f;
                if (length(e.lastSeen-e.pos) < 0.8f || e.searchTimer <= 0)
                    e.state = EnemyState::Patrol;
                break;
            case EnemyState::Attack: {
                const Vec3 toward = normalize(toPlayer);
                const Vec3 strafe{-toward.z,0,toward.x};
                float radial = 0.0f;
                if (dist > e.preferredMax) radial = 0.62f;
                if (dist < e.preferredMin) radial = -0.72f;

                if (e.archetype == EnemyClass::Scout) {
                    desired = normalize(strafe*e.strafeSign*1.45f + toward*radial);
                    speed *= 0.92f;
                } else if (e.archetype == EnemyClass::Heavy) {
                    desired = normalize(strafe*e.strafeSign*0.28f + toward*radial*1.25f);
                    speed *= 0.58f;
                } else {
                    desired = normalize(strafe*e.strafeSign + toward*radial);
                    speed *= 0.68f;
                }

                if (canSee && e.fireCooldown <= 0.0f) {
                    const float distancePenalty = dist * (e.archetype==EnemyClass::Scout ? 0.040f : 0.032f);
                    const float chance = clampf(e.accuracy-distancePenalty,0.22f,0.82f);
                    const Vec3 shotDir = normalize(playerEye-enemyEye);
                    const Vec3 muzzleLocal = (e.archetype==EnemyClass::Heavy)
                        ? Vec3{0.54f,1.24f,-0.98f}
                        : Vec3{0.46f,1.18f,-0.86f};
                    const Vec3 muzzle = e.pos + rotateY(muzzleLocal*e.scale,e.yaw);
                    particles_.spawnMuzzle(muzzle,shotDir,classColor(e.archetype));
                    audio_.play(AudioEvent::EnemyShot,e.pos);
                    if (rand01() < chance) {
                        player_.hp = std::max(0.0f,player_.hp-e.damage);
                        player_.damageFlash = e.archetype==EnemyClass::Heavy ? 0.95f : 0.75f;
                        audio_.play(AudioEvent::PlayerHit,player_.pos);
                    }
                    e.fireCooldown = e.fireInterval + rand01()*e.fireJitter;
                    const float flipChance = e.archetype==EnemyClass::Scout ? 0.42f : 0.20f;
                    if (rand01() < flipChance) e.strafeSign *= -1.0f;
                }
                break;
            }
            case EnemyState::Dead:
                break;
        }

        Vec3 separation{};
        for (int j=0;j<activeEnemies_;++j) {
            if (i==j || enemies_[j].state==EnemyState::Dead) continue;
            const Vec3 d = e.pos-enemies_[j].pos;
            const float dsq = lengthSq(d);
            const float sepRadius = e.radius+enemies_[j].radius+1.25f;
            if (dsq > 0.001f && dsq < sepRadius*sepRadius)
                separation += normalize(d)*(1.0f-std::sqrt(dsq)/sepRadius);
        }
        desired = normalize(desired + separation*0.85f);

        const Vec3 before=e.pos;
        const Vec3 next=e.pos+desired*speed*dt;
        const Vec3 tryX{next.x,0,e.pos.z};
        const Vec3 tryZ{e.pos.x,0,next.z};
        if (map_.validPosition(tryX,e.radius)) e.pos.x=tryX.x;
        if (map_.validPosition({e.pos.x,0,tryZ.z},e.radius)) e.pos.z=tryZ.z;

        const Vec3 travel=e.pos-before;
        if (lengthSq(travel)>0.00001f) e.yaw=std::atan2(travel.x,-travel.z);
        if (canSee && dist<e.attackRange+2.0f) e.yaw=std::atan2(toPlayer.x,-toPlayer.z);
    }
}

Vec3 Game::nextSpawnPoint() {
    for (int attempt=0;attempt<kMaxEnemies;++attempt) {
        const int index=(spawnCursor_++)%kMaxEnemies;
        const Vec3 p=kSpawnPoints[index];
        if (planarDistance(p,player_.pos)>7.0f) return p;
    }
    return kSpawnPoints[(spawnCursor_++)%kMaxEnemies];
}

int Game::spawnEnemy(EnemyClass archetype, const Vec3& position,
                     bool elite, EnemyState initialState) {
    int slot=-1;
    for (int i=0;i<kMaxEnemies;++i) {
        if (enemies_[i].state==EnemyState::Dead) { slot=i; break; }
    }
    if (slot<0) return -1;

    Enemy e{};
    e.archetype=archetype;
    e.elite=elite;
    e.pos=position;
    e.home=position;
    e.lastSeen=position;
    e.yaw=std::atan2(-position.x,position.z);
    e.fireCooldown=0.45f+rand01()*0.9f;
    e.patrolPhase=rand01()*2.0f*PI;
    e.anim=rand01()*2.0f*PI;
    e.strafeSign=(slot&1)?1.0f:-1.0f;
    e.state=initialState;

    switch (archetype) {
        case EnemyClass::Scout:
            e.maxHp=68.0f;
            e.moveSpeed=3.35f;
            e.preferredMin=2.5f;
            e.preferredMax=4.9f;
            e.attackRange=6.3f;
            e.detectionRange=21.5f;
            e.damage=4.6f;
            e.accuracy=0.72f;
            e.fireInterval=0.48f;
            e.fireJitter=0.32f;
            e.strafeWeight=1.45f;
            e.radius=0.36f;
            e.scale=0.86f;
            break;
        case EnemyClass::Soldier:
            e.maxHp=112.0f;
            e.moveSpeed=2.35f;
            e.preferredMin=4.4f;
            e.preferredMax=6.4f;
            e.attackRange=7.8f;
            e.detectionRange=19.0f;
            e.damage=7.4f;
            e.accuracy=0.66f;
            e.fireInterval=0.82f;
            e.fireJitter=0.52f;
            e.strafeWeight=1.0f;
            e.radius=0.44f;
            e.scale=1.0f;
            break;
        case EnemyClass::Heavy:
            e.maxHp=245.0f;
            e.moveSpeed=1.42f;
            e.preferredMin=6.6f;
            e.preferredMax=9.0f;
            e.attackRange=10.2f;
            e.detectionRange=22.0f;
            e.damage=15.5f;
            e.accuracy=0.70f;
            e.fireInterval=1.12f;
            e.fireJitter=0.48f;
            e.strafeWeight=0.30f;
            e.radius=0.58f;
            e.scale=1.18f;
            break;
    }

    if (elite) {
        e.maxHp *= 2.35f;
        e.hp=e.maxHp;
        e.moveSpeed *= 1.08f;
        e.damage *= 1.22f;
        e.accuracy = std::min(0.80f,e.accuracy+0.06f);
        e.fireInterval *= 0.78f;
        e.fireJitter *= 0.72f;
        e.radius *= 1.15f;
        e.scale *= 1.18f;
    } else {
        e.hp=e.maxHp;
    }

    enemies_[slot]=e;
    return slot;
}

void Game::spawnInitialPatrol() {
    spawnEnemy(EnemyClass::Scout,  {-16,0,-7}, false, EnemyState::Patrol);
    spawnEnemy(EnemyClass::Scout,  { 16,0, 7}, false, EnemyState::Patrol);
    spawnEnemy(EnemyClass::Soldier,{-17,0,-16},false, EnemyState::Patrol);
    spawnEnemy(EnemyClass::Soldier,{ 17,0,-16},false, EnemyState::Patrol);
    spawnEnemy(EnemyClass::Soldier,{-11,0, 15},false, EnemyState::Patrol);
    spawnEnemy(EnemyClass::Heavy,  { 15,0,-11},false, EnemyState::Patrol);
}

void Game::spawnDefenseSquad() {
    const int squad=mission_.defenseSquads++;
    switch (squad%5) {
        case 0:
            spawnEnemy(EnemyClass::Scout,nextSpawnPoint(),false,EnemyState::Chase);
            spawnEnemy(EnemyClass::Scout,nextSpawnPoint(),false,EnemyState::Chase);
            spawnEnemy(EnemyClass::Soldier,nextSpawnPoint(),false,EnemyState::Chase);
            break;
        case 1:
            spawnEnemy(EnemyClass::Scout,nextSpawnPoint(),false,EnemyState::Chase);
            spawnEnemy(EnemyClass::Soldier,nextSpawnPoint(),false,EnemyState::Chase);
            spawnEnemy(EnemyClass::Soldier,nextSpawnPoint(),false,EnemyState::Chase);
            break;
        case 2:
            spawnEnemy(EnemyClass::Heavy,nextSpawnPoint(),false,EnemyState::Chase);
            spawnEnemy(EnemyClass::Scout,nextSpawnPoint(),false,EnemyState::Chase);
            break;
        case 3:
            spawnEnemy(EnemyClass::Heavy,nextSpawnPoint(),false,EnemyState::Chase);
            spawnEnemy(EnemyClass::Soldier,nextSpawnPoint(),false,EnemyState::Chase);
            spawnEnemy(EnemyClass::Scout,nextSpawnPoint(),false,EnemyState::Chase);
            break;
        default:
            spawnEnemy(EnemyClass::Scout,nextSpawnPoint(),false,EnemyState::Chase);
            spawnEnemy(EnemyClass::Scout,nextSpawnPoint(),false,EnemyState::Chase);
            spawnEnemy(EnemyClass::Soldier,nextSpawnPoint(),false,EnemyState::Chase);
            spawnEnemy(EnemyClass::Heavy,nextSpawnPoint(),false,EnemyState::Chase);
            break;
    }

    for (auto& e:enemies_) {
        if (e.state!=EnemyState::Dead && e.state==EnemyState::Chase)
            e.lastSeen=mission_.terminal;
    }
}

void Game::spawnBossEncounter() {
    const Vec3 bossSpawn={0,0,-18.0f};
    mission_.bossEnemyIndex=spawnEnemy(EnemyClass::Heavy,bossSpawn,true,EnemyState::Chase);
    if (mission_.bossEnemyIndex>=0) enemies_[mission_.bossEnemyIndex].lastSeen=player_.pos;
    spawnEnemy(EnemyClass::Scout,{-16,0,2},false,EnemyState::Chase);
    spawnEnemy(EnemyClass::Scout,{ 16,0,-2},false,EnemyState::Chase);
    spawnEnemy(EnemyClass::Soldier,{-15,0,11},false,EnemyState::Chase);
    spawnEnemy(EnemyClass::Soldier,{ 15,0,11},false,EnemyState::Chase);
}

int Game::aliveCount() const {
    int count=0;
    for (const auto& e:enemies_) if (e.state!=EnemyState::Dead) ++count;
    return count;
}

bool Game::bossAlive() const {
    const int i=mission_.bossEnemyIndex;
    return i>=0 && i<kMaxEnemies && enemies_[i].state!=EnemyState::Dead;
}

void Game::beginMissionPhase(MissionPhase phase) {
    mission_.phase=phase;
    mission_.phaseTime=0.0f;
    mission_.actionProgress=0.0f;
    audio_.play(AudioEvent::WaveStart);

    switch (phase) {
        case MissionPhase::Infiltrate:
            break;
        case MissionPhase::ActivateTerminal:
            break;
        case MissionPhase::DefendTerminal:
            mission_.defendRemaining=kDefenseDuration;
            mission_.spawnCooldown=0.65f;
            mission_.defenseSquads=0;
            break;
        case MissionPhase::EliminateMiniboss:
            mission_.bossEnemyIndex=-1;
            spawnBossEncounter();
            break;
        case MissionPhase::Evacuate:
            break;
        case MissionPhase::Complete:
            break;
        case MissionPhase::Count:
            break;
    }
}

void Game::updateMission(const VPADStatus& pad, float dt) {
    mission_.phaseTime += dt;

    switch (mission_.phase) {
        case MissionPhase::Infiltrate:
            if (planarDistance(player_.pos,mission_.terminal) <= kTerminalInteractRadius+0.45f)
                beginMissionPhase(MissionPhase::ActivateTerminal);
            break;

        case MissionPhase::ActivateTerminal: {
            const bool near = planarDistance(player_.pos,mission_.terminal)<=kTerminalInteractRadius;
            if (near && (pad.hold & VPAD_BUTTON_A)) {
                mission_.actionProgress += dt;
            } else {
                mission_.actionProgress = std::max(0.0f,mission_.actionProgress-dt*0.72f);
            }
            if (mission_.actionProgress>=kTerminalInteractTime)
                beginMissionPhase(MissionPhase::DefendTerminal);
            break;
        }

        case MissionPhase::DefendTerminal: {
            const bool inside = planarDistance(player_.pos,mission_.terminal)<=kDefenseRadius;
            if (inside) mission_.defendRemaining=std::max(0.0f,mission_.defendRemaining-dt);

            mission_.spawnCooldown-=dt;
            if (mission_.spawnCooldown<=0.0f && mission_.defendRemaining>0.0f && aliveCount()<10) {
                spawnDefenseSquad();
                const float pace = std::max(3.7f,6.2f-float(mission_.defenseSquads)*0.32f);
                mission_.spawnCooldown=pace;
            }

            if (mission_.defendRemaining<=0.0f)
                beginMissionPhase(MissionPhase::EliminateMiniboss);
            break;
        }

        case MissionPhase::EliminateMiniboss:
            if (!bossAlive() && mission_.bossEnemyIndex>=0)
                beginMissionPhase(MissionPhase::Evacuate);
            break;

        case MissionPhase::Evacuate:
            if (planarDistance(player_.pos,mission_.extraction)<=kExtractionRadius)
                beginMissionPhase(MissionPhase::Complete);
            break;

        case MissionPhase::Complete:
        case MissionPhase::Count:
            break;
    }
}

void Game::renderMissionWorld(Renderer& r) const {
    const float pulse=0.55f+0.45f*std::sin(time_*4.0f);
    const bool terminalActive = mission_.phase!=MissionPhase::Evacuate && mission_.phase!=MissionPhase::Complete;

    Material terminalBase=materials::darkMetal();
    terminalBase.diffuse={0.10f,0.15f,0.18f,1};
    const Material terminalGlow=materials::emissive({0.05f,0.72f,0.95f,1},1.8f+pulse*1.6f);
    r.submitBox(mission_.terminal+Vec3{0,0.55f,0},{0.55f,0.55f,0.55f},time_*0.16f,terminalBase);
    r.submitBox(mission_.terminal+Vec3{0,1.15f,0},{0.32f,0.08f,0.32f},-time_*0.42f,terminalGlow);

    if (terminalActive) {
        for (int i=0;i<8;++i) {
            const float a=float(i)*2.0f*PI/8.0f+time_*0.25f;
            const Vec3 p=mission_.terminal+Vec3{std::cos(a)*1.15f,0.035f,std::sin(a)*1.15f};
            r.submitBox(p,{0.20f,0.025f,0.055f},-a,terminalGlow);
        }
    }

    if (mission_.phase==MissionPhase::Evacuate || mission_.phase==MissionPhase::Complete) {
        const Material evac=materials::emissive({0.12f,0.96f,0.38f,1},2.0f+pulse*1.8f);
        for (int i=0;i<12;++i) {
            const float a=float(i)*2.0f*PI/12.0f;
            const Vec3 p=mission_.extraction+Vec3{std::cos(a)*1.65f,0.04f,std::sin(a)*1.65f};
            r.submitBox(p,{0.24f,0.025f,0.07f},-a,evac);
        }
        r.submitBox(mission_.extraction+Vec3{0,1.8f,0},{0.08f,1.8f,0.08f},0,evac);
    }
}

void Game::renderMissionHUD(Renderer& r) const {
    const int step=missionStep(mission_.phase);
    const float start=-0.25f;
    for (int i=0;i<5;++i) {
        const float x0=start+float(i)*0.105f;
        const bool complete=i<step;
        const bool active=i==step && mission_.phase!=MissionPhase::Complete;
        Color c={0.10f,0.15f,0.18f,0.95f};
        if (complete) c={0.08f,0.64f,0.38f,1};
        if (active) c={0.08f,0.76f,0.96f,1};
        if (mission_.phase==MissionPhase::Complete) c={0.08f,0.82f,0.42f,1};
        r.rect2D(x0,0.875f,x0+0.075f,0.915f,c);
    }

    if (mission_.phase==MissionPhase::ActivateTerminal) {
        const float f=clampf(mission_.actionProgress/kTerminalInteractTime,0,1);
        r.rect2D(-0.28f,-0.66f,0.28f,-0.61f,{0.04f,0.07f,0.09f,0.94f});
        r.rect2D(-0.27f,-0.65f,-0.27f+0.54f*f,-0.62f,{0.06f,0.78f,0.96f,1});
    } else if (mission_.phase==MissionPhase::DefendTerminal) {
        const float f=1.0f-clampf(mission_.defendRemaining/kDefenseDuration,0,1);
        const bool inside=planarDistance(player_.pos,mission_.terminal)<=kDefenseRadius;
        r.rect2D(-0.34f,-0.66f,0.34f,-0.61f,{0.04f,0.07f,0.09f,0.94f});
        r.rect2D(-0.33f,-0.65f,-0.33f+0.66f*f,-0.62f,
                 inside?Color{0.08f,0.82f,0.44f,1}:Color{0.95f,0.22f,0.06f,1});
    } else if (mission_.phase==MissionPhase::EliminateMiniboss && bossAlive()) {
        const Enemy& boss=enemies_[mission_.bossEnemyIndex];
        const float f=clampf(boss.hp/boss.maxHp,0,1);
        r.rect2D(-0.36f,0.74f,0.36f,0.79f,{0.04f,0.03f,0.06f,0.95f});
        r.rect2D(-0.35f,0.75f,-0.35f+0.70f*f,0.78f,{0.84f,0.10f,0.92f,1});
    } else if (mission_.phase==MissionPhase::Complete) {
        r.rect2D(-0.42f,-0.13f,0.42f,0.13f,{0.02f,0.16f,0.08f,0.94f});
        for (int i=0;i<5;++i)
            r.rect2D(-0.29f+i*0.13f,-0.035f,-0.21f+i*0.13f,0.045f,{0.08f,0.92f,0.42f,1});
    }
}

void Game::renderTV(Renderer& r) const {
    const Camera cam=makeCamera();
    r.begin3D(cam);

    r.submitBox({0,-0.20f,0},{ARENA_HALF,0.20f,ARENA_HALF},0,materials::floor());

    Material lane = materials::emissive({0.02f,0.32f,0.38f,1},0.55f);
    lane.diffuse={0.05f,0.12f,0.14f,1};
    lane.textureMix=0.0f;
    for (int i=-4;i<=4;++i) {
        r.submitBox({float(i)*4.0f,0.015f,0},{0.025f,0.015f,ARENA_HALF-0.7f},0,lane);
        r.submitBox({0,0.015f,float(i)*4.0f},{ARENA_HALF-0.7f,0.015f,0.025f},0,lane);
    }

    for (const auto& b:map_.obstacles()) {
        r.submitBox({b.center.x,b.height*0.5f,b.center.z},
                    {b.half.x,b.height*0.5f,b.half.z},0,
                    materials::wall(b.color));
    }

    const Vec3 beacons[]={{-17,2.0f,-17},{17,2.0f,-17},{-17,2.0f,17},{17,2.0f,17}};
    for (const auto& p:beacons) {
        r.submitBox(p,{0.22f,0.22f,0.22f},time_*0.4f,
                    materials::emissive({0.08f,0.75f,0.95f,1},2.6f));
    }

    renderMissionWorld(r);

    for (const auto& e:enemies_)
        if (e.state!=EnemyState::Dead) renderEnemy(r,e);

    particles_.render(r);
    renderWeapon(r,cam);
    r.flush3D();

    GX2SetDepthOnlyControl(FALSE,FALSE,GX2_COMPARE_FUNC_ALWAYS);
    r.begin2D();

    const Weapon& w=player_.weapon;
    const Color hudBack{0.025f,0.035f,0.05f,0.92f};
    r.rect2D(-0.96f,-0.92f,-0.54f,-0.78f,hudBack);
    r.rect2D(-0.94f,-0.895f,-0.58f,-0.845f,{0.15f,0.16f,0.18f,1});
    const float hpFrac=clampf(player_.hp/100.0f,0,1);
    const Color hpColor=mixColor({0.85f,0.10f,0.08f,1},{0.08f,0.82f,0.42f,1},hpFrac);
    r.rect2D(-0.94f,-0.895f,-0.94f+0.36f*hpFrac,-0.845f,hpColor);

    r.rect2D(0.54f,-0.92f,0.96f,-0.78f,hudBack);
    r.rect2D(0.58f,-0.895f,0.94f,-0.845f,{0.12f,0.14f,0.18f,1});
    const float ammoFrac=clampf(float(w.ammo)/float(w.magazineSize),0,1);
    r.rect2D(0.58f,-0.895f,0.58f+0.36f*ammoFrac,-0.845f,{0.92f,0.68f,0.12f,1});

    const float gap=0.018f+w.recoil*0.018f;
    const Color cross{0.90f,0.95f,1,0.95f};
    r.rect2D(-0.003f,gap,0.003f,gap+0.028f,cross);
    r.rect2D(-0.003f,-gap-0.028f,0.003f,-gap,cross);
    r.rect2D(gap,-0.003f,gap+0.028f,0.003f,cross);
    r.rect2D(-gap-0.028f,-0.003f,-gap,0.003f,cross);

    if (w.reloadTimer>0) {
        const float f=1.0f-w.reloadTimer/w.reloadDuration;
        r.rect2D(-0.18f,-0.69f,0.18f,-0.64f,{0.06f,0.08f,0.11f,1});
        r.rect2D(-0.175f,-0.685f,-0.175f+0.35f*clampf(f,0,1),-0.645f,
                 {0.12f,0.72f,0.95f,1});
    }

    renderMissionHUD(r);

    if (player_.damageFlash>0) {
        const float a=clampf(player_.damageFlash*0.75f,0,0.55f);
        const Color c{0.95f,0.05f,0.03f,a};
        r.rect2D(-1,-1,1,-0.93f,c); r.rect2D(-1,0.93f,1,1,c);
        r.rect2D(-1,-0.93f,-0.94f,0.93f,c); r.rect2D(0.94f,-0.93f,1,0.93f,c);
    }

    if (player_.hp<=0) {
        r.rect2D(-0.48f,-0.12f,0.48f,0.12f,{0.12f,0.02f,0.025f,0.95f});
        for (int i=0;i<5;++i)
            r.rect2D(-0.30f+i*0.13f,-0.04f,-0.22f+i*0.13f,0.04f,{0.88f,0.12f,0.08f,1});
    }
    r.flush2D();
}

void Game::renderMap(Renderer& r) const {
    GX2SetDepthOnlyControl(FALSE,FALSE,GX2_COMPARE_FUNC_ALWAYS);
    r.begin2D();

    const Color panel{0.025f,0.045f,0.065f,1};
    const Color grid{0.08f,0.18f,0.22f,1};
    const Color wall{0.28f,0.38f,0.46f,1};
    r.rect2D(-0.97f,-0.97f,0.97f,0.97f,panel);

    for (int i=-4;i<=4;++i) {
        const float p=i*0.18f;
        r.rect2D(p-0.002f,-0.86f,p+0.002f,0.86f,grid);
        r.rect2D(-0.86f,p-0.003f,0.86f,p+0.003f,grid);
    }

    const auto& obs=map_.obstacles();
    for (std::size_t i=4;i<obs.size();++i) {
        const auto& b=obs[i];
        r.rect2D(mapX(b.center.x-b.half.x),mapY(b.center.z+b.half.z),
                 mapX(b.center.x+b.half.x),mapY(b.center.z-b.half.z),wall);
    }

    const Color frame{0.18f,0.55f,0.62f,1};
    r.rect2D(-0.88f,-0.89f,0.88f,-0.86f,frame);
    r.rect2D(-0.88f,0.86f,0.88f,0.89f,frame);
    r.rect2D(-0.89f,-0.86f,-0.86f,0.86f,frame);
    r.rect2D(0.86f,-0.86f,0.89f,0.86f,frame);

    const float radarRad=0.33f;
    for (int i=0;i<24;++i) {
        const float a0=float(i)*2*PI/24.0f;
        const float a1=float(i+1)*2*PI/24.0f;
        drawThinLine(r,mapX(player_.pos.x)+std::cos(a0)*radarRad,
                     mapY(player_.pos.z)+std::sin(a0)*radarRad,
                     mapX(player_.pos.x)+std::cos(a1)*radarRad,
                     mapY(player_.pos.z)+std::sin(a1)*radarRad,
                     0.004f,{0.08f,0.38f,0.42f,1});
    }

    for (const auto& e:enemies_) {
        if (e.state==EnemyState::Dead) continue;
        const float x=mapX(e.pos.x), y=mapY(e.pos.z);
        Color c=classColor(e.archetype);
        if (e.elite) c={1.0f,0.08f,0.82f,1};
        const float size=e.elite?0.040f:(e.archetype==EnemyClass::Heavy?0.030f:0.022f);
        r.rect2D(x-size,y-size,x+size,y+size,c);
        const Vec3 ef{std::sin(e.yaw),0,-std::cos(e.yaw)};
        drawThinLine(r,x,y,x+ef.x*0.045f,y-ef.z*0.045f,0.006f,c);
    }

    Vec3 objective=mission_.terminal;
    Color objectiveColor{0.06f,0.76f,0.96f,1};
    if (mission_.phase==MissionPhase::Evacuate || mission_.phase==MissionPhase::Complete) {
        objective=mission_.extraction;
        objectiveColor={0.08f,0.92f,0.42f,1};
    } else if (mission_.phase==MissionPhase::EliminateMiniboss && bossAlive()) {
        objective=enemies_[mission_.bossEnemyIndex].pos;
        objectiveColor={1.0f,0.08f,0.82f,1};
    }
    const float ox=mapX(objective.x), oy=mapY(objective.z);
    const float pulse=0.038f+0.012f*(0.5f+0.5f*std::sin(time_*5.0f));
    r.rect2D(ox-pulse,oy-pulse,ox+pulse,oy+pulse,objectiveColor);
    r.rect2D(ox-pulse-0.018f,oy-pulse-0.018f,ox+pulse+0.018f,oy-pulse-0.010f,objectiveColor);
    r.rect2D(ox-pulse-0.018f,oy+pulse+0.010f,ox+pulse+0.018f,oy+pulse+0.018f,objectiveColor);

    if (mission_.phase==MissionPhase::DefendTerminal) {
        const float cx=mapX(mission_.terminal.x), cy=mapY(mission_.terminal.z);
        const float rr=(kDefenseRadius/ARENA_HALF)*0.86f;
        for (int i=0;i<32;++i) {
            const float a0=float(i)*2*PI/32.0f;
            const float a1=float(i+1)*2*PI/32.0f;
            drawThinLine(r,cx+std::cos(a0)*rr,cy+std::sin(a0)*rr,
                         cx+std::cos(a1)*rr,cy+std::sin(a1)*rr,0.003f,
                         {0.08f,0.65f,0.86f,0.75f});
        }
    }

    const float px=mapX(player_.pos.x), py=mapY(player_.pos.z);
    const Vec3 fwd{std::sin(player_.yaw),0,-std::cos(player_.yaw)};
    const Color pc{0.08f,0.92f,0.96f,1};
    const Vec4 tip{px+fwd.x*0.055f,py-fwd.z*0.055f,0,1};
    const Vec4 left{px-fwd.x*0.025f-fwd.z*0.030f,
                    py+fwd.z*0.025f-fwd.x*0.030f,0,1};
    const Vec4 right{px-fwd.x*0.025f+fwd.z*0.030f,
                     py+fwd.z*0.025f+fwd.x*0.030f,0,1};
    r.tri2D(tip,left,right,pc);

    const int step=missionStep(mission_.phase);
    for (int i=0;i<5;++i) {
        const float x0=-0.27f+float(i)*0.115f;
        Color c={0.08f,0.13f,0.16f,1};
        if (i<step) c={0.08f,0.58f,0.34f,1};
        if (i==step && mission_.phase!=MissionPhase::Complete) c={0.06f,0.75f,0.94f,1};
        if (mission_.phase==MissionPhase::Complete) c={0.08f,0.82f,0.42f,1};
        r.rect2D(x0,0.90f,x0+0.082f,0.94f,c);
    }

    const Weapon& w=player_.weapon;
    r.rect2D(-0.90f,-0.95f,-0.20f,-0.91f,{0.10f,0.12f,0.14f,1});
    r.rect2D(-0.90f,-0.95f,-0.90f+0.70f*clampf(player_.hp/100.0f,0,1),-0.91f,{0.08f,0.78f,0.42f,1});
    r.rect2D(0.20f,-0.95f,0.90f,-0.91f,{0.10f,0.12f,0.14f,1});
    r.rect2D(0.20f,-0.95f,0.20f+0.70f*clampf(float(w.ammo)/w.magazineSize,0,1),-0.91f,{0.92f,0.66f,0.10f,1});

    if (mission_.phase==MissionPhase::ActivateTerminal) {
        const float f=clampf(mission_.actionProgress/kTerminalInteractTime,0,1);
        r.rect2D(-0.34f,-0.84f,0.34f,-0.80f,{0.04f,0.07f,0.09f,1});
        r.rect2D(-0.33f,-0.83f,-0.33f+0.66f*f,-0.81f,{0.06f,0.78f,0.96f,1});
    } else if (mission_.phase==MissionPhase::DefendTerminal) {
        const float f=1.0f-clampf(mission_.defendRemaining/kDefenseDuration,0,1);
        const bool inside=planarDistance(player_.pos,mission_.terminal)<=kDefenseRadius;
        r.rect2D(-0.34f,-0.84f,0.34f,-0.80f,{0.04f,0.07f,0.09f,1});
        r.rect2D(-0.33f,-0.83f,-0.33f+0.66f*f,-0.81f,
                 inside?Color{0.08f,0.82f,0.44f,1}:Color{0.95f,0.22f,0.06f,1});
    }
    r.flush2D();
}

float Game::mapX(float x) { return (x/ARENA_HALF)*0.86f; }
float Game::mapY(float z) { return (-z/ARENA_HALF)*0.86f; }

void Game::drawThinLine(Renderer& r,float x0,float y0,float x1,float y1,
                        float width,const Color& c) {
    const float dx=x1-x0,dy=y1-y0;
    const float l=std::sqrt(dx*dx+dy*dy);
    if (l<0.0001f) return;
    const float nx=-dy/l*width, ny=dx/l*width;
    const Vec4 a{x0+nx,y0+ny,0,1}, b{x1+nx,y1+ny,0,1};
    const Vec4 cc{x1-nx,y1-ny,0,1}, d{x0-nx,y0-ny,0,1};
    r.tri2D(a,b,cc,c); r.tri2D(a,cc,d,c);
}

void Game::renderEnemy(Renderer& r,const Enemy& e) const {
    const float walk=sampleLoop(kWalkKeys,sizeof(kWalkKeys)/sizeof(kWalkKeys[0]),2*PI,e.anim);
    const float bob=std::fabs(sampleLoop(kWalkKeys,sizeof(kWalkKeys)/sizeof(kWalkKeys[0]),2*PI,e.anim*0.5f))*0.045f;

    Color baseArmor{};
    switch (e.archetype) {
        case EnemyClass::Scout: baseArmor={0.12f,0.40f,0.46f,1}; break;
        case EnemyClass::Soldier: baseArmor={0.30f,0.34f,0.38f,1}; break;
        case EnemyClass::Heavy: baseArmor={0.30f,0.18f,0.34f,1}; break;
    }
    if (e.elite) baseArmor={0.42f,0.18f,0.46f,1};

    const float damageFrac=1.0f-clampf(e.hp/std::max(1.0f,e.maxHp),0.0f,1.0f);
    const Color armorColor=mixColor(baseArmor,{0.52f,0.08f,0.06f,1},clampf(damageFrac,0,0.58f));
    const Material armor=materials::armor(armorColor);
    const Material dark=materials::darkMetal();
    Material joint=materials::darkMetal();
    joint.diffuse={0.16f,0.18f,0.20f,1};
    Color eyeColor=classColor(e.archetype);
    if (e.state==EnemyState::Attack) eyeColor=mixColor(eyeColor,{1.0f,0.04f,0.02f,1},0.55f);
    if (e.elite) eyeColor={1.0f,0.08f,0.82f,1};
    const Material eye=materials::emissive(eyeColor,e.elite?4.2f:3.1f);

    Transform world{};
    world.position=e.pos;
    world.yaw=e.yaw;
    world.scale={e.scale,e.scale,e.scale};

    if (enemySkinnedMesh_.valid()) {
        const float hitReaction=clampf(damageFrac,0.0f,0.55f);
        const SkeletonPose pose=makeRobotPose(e.anim,e.state==EnemyState::Attack,hitReaction);
        r.submitSkinnedMesh(enemySkinnedMesh_,pose,world,armor);

        const auto& headBone=pose.bones[static_cast<std::size_t>(RobotBone::Head)];
        const auto& weaponBone=pose.bones[static_cast<std::size_t>(RobotBone::Weapon)];
        const Vec3 visorLocal=applyBonePoint(headBone,{0,1.98f,-0.305f});
        const Vec3 muzzleLocal=applyBonePoint(weaponBone,{0.49f,1.18f,-0.92f});
        r.submitBox(transformPoint(world,visorLocal),{0.22f*e.scale,0.055f*e.scale,0.025f*e.scale},e.yaw,eye);
        r.submitBox(transformPoint(world,muzzleLocal),{0.045f*e.scale,0.045f*e.scale,0.055f*e.scale},e.yaw,
                    materials::emissive(eyeColor,e.elite?2.8f:1.5f));
        if (e.archetype==EnemyClass::Scout) {
            const Material scoutGlow=materials::emissive(eyeColor,2.4f);
            r.submitBox(e.pos+rotateY({0,2.28f*e.scale,0.02f*e.scale},e.yaw),
                        {0.035f*e.scale,0.22f*e.scale,0.035f*e.scale},e.yaw,scoutGlow);
            r.submitBox(e.pos+rotateY({0,2.49f*e.scale,0.02f*e.scale},e.yaw),
                        {0.08f*e.scale,0.045f*e.scale,0.08f*e.scale},e.yaw,scoutGlow);
        } else if (e.archetype==EnemyClass::Heavy) {
            const Material core=materials::emissive(eyeColor,e.elite?3.8f:2.2f);
            r.submitBox(e.pos+rotateY({0,1.28f*e.scale,-0.31f*e.scale},e.yaw),
                        {0.18f*e.scale,0.13f*e.scale,0.035f*e.scale},e.yaw,core);
            r.submitBox(e.pos+rotateY({-0.62f*e.scale,1.60f*e.scale,0},e.yaw),
                        {0.24f*e.scale,0.22f*e.scale,0.28f*e.scale},e.yaw,armor);
            r.submitBox(e.pos+rotateY({ 0.62f*e.scale,1.60f*e.scale,0},e.yaw),
                        {0.24f*e.scale,0.22f*e.scale,0.28f*e.scale},e.yaw,armor);
            if (e.elite) {
                r.submitBox(e.pos+rotateY({0,2.32f*e.scale,0.04f*e.scale},e.yaw),
                            {0.30f*e.scale,0.08f*e.scale,0.18f*e.scale},e.yaw,core);
            }
        }
        return;
    }

    auto scaledLocal=[&](Vec3 local){return Vec3{local.x*e.scale,local.y*e.scale,local.z*e.scale};};
    auto worldPart=[&](Vec3 local){return e.pos+rotateY(scaledLocal(local),e.yaw);};
    auto half=[&](Vec3 h){return scaledLocal(h);};
    if (enemyBodyMesh_.valid()) {
        Transform t=world;
        t.position=e.pos+Vec3{0,bob*e.scale,0};
        r.submitMesh(enemyBodyMesh_,t,armor);
    } else {
        r.submitBox(worldPart({0,1.28f+bob,0}),half({0.42f,0.52f,0.25f}),e.yaw,armor);
        r.submitBox(worldPart({0,1.30f+bob,-0.285f}),half({0.30f,0.25f,0.055f}),e.yaw,dark);
        r.submitBox(worldPart({0,1.96f+bob,0}),half({0.30f,0.26f,0.27f}),e.yaw,dark);
        r.submitBox(worldPart({-0.58f,1.55f+bob,0}),half({0.17f,0.18f,0.20f}),e.yaw,armor);
        r.submitBox(worldPart({0.58f,1.55f+bob,0}),half({0.17f,0.18f,0.20f}),e.yaw,armor);
    }

    r.submitBox(worldPart({0,1.98f+bob,-0.305f}),half({0.22f,0.055f,0.025f}),e.yaw,eye);
    r.submitBox(worldPart({-0.59f,1.10f+bob,walk*0.11f}),half({0.14f,0.35f,0.14f}),e.yaw,joint);
    r.submitBox(worldPart({0.59f,1.10f+bob,-walk*0.11f}),half({0.14f,0.35f,0.14f}),e.yaw,joint);
    r.submitBox(worldPart({0.48f,1.13f+bob,-0.42f}),half({0.13f,0.10f,0.42f}),e.yaw,dark);
    r.submitBox(worldPart({0.48f,1.13f+bob,-0.86f}),half({0.055f,0.055f,0.18f}),e.yaw,dark);
    r.submitBox(worldPart({0,0.72f,0}),half({0.32f,0.18f,0.22f}),e.yaw,dark);
    r.submitBox(worldPart({-0.23f,0.34f,walk*0.13f}),half({0.16f,0.34f,0.18f}),e.yaw,armor);
    r.submitBox(worldPart({0.23f,0.34f,-walk*0.13f}),half({0.16f,0.34f,0.18f}),e.yaw,armor);
    r.submitBox(worldPart({-0.23f,0.09f,-0.10f+walk*0.10f}),half({0.18f,0.09f,0.30f}),e.yaw,dark);
    r.submitBox(worldPart({0.23f,0.09f,-0.10f-walk*0.10f}),half({0.18f,0.09f,0.30f}),e.yaw,dark);
    if (e.archetype==EnemyClass::Scout) {
        r.submitBox(worldPart({0,2.36f,0.02f}),half({0.04f,0.28f,0.04f}),e.yaw,eye);
    } else if (e.archetype==EnemyClass::Heavy) {
        r.submitBox(worldPart({-0.66f,1.62f,0}),half({0.24f,0.22f,0.28f}),e.yaw,armor);
        r.submitBox(worldPart({ 0.66f,1.62f,0}),half({0.24f,0.22f,0.28f}),e.yaw,armor);
    }
}

void Game::renderWeapon(Renderer& r,const Camera& cam) const {
    const Vec3 f=cameraForward(cam), right=cameraRight(cam), up=cameraUp(cam);
    const float kick=player_.weapon.recoil*0.08f;
    const Vec3 base=cam.pos+f*(0.67f-kick)+right*0.27f+up*(-0.23f);
    const float yaw=cam.yaw;
    r.submitBox(base,{0.10f,0.10f,0.42f},yaw,materials::darkMetal());
    Material barrel=materials::darkMetal(); barrel.diffuse={0.18f,0.21f,0.23f,1};
    r.submitBox(base+f*0.38f,{0.055f,0.055f,0.28f},yaw,barrel);
    r.submitBox(base+up*0.11f+f*0.05f,{0.055f,0.055f,0.12f},yaw,
                materials::emissive({0.06f,0.42f,0.48f,1},1.4f));
}

} // namespace aa
