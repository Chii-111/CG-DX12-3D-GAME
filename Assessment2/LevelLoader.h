#pragma once
#include "Collision.h"
#include "Maths.h"
#include <fstream>
#include <map>
#include <sstream>
#include <string>
#include <vector>

// Object data from level file
struct LevelObject {
  std::string modelName;
  Vec3 position;
  float rotation; 
  float scale;
  bool hasCollision;
};

// Level loader from txt file
class LevelLoader {
public:
  std::vector<LevelObject> objects;

  bool load(const std::string &filename) {
    std::ifstream file(filename);
    if (!file.is_open()) {
      return false;
    }

    objects.clear();
    std::string line;

    while (std::getline(file, line)) {
      if (line.empty() || line[0] == '#') {
        continue;
      }

      std::istringstream iss(line);
      LevelObject obj;
      obj.scale = 0.01f;       
      obj.rotation = 0.0f;     
      obj.hasCollision = true; 

      // modelName x y z [rotation] [scale] [hasCollision]
      std::string modelName;
      float x, y, z;

      if (!(iss >> modelName >> x >> y >> z)) {
        continue; 
      }

      obj.modelName = modelName;
      obj.position = Vec3(x, y, z);

      float rot = 0.0f, scl = 0.01f;
      int collision = 1;

      if (iss >> rot) {
        obj.rotation = rot;
      }
      if (iss >> scl) {
        obj.scale = scl;
      }
      if (iss >> collision) {
        obj.hasCollision = (collision != 0);
      }

      objects.push_back(obj);
    }

    file.close();
    return true;
  }

  // Count instances per model
  std::map<std::string, int> countInstances() const {
    std::map<std::string, int> counts;
    for (const auto &obj : objects) {
      counts[obj.modelName]++;
    }
    return counts;
  }

  // Get all objects of a specific model
  std::vector<LevelObject>
  getObjectsByModel(const std::string &modelName) const {
    std::vector<LevelObject> result;
    for (const auto &obj : objects) {
      if (obj.modelName == modelName) {
        result.push_back(obj);
      }
    }
    return result;
  }

  // Check if placing an object would cause collision
  static bool wouldCollide(const std::string &modelName, Vec3 position, const std::vector<AABB> &existingColliders,
                           float margin = 0.1f) {
    AABB newAABB = getStaticModelAABB(modelName, position);
    newAABB.min = newAABB.min - Vec3(margin, margin, margin);
    newAABB.max = newAABB.max + Vec3(margin, margin, margin);

    for (const auto &existing : existingColliders) {
      CollisionInfo info = CollisionSystem::checkAABB(newAABB, existing);
      if (info.collided) {
        return true;
      }
    }
    return false;
  }
};
