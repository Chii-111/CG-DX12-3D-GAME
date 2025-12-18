#pragma once
#include "Animation.h"
#include "Camera.h"
#include "Collision.h"
#include "Controller.h"
#include "Core.h"
#include "GEMLoader.h"
#include "Maths.h"
#include "Mesh.h"
#include "PSO.h"
#include "Shaders.h"
#include "Texture.h"

struct LightData {
  Vec3 cameraPos;
  float padding1;
  Vec3 lightDir;
  float padding2;
  Vec3 lightColor;
  float padding3;
  float ambientStrength;
};

static STATIC_VERTEX addVertex(Vec3 p, Vec3 n, float tu, float tv) {
  STATIC_VERTEX v;
  v.pos = p;
  v.normal = n;
  Frame frame;
  frame.fromVector(n);
  v.tangent = frame.u;
  v.tu = tu;
  v.tv = tv;
  return v;
}

class Crosshair {
public:
  Mesh mesh;
  Mesh outlineMesh;

  void init(Core *core, Shaders *shaders, PSOManager *psos) {
    float ratio = 16.0f / 9.0f;
    float thickness = 0.001f;   
    float lineLen = 0.01f;      
    float gap = 0.005f;         // Gap from center
    float outlineSize = 0.001f; // Outline thickness

    // Create white crosshair mesh
    std::vector<STATIC_VERTEX> vertices;
    std::vector<unsigned int> indices;
    createCrosshairVertices(vertices, indices, ratio, thickness, lineLen, gap, 0.0f);
    mesh.init(core, vertices, indices);

    // Create black outline mesh
    std::vector<STATIC_VERTEX> outlineVerts;
    std::vector<unsigned int> outlineInds;
    float outlineThickness = thickness + outlineSize;
    float outlineLen = lineLen + outlineSize * 2;
    float outlineGap = gap - outlineSize;
    createCrosshairVertices(outlineVerts, outlineInds, ratio, outlineThickness, outlineLen, outlineGap, 0.0f);
    outlineMesh.init(core, outlineVerts, outlineInds);

    shaders->load(core, "CrosshairShader", "VS.txt", "PSFlatColor.txt");
    psos->createPSO(core, "CrosshairPSO", shaders->find("CrosshairShader")->vs, shaders->find("CrosshairShader")->ps, VertexLayoutCache::getStaticLayout());
  }

  void createCrosshairVertices(std::vector<STATIC_VERTEX> &vertices, std::vector<unsigned int> &indices, float ratio,
                               float thickness, float lineLen, float gap, float zOffset = 0.0f) {
    // Left line
    vertices.push_back(addVertex(Vec3(-gap - lineLen, -thickness * ratio, zOffset), Vec3(0, 0, -1), 0, 0));
    vertices.push_back(addVertex(Vec3(-gap, -thickness * ratio, zOffset), Vec3(0, 0, -1), 1, 0));
    vertices.push_back(addVertex(Vec3(-gap - lineLen, thickness * ratio, zOffset), Vec3(0, 0, -1), 0, 1));
    vertices.push_back(addVertex(Vec3(-gap, thickness * ratio, zOffset), Vec3(0, 0, -1), 1, 1));

    // Right line
    vertices.push_back(addVertex(Vec3(gap, -thickness * ratio, zOffset), Vec3(0, 0, -1), 0, 0));
    vertices.push_back(addVertex(Vec3(gap + lineLen, -thickness * ratio, zOffset), Vec3(0, 0, -1), 1, 0));
    vertices.push_back(addVertex(Vec3(gap, thickness * ratio, zOffset), Vec3(0, 0, -1), 0, 1));
    vertices.push_back(addVertex(Vec3(gap + lineLen, thickness * ratio, zOffset), Vec3(0, 0, -1), 1, 1));

    // Top line
    vertices.push_back(addVertex(Vec3(-thickness, gap * ratio, zOffset), Vec3(0, 0, -1), 0, 0));
    vertices.push_back(addVertex(Vec3(thickness, gap * ratio, zOffset), Vec3(0, 0, -1), 1, 0));
    vertices.push_back(addVertex(Vec3(-thickness, (gap + lineLen) * ratio, zOffset), Vec3(0, 0, -1), 0, 1));
    vertices.push_back(addVertex(Vec3(thickness, (gap + lineLen) * ratio, zOffset), Vec3(0, 0, -1), 1, 1));

    // Bottom line
    vertices.push_back(addVertex(Vec3(-thickness, (-gap - lineLen) * ratio, zOffset), Vec3(0, 0, -1), 0, 0));
    vertices.push_back(addVertex(Vec3(thickness, (-gap - lineLen) * ratio, zOffset), Vec3(0, 0, -1), 1, 0));
    vertices.push_back(addVertex(Vec3(-thickness, -gap * ratio, zOffset), Vec3(0, 0, -1), 0, 1));
    vertices.push_back(addVertex(Vec3(thickness, -gap * ratio, zOffset), Vec3(0, 0, -1), 1, 1));

    // Indices
    for (int i = 0; i < 4; i++) {
      int base = i * 4;
      indices.push_back(base + 0);
      indices.push_back(base + 1);
      indices.push_back(base + 2);
      indices.push_back(base + 1);
      indices.push_back(base + 3);
      indices.push_back(base + 2);
    }
  }

  void draw(Core *core, PSOManager *psos, Shaders *shaders) {
    Matrix identity;

    shaders->updateConstantVS("UIBlack", "staticMeshBuffer", "VP", &identity);
    shaders->updateConstantVS("UIBlack", "staticMeshBuffer", "W", &identity);
    shaders->apply(core, "UIBlack");
    psos->bind(core, "UIBlackPSO");
    outlineMesh.draw(core);

    shaders->updateConstantVS("CrosshairShader", "staticMeshBuffer", "VP", &identity);
    shaders->updateConstantVS("CrosshairShader", "staticMeshBuffer", "W", &identity);
    shaders->apply(core, "CrosshairShader");
    psos->bind(core, "CrosshairPSO");
    mesh.draw(core);
  }
};

// Hit result types
enum class HitResult { NONE, HIT, KILL };

// Hit marker for showing hit/kill feedback
class HitMarker {
public:
  Mesh mesh;
  Mesh meshBold; // Thicker X for kills
  bool initialized = false;
  float aspectRatio = 16.0f / 9.0f;

  // Current state
  bool showHit = false;
  bool showKill = false;
  float hitTimer = 0.0f;
  float killTimer = 0.0f;
  const float hitDuration = 0.25f;
  const float killDuration = 0.5f;

  void addDiagonalLine(std::vector<STATIC_VERTEX> &verts, std::vector<unsigned int> &inds, float startX,
                       float startY, float endX, float endY, float thickness) {
    float dx = endX - startX;
    float dy = endY - startY;
    float len = sqrtf(dx * dx + dy * dy);
    float nx = -dy / len * thickness;
    float ny = dx / len * thickness * aspectRatio;

    unsigned int base = (unsigned int)verts.size();
    verts.push_back(addVertex(Vec3(startX - nx, startY - ny, 0), Vec3(0, 0, -1), 0, 0));
    verts.push_back(addVertex(Vec3(startX + nx, startY + ny, 0), Vec3(0, 0, -1), 1, 0));
    verts.push_back(addVertex(Vec3(endX - nx, endY - ny, 0), Vec3(0, 0, -1), 0, 1));
    verts.push_back(addVertex(Vec3(endX + nx, endY + ny, 0), Vec3(0, 0, -1), 1, 1));

    inds.push_back(base + 0);
    inds.push_back(base + 1);
    inds.push_back(base + 2);
    inds.push_back(base + 1);
    inds.push_back(base + 3);
    inds.push_back(base + 2);
  }

  void init(Core *core, Shaders *shaders, PSOManager *psos) {
    std::vector<STATIC_VERTEX> vertices;
    std::vector<unsigned int> indices;

    float thickness = 0.0016f;
    float gap = 0.01f;       
    float lineLen = 0.0085f; 

    float diag = 0.7071f; // cos(45) = sin(45) = sqrt(2)/2

    // Top-left line
    addDiagonalLine(vertices, indices, -gap * diag, gap * diag * aspectRatio, -(gap + lineLen) * diag,
                    (gap + lineLen) * diag * aspectRatio, thickness);

    // Top-right line
    addDiagonalLine(vertices, indices, gap * diag, gap * diag * aspectRatio, (gap + lineLen) * diag,
                    (gap + lineLen) * diag * aspectRatio, thickness);

    // Bottom-left line
    addDiagonalLine(vertices, indices, -gap * diag, -gap * diag * aspectRatio, -(gap + lineLen) * diag,
                    -(gap + lineLen) * diag * aspectRatio, thickness);

    // Bottom-right line
    addDiagonalLine(vertices, indices, gap * diag, -gap * diag * aspectRatio, (gap + lineLen) * diag,
                    -(gap + lineLen) * diag * aspectRatio, thickness);

    mesh.init(core, vertices, indices);

    // Bold version for kills
    std::vector<STATIC_VERTEX> verticesBold;
    std::vector<unsigned int> indicesBold;

    float thicknessBold = 0.003f;
    float gapBold = 0.01f;
    float lineLenBold = 0.012f;

    addDiagonalLine(verticesBold, indicesBold, -gapBold * diag, gapBold * diag * aspectRatio, 
                    -(gapBold + lineLenBold) * diag, (gapBold + lineLenBold) * diag * aspectRatio, thicknessBold);
    addDiagonalLine(verticesBold, indicesBold, gapBold * diag, gapBold * diag * aspectRatio, 
                    (gapBold + lineLenBold) * diag, (gapBold + lineLenBold) * diag * aspectRatio, thicknessBold);
    addDiagonalLine(verticesBold, indicesBold, -gapBold * diag, -gapBold * diag * aspectRatio, 
                    -(gapBold + lineLenBold) * diag, -(gapBold + lineLenBold) * diag * aspectRatio, thicknessBold);
    addDiagonalLine(verticesBold, indicesBold, gapBold * diag, -gapBold * diag * aspectRatio, 
                    (gapBold + lineLenBold) * diag, -(gapBold + lineLenBold) * diag * aspectRatio, thicknessBold);

    meshBold.init(core, verticesBold, indicesBold);

    initialized = true;
  }

  void triggerHit() {
    showHit = true;
    hitTimer = hitDuration;
  }

  void triggerKill() {
    showKill = true;
    killTimer = killDuration;
    // Kill overrides hit
    showHit = false;
    hitTimer = 0;
  }

  void update(float dt) {
    if (showHit) {
      hitTimer -= dt;
      if (hitTimer <= 0) {
        showHit = false;
        hitTimer = 0;
      }
    }
    if (showKill) {
      killTimer -= dt;
      if (killTimer <= 0) {
        showKill = false;
        killTimer = 0;
      }
    }
  }

  void draw(Core *core, PSOManager *psos, Shaders *shaders) {
    if (!initialized)
      return;
    if (!showHit && !showKill)
      return;

    Matrix identity;

    if (showKill) {
      shaders->updateConstantVS("UIRed", "staticMeshBuffer", "VP", &identity);
      shaders->updateConstantVS("UIRed", "staticMeshBuffer", "W", &identity);
      shaders->apply(core, "UIRed");
      psos->bind(core, "UIRedPSO");
      meshBold.draw(core);
    } else if (showHit) {
      shaders->updateConstantVS("CrosshairShader", "staticMeshBuffer", "VP", &identity);
      shaders->updateConstantVS("CrosshairShader", "staticMeshBuffer", "W", &identity);
      shaders->apply(core, "CrosshairShader");
      psos->bind(core, "CrosshairPSO");
      mesh.draw(core);
    }
  }
};

// Game UI
class GameUI {
private:
  Mesh barMesh;
  bool initialized = false;
  float aspectRatio = 16.0f / 9.0f;

  void createBarMesh(Core *core, Shaders *shaders, PSOManager *psos) {
    std::vector<STATIC_VERTEX> vertices;
    std::vector<unsigned int> indices;

    vertices.push_back(addVertex(Vec3(0, 0, 0), Vec3(0, 0, -1), 0, 0));
    vertices.push_back(addVertex(Vec3(1, 0, 0), Vec3(0, 0, -1), 1, 0));
    vertices.push_back(addVertex(Vec3(0, 1, 0), Vec3(0, 0, -1), 0, 1));
    vertices.push_back(addVertex(Vec3(1, 1, 0), Vec3(0, 0, -1), 1, 1));

    indices.push_back(0);
    indices.push_back(1);
    indices.push_back(2);
    indices.push_back(1);
    indices.push_back(3);
    indices.push_back(2);

    barMesh.init(core, vertices, indices);

    // Load color shaders
    shaders->load(core, "UIGreen", "VS.txt", "PSUIGreen.txt");
    shaders->load(core, "UIRed", "VS.txt", "PSUIRed.txt");
    shaders->load(core, "UIBlack", "VS.txt", "PSUIBlack.txt");
    shaders->load(core, "UIBlue", "VS.txt", "PSUIBlue.txt");
    shaders->load(core, "UIDarkBlue", "VS.txt", "PSUIDarkBlue.txt");
    shaders->load(core, "UIYellow", "VS.txt", "PSUIYellow.txt");

    psos->createPSO(core, "UIGreenPSO", shaders->find("UIGreen")->vs, shaders->find("UIGreen")->ps, VertexLayoutCache::getStaticLayout());
    psos->createPSO(core, "UIRedPSO", shaders->find("UIRed")->vs, shaders->find("UIRed")->ps, VertexLayoutCache::getStaticLayout());
    psos->createPSO(core, "UIBlackPSO", shaders->find("UIBlack")->vs, shaders->find("UIBlack")->ps, VertexLayoutCache::getStaticLayout());
    psos->createPSO(core, "UIBluePSO", shaders->find("UIBlue")->vs, shaders->find("UIBlue")->ps, VertexLayoutCache::getStaticLayout());
    psos->createPSO(core, "UIDarkBluePSO", shaders->find("UIDarkBlue")->vs, shaders->find("UIDarkBlue")->ps, VertexLayoutCache::getStaticLayout());
    psos->createPSO(core, "UIYellowPSO", shaders->find("UIYellow")->vs, shaders->find("UIYellow")->ps, VertexLayoutCache::getStaticLayout());

    initialized = true;
  }

  void drawRect(Core *core, Shaders *shaders, PSOManager *psos, float x, float y, float width, float height,
                const std::string &shaderName, const std::string &psoName, float zOffset = 0.0f) {
    if (!initialized)
      return;

    Matrix scale = Matrix::scaling(Vec3(width, height * aspectRatio, 1.0f));
    Matrix trans = Matrix::translation(Vec3(x, y, zOffset));
    Matrix w = scale * trans;
    Matrix identity;

    shaders->updateConstantVS(shaderName, "staticMeshBuffer", "VP", &identity);
    shaders->updateConstantVS(shaderName, "staticMeshBuffer", "W", &w);
    shaders->apply(core, shaderName);
    psos->bind(core, psoName);
    barMesh.draw(core);
  }

public:
  void init(Core *core, Shaders *shaders, PSOManager *psos, float aspect = 16.0f / 9.0f) {
    aspectRatio = aspect;
    createBarMesh(core, shaders, psos);
  }

  void drawHealthBar(Core *core, Shaders *shaders, PSOManager *psos, float x, float y, float width, float height, float percent) {
    float t = 0.004f;
    float tY = t / aspectRatio;
    // Black background (border)
    drawRect(core, shaders, psos, x - t, y - tY, width + 2 * t, height + 2 * tY, "UIBlack", "UIBlackPSO", 0.2f);
    // Red background (middle)
    drawRect(core, shaders, psos, x, y, width, height, "UIRed", "UIRedPSO", 0.1f);
    // Green fill (front)
    if (percent > 0.01f) {
      drawRect(core, shaders, psos, x, y, width * percent, height, "UIGreen", "UIGreenPSO", 0.0f);
    }
  }

  void drawAmmoBar(Core *core, Shaders *shaders, PSOManager *psos, float x, float y, float width, float height, float percent) {
    float t = 0.004f;
    float tY = t / aspectRatio;
    drawRect(core, shaders, psos, x - t, y - tY, width + 2 * t, height + 2 * tY, "UIBlack", "UIBlackPSO", 0.2f);
    drawRect(core, shaders, psos, x, y, width, height, "UIRed", "UIRedPSO", 0.1f);
    if (percent > 0.01f) {
      drawRect(core, shaders, psos, x, y, width * percent, height, "UIBlue", "UIBluePSO", 0.0f);
    }
  }

  void drawReserveBar(Core *core, Shaders *shaders, PSOManager *psos, float x, float y, float width, float height, float percent) {
    float t = 0.002f; 
    float tY = t / aspectRatio;
    drawRect(core, shaders, psos, x - t, y - tY, width + 2 * t, height + 2 * tY, "UIBlack", "UIBlackPSO", 0.2f);
    if (percent > 0.01f) {
      drawRect(core, shaders, psos, x, y, width * percent, height, "UIDarkBlue", "UIDarkBluePSO", 0.0f);
    }
  }

  void drawPlayerHealth(Core *core, Shaders *shaders, PSOManager *psos, int health, int maxHealth) {
    float percent = (float)health / (float)maxHealth;
    if (percent < 0)
      percent = 0;
    if (percent > 1)
      percent = 1;
    drawHealthBar(core, shaders, psos, -0.95f, -0.92f, 0.35f, 0.04f, percent);
  }

  void drawAmmo(Core *core, Shaders *shaders, PSOManager *psos, int magazine, int maxMag, int reserve, int maxReserve) {
    float magPercent = (float)magazine / (float)maxMag;
    if (magPercent < 0)
      magPercent = 0;
    if (magPercent > 1)
      magPercent = 1;
    drawAmmoBar(core, shaders, psos, 0.60f, -0.92f, 0.35f, 0.04f, magPercent);

    float reservePercent = (float)reserve / (float)maxReserve;
    if (reservePercent < 0)
      reservePercent = 0;
    if (reservePercent > 1)
      reservePercent = 1;
    drawReserveBar(core, shaders, psos, 0.60f, -0.8425f, 0.35f, 0.015f, reservePercent);
  }

  void drawProgressBar(Core *core, Shaders *shaders, PSOManager *psos, float percent, bool completed) {
    float barWidth = 0.35f;
    float barHeight = 0.015f;
    float x = -barWidth / 2.0f; 
    float y = 0.90f;            

    float t = 0.002f; 
    float tY = t / aspectRatio;

    drawRect(core, shaders, psos, x - t, y - tY, barWidth + 2 * t, barHeight + 2 * tY, "UIBlack", "UIBlackPSO", 0.2f);

    if (percent > 0.01f) {
      if (completed) {
        drawRect(core, shaders, psos, x, y, barWidth * percent, barHeight, "UIGreen", "UIGreenPSO", 0.0f);
      } else {
        drawRect(core, shaders, psos, x, y, barWidth * percent, barHeight, "UIYellow", "UIYellowPSO", 0.0f);
      }
    }
  }

  void drawEnemyHealth(Core *core, Shaders *shaders, PSOManager *psos, Matrix &vp, Vec3 worldPos, int health, 
                       int maxHealth, float offsetY = 2.0f) {
    if (health <= 0)
      return;

    Vec3 aboveHead = worldPos + Vec3(0, offsetY, 0);
    Vec3 screenPos = vp.mulPoint(aboveHead);

    if (screenPos.z < 0 || screenPos.z > 1)
      return;

    float percent = (float)health / (float)maxHealth;
    if (percent < 0)
      percent = 0;
    if (percent > 1)
      percent = 1;

    float barWidth = 0.12f;
    float barHeight = 0.018f;
    float x = screenPos.x - barWidth / 2;
    float y = screenPos.y;

    drawHealthBar(core, shaders, psos, x, y, barWidth, barHeight, percent);
  }
};

struct Bullet {
  float screenX;
  float screenY;
  float progress;
  float lifetime;
  bool active;
};

class BulletSystem {
public:
  std::vector<Bullet> bullets;
  Mesh bulletMesh;
  bool initialized = false;
  float aspectRatio = 16.0f / 9.0f;

  // Screen-space start and end positions
  const float startX = 0.21f;
  const float startY = -0.27f;
  const float endX = 0.0f;
  const float endY = 0.0f;

  void init(Core *core, Shaders *shaders, PSOManager *psos) {
    std::vector<STATIC_VERTEX> vertices;
    std::vector<unsigned int> indices;

    float length = 0.1f;
    float width = 0.002f;
    vertices.push_back(addVertex(Vec3(0, -width, 0), Vec3(0, 0, -1), 0, 0));
    vertices.push_back(addVertex(Vec3(-length, -width, 0), Vec3(0, 0, -1), 1, 0));
    vertices.push_back(addVertex(Vec3(0, width, 0), Vec3(0, 0, -1), 0, 1));
    vertices.push_back(addVertex(Vec3(-length, width, 0), Vec3(0, 0, -1), 1, 1));

    indices.push_back(0);
    indices.push_back(1);
    indices.push_back(2);
    indices.push_back(1);
    indices.push_back(3);
    indices.push_back(2);

    bulletMesh.init(core, vertices, indices);

    shaders->load(core, "BulletShader", "VS.txt", "PSBullet.txt");
    psos->createPSO(core, "BulletPSO", shaders->find("BulletShader")->vs, shaders->find("BulletShader")->ps, VertexLayoutCache::getStaticLayout());

    initialized = true;
  }

  void spawn() {
    Bullet b;
    b.screenX = startX;
    b.screenY = startY;
    b.progress = 0.0f;
    b.lifetime = 0.05f;
    b.active = true;
    bullets.push_back(b);
  }
  HitResult checkHit(Vec3 rayOrigin, Vec3 rayDir, std::vector<EnemyController *> &enemies, std::vector<AABB> &colliders,
                     std::vector<AABB> &sceneColliders, int damage) {
    float maxDist = 1000.0f;
    rayDir = rayDir.normalize();

    for (size_t j = 0; j < enemies.size(); j++) {
      if (enemies[j]->shouldRemove)
        continue;
      if (CollisionSystem::rayIntersectsAABB(rayOrigin, rayDir, colliders[j], maxDist)) {
        Vec3 noKnockback(0, 0, 0);
        // Check health before damage
        int healthBefore = enemies[j]->getHealth();
        enemies[j]->takeDamage(damage, noKnockback);
        int healthAfter = enemies[j]->getHealth();

        // Return KILL if enemy died from this hit
        if (healthBefore > 0 && healthAfter <= 0) {
          return HitResult::KILL;
        }
        return HitResult::HIT;
      }
    }

    return HitResult::NONE;
  }

  void update(float dt) {
    for (auto &b : bullets) {
      if (!b.active)
        continue;

      b.progress += dt / b.lifetime;

      b.screenX = startX + (endX - startX) * b.progress;
      b.screenY = startY + (endY - startY) * b.progress;

      if (b.progress >= 1.0f) {
        b.active = false;
      }
    }

    // Cleanup
    for (auto it = bullets.begin(); it != bullets.end();) {
      if (!it->active) {
        it = bullets.erase(it);
      } else {
        ++it;
      }
    }
  }

  void update(float dt, std::vector<EnemyController *> &enemies, std::vector<AABB> &colliders, 
              std::vector<AABB> &sceneColliders, int damage) {
    update(dt);
  }

  void draw(Core *core, Shaders *shaders, PSOManager *psos, Matrix &vp) {
    if (!initialized || bullets.empty())
      return;

    shaders->apply(core, "BulletShader");
    psos->bind(core, "BulletPSO");

    Matrix identity;
    shaders->updateConstantVS("BulletShader", "staticMeshBuffer", "VP", &identity);

    for (const auto &b : bullets) {
      float dx = endX - b.screenX;
      float dy = endY - b.screenY;

      float angle = -atan2f(dy * aspectRatio, dx) - 0.3f;

      Matrix rot = Matrix::rotateZ(angle);
      Matrix trans = Matrix::translation(Vec3(b.screenX, b.screenY, 0));
      Matrix w = rot * trans;

      shaders->updateConstantVS("BulletShader", "staticMeshBuffer", "W", &w);
      bulletMesh.draw(core);
    }
  }
};

class StaticModel {
public:
  std::vector<Mesh *> meshes;
  std::vector<std::string> textureFilenames;
  std::vector<std::string> normalFilenames;

  std::vector<Matrix> instanceTransforms;
  ID3D12Resource *instanceBuffer = nullptr;
  D3D12_VERTEX_BUFFER_VIEW instanceBufferView;
  int maxInstances = 0;

  void load(Core *core, std::string filename) {
    GEMLoader::GEMModelLoader loader;
    textureFilenames.clear();
    normalFilenames.clear();
    std::vector<GEMLoader::GEMMesh> gemmeshes;
    loader.load(filename, gemmeshes);
    for (int i = 0; i < gemmeshes.size(); i++) {
      Mesh *mesh = new Mesh();
      std::vector<STATIC_VERTEX> vertices;
      for (int j = 0; j < gemmeshes[i].verticesStatic.size(); j++) {
        STATIC_VERTEX v;
        memcpy(&v, &gemmeshes[i].verticesStatic[j], sizeof(STATIC_VERTEX));
        vertices.push_back(v);
      }
      textureFilenames.push_back("Models/Textures/Textures1_ALB.png");
      normalFilenames.push_back("Models/Textures/Textures1_NRM.png");
      mesh->init(core, vertices, gemmeshes[i].indices);
      meshes.push_back(mesh);
    }
  }

  void addInstance(Matrix transform) {
    instanceTransforms.push_back(transform);
  }

  void clearInstances() { instanceTransforms.clear(); }

  void uploadInstances(Core *core) {
    if (instanceTransforms.empty())
      return;

    int numInstances = instanceTransforms.size();
    int bufferSize = numInstances * sizeof(Matrix);

    if (instanceBuffer == nullptr || numInstances > maxInstances) {
      if (instanceBuffer)
        instanceBuffer->Release();

      maxInstances = numInstances + 100;
      int newSize = maxInstances * sizeof(Matrix);

      D3D12_HEAP_PROPERTIES heapProps = {};
      heapProps.Type = D3D12_HEAP_TYPE_UPLOAD;
      heapProps.CreationNodeMask = 1;
      heapProps.VisibleNodeMask = 1;

      D3D12_RESOURCE_DESC bufferDesc = {};
      bufferDesc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
      bufferDesc.Width = newSize;
      bufferDesc.Height = 1;
      bufferDesc.DepthOrArraySize = 1;
      bufferDesc.MipLevels = 1;
      bufferDesc.SampleDesc.Count = 1;
      bufferDesc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;

      core->device->CreateCommittedResource(
          &heapProps, D3D12_HEAP_FLAG_NONE, &bufferDesc,
          D3D12_RESOURCE_STATE_GENERIC_READ, nullptr,
          IID_PPV_ARGS(&instanceBuffer));
    }

    void *mappedData;
    instanceBuffer->Map(0, nullptr, &mappedData);
    memcpy(mappedData, instanceTransforms.data(), bufferSize);
    instanceBuffer->Unmap(0, nullptr);

    instanceBufferView.BufferLocation = instanceBuffer->GetGPUVirtualAddress();
    instanceBufferView.SizeInBytes = bufferSize;
    instanceBufferView.StrideInBytes = sizeof(Matrix);
  }

  void drawInstanced(Core* core, PSOManager* psos, Shaders* shaders, Matrix& vp, TextureManager* textures, LightData& lightData, float time = 0.0f, std::string shaderName = "StaticModelNormalMapped") {
      if (instanceTransforms.empty())
          return;

      std::string psoName = shaderName + "PSO";

      shaders->updateConstantVS(shaderName, "SceneConstantBuffer", "VP", &vp);

      if (shaderName == "GrassShader") {
          shaders->updateConstantVS(shaderName, "SceneConstantBuffer", "Time", &time);
      }

      shaders->updateConstantPS(shaderName, "LightBuffer", "cameraPos", &lightData.cameraPos);
      shaders->updateConstantPS(shaderName, "LightBuffer", "lightDir", &lightData.lightDir);
      shaders->updateConstantPS(shaderName, "LightBuffer", "lightColor", &lightData.lightColor);
      shaders->updateConstantPS(shaderName, "LightBuffer", "ambientStrength", &lightData.ambientStrength);

      shaders->apply(core, shaderName);
      psos->bind(core, psoName);

      auto commandList = core->getCommandList();
      commandList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

      for (int i = 0; i < meshes.size(); i++) {
          commandList->IASetVertexBuffers(0, 1, &meshes[i]->vbView);
          commandList->IASetVertexBuffers(1, 1, &instanceBufferView);
          commandList->IASetIndexBuffer(&meshes[i]->ibView);

          shaders->updateTexturePS(core, shaderName, "tex", textures->getHeapOffset(textureFilenames[i], core));
          commandList->DrawIndexedInstanced(meshes[i]->numMeshIndices, instanceTransforms.size(), 0, 0, 0);
      }
  }


  ~StaticModel() {
    if (instanceBuffer) {
      instanceBuffer->Release();
      instanceBuffer = nullptr;
    }
    for (auto m : meshes) {
      delete m;
    }
    meshes.clear();
  }
};

class AnimatedModel {
public:
  std::vector<Mesh *> meshes;
  Animation animation;
  std::vector<std::string> textureFilenames;
  std::vector<std::string> normalFilenames;

  void load(Core *core, std::string filename, PSOManager *psos, Shaders *shaders) {
    GEMLoader::GEMModelLoader loader;
    std::vector<GEMLoader::GEMMesh> gemmeshes;
    textureFilenames.clear();
    normalFilenames.clear();

    GEMLoader::GEMAnimation gemanimation;
    loader.load(filename, gemmeshes, gemanimation);
    std::cout << "Loading: " << filename << std::endl;

    for (int i = 0; i < gemmeshes.size(); i++) {
      Mesh *mesh = new Mesh();
      std::vector<ANIMATED_VERTEX> vertices;
      for (int j = 0; j < gemmeshes[i].verticesAnimated.size(); j++) {
        ANIMATED_VERTEX v;
        memcpy(&v, &gemmeshes[i].verticesAnimated[j], sizeof(ANIMATED_VERTEX));
        vertices.push_back(v);
      }

      std::string texName = gemmeshes[i].material.find("albedo").getValue();

      if (filename.find("Duck-mixed") != std::string::npos || filename.find("Bull-dark") != std::string::npos ||
          filename.find("Goat-01") != std::string::npos || filename.find("Pig") != std::string::npos) {
        texName = "T_Animalstextures_alb.png";
      } else if (filename.find("AutomaticCarbine") != std::string::npos) {
        if (texName.find("arms") != std::string::npos)
          texName = "arms_1_Albedo_alb.png";
        else if (texName.find("Collimator") != std::string::npos)
          texName = "AC5_Collimator_Albedo_alb.png";
        else if (texName.find("Glass") != std::string::npos)
          texName = "AC5_Collimator_Glass_Albedo_alb.png";
        else if (texName.find("Bullet") != std::string::npos || texName.find("Shell") != std::string::npos)
          texName = "AC5_Bullet_Shell_Albedo_alb.png";
        else
          texName = "AC5_Albedo_alb.png";
      } else {
        if (texName.length() > 0 && texName.find(".png") == std::string::npos) {
          texName += ".png";
        }
      }

      std::string normName = texName;
      size_t pos = normName.find("_alb");
      if (pos != std::string::npos)
        normName.replace(pos, 4, "_nrm");
      else {
        pos = normName.find("_ALB");
        if (pos != std::string::npos)
          normName.replace(pos, 4, "_NRM");
        else {
          pos = normName.find_last_of(".");
          if (pos != std::string::npos)
            normName.insert(pos, "_nrm");
        }
      }

      textureFilenames.push_back("Models/Textures/" + texName);
      normalFilenames.push_back("Models/Textures/" + normName);

      mesh->init(core, vertices, gemmeshes[i].indices);
      meshes.push_back(mesh);
    }

    shaders->load(core, "AnimatedNormalMapped", "VSAnim.txt", "PSNormalMap.txt");

    psos->createPSO(core, "AnimatedNormalMappedPSO", shaders->find("AnimatedNormalMapped")->vs, shaders->find("AnimatedNormalMapped")->ps, VertexLayoutCache::getAnimatedLayout());

    animation.skeleton.bones.clear();
    animation.animations.clear();
    memcpy(&animation.skeleton.globalInverse, &gemanimation.globalInverse, 16 * sizeof(float));
    for (int i = 0; i < gemanimation.bones.size(); i++) {
      Bone bone;
      bone.name = gemanimation.bones[i].name;
      memcpy(&bone.offset, &gemanimation.bones[i].offset, 16 * sizeof(float));
      bone.parentIndex = gemanimation.bones[i].parentIndex;
      animation.skeleton.bones.push_back(bone);
    }
    for (int i = 0; i < gemanimation.animations.size(); i++) {
      std::string name = gemanimation.animations[i].name;
      AnimationSequence aseq;
      aseq.ticksPerSecond = gemanimation.animations[i].ticksPerSecond;
      for (int j = 0; j < gemanimation.animations[i].frames.size(); j++) {
        AnimationFrame frame;
        for (int index = 0; index < gemanimation.animations[i].frames[j].positions.size(); index++) {
          Vec3 p;
          Quaternion q;
          Vec3 s;
          memcpy(&p, &gemanimation.animations[i].frames[j].positions[index], sizeof(Vec3));
          frame.positions.push_back(p);
          memcpy(&q, &gemanimation.animations[i].frames[j].rotations[index], sizeof(Quaternion));
          frame.rotations.push_back(q);
          memcpy(&s, &gemanimation.animations[i].frames[j].scales[index], sizeof(Vec3));
          frame.scales.push_back(s);
        }
        aseq.frames.push_back(frame);
      }
      animation.animations.insert({name, aseq});
    }
  }

  void draw(Core *core, PSOManager *psos, Shaders *shaders, AnimationInstance *instance, Matrix &vp, Matrix &w,
            TextureManager *textures, LightData &lightData) {
    psos->bind(core, "AnimatedNormalMappedPSO");

    std::string shaderName = "AnimatedNormalMapped";

    shaders->updateConstantVS(shaderName, "staticMeshBuffer", "W", &w);
    shaders->updateConstantVS(shaderName, "staticMeshBuffer", "VP", &vp);
    shaders->updateConstantVS(shaderName, "staticMeshBuffer", "bones", instance->matrices);
    shaders->updateConstantPS(shaderName, "LightBuffer", "cameraPos", &lightData.cameraPos);
    shaders->updateConstantPS(shaderName, "LightBuffer", "lightDir", &lightData.lightDir);
    shaders->updateConstantPS(shaderName, "LightBuffer", "lightColor", &lightData.lightColor);
    shaders->updateConstantPS(shaderName, "LightBuffer", "ambientStrength", &lightData.ambientStrength);

    shaders->apply(core, shaderName);
    for (int i = 0; i < meshes.size(); i++) {
      shaders->updateTexturePS(core, shaderName, "tex", textures->getHeapOffset(textureFilenames[i], core));
      meshes[i]->draw(core);
    }
  }
  ~AnimatedModel() {
    for (auto m : meshes) {
      delete m;
    }
    meshes.clear();
  }
};

class Skybox {
public:
  Mesh mesh;
  struct SimpleVertex {
    Vec3 pos;
    float u, v;
  };

  void init(Core *core, Shaders *shaders, PSOManager *psos, TextureManager *textures, std::string texturePath) {
    std::vector<SimpleVertex> vertices;
    std::vector<unsigned int> indices;

    const int stackCount = 20;
    const int sliceCount = 20;
    float radius = 500.0f;

    for (int i = 0; i <= stackCount; ++i) {
      float phi = 3.14159f * float(i) / float(stackCount);
      for (int j = 0; j <= sliceCount; ++j) {
        float theta = 2.0f * 3.14159f * float(j) / float(sliceCount);

        SimpleVertex v;
        v.pos.x = radius * sinf(phi) * cosf(theta);
        v.pos.y = radius * cosf(phi);
        v.pos.z = radius * sinf(phi) * sinf(theta);
        v.u = float(j) / sliceCount;
        v.v = float(i) / stackCount;
        vertices.push_back(v);
      }
    }

    for (int i = 0; i < stackCount; ++i) {
      for (int j = 0; j < sliceCount; ++j) {
        indices.push_back(i * (sliceCount + 1) + j);
        indices.push_back((i + 1) * (sliceCount + 1) + j);
        indices.push_back((i + 1) * (sliceCount + 1) + (j + 1));

        indices.push_back(i * (sliceCount + 1) + j);
        indices.push_back((i + 1) * (sliceCount + 1) + (j + 1));
        indices.push_back(i * (sliceCount + 1) + (j + 1));
      }
    }

    std::vector<STATIC_VERTEX> staticVertices;
    for (auto &sv : vertices) {
      STATIC_VERTEX v;
      v.pos = sv.pos;
      v.normal = -sv.pos.normalize();
      v.tu = sv.u;
      v.tv = sv.v;
      staticVertices.push_back(v);
    }
    mesh.init(core, staticVertices, indices);

    shaders->load(core, "SkyboxShader", "VSSky.txt", "PSSky.txt");

    D3D12_INPUT_LAYOUT_DESC layout = VertexLayoutCache::getStaticLayout();

    psos->createPSO(core, "SkyboxPSO", shaders->find("SkyboxShader")->vs, shaders->find("SkyboxShader")->ps, layout);

    textures->getTexture(texturePath, core);
  }

  void draw(Core *core, PSOManager *psos, Shaders *shaders, TextureManager *textures, Camera &camera, int width, 
            int height, std::string texturePath) {
    Matrix view = camera.getViewMatrix();
    view.m[3] = 0;
    view.m[7] = 0;
    view.m[11] = 0;

    Matrix proj = Matrix::perspective(0.01f, 10000.0f, (float)width / (float)height, 60.0f);
    Matrix world = Matrix::scaling(Vec3(1, 1, 1));

    Matrix wvp = world * view * proj;
    shaders->updateConstantVS("SkyboxShader", "SkyBuffer", "WVP", &wvp);
    shaders->apply(core, "SkyboxShader");
    psos->bind(core, "SkyboxPSO");

    shaders->updateTexturePS(core, "SkyboxShader", "tex", textures->getHeapOffset(texturePath, core));

    mesh.draw(core);
  }

  ~Skybox() {}
};

// menu/victory/fail screens
class FullScreenUI {
public:
  Mesh mesh;
  bool initialized = false;

  void init(Core *core, Shaders *shaders, PSOManager *psos) {
    if (initialized)
      return;

    std::vector<STATIC_VERTEX> vertices;
    std::vector<unsigned int> indices;

    STATIC_VERTEX v;
    v.normal = Vec3(0, 0, -1);

    // Bottom-left
    v.pos = Vec3(-1, -1, 0);
    v.tu = 0;
    v.tv = 1;
    vertices.push_back(v);
    // Bottom-right
    v.pos = Vec3(1, -1, 0);
    v.tu = 1;
    v.tv = 1;
    vertices.push_back(v);
    // Top-right
    v.pos = Vec3(1, 1, 0);
    v.tu = 1;
    v.tv = 0;
    vertices.push_back(v);
    // Top-left
    v.pos = Vec3(-1, 1, 0);
    v.tu = 0;
    v.tv = 0;
    vertices.push_back(v);

    indices = {0, 2, 1, 0, 3, 2};

    mesh.init(core, vertices, indices);
    initialized = true;
  }

  void draw(Core *core, Shaders *shaders, PSOManager *psos, TextureManager *textures, const std::string &texturePath) {
    if (!initialized)
      return;

    Texture *tex = textures->getTexture(texturePath, core);
    int heapOffset = tex->heapOffset;

    Matrix identity;
    identity.identity();

    shaders->updateConstantVS("SkyboxShader", "SkyBuffer", "WVP", &identity);
    shaders->apply(core, "SkyboxShader");
    psos->bind(core, "SkyboxPSO");

    D3D12_GPU_DESCRIPTOR_HANDLE texHandle = core->srvHeap.gpuHandle;
    texHandle.ptr += (UINT64)heapOffset * (UINT64)core->srvHeap.incrementSize;
    core->getCommandList()->SetGraphicsRootDescriptorTable(2, texHandle);

    mesh.draw(core);
  }
};