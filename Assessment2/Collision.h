#pragma once
#include "Maths.h"
#include <algorithm>
#include <string>
#include <unordered_map>
#include <vector>

struct AABB {
  Vec3 min;
  Vec3 max;

  AABB() : min(FLT_MAX, FLT_MAX, FLT_MAX), max(-FLT_MAX, -FLT_MAX, -FLT_MAX) {}
  AABB(Vec3 _min, Vec3 _max) : min(_min), max(_max) {}

  AABB transform(Vec3 position) const {
    return AABB(min + position, max + position);
  }

  // Create AABB 
  static AABB fromCenterExtent(Vec3 center, Vec3 halfExtent) {
    return AABB(center - halfExtent, center + halfExtent);
  }
};

struct ModelBounds {
  float halfExtentX, halfExtentY, halfExtentZ;

  Vec3 toVec3() const { return Vec3(halfExtentX, halfExtentY, halfExtentZ); }
};

// Static model AABB bounds
inline std::unordered_map<std::string, ModelBounds> StaticModelBounds = {
    {"Wall_003", {2.0f, 3.0f, 0.1f}},
    {"Wall_020", {2.0f, 3.0f, 0.1f}},
    {"acacia_003", {0.75f, 2.65f, 0.81f}},
    {"acacia_006", {0.81f, 3.32f, 0.83f}},
    {"barracks_001", {8.5f, 3.12f, 17.73f}},
    {"barrel_003", {0.31f, 0.43f, 0.31f}},
    {"barrier_001", {1.16f, 0.39f, 0.47f}},
    {"barrier_002", {2.51f, 0.12f, 0.47f}},
    {"barrier_003", {2.19f, 0.48f, 0.21f}},
    {"barrier_004", {0.65f, 0.66f, 0.65f}},
    {"box_003", {0.71f, 0.39f, 0.47f}},
    {"box_004", {0.78f, 0.34f, 0.34f}},
    {"box_020", {0.72f, 0.66f, 0.72f}},
    {"box_023", {0.72f, 0.66f, 0.72f}},
    {"building_001", {8.94f, 2.28f, 3.54f}},
    {"cactus_005", {0.40f, 0.91f, 0.43f}},
    {"cart_001", {0.59f, 0.42f, 1.02f}},
    {"coil_001", {0.66f, 0.86f, 0.86f}},
    {"construction_001", {0.95f, 0.52f, 0.86f}},
    {"container_004", {1.26f, 1.35f, 3.15f}},
    {"deadwood_007", {1.05f, 0.49f, 0.87f}},
    {"generator_002", {0.87f, 0.71f, 1.49f}},
    {"grass_003", {0.51f, 0.29f, 0.52f}},
    {"ground_005", {4.88f, 0.64f, 4.56f}},
    {"ground_007", {6.83f, 0.28f, 6.33f}},
    {"hangar_001", {14.07f, 5.72f, 16.41f}},
    {"helicopter_platform_001", {5.27f, 0.81f, 6.15f}},
    {"log_001", {1.97f, 0.78f, 0.94f}},
    {"machine_gun_005", {0.45f, 0.41f, 0.76f}},
    {"mortar_001", {0.35f, 0.56f, 0.62f}},
    {"obstacle_001", {6.47f, 0.50f, 1.09f}},
    {"protection_001", {0.55f, 0.17f, 0.55f}},
    {"rock_003", {2.11f, 1.32f, 1.73f}},
    {"stone_017", {0.10f, 0.06f, 0.09f}},
    {"table_001", {1.00f, 0.40f, 0.58f}},
    {"tree_017", {0.77f, 3.13f, 0.82f}},
};

// Animated model (animal) AABB bounds
inline std::unordered_map<std::string, ModelBounds> AnimatedModelBounds = {
    {"Bull-dark", {1.08f, 0.74f, 1.13f}},
    {"Duck-mixed", {0.23f, 0.32f, 0.31f}},
    {"Goat-01", {0.55f, 0.65f, 0.68f}},
    {"Pig", {0.67f, 0.45f, 0.85f}},
};

// Collision center offsets for helicopter_platform_001
inline std::unordered_map<std::string, Vec3> StaticModelOffsets = {
    {"helicopter_platform_001", {0.0f, 0.0f, 1.5f}},
};

inline AABB getStaticModelAABB(const std::string &modelName, Vec3 position) {
  auto it = StaticModelBounds.find(modelName);
  if (it != StaticModelBounds.end()) {
    Vec3 extent = it->second.toVec3();
    Vec3 center = position + Vec3(0, extent.y, 0);

    auto offsetIt = StaticModelOffsets.find(modelName);
    if (offsetIt != StaticModelOffsets.end()) {
      center = center + offsetIt->second;
    }

    return AABB::fromCenterExtent(center, extent);
  }
  return AABB::fromCenterExtent(position + Vec3(0, 1.0f, 0),
                                Vec3(0.5f, 1.0f, 0.5f));
}

inline AABB getAnimatedModelAABB(const std::string &modelName, Vec3 position) {
  auto it = AnimatedModelBounds.find(modelName);
  if (it != AnimatedModelBounds.end()) {
    Vec3 extent = it->second.toVec3();

    Vec3 center = position + Vec3(0, extent.y, 0);
    return AABB::fromCenterExtent(center, extent);
  }
  return AABB::fromCenterExtent(position + Vec3(0, 1.0f, 0),
                                Vec3(0.5f, 1.0f, 0.5f));
}

struct CollisionInfo {
  bool collided;
  Vec3 normal;
  float depth;
};

class CollisionSystem {
public:
  static CollisionInfo checkAABB(const AABB &one, const AABB &two) {
    CollisionInfo info = {false, Vec3(0, 0, 0), 0.0f};

    float px = std::min(one.max.x, two.max.x) - std::max(one.min.x, two.min.x);
    float py = std::min(one.max.y, two.max.y) - std::max(one.min.y, two.min.y);
    float pz = std::min(one.max.z, two.max.z) - std::max(one.min.z, two.min.z);

    if (px > 0 && py > 0 && pz > 0) {
      info.collided = true;

      if (px < py && px < pz) {
        info.depth = px;
        info.normal = (one.min.x < two.min.x) ? Vec3(-1, 0, 0) : Vec3(1, 0, 0);
      } else if (py < pz) {
        info.depth = py;
        info.normal = (one.min.y < two.min.y) ? Vec3(0, -1, 0) : Vec3(0, 1, 0);
      } else {
        info.depth = pz;
        info.normal = (one.min.z < two.min.z) ? Vec3(0, 0, -1) : Vec3(0, 0, 1);
      }
    }

    return info;
  }

  static void resolveCollision(Vec3 &position, const CollisionInfo &info) {
    if (info.collided) {
      position += info.normal * info.depth;
    }
  }

  // Ray-AABB
  static bool rayIntersectsAABB(Vec3 rayOrigin, Vec3 rayDir, const AABB &box,
                                float maxDist = 1000.0f) {
    float tmin = 0.0f;
    float tmax = maxDist;

    // X axis
    if (fabsf(rayDir.x) < 0.0001f) {
      if (rayOrigin.x < box.min.x || rayOrigin.x > box.max.x)
        return false;
    } else {
      float invD = 1.0f / rayDir.x;
      float t1 = (box.min.x - rayOrigin.x) * invD;
      float t2 = (box.max.x - rayOrigin.x) * invD;

      if (t1 > t2) {
        float tmp = t1;
        t1 = t2;
        t2 = tmp;
      }

      tmin = std::max(tmin, t1);
      tmax = std::min(tmax, t2);

      if (tmin > tmax)
        return false;
    }

    // Y axis
    if (fabsf(rayDir.y) < 0.0001f) {
      if (rayOrigin.y < box.min.y || rayOrigin.y > box.max.y)
        return false;
    } else {
      float invD = 1.0f / rayDir.y;
      float t1 = (box.min.y - rayOrigin.y) * invD;
      float t2 = (box.max.y - rayOrigin.y) * invD;

      if (t1 > t2) {
        float tmp = t1;
        t1 = t2;
        t2 = tmp;
      }

      tmin = std::max(tmin, t1);
      tmax = std::min(tmax, t2);

      if (tmin > tmax)
        return false;
    }

    // Z axis
    if (fabsf(rayDir.z) < 0.0001f) {
      if (rayOrigin.z < box.min.z || rayOrigin.z > box.max.z)
        return false;
    } else {
      float invD = 1.0f / rayDir.z;
      float t1 = (box.min.z - rayOrigin.z) * invD;
      float t2 = (box.max.z - rayOrigin.z) * invD;

      if (t1 > t2) {
        float tmp = t1;
        t1 = t2;
        t2 = tmp;
      }

      tmin = std::max(tmin, t1);
      tmax = std::min(tmax, t2);

      if (tmin > tmax)
        return false;
    }

    return true;
  }
};
