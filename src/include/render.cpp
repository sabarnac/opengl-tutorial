#ifndef INCLUDE_RENDER_CPP
#define INCLUDE_RENDER_CPP

#include <map>
#include <set>
#include <unistd.h>

#include <GL/glew.h>

#include "window.cpp"
#include "constants.cpp"
#include "../light/light_base.cpp"
#include "../camera/camera_base.cpp"
#include "../models/model_base.cpp"

struct LightDetails
{
  glm::vec3 lightPosition;
  glm::mat4 lightVpMatrix;
  glm::vec3 lightColor;
  double lightIntensity;
  int mapWidth;
  int mapHeight;
  double nearPlane;
  double farPlane;
  GLuint textureId;
};

class VertexAttributeArray
{
private:
  static std::set<GLuint> attributeIds;

  GLuint attributeId;
  std::string attributeName;
  GLuint bufferId;
  unsigned int bufferElementSize;

  GLuint createAttributeId()
  {
    GLuint maxId = std::numeric_limits<GLuint>::max();
    for (GLuint i = 0; i < maxId; i++)
    {
      if (attributeIds.find(i) == attributeIds.end())
      {
        return i;
      }
    }
    std::cout << "Failed at render" << std::endl;
    exit(1);
  }

public:
  VertexAttributeArray(VertexAttributeArray &) = delete;

  VertexAttributeArray(std::string attributeName, GLuint bufferId, unsigned int bufferElementSize)
      : attributeName(attributeName),
        bufferId(bufferId),
        bufferElementSize(bufferElementSize)
  {
    attributeId = createAttributeId();
    attributeIds.insert(attributeId);
  }

  ~VertexAttributeArray()
  {
    attributeIds.erase(attributeId);
    glDisableVertexAttribArray(attributeId);
  }

  void enableAttribute()
  {
    glEnableVertexAttribArray(attributeId);
    glBindBuffer(GL_ARRAY_BUFFER, bufferId);
    glVertexAttribPointer(attributeId, bufferElementSize, GL_FLOAT, GL_FALSE, 0, (void *)0);
  }
};

std::set<GLuint> VertexAttributeArray::attributeIds = std::set<GLuint>({});

class RenderManager
{
  friend class DebugRenderManager;

private:
  static double ambientFactor;
  static RenderManager instance;

  WindowManager &windowManager;

  std::string activeCameraId;
  std::shared_ptr<LightBase> deadSimpleLight;
  std::shared_ptr<LightBase> deadCubeLight;
  std::map<std::string, std::shared_ptr<LightBase>> registeredLights;
  std::map<std::string, std::shared_ptr<ModelBase>> registeredModels;
  std::map<std::string, std::shared_ptr<CameraBase>> registeredCameras;

  double startTime;
  double lastTime;

  RenderManager()
      : windowManager(WindowManager::getInstance()),
        registeredLights({}),
        registeredModels({}),
        registeredCameras({}),
        startTime(glfwGetTime()),
        lastTime(glfwGetTime()) {}

public:
  RenderManager(RenderManager &) = delete;

  void registerDeadSimpleLight(std::shared_ptr<LightBase> light)
  {
    deadSimpleLight = light;
  }

  void registerDeadCubeLight(std::shared_ptr<LightBase> light)
  {
    deadCubeLight = light;
  }

  void registerLight(std::shared_ptr<LightBase> light)
  {
    registeredLights.insert(std::pair<std::string, std::shared_ptr<LightBase>>(light->getLightId(), light));
  }

  void deregisterLight(std::shared_ptr<LightBase> light)
  {
    registeredLights.erase(light->getLightId());
  }

  void deregisterLight(std::string lightId)
  {
    registeredLights.erase(lightId);
  }

  void registerModel(std::shared_ptr<ModelBase> model)
  {
    registeredModels.insert(std::pair<std::string, std::shared_ptr<ModelBase>>(model->getModelId(), model));
  }

  void deregisterModel(std::shared_ptr<ModelBase> model)
  {
    registeredModels.erase(model->getModelId());
  }

  void deregisterModel(std::string modelId)
  {
    registeredModels.erase(modelId);
  }

  void registerCamera(std::shared_ptr<CameraBase> camera)
  {
    registeredCameras.insert(std::pair<std::string, std::shared_ptr<CameraBase>>(camera->getCameraId(), camera));
  }

  void deregisterCamera(std::shared_ptr<CameraBase> camera)
  {
    registeredCameras.erase(camera->getCameraId());
  }

  void deregisterCamera(std::string cameraId)
  {
    registeredCameras.erase(cameraId);
  }

  void registerActiveCamera(std::shared_ptr<CameraBase> camera)
  {
    activeCameraId = camera->getCameraId();
  }

  void registerActiveCamera(std::string cameraId)
  {
    activeCameraId = cameraId;
  }

  std::map<ShadowBufferType, std::vector<LightDetails>> renderLights()
  {
    windowManager.switchToFrameBufferViewport();
    windowManager.setClearColor(glm::vec4(1.0, 1.0, 1.0, 1.0));

    std::map<ShadowBufferType, std::vector<LightDetails>> categorizedLights({{ShadowBufferType::SIMPLE, {}}, {ShadowBufferType::CUBE, {}}});
    auto shaderId = -1;
    for (auto light = registeredLights.begin(); light != registeredLights.end(); light++)
    {
      LightDetails lightDetails = {
          light->second->getLightPosition(),
          light->second->getProjectionMatrices()[0] * light->second->getViewMatrices()[0] * glm::mat4(),
          light->second->getLightColor(),
          light->second->getLightIntensity(),
          FRAMEBUFFER_WIDTH,
          FRAMEBUFFER_WIDTH,
          light->second->getNearPlane(),
          light->second->getFarPlane(),
          light->second->getShadowBufferDetails()->getShadowBufferTextureId()};
      categorizedLights[light->second->getShadowBufferDetails()->getShadowBufferType()].push_back(lightDetails);

      glBindFramebuffer(GL_FRAMEBUFFER, light->second->getShadowBufferDetails()->getShadowBufferId());

      windowManager.clearScreen(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

      if (shaderId != light->second->getShaderDetails()->getShaderId())
      {
        shaderId = light->second->getShaderDetails()->getShaderId();
        glUseProgram(shaderId);
      }

      auto viewMatrices = light->second->getViewMatrices();
      auto projectionMatrices = light->second->getProjectionMatrices();
      for (auto model = registeredModels.begin(); model != registeredModels.end(); model++)
      {
        auto vpMatrixCountVertexId = glGetUniformLocation(light->second->getShaderDetails()->getShaderId(), "lightDetails_vertex.vpMatrixCount");
        glUniform1i(vpMatrixCountVertexId, viewMatrices.size());
        auto vpMatrixCountGeometryId = glGetUniformLocation(light->second->getShaderDetails()->getShaderId(), "lightDetails_geometry.vpMatrixCount");
        glUniform1i(vpMatrixCountGeometryId, viewMatrices.size());
        auto vpMatrixCountFragmentId = glGetUniformLocation(light->second->getShaderDetails()->getShaderId(), "lightDetails_fragment.vpMatrixCount");
        glUniform1i(vpMatrixCountFragmentId, viewMatrices.size());

        auto lightPositionVertexId = glGetUniformLocation(light->second->getShaderDetails()->getShaderId(), "lightDetails_vertex.lightPosition");
        glUniform3f(lightPositionVertexId, lightDetails.lightPosition.x, lightDetails.lightPosition.y, lightDetails.lightPosition.z);
        auto lightPositionGeometryId = glGetUniformLocation(light->second->getShaderDetails()->getShaderId(), "lightDetails_geometry.lightPosition");
        glUniform3f(lightPositionGeometryId, lightDetails.lightPosition.x, lightDetails.lightPosition.y, lightDetails.lightPosition.z);
        auto lightPositionFragmentId = glGetUniformLocation(light->second->getShaderDetails()->getShaderId(), "lightDetails_fragment.lightPosition");
        glUniform3f(lightPositionFragmentId, lightDetails.lightPosition.x, lightDetails.lightPosition.y, lightDetails.lightPosition.z);

        auto nearPlaneVertexId = glGetUniformLocation(light->second->getShaderDetails()->getShaderId(), "projectionDetails_vertex.nearPlane");
        glUniform1f(nearPlaneVertexId, lightDetails.nearPlane);
        auto nearPlaneGeometryId = glGetUniformLocation(light->second->getShaderDetails()->getShaderId(), "projectionDetails_geometry.nearPlane");
        glUniform1f(nearPlaneGeometryId, lightDetails.nearPlane);
        auto nearPlaneFragmentId = glGetUniformLocation(light->second->getShaderDetails()->getShaderId(), "projectionDetails_fragment.nearPlane");
        glUniform1f(nearPlaneFragmentId, lightDetails.nearPlane);

        auto farPlaneVertexId = glGetUniformLocation(light->second->getShaderDetails()->getShaderId(), "projectionDetails_vertex.farPlane");
        glUniform1f(farPlaneVertexId, lightDetails.farPlane);
        auto farPlaneGeometryId = glGetUniformLocation(light->second->getShaderDetails()->getShaderId(), "projectionDetails_geometry.farPlane");
        glUniform1f(farPlaneGeometryId, lightDetails.farPlane);
        auto farPlaneFragmentId = glGetUniformLocation(light->second->getShaderDetails()->getShaderId(), "projectionDetails_fragment.farPlane");
        glUniform1f(farPlaneFragmentId, lightDetails.farPlane);

        auto modelMatrixId = glGetUniformLocation(light->second->getShaderDetails()->getShaderId(), "modelMatrix");
        auto modelMatrix = model->second->getModelMatrix();
        glUniformMatrix4fv(modelMatrixId, 1, GL_FALSE, &modelMatrix[0][0]);

        for (auto i = 0; i < viewMatrices.size(); i++)
        {
          auto vpMatrix = projectionMatrices[i] * viewMatrices[i];
          auto vpMatrixVertexId = glGetUniformLocation(light->second->getShaderDetails()->getShaderId(), ("lightDetails_vertex.vpMatrices[" + std::to_string(i) + "]").c_str());
          glUniformMatrix4fv(vpMatrixVertexId, 1, GL_FALSE, &vpMatrix[0][0]);
          auto vpMatrixGeometryId = glGetUniformLocation(light->second->getShaderDetails()->getShaderId(), ("lightDetails_geometry.vpMatrices[" + std::to_string(i) + "]").c_str());
          glUniformMatrix4fv(vpMatrixGeometryId, 1, GL_FALSE, &vpMatrix[0][0]);
          auto vpMatrixFragmentId = glGetUniformLocation(light->second->getShaderDetails()->getShaderId(), ("lightDetails_fragment.vpMatrices[" + std::to_string(i) + "]").c_str());
          glUniformMatrix4fv(vpMatrixFragmentId, 1, GL_FALSE, &vpMatrix[0][0]);
        }

        VertexAttributeArray vertexArray("VertexArray", model->second->getObjectDetails()->getVertexBufferId(), 3);

        vertexArray.enableAttribute();

        glDrawArrays(GL_TRIANGLES, 0, model->second->getObjectDetails()->getBufferSize());
      }

      glBindFramebuffer(GL_FRAMEBUFFER, 0);
    }
    return categorizedLights;
  }

  void renderModels(std::map<ShadowBufferType, std::vector<LightDetails>> categorizedLights)
  {
    windowManager.switchToWindowViewport();
    windowManager.setClearColor(glm::vec4(0.0, 0.0, 0.0, 1.0));
    windowManager.clearScreen(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    auto currentTime = glfwGetTime();
    auto deltaTime = currentTime - lastTime;
    auto totalTime = currentTime - startTime;

    auto shaderId = -1;
    auto activeCamera = registeredCameras[activeCameraId];
    auto viewMatrix = activeCamera->getViewMatrix();
    auto projectionMatrix = activeCamera->getProjectionMatrix();
    for (auto model = registeredModels.begin(); model != registeredModels.end(); model++)
    {
      if (shaderId != model->second->getShaderDetails()->getShaderId())
      {
        shaderId = model->second->getShaderDetails()->getShaderId();
        glUseProgram(shaderId);
      }

      auto modelMatrixId = glGetUniformLocation(model->second->getShaderDetails()->getShaderId(), "modelDetails.modelMatrix");
      auto viewMatrixId = glGetUniformLocation(model->second->getShaderDetails()->getShaderId(), "modelDetails.viewMatrix");
      auto projectionMatrixId = glGetUniformLocation(model->second->getShaderDetails()->getShaderId(), "modelDetails.projectionMatrix");
      auto mvpMatrixId = glGetUniformLocation(model->second->getShaderDetails()->getShaderId(), "modelDetails.mvpMatrix");

      auto totalTimeId = glGetUniformLocation(model->second->getShaderDetails()->getShaderId(), "timeDetails.totalTime");
      auto deltaTimeId = glGetUniformLocation(model->second->getShaderDetails()->getShaderId(), "timeDetails.deltaTime");

      auto ambientFactorId = glGetUniformLocation(model->second->getShaderDetails()->getShaderId(), "ambientFactor");

      auto simpleLightsCountId = glGetUniformLocation(model->second->getShaderDetails()->getShaderId(), "simpleLightsCount");
      auto cubeLightsCountId = glGetUniformLocation(model->second->getShaderDetails()->getShaderId(), "cubeLightsCount");

      auto diffuseTextureId = glGetUniformLocation(model->second->getShaderDetails()->getShaderId(), "diffuseTexture");

      auto modelMatrix = model->second->getModelMatrix();
      glUniformMatrix4fv(modelMatrixId, 1, GL_FALSE, &modelMatrix[0][0]);
      glUniformMatrix4fv(viewMatrixId, 1, GL_FALSE, &viewMatrix[0][0]);
      glUniformMatrix4fv(projectionMatrixId, 1, GL_FALSE, &projectionMatrix[0][0]);
      auto mvpMatrix = projectionMatrix * viewMatrix * modelMatrix * glm::mat4();
      glUniformMatrix4fv(mvpMatrixId, 1, GL_FALSE, &mvpMatrix[0][0]);

      glUniform1f(totalTimeId, totalTime);
      glUniform1f(deltaTimeId, deltaTime);

      glUniform1f(ambientFactorId, ambientFactor);

      glUniform1i(simpleLightsCountId, categorizedLights[ShadowBufferType::SIMPLE].size());
      glUniform1i(cubeLightsCountId, categorizedLights[ShadowBufferType::CUBE].size());

      glActiveTexture(GL_TEXTURE0);
      glBindTexture(GL_TEXTURE_2D, model->second->getTextureDetails()->getTextureId());
      glUniform1i(diffuseTextureId, 0);

      for (auto i = 0; i < categorizedLights[ShadowBufferType::SIMPLE].size(); i++)
      {
        auto lightDetails = categorizedLights[ShadowBufferType::SIMPLE][i];

        auto lightPositionVertexId = glGetUniformLocation(model->second->getShaderDetails()->getShaderId(), ("simpleLightDetails_vertex[" + std::to_string(i) + "].lightPosition").c_str());
        glUniform3f(lightPositionVertexId, lightDetails.lightPosition.x, lightDetails.lightPosition.y, lightDetails.lightPosition.z);
        auto lightPositionFragmentId = glGetUniformLocation(model->second->getShaderDetails()->getShaderId(), ("simpleLightDetails_fragment[" + std::to_string(i) + "].lightPosition").c_str());
        glUniform3f(lightPositionFragmentId, lightDetails.lightPosition.x, lightDetails.lightPosition.y, lightDetails.lightPosition.z);

        auto lightVpMatrixVertexId = glGetUniformLocation(model->second->getShaderDetails()->getShaderId(), ("simpleLightDetails_vertex[" + std::to_string(i) + "].lightVpMatrix").c_str());
        glUniformMatrix4fv(lightVpMatrixVertexId, 1, GL_FALSE, &lightDetails.lightVpMatrix[0][0]);
        auto lightVpMatrixFragmentId = glGetUniformLocation(model->second->getShaderDetails()->getShaderId(), ("simpleLightDetails_fragment[" + std::to_string(i) + "].lightVpMatrix").c_str());
        glUniformMatrix4fv(lightVpMatrixFragmentId, 1, GL_FALSE, &lightDetails.lightVpMatrix[0][0]);

        auto lightColorVertexId = glGetUniformLocation(model->second->getShaderDetails()->getShaderId(), ("simpleLightDetails_vertex[" + std::to_string(i) + "].lightColor").c_str());
        glUniform3f(lightColorVertexId, lightDetails.lightColor.r, lightDetails.lightColor.g, lightDetails.lightColor.b);
        auto lightColorFragmentId = glGetUniformLocation(model->second->getShaderDetails()->getShaderId(), ("simpleLightDetails_fragment[" + std::to_string(i) + "].lightColor").c_str());
        glUniform3f(lightColorFragmentId, lightDetails.lightColor.r, lightDetails.lightColor.g, lightDetails.lightColor.b);

        auto lightIntensityVertexId = glGetUniformLocation(model->second->getShaderDetails()->getShaderId(), ("simpleLightDetails_vertex[" + std::to_string(i) + "].lightIntensity").c_str());
        glUniform1f(lightIntensityVertexId, lightDetails.lightIntensity);
        auto lightIntensityFragmentId = glGetUniformLocation(model->second->getShaderDetails()->getShaderId(), ("simpleLightDetails_fragment[" + std::to_string(i) + "].lightIntensity").c_str());
        glUniform1f(lightIntensityFragmentId, lightDetails.lightIntensity);

        auto lightMapWidthVertexId = glGetUniformLocation(model->second->getShaderDetails()->getShaderId(), ("simpleLightDetails_vertex[" + std::to_string(i) + "].mapWidth").c_str());
        glUniform1i(lightMapWidthVertexId, lightDetails.mapWidth);
        auto lightMapWidthFragmentId = glGetUniformLocation(model->second->getShaderDetails()->getShaderId(), ("simpleLightDetails_fragment[" + std::to_string(i) + "].mapWidth").c_str());
        glUniform1i(lightMapWidthFragmentId, lightDetails.mapWidth);

        auto lightMapHeightVertexId = glGetUniformLocation(model->second->getShaderDetails()->getShaderId(), ("simpleLightDetails_vertex[" + std::to_string(i) + "].mapHeight").c_str());
        glUniform1i(lightMapHeightVertexId, lightDetails.mapHeight);
        auto lightMapHeightFragmentId = glGetUniformLocation(model->second->getShaderDetails()->getShaderId(), ("simpleLightDetails_fragment[" + std::to_string(i) + "].mapHeight").c_str());
        glUniform1i(lightMapHeightFragmentId, lightDetails.mapHeight);

        auto lightNearPlaneVertexId = glGetUniformLocation(model->second->getShaderDetails()->getShaderId(), ("simpleLightDetails_vertex[" + std::to_string(i) + "].nearPlane").c_str());
        glUniform1f(lightNearPlaneVertexId, lightDetails.nearPlane);
        auto lightNearPlaneFragmentId = glGetUniformLocation(model->second->getShaderDetails()->getShaderId(), ("simpleLightDetails_fragment[" + std::to_string(i) + "].nearPlane").c_str());
        glUniform1f(lightNearPlaneFragmentId, lightDetails.nearPlane);

        auto lightFarPlaneVertexId = glGetUniformLocation(model->second->getShaderDetails()->getShaderId(), ("simpleLightDetails_vertex[" + std::to_string(i) + "].farPlane").c_str());
        glUniform1f(lightFarPlaneVertexId, lightDetails.farPlane);
        auto lightFarPlaneFragmentId = glGetUniformLocation(model->second->getShaderDetails()->getShaderId(), ("simpleLightDetails_fragment[" + std::to_string(i) + "].farPlane").c_str());
        glUniform1f(lightFarPlaneFragmentId, lightDetails.farPlane);

        auto lightTextureId = glGetUniformLocation(model->second->getShaderDetails()->getShaderId(), ("simpleLightTextures[" + std::to_string(i) + "]").c_str());
        glActiveTexture(GL_TEXTURE0 + (i + 1));
        glBindTexture(GL_TEXTURE_2D, lightDetails.textureId);
        glUniform1i(lightTextureId, i + 1);
      }
      for (auto i = categorizedLights[ShadowBufferType::SIMPLE].size(); i < 7; i++)
      {
        auto lightTextureId = glGetUniformLocation(model->second->getShaderDetails()->getShaderId(), ("simpleLightTextures[" + std::to_string(i) + "]").c_str());
        glActiveTexture(GL_TEXTURE0 + (i + 1));
        glBindTexture(GL_TEXTURE_2D, deadSimpleLight->getShadowBufferDetails()->getShadowBufferTextureId());
        glUniform1i(lightTextureId, i + 1);
      }

      for (auto i = 0; i < categorizedLights[ShadowBufferType::CUBE].size(); i++)
      {
        auto lightDetails = categorizedLights[ShadowBufferType::CUBE][i];

        auto lightPositionVertexId = glGetUniformLocation(model->second->getShaderDetails()->getShaderId(), ("cubeLightDetails_vertex[" + std::to_string(i) + "].lightPosition").c_str());
        glUniform3f(lightPositionVertexId, lightDetails.lightPosition.x, lightDetails.lightPosition.y, lightDetails.lightPosition.z);
        auto lightPositionFragmentId = glGetUniformLocation(model->second->getShaderDetails()->getShaderId(), ("cubeLightDetails_fragment[" + std::to_string(i) + "].lightPosition").c_str());
        glUniform3f(lightPositionFragmentId, lightDetails.lightPosition.x, lightDetails.lightPosition.y, lightDetails.lightPosition.z);

        auto lightVpMatrixVertexId = glGetUniformLocation(model->second->getShaderDetails()->getShaderId(), ("cubeLightDetails_vertex[" + std::to_string(i) + "].lightVpMatrix").c_str());
        glUniformMatrix4fv(lightVpMatrixVertexId, 1, GL_FALSE, &lightDetails.lightVpMatrix[0][0]);
        auto lightVpMatrixFragmentId = glGetUniformLocation(model->second->getShaderDetails()->getShaderId(), ("cubeLightDetails_fragment[" + std::to_string(i) + "].lightVpMatrix").c_str());
        glUniformMatrix4fv(lightVpMatrixFragmentId, 1, GL_FALSE, &lightDetails.lightVpMatrix[0][0]);

        auto lightColorVertexId = glGetUniformLocation(model->second->getShaderDetails()->getShaderId(), ("cubeLightDetails_vertex[" + std::to_string(i) + "].lightColor").c_str());
        glUniform3f(lightColorVertexId, lightDetails.lightColor.r, lightDetails.lightColor.g, lightDetails.lightColor.b);
        auto lightColorFragmentId = glGetUniformLocation(model->second->getShaderDetails()->getShaderId(), ("cubeLightDetails_fragment[" + std::to_string(i) + "].lightColor").c_str());
        glUniform3f(lightColorFragmentId, lightDetails.lightColor.r, lightDetails.lightColor.g, lightDetails.lightColor.b);

        auto lightIntensityVertexId = glGetUniformLocation(model->second->getShaderDetails()->getShaderId(), ("cubeLightDetails_vertex[" + std::to_string(i) + "].lightIntensity").c_str());
        glUniform1f(lightIntensityVertexId, lightDetails.lightIntensity);
        auto lightIntensityFragmentId = glGetUniformLocation(model->second->getShaderDetails()->getShaderId(), ("cubeLightDetails_fragment[" + std::to_string(i) + "].lightIntensity").c_str());
        glUniform1f(lightIntensityFragmentId, lightDetails.lightIntensity);

        auto lightMapWidthVertexId = glGetUniformLocation(model->second->getShaderDetails()->getShaderId(), ("cubeLightDetails_vertex[" + std::to_string(i) + "].mapWidth").c_str());
        glUniform1i(lightMapWidthVertexId, lightDetails.mapWidth);
        auto lightMapWidthFragmentId = glGetUniformLocation(model->second->getShaderDetails()->getShaderId(), ("cubeLightDetails_fragment[" + std::to_string(i) + "].mapWidth").c_str());
        glUniform1i(lightMapWidthFragmentId, lightDetails.mapWidth);

        auto lightMapHeightVertexId = glGetUniformLocation(model->second->getShaderDetails()->getShaderId(), ("cubeLightDetails_vertex[" + std::to_string(i) + "].mapHeight").c_str());
        glUniform1i(lightMapHeightVertexId, lightDetails.mapHeight);
        auto lightMapHeightFragmentId = glGetUniformLocation(model->second->getShaderDetails()->getShaderId(), ("cubeLightDetails_fragment[" + std::to_string(i) + "].mapHeight").c_str());
        glUniform1i(lightMapHeightFragmentId, lightDetails.mapHeight);

        auto lightNearPlaneVertexId = glGetUniformLocation(model->second->getShaderDetails()->getShaderId(), ("cubeLightDetails_vertex[" + std::to_string(i) + "].nearPlane").c_str());
        glUniform1f(lightNearPlaneVertexId, lightDetails.nearPlane);
        auto lightNearPlaneFragmentId = glGetUniformLocation(model->second->getShaderDetails()->getShaderId(), ("cubeLightDetails_fragment[" + std::to_string(i) + "].nearPlane").c_str());
        glUniform1f(lightNearPlaneFragmentId, lightDetails.nearPlane);

        auto lightFarPlaneVertexId = glGetUniformLocation(model->second->getShaderDetails()->getShaderId(), ("cubeLightDetails_vertex[" + std::to_string(i) + "].farPlane").c_str());
        glUniform1f(lightFarPlaneVertexId, lightDetails.farPlane);
        auto lightFarPlaneFragmentId = glGetUniformLocation(model->second->getShaderDetails()->getShaderId(), ("cubeLightDetails_fragment[" + std::to_string(i) + "].farPlane").c_str());
        glUniform1f(lightFarPlaneFragmentId, lightDetails.farPlane);

        auto lightTextureId = glGetUniformLocation(model->second->getShaderDetails()->getShaderId(), ("cubeLightTextures[" + std::to_string(i) + "]").c_str());
        glActiveTexture(GL_TEXTURE0 + (i + 8));
        glBindTexture(GL_TEXTURE_CUBE_MAP, lightDetails.textureId);
        glUniform1i(lightTextureId, categorizedLights[ShadowBufferType::SIMPLE].size() + i + 8);
      }
      for (auto i = categorizedLights[ShadowBufferType::CUBE].size(); i < 8; i++)
      {
        auto lightTextureId = glGetUniformLocation(model->second->getShaderDetails()->getShaderId(), ("cubeLightTextures[" + std::to_string(i) + "]").c_str());
        glActiveTexture(GL_TEXTURE0 + (i + 8));
        glBindTexture(GL_TEXTURE_CUBE_MAP, deadCubeLight->getShadowBufferDetails()->getShadowBufferTextureId());
        glUniform1i(lightTextureId, categorizedLights[ShadowBufferType::SIMPLE].size() + i + 8);
      }

      VertexAttributeArray vertexArray("VertexArray", model->second->getObjectDetails()->getVertexBufferId(), 3);
      VertexAttributeArray uvArray("UvArray", model->second->getObjectDetails()->getUvBufferId(), 2);
      VertexAttributeArray normalArray("NormalArray", model->second->getObjectDetails()->getNormalBufferId(), 3);

      vertexArray.enableAttribute();
      uvArray.enableAttribute();
      normalArray.enableAttribute();

      glDrawArrays(GL_TRIANGLES, 0, model->second->getObjectDetails()->getBufferSize());
    }

    lastTime = currentTime;
  }

  void render()
  {
    auto categorizedLights = renderLights();
    renderModels(categorizedLights);
  }

  static RenderManager &getInstance()
  {
    return instance;
  }
};

RenderManager RenderManager::instance;
double RenderManager::ambientFactor = 0.25;

#endif