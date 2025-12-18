#include "Animation.h"
#include "Camera.h"
#include "Collision.h"
#include "Controller.h"
#include "Core.h"
#include "LevelLoader.h"
#include "Maths.h"
#include "Mesh.h"
#include "Model.h"
#include "PSO.h"
#include "Shaders.h"
#include "Sounds.h"
#include "Texture.h"
#include "Timer.h"
#include "Window.h"
#include <d3dcompiler.h>
#include <fstream>

#pragma comment(lib, "d3dcompiler.lib")
#define WIDTH 1920
#define HEIGHT 1080
int WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, PSTR lpCmdLine, int nCmdShow) {
  Window window;
  window.create(WIDTH, HEIGHT, "Escape from TakoFarm");

  // Game state management
  enum class GameState { MENU, PLAYING, VICTORY, FAIL };
  GameState gameState = GameState::MENU;
  enum class TaskMode { NONE, TASK_1, TASK_2 };
  TaskMode currentTask = TaskMode::NONE;
  int killCount = 0;
  int killTarget = 9999;

  // Generator System for Task 2
  struct Generator {
    Vec3 position;
    float timer = 0.0f;
    bool isCounting = false;
    bool isCompleted = false;
    AABB collider;
  };
  std::vector<Generator> task2Generators;

  // Task progress tracking 
  float taskProgress = 0.0f;
  bool taskProgressCompletePlayed = false; 

  Core core;
  core.init(window.hwnd, WIDTH, HEIGHT);
  Shaders shaders;
  PSOManager psos;
  TextureManager textures;
  shaders.load(&core, "StaticModelNormalMapped", "VSInstance.txt",  "PSNormalMap.txt");
  psos.createPSO(&core, "StaticModelNormalMappedPSO", shaders.find("StaticModelNormalMapped")->vs, shaders.find("StaticModelNormalMapped")->ps, VertexLayoutCache::getInstancedLayout());
  shaders.load(&core, "AnimatedNormalMapped", "VSAnim.txt", "PSNormalMap.txt");
  psos.createPSO(&core, "AnimatedNormalMappedPSO", shaders.find("AnimatedNormalMapped")->vs, shaders.find("AnimatedNormalMapped")->ps, VertexLayoutCache::getAnimatedLayout());
  shaders.load(&core, "AnimatedUntextured", "VSAnim.txt", "PSUntextured.txt");

  shaders.load(&core, "GrassShader", "VSGrass.txt", "PSNormalMap.txt"); 
  psos.createPSO(&core, "GrassShaderPSO", shaders.find("GrassShader")->vs, shaders.find("GrassShader")->ps, VertexLayoutCache::getInstancedLayout());
 
  Skybox skybox;
  skybox.init(&core, &shaders, &psos, &textures, "Models/Textures/sky_25_2k.png");
  std::vector<std::string> staticModelNames = {
      "acacia_003",      "acacia_006",   "barrel_003",
      "barrier_001",     "barrier_004",  "box_003",
      "box_004",         "box_020",      "box_023",
      "machine_gun_005", "mortar_001",   "grass_003",
      "ground_002",      "ground_008",   "barracks_001",
      "barrier_002",     "barrier_003",  "cactus_005",
      "cart_001",        "coil_001",     "construction_001",
      "container_004",   "deadwood_007", "generator_002",
      "log_001",         "obstacle_001", "protection_001",
      "rock_003",        "table_001",    "tree_017",
      "Wall_003",        "Wall_020",     "helicopter_platform_001"};
  std::map<std::string, StaticModel *> staticModels;
  std::vector<AABB> sceneColliders;
  std::vector<AABB> enemySceneColliders; 
  std::vector<std::pair<std::string, Vec3>> staticModelPositions;

  // Load all static models 
  for (const auto &name : staticModelNames) {
    StaticModel *model = new StaticModel();
    model->load(&core, "Models/" + name + ".gem");

    for (size_t i = 0; i < model->textureFilenames.size(); i++) {
      textures.getTexture(model->textureFilenames[i], &core);
      textures.getTexture(model->normalFilenames[i], &core);
    }

    staticModels[name] = model;
  }

  // Load level from file
  LevelLoader levelLoader;
  if (levelLoader.load("level.txt")) {
    // Create instances from level data
    for (const auto &obj : levelLoader.objects) {
      auto it = staticModels.find(obj.modelName);
      if (it == staticModels.end()) {
        continue; // Model not found
      }

      StaticModel *model = it->second;
      Matrix scale = Matrix::scaling(Vec3(obj.scale, obj.scale, obj.scale));
      Matrix rot = Matrix::rotateY(obj.rotation * 3.14159f / 180.0f);
      Matrix trans = Matrix::translation(obj.position);

      // Will be rendered separately for explosion control
      if (obj.modelName != "barrel_003") {
        model->addInstance(scale * rot * trans);
      }

      // Add collision if enabled
      if (obj.hasCollision) {
        // Handle rotated collision boxes 
        float rotRad = obj.rotation * 3.14159f / 180.0f;
        bool isRotated90 = (fabs(fmod(obj.rotation, 180.0f) - 90.0f) < 1.0f);
        AABB colliderAABB;

        if (isRotated90) {
          auto it = StaticModelBounds.find(obj.modelName);
          if (it != StaticModelBounds.end()) {
            Vec3 extent = it->second.toVec3();
            Vec3 swappedExtent(extent.z, extent.y, extent.x); 
            Vec3 center = obj.position + Vec3(0, extent.y, 0);
            colliderAABB = AABB::fromCenterExtent(center, swappedExtent);
          } else {
            colliderAABB = getStaticModelAABB(obj.modelName, obj.position);
          }
        } else {
          colliderAABB = getStaticModelAABB(obj.modelName, obj.position);
        }

        // Add to player colliders 
        sceneColliders.push_back(colliderAABB);

        // Add to enemy colliders 
        if (obj.modelName != "Wall_003") {
          enemySceneColliders.push_back(colliderAABB);
        }

        staticModelPositions.push_back({obj.modelName, obj.position});
      }
    }
  } else {
    // Place ground only if level file not found
    Matrix scale = Matrix::scaling(Vec3(0.01f, 0.01f, 0.01f));
    Vec3 pos(0.0f, -1.0f, 0.0f);
    Matrix trans = Matrix::translation(pos);
    staticModels["ground_007"]->addInstance(scale * trans);
    sceneColliders.push_back(AABB(Vec3(-50, -1.0f, -50), Vec3(50, -0.5f, 50)));
  }

  // Upload all instances
  for (auto &pair : staticModels) {
    pair.second->uploadInstances(&core);
  }

  // Invisible boundary walls 
  AABB leftBoundary(Vec3(-50, -20, -50), Vec3(-22, 50, 50));
  sceneColliders.push_back(leftBoundary);
  enemySceneColliders.push_back(leftBoundary);

  AABB rightBoundaryPlayer(Vec3(22, -20, -50), Vec3(50, 50, 50));
  sceneColliders.push_back(rightBoundaryPlayer);
  AABB rightBoundaryEnemy(Vec3(22, -20, -50), Vec3(50, 50, 20));
  enemySceneColliders.push_back(rightBoundaryEnemy);

  AABB frontBoundaryPlayer(Vec3(-50, -20, -50), Vec3(50, 50, -21));
  sceneColliders.push_back(frontBoundaryPlayer);
  AABB frontBoundaryEnemy(Vec3(-12, -20, -50), Vec3(50, 50, -21));
  enemySceneColliders.push_back(frontBoundaryEnemy);

  AABB backBoundaryPlayer(Vec3(-50, -20, 25), Vec3(50, 50, 50));
  sceneColliders.push_back(backBoundaryPlayer);
  AABB backBoundaryEnemy(Vec3(-50, -20, 25), Vec3(12, 50, 50));
  enemySceneColliders.push_back(backBoundaryEnemy);

  // Spawn zone box (only for enemies) 
  enemySceneColliders.push_back(AABB(Vec3(-35, -20, -45), Vec3(-30, 50, -21))); 
  enemySceneColliders.push_back(AABB(Vec3(-12, -20, -45), Vec3(-8, 50, -21))); 
  enemySceneColliders.push_back(AABB(Vec3(-35, -20, -45), Vec3(-8, 50, -40))); 
  enemySceneColliders.push_back(AABB(Vec3(8, -20, 25), Vec3(12, 50, 45))); 
  enemySceneColliders.push_back(AABB(Vec3(30, -20, 25), Vec3(35, 50, 45))); 
  enemySceneColliders.push_back(AABB(Vec3(8, -20, 40), Vec3(35, 50, 45))); 

  AABB playerLocalAABB(Vec3(-0.3f, 0.0f, -0.3f), Vec3(0.3f, 1.8f, 0.3f));
  auto loadAnimated = [&](AnimatedModel &model, std::string path, float speed, AnimationInstance &inst, AnimationController &ctrl) {
    model.load(&core, path, &psos, &shaders);
    for (size_t i = 0; i < model.textureFilenames.size(); i++) {
      textures.getTexture(model.textureFilenames[i], &core);
      textures.getTexture(model.normalFilenames[i], &core);
    }
    inst.init(&model.animation, 0);
    ctrl.init(&inst, speed);
  };
  AnimatedModel goatModel, pigModel, bullModel, duckModel, gunModel;
  // Create enemy pool (max 50) 
  const int MAX_ENEMIES = 50;
  std::vector<AnimationInstance> goatInstPool(MAX_ENEMIES);
  std::vector<AnimationInstance> pigInstPool(MAX_ENEMIES);
  std::vector<AnimationInstance> bullInstPool(MAX_ENEMIES);
  std::vector<AnimationInstance> duckInstPool(MAX_ENEMIES);
  std::vector<AnimationController> goatCtrlPool(MAX_ENEMIES);
  std::vector<AnimationController> pigCtrlPool(MAX_ENEMIES);
  std::vector<AnimationController> bullCtrlPool(MAX_ENEMIES);
  std::vector<AnimationController> duckCtrlPool(MAX_ENEMIES);

  AnimationInstance gunInst;
  GunAnimationController gunCtrl;

  // Load animated models
  auto loadAnimatedModel = [&](AnimatedModel &model, std::string path) {
    model.load(&core, path, &psos, &shaders);
    for (size_t i = 0; i < model.textureFilenames.size(); i++) {
      textures.getTexture(model.textureFilenames[i], &core);
      textures.getTexture(model.normalFilenames[i], &core);
    }
  };

  loadAnimatedModel(goatModel, "Models/Goat-01.gem");
  loadAnimatedModel(pigModel, "Models/Pig.gem");
  loadAnimatedModel(bullModel, "Models/Bull-dark.gem");
  loadAnimatedModel(duckModel, "Models/Duck-mixed.gem");

  // Initialize animation instance pools
  for (int i = 0; i < MAX_ENEMIES; i++) {
    goatInstPool[i].init(&goatModel.animation, 0);
    goatCtrlPool[i].init(&goatInstPool[i], 3.0f);
    pigInstPool[i].init(&pigModel.animation, 0);
    pigCtrlPool[i].init(&pigInstPool[i], 3.0f);
    bullInstPool[i].init(&bullModel.animation, 0);
    bullCtrlPool[i].init(&bullInstPool[i], 3.0f);
    duckInstPool[i].init(&duckModel.animation, 0);
    duckCtrlPool[i].init(&duckInstPool[i], 3.0f);
  }

  gunModel.load(&core, "Models/AutomaticCarbine.gem", &psos, &shaders);
  for (size_t i = 0; i < gunModel.textureFilenames.size(); i++) {
    textures.getTexture(gunModel.textureFilenames[i], &core);
    textures.getTexture(gunModel.normalFilenames[i], &core);
  }
  gunInst.init(&gunModel.animation, 0);
  gunCtrl.init(&gunInst);

  // Enemy data pool
  std::vector<AnimalData> goatDataPool(MAX_ENEMIES);
  std::vector<AnimalData> pigDataPool(MAX_ENEMIES);
  std::vector<AnimalData> bullDataPool(MAX_ENEMIES);
  std::vector<AnimalData> duckDataPool(MAX_ENEMIES);

  // Enemy AI pools
  std::vector<EnemyController> goatAIPool(MAX_ENEMIES);
  std::vector<EnemyController> pigAIPool(MAX_ENEMIES);
  std::vector<EnemyController> bullAIPool(MAX_ENEMIES);
  std::vector<EnemyController> duckAIPool(MAX_ENEMIES);
  std::vector<Vec3> goatPosPool(MAX_ENEMIES);
  std::vector<Vec3> pigPosPool(MAX_ENEMIES);
  std::vector<Vec3> bullPosPool(MAX_ENEMIES);
  std::vector<Vec3> duckPosPool(MAX_ENEMIES);
  std::vector<bool> goatActivePool(MAX_ENEMIES, false);
  std::vector<bool> pigActivePool(MAX_ENEMIES, false);
  std::vector<bool> bullActivePool(MAX_ENEMIES, false);
  std::vector<bool> duckActivePool(MAX_ENEMIES, false);

  // Spawn points
  Vec3 spawnFrontLeft(-18, 0, -28);
  Vec3 spawnBackRight(18, 0, 28);

  // Wave spawning 
  float gameTimer = 0.0f;
  float spawnTimer = 0.0f;
  int spawnWave = 0; // 0 = back spawn, 1 = front spawn
  bool gameStarted = false;
  const float GAME_START_DELAY = 2.0f;
  const float BACK_TO_FRONT_INTERVAL = 4.0f; 
  const float FRONT_TO_BACK_INTERVAL = 6.0f; 
  int nextGoatIdx = 0, nextPigIdx = 0, nextBullIdx = 0, nextDuckIdx = 0;

  // Spawn enemy with entry target 
  auto spawnEnemy = [&](const std::string &type, Vec3 pos, Vec3 entryTarget) {
    if (type == "duck" && nextDuckIdx < MAX_ENEMIES) {
      int idx = nextDuckIdx++;
      duckPosPool[idx] = pos;
      duckDataPool[idx] = AnimalData(40, 5, 2.0f, 10.0f);
      duckAIPool[idx].init(&duckInstPool[idx], &duckDataPool[idx], pos, true, entryTarget);
      duckActivePool[idx] = true;
    } else if (type == "goat" && nextGoatIdx < MAX_ENEMIES) {
      int idx = nextGoatIdx++;
      goatPosPool[idx] = pos;
      goatDataPool[idx] = AnimalData(70, 10, 3.0f, 8.0f);
      goatAIPool[idx].init(&goatInstPool[idx], &goatDataPool[idx], pos, false, entryTarget);
      goatActivePool[idx] = true;
    } else if (type == "pig" && nextPigIdx < MAX_ENEMIES) {
      int idx = nextPigIdx++;
      pigPosPool[idx] = pos;
      pigDataPool[idx] = AnimalData(130, 10, 4.0f, 6.0f);
      pigAIPool[idx].init(&pigInstPool[idx], &pigDataPool[idx], pos, false, entryTarget);
      pigActivePool[idx] = true;
    } else if (type == "bull" && nextBullIdx < MAX_ENEMIES) {
      int idx = nextBullIdx++;
      bullPosPool[idx] = pos;
      bullDataPool[idx] = AnimalData(100, 20, 3.5f, 7.5f);
      bullAIPool[idx].init(&bullInstPool[idx], &bullDataPool[idx], pos, false, entryTarget);
      bullActivePool[idx] = true;
    }
  };

  Vec3 entryTargetFront(-18, 0, -18); 
  Vec3 entryTargetBack(18, 0, 18);    

  std::vector<AABB> animalColliders;

  // Explosive barrels
  struct ExplosiveBarrel {
    Vec3 position;
    int health = 60;
    bool isActive = true;
    AABB collider;
  };
  std::vector<ExplosiveBarrel> explosiveBarrels;

  // Find all barrel_003 positions from level data
  for (const auto &pair : staticModelPositions) {
    if (pair.first == "barrel_003") {
      ExplosiveBarrel barrel;
      barrel.position = pair.second;
      barrel.health = 60;
      barrel.isActive = true;
      barrel.collider = getStaticModelAABB("barrel_003", pair.second);
      explosiveBarrels.push_back(barrel);
    }
  }

  AABB victoryPlatformAABB;
  bool foundVictoryPlatform = false;
  for (const auto &pair : staticModelPositions) {
    if (pair.first == "helicopter_platform_001") {
      victoryPlatformAABB =
          getStaticModelAABB("helicopter_platform_001", pair.second);
      foundVictoryPlatform = true;
    }
  }

  Crosshair crosshair;
  crosshair.init(&core, &shaders, &psos);
  GameUI gameUI;
  gameUI.init(&core, &shaders, &psos, (float)WIDTH / (float)HEIGHT);
  BulletSystem bulletSystem;
  bulletSystem.init(&core, &shaders, &psos);
  HitMarker hitMarker;
  hitMarker.init(&core, &shaders, &psos);
  SoundManager soundManager;
  soundManager.load("Resources/hit.wav");
  soundManager.load("Resources/Fire.wav");
  soundManager.load("Resources/Reload.wav");
  soundManager.load("Resources/DryFire.wav");
  soundManager.load("Resources/enemyAttack.wav");
  soundManager.load("Resources/playerHurt.wav");
  soundManager.load("Resources/kill.wav");
  soundManager.load("Resources/Melee.wav");
  soundManager.load("Resources/jump.wav");
  soundManager.load("Resources/step.wav");
  soundManager.load("Resources/heal.wav");
  soundManager.load("Resources/pickup.wav");
  soundManager.load("Resources/explosion.wav");
  soundManager.load("Resources/generator.wav");
  soundManager.load("Resources/click.wav");
  soundManager.load("Resources/finish.wav");
  soundManager.loadMusic("Resources/music.wav");
  soundManager.playMusic();
  LightData lightData;
  lightData.lightDir = Vec3(0.5f, -1.0f, 0.5f).normalize();
  lightData.lightColor = Vec3(1.0f, 0.95f, 0.8f);
  lightData.ambientStrength = 0.3f;
  Camera camera;
  Timer timer;
  float t = 0;

  FullScreenUI fullScreenUI;
  fullScreenUI.init(&core, &shaders, &psos);
  textures.getTexture("Resources/fail.png", &core);
  textures.getTexture("Resources/victory.png", &core);
  textures.getTexture("Resources/menu.png", &core);

  // Player data
  int playerHealth = 100;
  int maxPlayerHealth = 100;

  // Pickup interaction data
  Vec3 healBoxPos, ammoBoxPos;
  bool foundHealBox = false, foundAmmoBox = false;
  for (const auto &pair : staticModelPositions) {
    if (pair.first == "box_003") {
      healBoxPos = pair.second;
      foundHealBox = true;
    }
    if (pair.first == "box_004") {
      ammoBoxPos = pair.second;
      foundAmmoBox = true;
    }
  }

  // Find generator_002 positions for interaction
  for (const auto &pair : staticModelPositions) {
    if (pair.first == "generator_002") {
      Generator gen;
      gen.position = pair.second;
      gen.collider = getStaticModelAABB("generator_002", pair.second);
      gen.timer = 0.0f;
      gen.isCounting = false;
      gen.isCompleted = false;
      task2Generators.push_back(gen);
    }
  }

  float healCooldown = 0.0f; // 15s cooldown for box_003 (heal)
  float ammoCooldown = 0.0f; // 15s cooldown for box_004 (ammo)
  bool prevKeyE = false;     // Edge detection for E key

  while (1) {
    core.beginFrame();
    float dt = timer.dt();
    window.checkInput();

    // Get mouse position for UI click detection
    POINT mousePos;
    GetCursorPos(&mousePos);
    ScreenToClient(window.hwnd, &mousePos);
    float mouseX = (float)mousePos.x / WIDTH;
    float mouseY = (float)mousePos.y / HEIGHT;
    static bool prevMouseClicked = false;
    bool mouseDown = (window.mouseButtons[0] == 1);
    bool mouseClicked = mouseDown && !prevMouseClicked;
    prevMouseClicked = mouseDown;

    if (gameState == GameState::MENU) {
      // ESC exits game from menu
      if (window.keys[VK_ESCAPE] == 1)
        break;

      // Draw menu UI
      core.beginRenderPass();
      fullScreenUI.draw(&core, &shaders, &psos, &textures,  "Resources/menu.png");
      core.finishFrame();

      bool startGame = false;
      bool loadGame = false;

      if (mouseClicked && mouseX >= 0.40f && mouseX <= 0.60f) {
        if (mouseY >= 0.43f && mouseY <= 0.52f) {
          // TASK 1 Clicked
          soundManager.play("Resources/click.wav");
          currentTask = TaskMode::TASK_1;
          killTarget = 40;
          startGame = true;
        } else if (mouseY >= 0.56f && mouseY <= 0.65f) {
          // TASK 2 Clicked
          soundManager.play("Resources/click.wav");
          currentTask = TaskMode::TASK_2;
          killTarget = 9999; // Kills don't matter for Task 2 victory 
          startGame = true;
        } else if (mouseY >= 0.70f && mouseY <= 0.80f) {
          // LOAD GAME Clicked
          soundManager.play("Resources/click.wav");
          loadGame = true;
        }
      }

      if (startGame) {
        gameState = GameState::PLAYING;
        // Hide cursor for gameplay
        while (ShowCursor(FALSE) >= 0);
        // Reset game state
        killCount = 0;
        playerHealth = 100;
        gameTimer = 0.0f;
        spawnTimer = 0.0f;
        spawnWave = 0;
        gameStarted = false;
        // Reset enemy pools
        nextGoatIdx = 0;
        nextPigIdx = 0;
        nextBullIdx = 0;
        nextDuckIdx = 0;
        for (int i = 0; i < MAX_ENEMIES; i++) {
          goatActivePool[i] = false;
          pigActivePool[i] = false;
          bullActivePool[i] = false;
          duckActivePool[i] = false;
        }
        // Reset player position and gun state
        camera.position = Vec3(0, 1.5f, 0);
        camera.yaw = 0;
        camera.pitch = 0;
        gunCtrl.reset();
        // Reset explosive barrels
        for (auto &barrel : explosiveBarrels) {
          barrel.isActive = true;
          barrel.health = 60;
        }
        // Reset generator states
        for (auto &gen : task2Generators) {
          gen.timer = 0.0f;
          gen.isCounting = false;
          gen.isCompleted = false;
        }
        // Reset task progress
        taskProgress = 0.0f;
        taskProgressCompletePlayed = false;
        // Clear mouse state to prevent firing on entry
        window.mouseButtons[0] = 0;
        window.mouseButtons[1] = 0;
      }

      if (loadGame) {
        std::ifstream loadFile("load.txt");
        if (loadFile.is_open()) {
          int taskInt;
          loadFile >> taskInt >> killCount >> killTarget;
          currentTask = (TaskMode)taskInt;

          float px, py, pz;
          loadFile >> px >> py >> pz;
          camera.position = Vec3(px, py, pz);
          loadFile >> camera.yaw >> camera.pitch;
          loadFile >> playerHealth;

          int mag, res;
          loadFile >> mag >> res;
          gunCtrl.reset();
          gunCtrl.setAmmo(mag, res);

          int gs;
          loadFile >> gameTimer >> spawnTimer >> spawnWave >> gs;
          gameStarted = (gs == 1);

          // Task progress
          loadFile >> taskProgress;

          // Generator states
          size_t genCount;
          loadFile >> genCount;
          for (size_t i = 0; i < genCount && i < task2Generators.size(); i++) {
            int counting, completed;
            loadFile >> task2Generators[i].timer >> counting >> completed;
            task2Generators[i].isCounting = (counting == 1);
            task2Generators[i].isCompleted = (completed == 1);
          }

          // Barrel states
          size_t barrelCount;
          loadFile >> barrelCount;
          for (size_t i = 0; i < barrelCount && i < explosiveBarrels.size(); i++) {
            int active;
            loadFile >> active >> explosiveBarrels[i].health;
            explosiveBarrels[i].isActive = (active == 1);
          }

          // Enemy counts
          loadFile >> nextGoatIdx >> nextPigIdx >> nextBullIdx >> nextDuckIdx;

          // Reset all pools first
          for (int i = 0; i < MAX_ENEMIES; i++) {
            goatActivePool[i] = false;
            pigActivePool[i] = false;
            bullActivePool[i] = false;
            duckActivePool[i] = false;
          }

          // Load goat data
          for (int i = 0; i < nextGoatIdx; i++) {
            int active, hp, removed;
            float x, y, z;
            loadFile >> active >> x >> y >> z >> hp >> removed;
            goatPosPool[i] = Vec3(x, y, z);
            goatDataPool[i] = AnimalData(70, 10, 3.0f, 8.0f);
            goatDataPool[i].health = hp;
            if (removed == 1) {
              // Dead enemy marked as inactive and removed
              goatActivePool[i] = false;
              goatAIPool[i].shouldRemove = true;
            } else if (active == 1) {
              goatActivePool[i] = true;
              goatAIPool[i].init(&goatInstPool[i], &goatDataPool[i], goatPosPool[i], false, Vec3(0, 0, 0));
            } else {
              goatActivePool[i] = false;
            }
          }
          // Load pig data
          for (int i = 0; i < nextPigIdx; i++) {
            int active, hp, removed;
            float x, y, z;
            loadFile >> active >> x >> y >> z >> hp >> removed;
            pigPosPool[i] = Vec3(x, y, z);
            pigDataPool[i] = AnimalData(130, 10, 4.0f, 6.0f);
            pigDataPool[i].health = hp;
            if (removed == 1) {
              pigActivePool[i] = false;
              pigAIPool[i].shouldRemove = true;
            } else if (active == 1) {
              pigActivePool[i] = true;
              pigAIPool[i].init(&pigInstPool[i], &pigDataPool[i], pigPosPool[i], false, Vec3(0, 0, 0));
            } else {
              pigActivePool[i] = false;
            }
          }
          // Load bull data
          for (int i = 0; i < nextBullIdx; i++) {
            int active, hp, removed;
            float x, y, z;
            loadFile >> active >> x >> y >> z >> hp >> removed;
            bullPosPool[i] = Vec3(x, y, z);
            bullDataPool[i] = AnimalData(100, 20, 3.5f, 7.5f);
            bullDataPool[i].health = hp;
            if (removed == 1) {
              bullActivePool[i] = false;
              bullAIPool[i].shouldRemove = true;
            } else if (active == 1) {
              bullActivePool[i] = true;
              bullAIPool[i].init(&bullInstPool[i], &bullDataPool[i], bullPosPool[i], false, Vec3(0, 0, 0));
            } else {
              bullActivePool[i] = false;
            }
          }
          // Load duck data
          for (int i = 0; i < nextDuckIdx; i++) {
            int active, hp, removed;
            float x, y, z;
            loadFile >> active >> x >> y >> z >> hp >> removed;
            duckPosPool[i] = Vec3(x, y, z);
            duckDataPool[i] = AnimalData(40, 5, 2.0f, 10.0f);
            duckDataPool[i].health = hp;
            if (removed == 1) {
              duckActivePool[i] = false;
              duckAIPool[i].shouldRemove = true;
            } else if (active == 1) {
              duckActivePool[i] = true;
              duckAIPool[i].init(&duckInstPool[i], &duckDataPool[i], duckPosPool[i], true, Vec3(0, 0, 0));
            } else {
              duckActivePool[i] = false;
            }
          }

          loadFile.close();

          gameState = GameState::PLAYING;
          while (ShowCursor(FALSE) >= 0);
          window.mouseButtons[0] = 0;
          window.mouseButtons[1] = 0;
        }
      }
      continue;
    }

    if (gameState == GameState::VICTORY || gameState == GameState::FAIL) {
      // ESC is ignored in victory/fail screens
      core.beginRenderPass();
      if (gameState == GameState::VICTORY) {
        fullScreenUI.draw(&core, &shaders, &psos, &textures, "Resources/victory.png");
      } else {
        fullScreenUI.draw(&core, &shaders, &psos, &textures, "Resources/fail.png");
      }
      core.finishFrame();

      // Check for CONTINUE button 
      if (mouseClicked && mouseX >= 0.35f && mouseX <= 0.65f && mouseY >= 0.48f && mouseY <= 0.58f) {
        soundManager.play("Resources/click.wav");

        // Victory switches task, Fail keeps current task
        if (gameState == GameState::VICTORY) {
          if (currentTask == TaskMode::TASK_1) {
            currentTask = TaskMode::TASK_2;
            killTarget = 9999;
          } else {
            currentTask = TaskMode::TASK_1;
            killTarget = 40;
          }
        }

        gameState = GameState::PLAYING;
        while (ShowCursor(FALSE) >= 0);
        killCount = 0;
        playerHealth = 100;
        gameTimer = 0.0f;
        spawnTimer = 0.0f;
        spawnWave = 0;
        gameStarted = false;
        nextGoatIdx = 0;
        nextPigIdx = 0;
        nextBullIdx = 0;
        nextDuckIdx = 0;
        for (int i = 0; i < MAX_ENEMIES; i++) {
          goatActivePool[i] = false;
          pigActivePool[i] = false;
          bullActivePool[i] = false;
          duckActivePool[i] = false;
        }
        // Reset player position and gun state
        camera.position = Vec3(0, 1.5f, 0);
        camera.yaw = 0;
        camera.pitch = 0;
        gunCtrl.reset();
        // Reset explosive barrels
        for (auto &barrel : explosiveBarrels) {
          barrel.isActive = true;
          barrel.health = 60;
        }
        // Reset generator states for Task 2
        for (auto &gen : task2Generators) {
          gen.timer = 0.0f;
          gen.isCounting = false;
          gen.isCompleted = false;
        }
        // Reset task progress
        taskProgress = 0.0f;
        taskProgressCompletePlayed = false;
        // Clear mouse state to prevent firing on entry
        window.mouseButtons[0] = 0;
        window.mouseButtons[1] = 0;
      }
      // Check for BACK button 
      if (mouseClicked && mouseX >= 0.35f && mouseX <= 0.65f && mouseY >= 0.60f && mouseY <= 0.70f) {
        soundManager.play("Resources/click.wav");
        gameState = GameState::MENU;
        while (ShowCursor(TRUE) < 0);
      }
      continue;
    }

    // PLAYING state
    if (window.keys[VK_ESCAPE] == 1) {
      // Save game state to load.txt
      std::ofstream saveFile("load.txt");
      if (saveFile.is_open()) {
        // Task info
        saveFile << (int)currentTask << " " << killCount << " " << killTarget << "\n";
        // Player info
        saveFile << camera.position.x << " " << camera.position.y << " " << camera.position.z << "\n";
        saveFile << camera.yaw << " " << camera.pitch << "\n";
        saveFile << playerHealth << "\n";
        saveFile << gunCtrl.getMagazine() << " " << gunCtrl.getReserve() << "\n";
        // Game timers
        saveFile << gameTimer << " " << spawnTimer << " " << spawnWave << " " << (gameStarted ? 1 : 0) << "\n";
        // Task progress
        saveFile << taskProgress << "\n";
        // Generator states
        saveFile << task2Generators.size() << "\n";
        for (const auto &gen : task2Generators) {
          saveFile << gen.timer << " " << (gen.isCounting ? 1 : 0) << " " << (gen.isCompleted ? 1 : 0) << "\n";
        }
        // Barrel states
        saveFile << explosiveBarrels.size() << "\n";
        for (const auto &barrel : explosiveBarrels) {
          saveFile << (barrel.isActive ? 1 : 0) << " " << barrel.health << "\n";
        }
        // Enemy counts
        saveFile << nextGoatIdx << " " << nextPigIdx << " " << nextBullIdx << " " << nextDuckIdx << "\n";
        // Active enemy data
        for (int i = 0; i < nextGoatIdx; i++) {
          saveFile << (goatActivePool[i] ? 1 : 0) << " ";
          saveFile << goatPosPool[i].x << " " << goatPosPool[i].y << " " << goatPosPool[i].z << " ";
          saveFile << goatDataPool[i].health << " " << (goatAIPool[i].shouldRemove ? 1 : 0) << "\n";
        }
        for (int i = 0; i < nextPigIdx; i++) {
          saveFile << (pigActivePool[i] ? 1 : 0) << " ";
          saveFile << pigPosPool[i].x << " " << pigPosPool[i].y << " " << pigPosPool[i].z << " ";
          saveFile << pigDataPool[i].health << " " << (pigAIPool[i].shouldRemove ? 1 : 0) << "\n";
        }
        for (int i = 0; i < nextBullIdx; i++) {
          saveFile << (bullActivePool[i] ? 1 : 0) << " ";
          saveFile << bullPosPool[i].x << " " << bullPosPool[i].y << " " << bullPosPool[i].z << " ";
          saveFile << bullDataPool[i].health << " " << (bullAIPool[i].shouldRemove ? 1 : 0) << "\n";
        }
        for (int i = 0; i < nextDuckIdx; i++) {
          saveFile << (duckActivePool[i] ? 1 : 0) << " ";
          saveFile << duckPosPool[i].x << " " << duckPosPool[i].y << " " << duckPosPool[i].z << " ";
          saveFile << duckDataPool[i].health << " " << (duckAIPool[i].shouldRemove ? 1 : 0) << "\n";
        }
        saveFile.close();
      }
      // Return to menu 
      gameState = GameState::MENU;
      while (ShowCursor(TRUE) < 0);
      continue;
    }

    if (!gunModel.animation.animations.empty()) {
      gunCtrl.update(dt, window.keys, window.mouseButtons);
    }
    camera.update(&window, dt, gunCtrl.isSprinting);

    // Play jump sound
    if (camera.hasJumped()) {
      soundManager.play("Resources/jump.wav");
    }

    // Play sprint sound
    if (camera.hasStartedSprinting()) {
      soundManager.play("Resources/step.wav");
    }

    // Update pickup cooldowns
    if (healCooldown > 0.0f)
      healCooldown -= dt;
    if (ammoCooldown > 0.0f)
      ammoCooldown -= dt;

    // E key edge detection
    bool eJustPressed = window.keys['E'] && !prevKeyE;
    prevKeyE = window.keys['E'];

    // Pickup interactions
    if (eJustPressed && (foundHealBox || foundAmmoBox)) {
      Vec3 forward = Vec3(sinf(camera.yaw) * cosf(camera.pitch), sinf(camera.pitch), cosf(camera.yaw) * cosf(camera.pitch)).normalize();

      // Check heal box (box_003)
      if (foundHealBox && healCooldown <= 0.0f && playerHealth < maxPlayerHealth) {
        Vec3 toBox = healBoxPos - camera.position;
        float dist = toBox.length();
        if (dist <= 5.0f) {
          AABB healBoxAABB = getStaticModelAABB("box_003", healBoxPos);
          if (CollisionSystem::rayIntersectsAABB(camera.position, forward, healBoxAABB, 5.0f)) {
            // Heal player 40 HP (capped at max)
            playerHealth = (playerHealth + 40 > maxPlayerHealth) ? maxPlayerHealth : playerHealth + 40;
            soundManager.play("Resources/heal.wav");
            healCooldown = 15.0f; 
          }
        }
      }

      // Check ammo box (box_004)
      if (foundAmmoBox && ammoCooldown <= 0.0f && gunCtrl.getReserve() < gunCtrl.getMaxReserve()) {
        Vec3 toBox = ammoBoxPos - camera.position;
        float dist = toBox.length();
        if (dist <= 5.0f) {
          AABB ammoBoxAABB = getStaticModelAABB("box_004", ammoBoxPos);
          if (CollisionSystem::rayIntersectsAABB(camera.position, forward, ammoBoxAABB, 5.0f)) {
            // Add 93 reserve ammo (capped at max)
            gunCtrl.addReserve(93);
            soundManager.play("Resources/pickup.wav");
            ammoCooldown = 15.0f; 
          }
        }
      }

      // Check generator_002 interaction (Task 2)
      if (currentTask == TaskMode::TASK_2) {
        for (auto &gen : task2Generators) {
          if (!gen.isCompleted && !gen.isCounting) {
            Vec3 toGen = gen.position - camera.position;
            if (toGen.length() <= 5.0f) {
              if (CollisionSystem::rayIntersectsAABB(camera.position, forward, gen.collider, 5.0f)) {
                gen.isCounting = true;
                gen.timer = 45.0f;
                soundManager.play("Resources/generator.wav");
              }
            }
          }
        }
      }
    }

    // Update generator timers (Task 2)
    if (currentTask == TaskMode::TASK_2) {
      // 3 generators x 45 seconds each = 135 total seconds
      const float TASK2_TOTAL_SECONDS = 135.0f;
      for (auto &gen : task2Generators) {
        if (gen.isCounting) {
          float prevTimer = gen.timer;
          gen.timer -= dt;
          // Progress increases for each second counted (dt worth of time)
          taskProgress += dt / TASK2_TOTAL_SECONDS;
          if (gen.timer <= 0.0f) {
            gen.isCounting = false;
            gen.isCompleted = true;
          }
        }
      }
      if (taskProgress > 1.0f)
        taskProgress = 1.0f;
    }

    t += dt;
    Vec3 playerFeetPos = camera.position - Vec3(0, 1.5f, 0);
    AABB playerWorldAABB = playerLocalAABB.transform(playerFeetPos);
    for (const auto &wall : sceneColliders) {CollisionInfo info = CollisionSystem::checkAABB(playerWorldAABB, wall);
      if (info.collided) {
        CollisionSystem::resolveCollision(playerFeetPos, info);
        camera.position = playerFeetPos + Vec3(0, 1.5f, 0);
        playerWorldAABB = playerLocalAABB.transform(playerFeetPos);
      }
    }

    // Detect ground height
    float groundHeight = camera.defaultGroundY;
    Vec3 playerXZ = Vec3(playerFeetPos.x, 0, playerFeetPos.z);
    for (const auto &wall : sceneColliders) {
      if (playerFeetPos.x >= wall.min.x && playerFeetPos.x <= wall.max.x && playerFeetPos.z >= wall.min.z && playerFeetPos.z <= wall.max.z) {
        if (playerFeetPos.y >= wall.max.y - 0.5f) {
          float surfaceHeight = wall.max.y + 1.5f;
          if (surfaceHeight > groundHeight) {
            groundHeight = surfaceHeight;
          }
        }
      }
    }
    camera.setGroundHeight(groundHeight);

    // Wave spawning 
    gameTimer += dt;
    if (!gameStarted && gameTimer >= GAME_START_DELAY) {
      gameStarted = true;
      spawnTimer = 0.0f;
      spawnWave = 0;
      // First wave (2 duck, 2 goat, 1 pig)
      spawnEnemy("duck", spawnBackRight + Vec3(-2, 0, 0), entryTargetBack);
      spawnEnemy("duck", spawnBackRight + Vec3(2, 0, 0), entryTargetBack);
      spawnEnemy("goat", spawnBackRight + Vec3(-1, 0, 0), entryTargetBack);
      spawnEnemy("goat", spawnBackRight + Vec3(1, 0, 0), entryTargetBack);
      spawnEnemy("pig", spawnBackRight, entryTargetBack);
    }
    if (gameStarted) {
      spawnTimer += dt;
      float currentInterval = (spawnWave == 0) ? BACK_TO_FRONT_INTERVAL : FRONT_TO_BACK_INTERVAL;
      if (spawnTimer >= currentInterval) {
        spawnTimer = 0.0f;
        spawnWave = (spawnWave + 1) % 2;
        if (spawnWave == 1) {
          // Front spawn (1 goat, 1 pig, 1 bull)
          spawnEnemy("goat", spawnFrontLeft + Vec3(-1, 0, 0), entryTargetFront);
          spawnEnemy("pig", spawnFrontLeft, entryTargetFront);
          spawnEnemy("bull", spawnFrontLeft + Vec3(1, 0, 0), entryTargetFront);
        } else {
          // Back spawn (2 duck, 2 goat, 1 pig)
          spawnEnemy("duck", spawnBackRight + Vec3(-2, 0, 0), entryTargetBack);
          spawnEnemy("duck", spawnBackRight + Vec3(2, 0, 0), entryTargetBack);
          spawnEnemy("goat", spawnBackRight + Vec3(-1, 0, 0), entryTargetBack);
          spawnEnemy("goat", spawnBackRight + Vec3(1, 0, 0), entryTargetBack);
          spawnEnemy("pig", spawnBackRight, entryTargetBack);
        }
      }
    }

    // Build animal colliders from active enemies
    AABB deadAABB(Vec3(0, 0, 0), Vec3(0, 0, 0));
    animalColliders.clear();
    std::vector<EnemyController *> activeEnemies;

    for (int i = 0; i < nextGoatIdx; i++) {
      if (goatActivePool[i] && !goatAIPool[i].shouldRemove) {
        animalColliders.push_back(getAnimatedModelAABB("Goat-01", goatPosPool[i]));
        activeEnemies.push_back(&goatAIPool[i]);
      }
    }
    for (int i = 0; i < nextPigIdx; i++) {
      if (pigActivePool[i] && !pigAIPool[i].shouldRemove) {
        animalColliders.push_back(getAnimatedModelAABB("Pig", pigPosPool[i]));
        activeEnemies.push_back(&pigAIPool[i]);
      }
    }
    for (int i = 0; i < nextBullIdx; i++) {
      if (bullActivePool[i] && !bullAIPool[i].shouldRemove) {
        animalColliders.push_back(getAnimatedModelAABB("Bull-dark", bullPosPool[i]));
        activeEnemies.push_back(&bullAIPool[i]);
      }
    }
    for (int i = 0; i < nextDuckIdx; i++) {
      if (duckActivePool[i] && !duckAIPool[i].shouldRemove) {
        animalColliders.push_back(getAnimatedModelAABB("Duck-mixed", duckPosPool[i]));
        activeEnemies.push_back(&duckAIPool[i]);
      }
    }

    for (int i = 0; i < animalColliders.size(); i++) {
      CollisionInfo info = CollisionSystem::checkAABB(playerWorldAABB, animalColliders[i]);
      if (info.collided) {
        if (fabsf(info.normal.y) > 0.5f) {
          // Vertical collision detected convert to horizontal push
          Vec3 animalCenter = (animalColliders[i].min + animalColliders[i].max) * 0.5f;
          Vec3 pushDir = playerFeetPos - animalCenter;
          pushDir.y = 0;
          if (pushDir.length() > 0.01f) {
            pushDir = pushDir.normalize();
            playerFeetPos = playerFeetPos + pushDir * info.depth;
          }
        } else {
          CollisionSystem::resolveCollision(playerFeetPos, info);
        }
        camera.position = playerFeetPos + Vec3(0, 1.5f, 0);
        playerWorldAABB = playerLocalAABB.transform(playerFeetPos);
      }
    }
    lightData.cameraPos = camera.position;
    Matrix p = Matrix::perspective(0.01f, 10000.0f, (float)WIDTH / (float)HEIGHT, 60.0f);
    Matrix v = camera.getViewMatrix();
    Matrix vp = v * p;
    core.beginRenderPass();
    for (auto it = staticModels.begin(); it != staticModels.end(); ++it) {
        StaticModel* model = it->second;
        if (it->first == "grass_003") {
            model->drawInstanced(&core, &psos, &shaders, vp, &textures, lightData, t, "GrassShader");
        }
        else {
            model->drawInstanced(&core, &psos, &shaders, vp, &textures, lightData);
        }
    }
    StaticModel* barrelModel = staticModels["barrel_003"];
    if (barrelModel) {
      barrelModel->clearInstances();
      for (const auto &barrel : explosiveBarrels) {
        if (barrel.isActive) {
          Matrix scale = Matrix::scaling(Vec3(0.01f, 0.01f, 0.01f));
          Matrix trans = Matrix::translation(barrel.position);
          barrelModel->addInstance(scale * trans);
        }
      }
      barrelModel->uploadInstances(&core);
      barrelModel->drawInstanced(&core, &psos, &shaders, vp, &textures, lightData);
    }
    // Update enemy AI and get damage to player
    int enemyDamage = 0;
    int totalDamage = 0;

    for (int i = 0; i < nextGoatIdx; i++) {
      if (goatActivePool[i] && !goatAIPool[i].shouldRemove) {
        goatAIPool[i].update(dt, camera.position, enemyDamage, &enemySceneColliders, "Goat-01");
        totalDamage += enemyDamage;
        goatPosPool[i] = goatAIPool[i].position;
      }
    }
    for (int i = 0; i < nextPigIdx; i++) {
      if (pigActivePool[i] && !pigAIPool[i].shouldRemove) {
        pigAIPool[i].update(dt, camera.position, enemyDamage, &enemySceneColliders, "Pig");
        totalDamage += enemyDamage;
        pigPosPool[i] = pigAIPool[i].position;
      }
    }
    for (int i = 0; i < nextBullIdx; i++) {
      if (bullActivePool[i] && !bullAIPool[i].shouldRemove) {
        bullAIPool[i].update(dt, camera.position, enemyDamage, &enemySceneColliders, "Bull-dark");
        totalDamage += enemyDamage;
        bullPosPool[i] = bullAIPool[i].position;
      }
    }
    for (int i = 0; i < nextDuckIdx; i++) {
      if (duckActivePool[i] && !duckAIPool[i].shouldRemove) {
        duckAIPool[i].update(dt, camera.position, enemyDamage, &enemySceneColliders, "Duck-mixed");
        totalDamage += enemyDamage;
        duckPosPool[i] = duckAIPool[i].position;
      }
    }

    // Apply enemy damage to player
    if (totalDamage > 0) {
      soundManager.play("Resources/enemyAttack.wav");
      soundManager.play("Resources/playerHurt.wav");
    }
    playerHealth -= totalDamage;
    if (playerHealth <= 0) {
      // Game over (switch to fail screen)
      gameState = GameState::FAIL;
      while (ShowCursor(TRUE) < 0);
      continue;
    }

    // Check if player shot enemies
    if (gunCtrl.hasFired()) {
      soundManager.play("Resources/Fire.wav");
      Vec3 forward = Vec3(sinf(camera.yaw) * cosf(camera.pitch), sinf(camera.pitch), cosf(camera.yaw) * cosf(camera.pitch)).normalize();
      bulletSystem.spawn();
      bulletSystem.spawn();
      HitResult hitResult = bulletSystem.checkHit(camera.position, forward, activeEnemies, animalColliders, sceneColliders, gunCtrl.getDamage());
      if (hitResult == HitResult::KILL) {
        hitMarker.triggerKill();
        soundManager.play("Resources/hit.wav");
        soundManager.play("Resources/kill.wav");
        killCount++;
        // Update Task 1 progress on kill
        if (currentTask == TaskMode::TASK_1) {
          taskProgress = (float)killCount / 40.0f;
          if (taskProgress > 1.0f)
            taskProgress = 1.0f;
        }
        if (killCount >= killTarget) {

        }

      } else if (hitResult == HitResult::HIT) {
        hitMarker.triggerHit();
        soundManager.play("Resources/hit.wav");
      }

      // Check if player shot explosive barrels
      for (auto &barrel : explosiveBarrels) {
        if (!barrel.isActive)
          continue;

        // Ray-AABB intersection check
        float tMin = 0.0f, tMax = 1000.0f;
        Vec3 rayOrigin = camera.position;
        Vec3 rayDir = forward;
        bool hit = true;

        for (int axis = 0; axis < 3; axis++) {
          float origin = (axis == 0) ? rayOrigin.x : (axis == 1) ? rayOrigin.y : rayOrigin.z;
          float dir = (axis == 0) ? rayDir.x : (axis == 1) ? rayDir.y : rayDir.z;
          float minB = (axis == 0) ? barrel.collider.min.x : (axis == 1) ? barrel.collider.min.y : barrel.collider.min.z;
          float maxB = (axis == 0) ? barrel.collider.max.x : (axis == 1) ? barrel.collider.max.y : barrel.collider.max.z;

          if (fabsf(dir) < 0.0001f) {
            if (origin < minB || origin > maxB) {
              hit = false;
              break;
            }
          } else {
            float t1 = (minB - origin) / dir;
            float t2 = (maxB - origin) / dir;
            if (t1 > t2) {
              float tmp = t1;
              t1 = t2;
              t2 = tmp;
            }
            if (t1 > tMin)
              tMin = t1;
            if (t2 < tMax)
              tMax = t2;
            if (tMin > tMax) {
              hit = false;
              break;
            }
          }
        }

        if (hit && tMin > 0 && tMin < 100.0f) {
          barrel.health -= gunCtrl.getDamage();
          hitMarker.triggerHit();
          soundManager.play("Resources/hit.wav");

          // Barrel destroyed 
          if (barrel.health <= 0) {
            barrel.isActive = false;
            bool explosionKill = false;
            bool explosionHit = false;

            // Deal 60 damage to all enemies within 5 units
            const float explosionRadius = 5.0f;
            const int explosionDamage = 60;

            auto damageEnemy = [&](EnemyController &ai, Vec3 &pos, bool isDuck) {
              if (ai.shouldRemove)
                return;
              Vec3 toEnemy = pos - barrel.position;
              toEnemy.y = 0;
              float dist = toEnemy.length();
              if (dist < explosionRadius) {
                bool wasAlive = ai.isAlive();
                ai.takeDamage(explosionDamage, toEnemy.normalize());
                explosionHit = true;
                if (wasAlive && !ai.isAlive()) {
                  explosionKill = true;
                }
              }
            };

            for (int i = 0; i < nextGoatIdx; i++) {
              if (goatActivePool[i])
                damageEnemy(goatAIPool[i], goatPosPool[i], false);
            }
            for (int i = 0; i < nextPigIdx; i++) {
              if (pigActivePool[i])
                damageEnemy(pigAIPool[i], pigPosPool[i], false);
            }
            for (int i = 0; i < nextBullIdx; i++) {
              if (bullActivePool[i])
                damageEnemy(bullAIPool[i], bullPosPool[i], false);
            }
            for (int i = 0; i < nextDuckIdx; i++) {
              if (duckActivePool[i])
                damageEnemy(duckAIPool[i], duckPosPool[i], true);
            }

            // Damage player if in explosion radius
            Vec3 toPlayer = camera.position - barrel.position;
            toPlayer.y = 0;
            float playerDist = toPlayer.length();
            if (playerDist < explosionRadius) {
              playerHealth -= 15;
              // Knockback player 2 units
              Vec3 knockbackDir = toPlayer.normalize();
              camera.position = camera.position + knockbackDir * 2.0f;
            }

            // Play explosion sound
            soundManager.play("Resources/explosion.wav");

            // Show hitmarker for explosion kills/hits
            if (explosionKill) {
              hitMarker.triggerKill();
              soundManager.play("Resources/kill.wav");
              killCount++;
              // Update Task 1 progress on kill
              if (currentTask == TaskMode::TASK_1) {
                taskProgress = (float)killCount / 40.0f;
                if (taskProgress > 1.0f)
                  taskProgress = 1.0f;
              }
              if (killCount >= killTarget) {

              }

            } else if (explosionHit) {
              hitMarker.triggerHit();
            }
          }
          break; // Only damage one barrel per shot
        }
      }
    }

    // Play reload sound
    if (gunCtrl.hasReloaded()) {
      soundManager.play("Resources/Reload.wav");
    }

    // Play dryfire sound
    if (gunCtrl.hasDryfired()) {
      soundManager.play("Resources/DryFire.wav");
    }

    // Update bullet visual animations
    bulletSystem.update(dt);
    hitMarker.update(dt);

    if (gunCtrl.hasMeleed()) {
      soundManager.play("Resources/Melee.wav");
      int damage = gunCtrl.getMeleeDamage();
      Vec3 forward = Vec3(sinf(camera.yaw), 0, cosf(camera.yaw)).normalize();
      bool meleeHit = false;
      bool meleeKill = false;

      auto handleMelee = [&](EnemyController &enemy, Vec3 &enemyPos) {
        if (enemy.shouldRemove)
          return;

        Vec3 toEnemy = enemyPos - camera.position;
        float dist = toEnemy.length();
        if (dist < 4.0f) {
          toEnemy = toEnemy.normalize();
          float dot = Dot(forward, toEnemy);
          if (dot > 0.5f) {
            bool wasAlive = enemy.isAlive();
            enemy.takeDamage(damage, forward);
            meleeHit = true;
            if (wasAlive && !enemy.isAlive()) {
              meleeKill = true;
            }
          }
        }
      };

      for (int i = 0; i < nextGoatIdx; i++) {
        if (goatActivePool[i])
          handleMelee(goatAIPool[i], goatPosPool[i]);
      }
      for (int i = 0; i < nextPigIdx; i++) {
        if (pigActivePool[i])
          handleMelee(pigAIPool[i], pigPosPool[i]);
      }
      for (int i = 0; i < nextBullIdx; i++) {
        if (bullActivePool[i])
          handleMelee(bullAIPool[i], bullPosPool[i]);
      }
      for (int i = 0; i < nextDuckIdx; i++) {
        if (duckActivePool[i])
          handleMelee(duckAIPool[i], duckPosPool[i]);
      }

      // Trigger hitmarker for melee attacks
      if (meleeKill) {
        hitMarker.triggerKill();
        soundManager.play("Resources/hit.wav");
        soundManager.play("Resources/kill.wav");
        killCount++;
        // Update Task 1 progress on kill
        if (currentTask == TaskMode::TASK_1) {
          taskProgress = (float)killCount / 40.0f;
          if (taskProgress > 1.0f)
            taskProgress = 1.0f;
        }
        if (killCount >= killTarget) {

        }

      } else if (meleeHit) {
        hitMarker.triggerHit();
        soundManager.play("Resources/hit.wav");
      }

      // Melee attack explosive barrels
      for (auto &barrel : explosiveBarrels) {
        if (!barrel.isActive)
          continue;

        Vec3 toBarrel = barrel.position - camera.position;
        float dist = toBarrel.length();
        if (dist < 4.0f) {
          toBarrel = toBarrel.normalize();
          float dot = Dot(forward, toBarrel);
          if (dot > 0.5f) {
            barrel.health -= damage;
            hitMarker.triggerHit();
            soundManager.play("Resources/hit.wav");

            if (barrel.health <= 0) {
              barrel.isActive = false;
              bool explosionKill = false;
              bool explosionHit = false;

              const float explosionRadius = 5.0f;
              const int explosionDamage = 60;

              auto damageEnemyMelee = [&](EnemyController &ai, Vec3 &pos) {
                if (ai.shouldRemove)
                  return;
                Vec3 toEnemy = pos - barrel.position;
                toEnemy.y = 0;
                float d = toEnemy.length();
                if (d < explosionRadius) {
                  bool wasAlive = ai.isAlive();
                  ai.takeDamage(explosionDamage, toEnemy.normalize());
                  explosionHit = true;
                  if (wasAlive && !ai.isAlive())
                    explosionKill = true;
                }
              };

              for (int i = 0; i < nextGoatIdx; i++) {
                if (goatActivePool[i])
                  damageEnemyMelee(goatAIPool[i], goatPosPool[i]);
              }
              for (int i = 0; i < nextPigIdx; i++) {
                if (pigActivePool[i])
                  damageEnemyMelee(pigAIPool[i], pigPosPool[i]);
              }
              for (int i = 0; i < nextBullIdx; i++) {
                if (bullActivePool[i])
                  damageEnemyMelee(bullAIPool[i], bullPosPool[i]);
              }
              for (int i = 0; i < nextDuckIdx; i++) {
                if (duckActivePool[i])
                  damageEnemyMelee(duckAIPool[i], duckPosPool[i]);
              }

              Vec3 toPlayer = camera.position - barrel.position;
              toPlayer.y = 0;
              float playerDist = toPlayer.length();
              if (playerDist < explosionRadius) {
                playerHealth -= 15;
                Vec3 knockbackDir = toPlayer.normalize();
                camera.position = camera.position + knockbackDir * 2.0f;
              }

              soundManager.play("Resources/explosion.wav");

              if (explosionKill) {
                hitMarker.triggerKill();
                soundManager.play("Resources/kill.wav");
                killCount++;
                if (currentTask == TaskMode::TASK_1) {
                  taskProgress = (float)killCount / 40.0f;
                  if (taskProgress > 1.0f)
                    taskProgress = 1.0f;
                }
                if (killCount >= killTarget) {

                }

              } else if (explosionHit) {
                hitMarker.triggerHit();
              }
            }
            break;
          }
        }
      }
    }
    Matrix commonScale = Matrix::scaling(Vec3(0.01f, 0.01f, 0.01f));
    float modelYawOffset = 0.0f;

    // Draw enemies
    for (int i = 0; i < nextGoatIdx; i++) {
      if (goatActivePool[i] && !goatAIPool[i].shouldRemove) {
        Matrix W = commonScale * Matrix::rotateY(goatAIPool[i].yaw + modelYawOffset) * Matrix::translation(goatPosPool[i]);
        goatModel.draw(&core, &psos, &shaders, &goatInstPool[i], vp, W, &textures, lightData);
      }
    }
    for (int i = 0; i < nextPigIdx; i++) {
      if (pigActivePool[i] && !pigAIPool[i].shouldRemove) {
        Matrix W = commonScale * Matrix::rotateY(pigAIPool[i].yaw + modelYawOffset) * Matrix::translation(pigPosPool[i]);
        pigModel.draw(&core, &psos, &shaders, &pigInstPool[i], vp, W, &textures, lightData);
      }
    }
    for (int i = 0; i < nextBullIdx; i++) {
      if (bullActivePool[i] && !bullAIPool[i].shouldRemove) {
        Matrix W = commonScale * Matrix::rotateY(bullAIPool[i].yaw + modelYawOffset) * Matrix::translation(bullPosPool[i]);
        bullModel.draw(&core, &psos, &shaders, &bullInstPool[i], vp, W, &textures, lightData);
      }
    }
    for (int i = 0; i < nextDuckIdx; i++) {
      if (duckActivePool[i] && !duckAIPool[i].shouldRemove) {
        Matrix W = commonScale * Matrix::rotateY(duckAIPool[i].yaw + modelYawOffset) * Matrix::translation(duckPosPool[i]);
        duckModel.draw(&core, &psos, &shaders, &duckInstPool[i], vp, W, &textures, lightData);
      }
    }

    Matrix camWorld = v;
    camWorld = camWorld.invert();
    Matrix gunScale = Matrix::scaling(Vec3(0.05f, 0.05f, 0.05f));
    Matrix gunOffset = Matrix::translation(Vec3(0.50f, -0.1f, 0.40f));
    Matrix gunRot = Matrix::rotateY(3.14159f);
    Matrix W_Gun = gunScale * gunRot * gunOffset * camWorld;
    // Draw skybox
    skybox.draw(&core, &psos, &shaders, &textures, camera, WIDTH, HEIGHT, "Models/Textures/sky_25_2k.png");
    // Clear depth buffer 
    core.clearDepthBuffer();
    gunModel.draw(&core, &psos, &shaders, &gunInst, vp, W_Gun, &textures, lightData);
    crosshair.draw(&core, &psos, &shaders);
    hitMarker.draw(&core, &psos, &shaders);
    // Draw UI elements
    gameUI.drawPlayerHealth(&core, &shaders, &psos, playerHealth, maxPlayerHealth);
    gameUI.drawAmmo(&core, &shaders, &psos, gunCtrl.getMagazine(), 31, gunCtrl.getReserve(), gunCtrl.getMaxReserve());
    // Draw task progress bar
    bool taskCompleted = false;
    if (currentTask == TaskMode::TASK_1) {
      taskCompleted = (killCount >= 40);
    } else if (currentTask == TaskMode::TASK_2) {
      int completedGens = 0;
      for (const auto &gen : task2Generators) {
        if (gen.isCompleted)
          completedGens++;
      }
      taskCompleted = (completedGens >= 3);
    }
    // Play finish task sound 
    if (taskCompleted && !taskProgressCompletePlayed) {
      soundManager.play("Resources/finish.wav");
      taskProgressCompletePlayed = true;
    }
    gameUI.drawProgressBar(&core, &shaders, &psos, taskProgress, taskCompleted);

    // Draw enemy health bars 
    for (int i = 0; i < nextGoatIdx; i++) {
      if (goatActivePool[i] && !goatAIPool[i].shouldRemove) {
        gameUI.drawEnemyHealth(&core, &shaders, &psos, vp, goatPosPool[i], goatDataPool[i].health, goatDataPool[i].maxHealth, 1.5f);
      }
    }
    for (int i = 0; i < nextPigIdx; i++) {
      if (pigActivePool[i] && !pigAIPool[i].shouldRemove) {
        gameUI.drawEnemyHealth(&core, &shaders, &psos, vp, pigPosPool[i], pigDataPool[i].health, pigDataPool[i].maxHealth, 1.0f);
      }
    }
    for (int i = 0; i < nextBullIdx; i++) {
      if (bullActivePool[i] && !bullAIPool[i].shouldRemove) {
        gameUI.drawEnemyHealth(&core, &shaders, &psos, vp, bullPosPool[i], bullDataPool[i].health, bullDataPool[i].maxHealth, 2.0f);
      }
    }
    for (int i = 0; i < nextDuckIdx; i++) {
      if (duckActivePool[i] && !duckAIPool[i].shouldRemove) {
        gameUI.drawEnemyHealth(&core, &shaders, &psos, vp, duckPosPool[i], duckDataPool[i].health, duckDataPool[i].maxHealth, 1.0f);
      }
    }

    // Draw bullets
    bulletSystem.draw(&core, &shaders, &psos, vp);

    // Check Victory Conditions
    if (foundVictoryPlatform) {
      // Platform top check
      Vec3 playerFeet = camera.position - Vec3(0, 1.5f, 0);
      bool onPlatform = (playerFeet.x >= victoryPlatformAABB.min.x && playerFeet.x <= victoryPlatformAABB.max.x && playerFeet.z >= victoryPlatformAABB.min.z && playerFeet.z <= victoryPlatformAABB.max.z && playerFeet.y >= victoryPlatformAABB.max.y - 0.1f);

      if (onPlatform) {
        if (currentTask == TaskMode::TASK_1 && killCount >= 40) {
          gameState = GameState::VICTORY;
          while (ShowCursor(TRUE) < 0);
        } else if (currentTask == TaskMode::TASK_2) {
          int completedGens = 0;
          for (const auto &gen : task2Generators) {
            if (gen.isCompleted)
              completedGens++;
          }
          if (completedGens >= 3) {
            gameState = GameState::VICTORY;
            while (ShowCursor(TRUE) < 0);
          }
        }
      }
    }

    core.finishFrame();
  }

  core.flushGraphicsQueue();

  for (auto &pair : staticModels) {
    delete pair.second;
  }
  staticModels.clear();

  return 0;
}