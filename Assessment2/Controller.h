#pragma once

#include "Animation.h"
#include "Collision.h"
#include "GEMLoader.h"
#include <Windows.h>
#include <algorithm>
#include <iostream>
#include <string>
#include <vector>

static void listAnimationNames(const GEMLoader::GEMAnimation &gemanimation) {
  std::cout << "----- Animation List Start -----" << std::endl;
  for (int i = 0; i < gemanimation.animations.size(); i++) {
    std::cout << "[" << i << "] " << gemanimation.animations[i].name
              << std::endl;
  }
  std::cout << "----- Animation List End -----" << std::endl;
}

// Animal NPC data structure
struct AnimalData {
  int health;
  int maxHealth;
  int attackDamage;
  float attackInterval;
  float attackTimer;
  float moveSpeed;
  bool isAlive;

  AnimalData(int hp = 100, int dmg = 10, float atkInterval = 2.0f, float speed = 10.0f): health(hp), maxHealth(hp), 
             attackDamage(dmg), attackInterval(atkInterval), attackTimer(0.0f), moveSpeed(speed), isAlive(true) {}

  void takeDamage(int dmg) {
    health -= dmg;
    if (health <= 0) {
      health = 0;
      isAlive = false;
    }
  }

  void update(float dt) {
    if (isAlive) {
      attackTimer += dt;
    }
  }

  bool canAttack() {
    if (attackTimer >= attackInterval) {
      attackTimer = 0.0f;
      return true;
    }
    return false;
  }
};

class AnimationController {
private:
  AnimationInstance *instance = nullptr;
  std::vector<std::string> animationNames;
  int currentIndex = 0;
  float timer = 0.0f;
  float switchInterval = 3.0f;

public:
  void init(AnimationInstance *_instance, float interval = 3.0f) {
    instance = _instance;
    switchInterval = interval;
    currentIndex = 0;
    timer = 0.0f;

    if (instance && instance->animation) {
      animationNames.clear();
      for (const auto &animPair : instance->animation->animations) {
        animationNames.push_back(animPair.first);
      }
    }
  }

  void update(float dt) {
    if (!instance || animationNames.empty())
      return;

    timer += dt;
    if (timer >= switchInterval) {
      timer = 0.0f;
      currentIndex++;
      if (currentIndex >= animationNames.size()) {
        currentIndex = 0;
      }
    }

    std::string currentAnimName = animationNames[currentIndex];
    instance->update(currentAnimName, dt);

    if (instance->animationFinished()) {
      instance->resetAnimationTime();
    }
  }
};

enum class GunAnimState {
  IDLE,
  WALK,
  RUN,
  FIRE,
  DRYFIRE,
  RELOAD,
  EMPTY_RELOAD,
  MELEE
};

class GunAnimationController {
private:
  AnimationInstance *instance = nullptr;
  GunAnimState currentState = GunAnimState::IDLE;
  bool initialized = false;

  // Animation names
  std::string idleAnim;
  std::string walkAnim;
  std::string runAnim;
  std::string fireAnim;
  std::string dryfireAnim;
  std::string meleeAnim;
  std::string reloadAnim;
  std::string emptyReloadAnim;
  std::string fallbackAnim;

  // Gun data
  int magazineCapacity = 31;
  int currentMagazine = 31;
  int reserveAmmo = 186;
  int maxReserveAmmo = 186;
  float fireInterval = 0.2f; 
  float fireTimer = 0.0f;
  int bulletDamage = 30;
  int meleeDamage = 10;
  bool firedThisFrame = false;
  bool meleedThisFrame = false;
  bool reloadedThisFrame = false;
  bool dryfiredThisFrame = false;

  // Melee 
  float meleeCooldown = 1.0f;
  float meleeCooldownTimer = 0.0f;
  bool canMelee = true;

  // Edge detection for inputs
  bool prevKeyR = false;
  bool prevMouseRight = false;

  bool isInterruptible(GunAnimState state) {
    return (state == GunAnimState::IDLE || state == GunAnimState::WALK || state == GunAnimState::RUN || 
            state == GunAnimState::FIRE || state == GunAnimState::DRYFIRE || state == GunAnimState::MELEE ||
            state == GunAnimState::RELOAD || state == GunAnimState::EMPTY_RELOAD);
  }

  std::string getAnimName(GunAnimState state) {
    switch (state) {
    case GunAnimState::FIRE:
      return fireAnim;
    case GunAnimState::DRYFIRE:
      return dryfireAnim;
    case GunAnimState::WALK:
      return walkAnim;
    case GunAnimState::RUN:
      return runAnim;
    case GunAnimState::RELOAD:
      return reloadAnim;
    case GunAnimState::EMPTY_RELOAD:
      return emptyReloadAnim;
    case GunAnimState::MELEE:
      return meleeAnim;
    default:
      return idleAnim;
    }
  }

  std::string findAnimContaining(const std::string &keyword) {
    if (!instance || !instance->animation)
      return "";
    for (const auto &animPair : instance->animation->animations) {
      if (animPair.first.find(keyword) != std::string::npos) {
        return animPair.first;
      }
    }
    return "";
  }

  void performReload() {
    if (reserveAmmo <= 0)
      return;

    int needed = magazineCapacity - currentMagazine;
    if (currentMagazine == 0) {
      // Empty reload (only load 30 rounds)
      needed = magazineCapacity - 1;
    }

    int toLoad = (reserveAmmo >= needed) ? needed : reserveAmmo;
    currentMagazine += toLoad;
    reserveAmmo -= toLoad;
  }

public:
  bool isSprinting = false;

  int getMagazine() const { return currentMagazine; }
  int getReserve() const { return reserveAmmo; }
  int getDamage() const { return bulletDamage; }
  bool hasFired() const { return firedThisFrame; }
  bool hasMeleed() const { return meleedThisFrame; }
  bool hasReloaded() const { return reloadedThisFrame; }
  bool hasDryfired() const { return dryfiredThisFrame; }
  int getMeleeDamage() const { return meleeDamage; }
  int getMaxReserve() const { return maxReserveAmmo; }

  // Add reserve ammo (for pickup), returns actual amount added
  int addReserve(int amount) {
    int space = maxReserveAmmo - reserveAmmo;
    int toAdd = (amount > space) ? space : amount;
    reserveAmmo += toAdd;
    return toAdd;
  }

  // Set ammo directly (for save/load)
  void setAmmo(int mag, int res) {
    currentMagazine = mag;
    reserveAmmo = res;
  }

  void init(AnimationInstance *_instance) {
    instance = _instance;
    currentState = GunAnimState::IDLE;
    initialized = false;
    isSprinting = false;
    prevKeyR = false;
    prevMouseRight = false;

    currentMagazine = 31;
    reserveAmmo = 186;
    fireTimer = 0.0f;

    if (instance && instance->animation &&
        !instance->animation->animations.empty()) {
      for (const auto &animPair : instance->animation->animations) {
        if (fallbackAnim.empty())
          fallbackAnim = animPair.first;
      }

      idleAnim = findAnimContaining("idle");
      walkAnim = findAnimContaining("walk");
      runAnim = findAnimContaining("run");
      fireAnim = findAnimContaining("08 fire");
      dryfireAnim = findAnimContaining("dryfire");
      meleeAnim = findAnimContaining("melee");
      reloadAnim = findAnimContaining("17 reload");
      emptyReloadAnim = findAnimContaining("18 empty");

      if (idleAnim.empty())
        idleAnim = fallbackAnim;
      if (walkAnim.empty())
        walkAnim = fallbackAnim;
      if (runAnim.empty())
        runAnim = walkAnim;
      if (fireAnim.empty())
        fireAnim = fallbackAnim;
      if (dryfireAnim.empty())
        dryfireAnim = fireAnim;
      if (meleeAnim.empty())
        meleeAnim = fallbackAnim;
      if (reloadAnim.empty())
        reloadAnim = fallbackAnim;
      if (emptyReloadAnim.empty())
        emptyReloadAnim = reloadAnim;

      initialized = true;
    }
  }

  void reset() {
    currentMagazine = 31;
    reserveAmmo = 186;
    fireTimer = 0.0f;
    currentState = GunAnimState::IDLE;
    if (instance) {
      instance->resetAnimationTime();
    }
  }

  void update(float dt, bool *keys, bool *mouseButtons) {
    if (!instance || !initialized)
      return;

    firedThisFrame = false;
    meleedThisFrame = false;
    reloadedThisFrame = false;
    dryfiredThisFrame = false;
    fireTimer += dt;

    if (!canMelee) {
      meleeCooldownTimer += dt;
      if (meleeCooldownTimer >= meleeCooldown) {
        canMelee = true;
      }
    }

    bool rJustPressed = keys['R'] && !prevKeyR;
    bool rightMouseJustPressed = mouseButtons[2] && !prevMouseRight;

    // Update previous state
    prevKeyR = keys['R'];
    prevMouseRight = mouseButtons[2];

    if ((currentState == GunAnimState::RELOAD || currentState == GunAnimState::EMPTY_RELOAD) && instance->animationFinished()) {
      performReload();
    }

    if (rJustPressed && reserveAmmo > 0 && currentMagazine < magazineCapacity) {
      if (currentMagazine == 0) {
        currentState = GunAnimState::EMPTY_RELOAD;
      } else {
        currentState = GunAnimState::RELOAD;
      }
      reloadedThisFrame = true;
      instance->resetAnimationTime();
    }
    else if (rightMouseJustPressed && canMelee) {
      currentState = GunAnimState::MELEE;
      meleedThisFrame = true;
      canMelee = false;
      meleeCooldownTimer = 0.0f;
      instance->resetAnimationTime();
    }
    else if (instance->animationFinished()) {
      // After melee/reload ends, if left mouse is held, continue fire
      if ((currentState == GunAnimState::MELEE ||
           currentState == GunAnimState::RELOAD ||
           currentState == GunAnimState::EMPTY_RELOAD)) {
        if (mouseButtons[0] && currentMagazine > 0) {
          currentState = GunAnimState::FIRE;
          currentMagazine--;
          fireTimer = 0.0f;
          firedThisFrame = true;
          instance->resetAnimationTime();
        } else {
          // Return to idle/walk/run
          if (keys[VK_SHIFT] && keys['W']) {
            currentState = GunAnimState::RUN;
          } else if (keys['W'] || keys['A'] || keys['S'] || keys['D']) {
            currentState = GunAnimState::WALK;
          } else {
            currentState = GunAnimState::IDLE;
          }
          instance->resetAnimationTime();
        }
      }
      // Melee finished - do NOT repeat while held, wait for cooldown
      else if (currentState == GunAnimState::MELEE) {
        // Return to movement state after melee ends
        if (keys[VK_SHIFT] && keys['W']) {
          currentState = GunAnimState::RUN;
        } else if (keys['W'] || keys['A'] || keys['S'] || keys['D']) {
          currentState = GunAnimState::WALK;
        } else {
          currentState = GunAnimState::IDLE;
        }
        instance->resetAnimationTime();
      }
      else if (currentState == GunAnimState::FIRE || currentState == GunAnimState::DRYFIRE) {
        if (mouseButtons[0] && currentMagazine > 0) {
          // Continue firing
          currentState = GunAnimState::FIRE;
          currentMagazine--;
          fireTimer = 0.0f;
          firedThisFrame = true;
          instance->resetAnimationTime();
        } else if (mouseButtons[0] && currentMagazine == 0) {
          // Out of ammo, dryfire
          currentState = GunAnimState::DRYFIRE;
          dryfiredThisFrame = true;
          instance->resetAnimationTime();
        } else {
          // Stop firing, return to movement state
          if (keys[VK_SHIFT] && keys['W']) {
            currentState = GunAnimState::RUN;
          } else if (keys['W'] || keys['A'] || keys['S'] || keys['D']) {
            currentState = GunAnimState::WALK;
          } else {
            currentState = GunAnimState::IDLE;
          }
          instance->resetAnimationTime();
        }
      }
      // Loop idle/walk/run
      else if (currentState == GunAnimState::IDLE || currentState == GunAnimState::WALK || currentState == GunAnimState::RUN) {
        instance->resetAnimationTime();
      }
    }
    else if (currentState == GunAnimState::FIRE && mouseButtons[0]) {
      if (fireTimer >= fireInterval && currentMagazine > 0) {
        currentMagazine--;
        fireTimer = 0.0f;
        firedThisFrame = true;
        instance->resetAnimationTime();
      }
    }
    // Starting to fire from idle/walk/run states
    if ((currentState == GunAnimState::IDLE || currentState == GunAnimState::WALK || currentState == GunAnimState::RUN) && mouseButtons[0]) {
      if (currentMagazine > 0) {
        currentState = GunAnimState::FIRE;
        currentMagazine--;
        fireTimer = 0.0f;
        firedThisFrame = true;
        instance->resetAnimationTime();
      } else {
        currentState = GunAnimState::DRYFIRE;
        dryfiredThisFrame = true;
        instance->resetAnimationTime();
      }
    }
    // Movement state 
    if (currentState == GunAnimState::IDLE || currentState == GunAnimState::WALK || currentState == GunAnimState::RUN) {
      if (!mouseButtons[0]) {
        if (keys[VK_SHIFT] && keys['W']) {
          if (currentState != GunAnimState::RUN) {
            currentState = GunAnimState::RUN;
            instance->resetAnimationTime();
          }
        } else if (keys['W'] || keys['A'] || keys['S'] || keys['D']) {
          if (currentState != GunAnimState::WALK) {
            currentState = GunAnimState::WALK;
            instance->resetAnimationTime();
          }
        } else {
          if (currentState != GunAnimState::IDLE) {
            currentState = GunAnimState::IDLE;
            instance->resetAnimationTime();
          }
        }
      }
    }

    isSprinting = (currentState == GunAnimState::RUN);
    std::string animName = getAnimName(currentState);
    instance->update(animName, dt);
  }
};

// Enemy AI States
enum class EnemyState {
  IDLE,
  ENTERING, // Enter the map
  CHASE,
  TURN_LEFT,
  TURN_RIGHT,
  ATTACK,
  HIT_REACT,
  DEATH,
  REMOVED
};

class EnemyController {
private:
  AnimationInstance *instance = nullptr;
  AnimalData *data = nullptr;
  EnemyState currentState = EnemyState::IDLE;
  bool initialized = false;
  bool isDuck = false; // Duck uses "bird + xx" 

  float deathTimer = 0.0f;
  float hitReactTimer = 0.0f;
  float attackTimer = 0.0f;
  bool hasDealtDamage = false;
  const float attackHitTime = 0.5f; // Damage dealt 0.5s after attack starts

  // Obstacle avoidance
  Vec3 lastPosition;
  float stuckTimer = 0.0f;
  float avoidanceTimer = 0.0f;
  int avoidanceDir = 1;
  bool isAvoiding = false;
  bool directionLocked = false;
  int avoidanceAttempts = 0;
  const float stuckThreshold = 0.25f;
  const float avoidanceDuration = 1.2f;
  // Obstacle jump
  Vec3 avoidanceStartPos;
  float totalAvoidanceTime = 0.0f;
  const float circlingJumpThreshold = 3.0f; 
  const float circlingRadius = 5.0f; 

  // Jump 
  float velocityY = 0.0f;
  bool isJumping = false;
  const float gravity = 15.0f;
  const float jumpForce = 8.0f;
  const float defaultGroundY = 0.0f;
  const float heightTolerance = 1.0f;
  const float playerEyeHeight = 1.5f;

  // Entry path 
  Vec3 entryTarget; // Target position to move to before chasing
  bool hasEntryTarget = false;
  const float entryReachDist = 3.0f; 

  // Animation names
  std::string idleAnim, runAnim, attackAnim, hitAnim, deathAnim;
  std::string turnLeftAnim, turnRightAnim;

  std::string getPrefix() { return isDuck ? "bird " : ""; }

  float getAngleToTarget(Vec3 &myPos, Vec3 &targetPos, float myYaw) {
    Vec3 toTarget = targetPos - myPos;
    toTarget.y = 0;
    float targetAngle = atan2f(toTarget.x, toTarget.z);
    float angleDiff = targetAngle - myYaw;
    while (angleDiff > 3.14159f)
      angleDiff -= 6.28318f;
    while (angleDiff < -3.14159f)
      angleDiff += 6.28318f;
    return angleDiff;
  }

public:
  Vec3 position;
  float yaw = 0.0f;
  bool shouldRemove = false;

  void init(AnimationInstance *_instance, AnimalData *_data, Vec3 startPos,
            bool _isDuck = false, Vec3 _entryTarget = Vec3(0, 0, 0)) {
    instance = _instance;
    data = _data;
    position = startPos;
    lastPosition = startPos;
    isDuck = _isDuck;

    // Set entry target and initial state
    entryTarget = _entryTarget;
    hasEntryTarget = (_entryTarget.x != 0 || _entryTarget.z != 0);
    currentState = hasEntryTarget ? EnemyState::ENTERING : EnemyState::CHASE;

    initialized = true;
    shouldRemove = false;
    stuckTimer = 0.0f;
    avoidanceTimer = 0.0f;
    isAvoiding = false;
    velocityY = 0.0f;
    isJumping = false;

    std::string prefix = getPrefix();

    // Find animation name
    auto findAnim = [this](const std::string &name) -> std::string {
      if (instance && instance->animation) {
        if (instance->animation->hasAnimation(name)) {
          return name;
        }
        for (const auto &animPair : instance->animation->animations) {
          if (animPair.first.find(name) != std::string::npos) {
            return animPair.first;
          }
        }
        if (!instance->animation->animations.empty()) {
          return instance->animation->animations.begin()->first;
        }
      }
      return name; 
    };

    idleAnim = findAnim(prefix + "idle");
    runAnim = findAnim(prefix + "run");
    attackAnim = findAnim(prefix + "attack");
    hitAnim = findAnim(prefix + "hit");
    deathAnim = findAnim(prefix + "death");
    turnLeftAnim = findAnim(prefix + "turn");
    turnRightAnim = findAnim(prefix + "turn");
  }

  void takeDamage(int damage, Vec3 knockbackDir) {
    if (!data || !data->isAlive || currentState == EnemyState::DEATH)
      return;

    data->takeDamage(damage);

    if (data->health <= 0) {
      currentState = EnemyState::DEATH;
      deathTimer = 0.0f;
      instance->resetAnimationTime();
    } else {
      currentState = EnemyState::HIT_REACT;
      hitReactTimer = 0.0f;
      knockbackDir.y = 0;
      if (knockbackDir.length() > 0.01f) {
        knockbackDir = knockbackDir.normalize();
        position = position + knockbackDir * 3.0f;
      }
      instance->resetAnimationTime();
    }
  }

  bool update(float dt, Vec3 playerPos, int &playerDamageOut,
              const std::vector<AABB> *staticColliders = nullptr,
              const std::string &modelName = "") {
    if (!instance || !data || !initialized)
      return false;
    playerDamageOut = 0;

    if (currentState == EnemyState::REMOVED) {
      shouldRemove = true;
      return false;
    }

    Vec3 toPlayer = playerPos - position;
    float playerFeetY = playerPos.y - playerEyeHeight;
    float heightDiff = playerFeetY - position.y;
    toPlayer.y = 0;
    float distToPlayer = toPlayer.length();
    float angleDiff = getAngleToTarget(position, playerPos, yaw);
    bool sameHeight = fabsf(heightDiff) < heightTolerance;

    // Calculate dynamic ground height
    float currentGroundY = defaultGroundY;
    if (staticColliders && !staticColliders->empty()) {
      for (const auto &wall : *staticColliders) {
        if (position.x >= wall.min.x && position.x <= wall.max.x &&
            position.z >= wall.min.z && position.z <= wall.max.z) {
          if (position.y >= wall.max.y - 0.5f) {
            if (wall.max.y > currentGroundY) {
              currentGroundY = wall.max.y;
            }
          }
        }
      }
    }

    // Update jump physics
    if (isJumping || position.y > currentGroundY) {
      velocityY -= gravity * dt;
      position.y += velocityY * dt;
      if (position.y <= currentGroundY) {
        position.y = currentGroundY;
        velocityY = 0.0f;
        isJumping = false;
      }
    }

    switch (currentState) {
    case EnemyState::IDLE:
      instance->update(idleAnim, dt);
      if (instance->animationFinished())
        instance->resetAnimationTime();
      currentState = EnemyState::CHASE;
      break;

    case EnemyState::ENTERING: {
      Vec3 toTarget = entryTarget - position;
      toTarget.y = 0;
      float distToTarget = toTarget.length();

      if (distToTarget < entryReachDist) {
        // Reached entry point, start chasing player
        currentState = EnemyState::CHASE;
        hasEntryTarget = false;
      } else {
        // Move toward entry target
        float angleDiff = getAngleToTarget(position, entryTarget, yaw);
        yaw += angleDiff;
        Vec3 forward(sinf(yaw), 0, cosf(yaw));
        position = position + forward * data->moveSpeed * dt;
        lastPosition = position;
      }

      instance->update(runAnim, dt);
      if (instance->animationFinished())
        instance->resetAnimationTime();
    } break;

    case EnemyState::CHASE: {
      Vec3 posDelta = position - lastPosition;
      posDelta.y = 0;
      float moveDistance = posDelta.length();
      float expectedMove = data->moveSpeed * dt * 0.4f;

      if (moveDistance < expectedMove) {
        stuckTimer += dt;
        if (stuckTimer > stuckThreshold) {
          if (!isAvoiding) {
            isAvoiding = true;
            avoidanceTimer = 0.0f;
            avoidanceStartPos = position; // Record start position for circling detection
            if (!directionLocked) {
              Vec3 toPlayerDir = playerPos - position;
              Vec3 rightDir(cosf(yaw), 0, -sinf(yaw));
              avoidanceDir = (Dot(toPlayerDir, rightDir) > 0) ? -1 : 1;
              directionLocked = true;
              avoidanceAttempts = 0;
            }
          } else {
            avoidanceAttempts++;
            if (avoidanceAttempts > 3) {
              avoidanceDir = -avoidanceDir;
              avoidanceAttempts = 0;
            }
            stuckTimer = 0.0f;
          }
        }
      } else {
        stuckTimer = 0.0f;
        if (!isAvoiding) {
          directionLocked = false;
          avoidanceAttempts = 0;
        }
      }

      lastPosition = position;

      if (isAvoiding) {
        avoidanceTimer += dt;
        yaw += angleDiff * 0.2f;
        Vec3 forward(sinf(yaw), 0, cosf(yaw));
        Vec3 sideDir(cosf(yaw), 0, -sinf(yaw));
        Vec3 moveDir = forward * 0.5f + sideDir * (float)avoidanceDir * 0.7f;
        moveDir = moveDir.normalize();
        position = position + moveDir * data->moveSpeed * dt;

        if (staticColliders && !staticColliders->empty()) {
          resolveStaticCollisions(*staticColliders, modelName);
        }

        if (avoidanceTimer > avoidanceDuration) {
          isAvoiding = false;
          stuckTimer = 0.0f;
          totalAvoidanceTime += avoidanceTimer;
        }

        // Check for circling
        Vec3 fromStart = position - avoidanceStartPos;
        fromStart.y = 0;
        float distFromStart = fromStart.length();
        if (totalAvoidanceTime > circlingJumpThreshold &&
            distFromStart < circlingRadius && !isJumping) {
          // Try to jump over the obstacle
          isJumping = true;
          velocityY = jumpForce;
          Vec3 jumpDir = (playerPos - position);
          jumpDir.y = 0;
          if (jumpDir.length() > 0.01f) {
            jumpDir = jumpDir.normalize();
            position = position + jumpDir * 2.0f; 
          }
          // Reset circling detection
          totalAvoidanceTime = 0.0f;
          isAvoiding = false;
          directionLocked = false;
        }
      } else {
        yaw += angleDiff;
        Vec3 forward(sinf(yaw), 0, cosf(yaw));
        position = position + forward * data->moveSpeed * dt;

        if (staticColliders && !staticColliders->empty()) {
          resolveStaticCollisions(*staticColliders, modelName);
        }
      }

      instance->update(runAnim, dt);
      if (instance->animationFinished())
        instance->resetAnimationTime();

      if (distToPlayer < 2.0f && sameHeight) {
        currentState = EnemyState::ATTACK;
        data->attackTimer = data->attackInterval;
        instance->resetAnimationTime();
        isAvoiding = false;
        stuckTimer = 0.0f;
      }

      if (!isJumping && distToPlayer < 8.0f && heightDiff > 0.5f && isAvoiding) {
        isJumping = true;
        velocityY = jumpForce;
        Vec3 jumpDir = (playerPos - position);
        jumpDir.y = 0;
        if (jumpDir.length() > 0.01f) {
          jumpDir = jumpDir.normalize();
          position = position + jumpDir * 1.0f;
        }
      }
    } break;

    case EnemyState::TURN_LEFT:
    case EnemyState::TURN_RIGHT:
      currentState = EnemyState::CHASE;
      break;

    case EnemyState::ATTACK:
      instance->update(attackAnim, dt);
      attackTimer += dt;
      if (!sameHeight) {
        currentState = EnemyState::CHASE;
        instance->resetAnimationTime();
        attackTimer = 0.0f;
        hasDealtDamage = false;
        break;
      }
      if (!hasDealtDamage && attackTimer >= attackHitTime) {
        if (distToPlayer < 2.5f && sameHeight) {
          playerDamageOut = data->attackDamage;
        }
        hasDealtDamage = true;
      }
      if (instance->animationFinished()) {
        instance->resetAnimationTime();
        attackTimer = 0.0f;
        hasDealtDamage = false;
        if (distToPlayer > 2.5f) {
          currentState = EnemyState::CHASE;
        }
      }
      break;

    case EnemyState::HIT_REACT:
      instance->update(hitAnim, dt);
      hitReactTimer += dt;
      if (instance->animationFinished() || hitReactTimer > 1.0f) {
        currentState = EnemyState::CHASE;
        instance->resetAnimationTime();
      }
      break;

    case EnemyState::DEATH:
      instance->update(deathAnim, dt);
      deathTimer += dt;
      if (deathTimer >= 1.0f) {
        currentState = EnemyState::REMOVED;
        shouldRemove = true;
      }
      break;

    default:
      break;
    }

    return data->isAlive;
  }

  bool isAlive() const { return data && data->isAlive; }
  int getHealth() const { return data ? data->health : 0; }
  EnemyState getState() const { return currentState; }

private:
  void resolveStaticCollisions(const std::vector<AABB> &colliders,
                               const std::string &modelName) {
    AABB enemyAABB = getAnimatedModelAABB(modelName, position);
    for (const auto &wall : colliders) {
      CollisionInfo info = CollisionSystem::checkAABB(enemyAABB, wall);
      if (info.collided) {
        info.normal.y = 0;
        if (info.normal.length() > 0.01f) {
          CollisionSystem::resolveCollision(position, info);
          enemyAABB = getAnimatedModelAABB(modelName, position);
        }
      }
    }
  }
};