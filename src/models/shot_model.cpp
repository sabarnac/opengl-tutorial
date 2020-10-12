#ifndef MODELS_SHOT_MODEL_CPP
#define MODELS_SHOT_MODEL_CPP

#include <string>
#include <memory>

#include <GLFW/glfw3.h>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtx/transform.hpp>
#include <glm/gtc/quaternion.hpp>
#include <glm/gtx/quaternion.hpp>

#include "../include/models.cpp"
#include "../include/light.cpp"
#include "../include/control.cpp"

#include "model_base.cpp"
#include "../light/point_light.cpp"

/**
 * Class that represents a shot/bullet model.
 */
class ShotModel : public ModelBase
{
private:
  // Whether to show the light or not.
  static bool isShotLightPresent;
  // The timestamp for the last time the shot light was toggled.
  static double lastShotLightChange;

  // The model manager responsible for managing the models in the scene.
  ModelManager &modelManager;
  // The light manager responsible for managing the lights in the scene.
  LightManager &lightManager;
  // The control manager responsible for managing controls and inputs of the window.
  const ControlManager &controlManager;

  // The timestamp of the last time the update for the camera was started.
  double lastTime;

  // The instance of the point light for the shot.
  std::shared_ptr<PointLight> shotLight;

  /**
   * Create a new shot light.
   */
  void createShotLight()
  {
    // Destroy any existing shot light.
    destroyShotLight();

    // Create shot light and set its properties.
    shotLight = PointLight::create(getModelId() + "::ShotLight");
    shotLight->setLightPosition(getModelPosition());

    // Register the shot light.
    lightManager.registerLight(shotLight);

    // Update the shot light toggle.
    isShotLightPresent = true;
  }

  /**
   * Destroy the existing shot light.
   */
  void destroyShotLight()
  {
    // Check if shot light exists
    if (shotLight != nullptr)
    {
      // Destroy the shot light.
      lightManager.deregisterLight(shotLight);
      shotLight = nullptr;
    }
    // Update the shot light toggle.
    isShotLightPresent = false;
  }

  /**
   * Update the existing shot light.
   */
  void updateShotLight()
  {
    // Check if shot light toggle is enabled.
    if (isShotLightPresent)
    {
      // check if shot light doesn't exist.
      if (shotLight == nullptr)
      {
        // Create shot light.
        createShotLight();
      }
      // Update the shot light.
      shotLight->setLightPosition(getModelPosition());
    }
    // Check if shot light exists.
    else if (shotLight != nullptr)
    {
      // Destroy any existing shot light.
      destroyShotLight();
    }
  }

public:
  ShotModel(const std::string &modelId)
      : ModelBase(
            modelId,
            "Shot",
            glm::vec3(0.0f, 0.0f, 0.0f), glm::vec3(0.0f, 0.0f, 0.0f), glm::vec3(1.0f, 1.0f, 1.0f),
            "assets/objects/sphere.obj",
            "assets/textures/shot.bmp",
            "assets/shaders/vertex/shot.glsl", "assets/shaders/fragment/shot.glsl",
            ColliderShapeType::SPHERE),
        modelManager(ModelManager::getInstance()),
        lightManager(LightManager::getInstance()),
        controlManager(ControlManager::getInstance()),
        lastTime(glfwGetTime()),
        shotLight(nullptr) {}

  /**
   * Creates a new instance of the shot model.
   */
  const static std::shared_ptr<ShotModel> create(const std::string &modelId)
  {
    return std::make_shared<ShotModel>(modelId);
  };

  void init() override
  {
    // Set the scale of the model.
    setModelScale(glm::vec3(0.5));

    // Check if shot light toggle is enabled.
    if (isShotLightPresent)
    {
      // Create a shot light.
      createShotLight();
    }
  }

  void deinit() override
  {
    // Check if shot light toggle is enabled.
    if (isShotLightPresent)
    {
      // Destroy the shot light and update the toggle.
      destroyShotLight();
      isShotLightPresent = true;
    }
  }

  void update() override
  {
    // Get the timestamp for the start of the update.
    const auto currentTime = glfwGetTime();
    // Get the time difference since the start of the last update.
    const auto deltaTime = currentTime - lastTime;

    // Get the position of the shot.
    const auto currentPosition = getModelPosition();
    // Check if the shot has already gone beyond a threshold.
    if (currentPosition.z < -50.0)
    {
      // If it has, destroy it.
      modelManager.deregisterModel(this->getModelId());
      return;
    }

    // Check if "H" key was pressed after 500ms since the last shot light toggle.
    if (controlManager.isKeyPressed(GLFW_KEY_H) && (currentTime - lastShotLightChange) > 0.5)
    {
      // "H" key was pressed. Toggle the shot light.
      isShotLightPresent = !isShotLightPresent;
      // Update the timestamp for when the shot light was last toggled.
      lastShotLightChange = currentTime;
    }

    // Update the shot position.
    setModelPosition(getModelPosition() - glm::vec3(0.0, 0.0, 100.0 * deltaTime));
    // Update the shot light.
    updateShotLight();

    // Get the list of registered models.
    const auto models = modelManager.getAllModels();
    // Iterate over the list of registered models.
    for (auto model = models.begin(); model != models.end(); model++)
    {
      // Check if the current model is an enemy model.
      if ((*model)->getModelName() == "Enemy")
      {
        // Check if collided with the enemy model.
        if (DeepCollisionValidator::haveShapesCollided(getColliderDetails()->getColliderShape(), (*model)->getColliderDetails()->getColliderShape(), true))
        {
          // Shot has collided with an enemy. Destroy both.
          modelManager.deregisterModel(*model);
          modelManager.deregisterModel(this->getModelId());
          break;
        }
      }
    }

    // Set the timestamp for the start of the last update to the starting timestamp of the current update.
    lastTime = currentTime;
  }
};

// Initialize the shot light toggle static variable.
bool ShotModel::isShotLightPresent = true;
// Initialize the last time the shot light toggle was changed static variable.
double ShotModel::lastShotLightChange = -1;

#endif