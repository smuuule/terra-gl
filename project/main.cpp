#ifdef _WIN32
extern "C" _declspec(dllexport) unsigned int NvOptimusEnablement = 0x00000001;
#endif

#include <GL/glew.h>
#include <chrono>

#include <imgui.h>
#include <imgui_internal.h>
#include <labhelper.h>

#include <perf.h>

#include <glm/glm.hpp>
#include <glm/gtx/transform.hpp>
using namespace glm;

#include "stb_image.h"

#include "hdr.h"
#include "terrain.h"
#include <Model.h>
#include <iostream>

///////////////////////////////////////////////////////////////////////////////
// Various globals
///////////////////////////////////////////////////////////////////////////////
SDL_Window *g_window = nullptr;
float currentTime = 0.0f;
float previousTime = 0.0f;
float deltaTime = 0.0f;
int windowWidth, windowHeight;

// Mouse input
ivec2 g_prevMouseCoords = {-1, -1};
bool g_isMouseDragging = false;

///////////////////////////////////////////////////////////////////////////////
// Shader programs
///////////////////////////////////////////////////////////////////////////////
GLuint shaderProgram;
GLuint backgroundProgram;
GLuint waterProgram;

///////////////////////////////////////////////////////////////////////////////
// Environment
///////////////////////////////////////////////////////////////////////////////
float environment_multiplier = 1.25f;
float lighting_multiplier = 1.0f;
GLuint environmentMap, irradianceMap, reflectionMap;
const std::string envmap_base_name = "001";

///////////////////////////////////////////////////////////////////////////////
// Camera parameters.
///////////////////////////////////////////////////////////////////////////////
vec3 cameraPosition(-70.0f, 50.0f, 70.0f);
vec3 cameraDirection = normalize(vec3(0.0f) - cameraPosition);
float cameraSpeed = 20.f;
float sunSpeedMultiplier = 1.0f;
bool timeProgress = true;

vec3 worldUp(0.0f, 1.0f, 0.0f);

///////////////////////////////////////////////////////////////////////////////
// Models
///////////////////////////////////////////////////////////////////////////////
Terrain *terrain = nullptr;

TerrainParams terrainParams;
mat4 terrainModelMatrix;

float waterLevel = -3.5f;
float sandLevel = -2.5f;
float grassLevel = 1.0f;
float rockLevel = 3.5f;
float slopeThreshold = 0.35f;

GLuint heightmapTexture;
GLuint sandTexture, grassTexture, rockTexture, snowTexture;
GLuint waterDisplacementTexture, normalWaterDisplacementTexture;
float waterDisplacementStrength = 0.007f;
float waterTiling = 16.0f;
float timeOffset = 0.0f;
float waveSpeed = 0.04f;


GLuint waterVAO, waterVBO;

vec3 sunDirection = normalize(vec3(1.0f, 0.5f, 0.0f));
float sunIntensity = 3.0f;
vec3 sunColor = vec3(1.0f, 1.0f, 0.8f);
vec3 moonColor = vec3(0.5f, 0.5f, 1.0f);

float timeOfDay = 12.0f;

///////////////////////////////////////////////////////////////////////////////
// Framebuffers
///////////////////////////////////////////////////////////////////////////////

struct FboInfo;
std::vector<FboInfo> fboList;

///////////////////////////////////////////////////////////////////////////////
/// Holds and manages a framebuffer object
///////////////////////////////////////////////////////////////////////////////
struct FboInfo {
  GLuint framebufferId;
  GLuint colorTextureTarget;
  GLuint depthRenderbuffer;
  int width;
  int height;
  bool isComplete;

  FboInfo(int w, int h) {
    isComplete = false;
    width = w;
    height = h;

    glGenFramebuffers(1, &framebufferId);
    glBindFramebuffer(GL_FRAMEBUFFER, framebufferId);

    glGenTextures(1, &colorTextureTarget);
    glBindTexture(GL_TEXTURE_2D, colorTextureTarget);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, width, height, 0, GL_RGBA,
                 GL_UNSIGNED_BYTE, nullptr);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

    glGenRenderbuffers(1, &depthRenderbuffer);
    glBindRenderbuffer(GL_RENDERBUFFER, depthRenderbuffer);
    glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH_COMPONENT32, width, height);

    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D,
                           colorTextureTarget, 0);
    glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT,
                              GL_RENDERBUFFER, depthRenderbuffer);

    GLenum status = glCheckFramebufferStatus(GL_FRAMEBUFFER);
    if (status != GL_FRAMEBUFFER_COMPLETE) {
      labhelper::fatal_error("Framebuffer not complete");
    }

    isComplete = true;
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
  }

  FboInfo()
      : isComplete(false), framebufferId(UINT32_MAX),
        colorTextureTarget(UINT32_MAX), depthRenderbuffer(UINT32_MAX), width(0),
        height(0) {};

  void resize(int w, int h) {
    width = w;
    height = h;

    glBindTexture(GL_TEXTURE_2D, colorTextureTarget);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, width, height, 0, GL_RGBA,
                 GL_UNSIGNED_BYTE, nullptr);

    glBindRenderbuffer(GL_RENDERBUFFER, depthRenderbuffer);
    glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH_COMPONENT32, width, height);
  }

  bool checkFramebufferComplete(void) {
    glBindFramebuffer(GL_FRAMEBUFFER, framebufferId);
    GLenum status = glCheckFramebufferStatus(GL_FRAMEBUFFER);
    if (status != GL_FRAMEBUFFER_COMPLETE) {
      labhelper::fatal_error("Framebuffer not complete");
    }
    return (status == GL_FRAMEBUFFER_COMPLETE);
  }
};

void loadShaders(bool is_reload) {
  GLuint shader = labhelper::loadShaderProgram(
      "../project/background.vert", "../project/background.frag", is_reload);
  if (shader != 0) {
    backgroundProgram = shader;
  }

  shader = labhelper::loadShaderProgram("../project/terrain.vert",
                                        "../project/terrain.frag", is_reload);
  if (shader != 0) {
    shaderProgram = shader;
  }

  shader = labhelper::loadShaderProgram("../project/water.vert",
                                        "../project/water.frag", is_reload);
  if (shader != 0) {
    waterProgram = shader;
  }
}

///////////////////////////////////////////////////////////////////////////////
/// This function is called once at the start of the program and never again
///////////////////////////////////////////////////////////////////////////////
void initialize() {
  ENSURE_INITIALIZE_ONLY_ONCE();

  ///////////////////////////////////////////////////////////////////////
  //		Load Shaders
  ///////////////////////////////////////////////////////////////////////
  loadShaders(false);

  ///////////////////////////////////////////////////////////////////////
  //		Load Terrain Textures
  ///////////////////////////////////////////////////////////////////////
  // Load and setup terrain textures
  glGenTextures(1, &sandTexture);
  glGenTextures(1, &grassTexture);
  glGenTextures(1, &rockTexture);
  glGenTextures(1, &snowTexture);

  glGenTextures(1, &waterDisplacementTexture);
  glGenTextures(1, &normalWaterDisplacementTexture);

  // Load water displacment texture
  int width, height, channels;
  unsigned char *data = stbi_load("../scenes/textures/water_displacement.png",
                                  &width, &height, &channels, 0);
  if (data) {
    glBindTexture(GL_TEXTURE_2D, waterDisplacementTexture);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, width, height, 0, GL_RGB,
                 GL_UNSIGNED_BYTE, data);
    glGenerateMipmap(GL_TEXTURE_2D);
    stbi_image_free(data);
  }
  data = stbi_load("../scenes/textures/normal_water_displacement.png",
                                  &width, &height, &channels, 0);
  if (data) {
    glBindTexture(GL_TEXTURE_2D, normalWaterDisplacementTexture);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, width, height, 0, GL_RGB,
                 GL_UNSIGNED_BYTE, data);
    glGenerateMipmap(GL_TEXTURE_2D);
    stbi_image_free(data);
  }

  // Load sand texture
  data =
      stbi_load("../scenes/textures/sand.jpg", &width, &height, &channels, 0);
  if (data) {
    glBindTexture(GL_TEXTURE_2D, sandTexture);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, width, height, 0, GL_RGB,
                 GL_UNSIGNED_BYTE, data);
    glGenerateMipmap(GL_TEXTURE_2D);
    stbi_image_free(data);
  }

  // Load grass texture
  data =
      stbi_load("../scenes/textures/grass.jpg", &width, &height, &channels, 0);
  if (data) {
    glBindTexture(GL_TEXTURE_2D, grassTexture);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, width, height, 0, GL_RGB,
                 GL_UNSIGNED_BYTE, data);
    glGenerateMipmap(GL_TEXTURE_2D);
    stbi_image_free(data);
  }

  // Load rock texture
  data =
      stbi_load("../scenes/textures/rock.jpg", &width, &height, &channels, 0);
  if (data) {
    glBindTexture(GL_TEXTURE_2D, rockTexture);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, width, height, 0, GL_RGB,
                 GL_UNSIGNED_BYTE, data);
    glGenerateMipmap(GL_TEXTURE_2D);
    stbi_image_free(data);
  }

  // Load snow texture
  data =
      stbi_load("../scenes/textures/snow.jpg", &width, &height, &channels, 0);
  if (data) {
    glBindTexture(GL_TEXTURE_2D, snowTexture);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, width, height, 0, GL_RGB,
                 GL_UNSIGNED_BYTE, data);
    glGenerateMipmap(GL_TEXTURE_2D);
    stbi_image_free(data);
  }

  // Set texture parameters for all terrain textures
  GLuint textures[] = {sandTexture, grassTexture, rockTexture,
                       snowTexture};
  for (GLuint tex : textures) {
    glBindTexture(GL_TEXTURE_2D, tex);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER,
                    GL_LINEAR_MIPMAP_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
  }
  glBindTexture(GL_TEXTURE_2D, 0);

  terrainParams.seed = rand();
  terrain = new Terrain(terrainParams);

  terrainModelMatrix = translate(vec3(0.0f, 0.0f, 0.0f));

  glGenTextures(1, &heightmapTexture);
  glBindTexture(GL_TEXTURE_2D, heightmapTexture);

  std::vector<float> normalizedHeightmap;
  normalizedHeightmap.reserve(terrainParams.size * terrainParams.size * 3);
  float minHeight = FLT_MAX;
  float maxHeight = -FLT_MAX;

  auto heightMap = terrain->getHeightMap();
  for (const auto &row : heightMap) {
    for (float height : row) {
      minHeight = std::min(minHeight, height);
      maxHeight = std::max(maxHeight, height);
    }
  }

  for (const auto &row : heightMap) {
    for (float height : row) {
      float normalizedHeight = (height - minHeight) / (maxHeight - minHeight);
      normalizedHeightmap.push_back(normalizedHeight);
      normalizedHeightmap.push_back(normalizedHeight);
      normalizedHeightmap.push_back(normalizedHeight);
    }
  }

  glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, terrainParams.size, terrainParams.size,
               0, GL_RGB, GL_FLOAT, normalizedHeightmap.data());
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
  glBindTexture(GL_TEXTURE_2D, 0);

  ///////////////////////////////////////////////////////////////////////
  // Load environment map
  ///////////////////////////////////////////////////////////////////////
  const int roughnesses = 8;
  std::vector<std::string> filenames;
  for (int i = 0; i < roughnesses; i++)
    filenames.push_back("../scenes/envmaps/" + envmap_base_name + "_dl_" +
                        std::to_string(i) + ".hdr");

  environmentMap = labhelper::loadHdrTexture("../scenes/envmaps/" +
                                             envmap_base_name + ".hdr");
  irradianceMap = labhelper::loadHdrTexture(
      "../scenes/envmaps/" + envmap_base_name + "_irradiance.hdr");
  reflectionMap = labhelper::loadHdrMipmapTexture(filenames);

  glEnable(GL_DEPTH_TEST);     // enable Z-buffering
  glEnable(GL_CULL_FACE);      // enables backface culling
  glEnable(GL_CLIP_DISTANCE0); // Clipping plane for water

  glGenVertexArrays(1, &waterVAO);
  glGenBuffers(1, &waterVBO);

  glBindVertexArray(waterVAO);

  float halfSize = terrainParams.size / 2.0f;
  float waterVertices[] = {-halfSize, 0.0f, -halfSize, 0.0f, 0.0f,
                           -halfSize, 0.0f, halfSize,  0.0f, 1.0f,
                           halfSize,  0.0f, -halfSize, 1.0f, 0.0f,
                           halfSize,  0.0f, halfSize,  1.0f, 1.0f};

  glBindBuffer(GL_ARRAY_BUFFER, waterVBO);
  glBufferData(GL_ARRAY_BUFFER, sizeof(waterVertices), waterVertices,
               GL_STATIC_DRAW);

  glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 5 * sizeof(float), (void *)0);
  glEnableVertexAttribArray(0);

  glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 5 * sizeof(float),
                        (void *)(3 * sizeof(float)));
  glEnableVertexAttribArray(1);

  glBindVertexArray(0);

  ///////////////////////////////////////////////////////////////////////////
  // Setup Framebuffers
  ///////////////////////////////////////////////////////////////////////////
  int w, h;
  SDL_GetWindowSize(g_window, &w, &h);
  const int numFbos = 2;
  for (int i = 0; i < numFbos; i++) {
    fboList.push_back(FboInfo(w, h));
  }
}

void drawBackground(const mat4 &viewMatrix, const mat4 &projectionMatrix) {
  glUseProgram(backgroundProgram);
  labhelper::setUniformSlow(backgroundProgram, "environment_multiplier",
                            environment_multiplier);
  labhelper::setUniformSlow(backgroundProgram, "inv_PV",
                            inverse(projectionMatrix * viewMatrix));
  labhelper::setUniformSlow(backgroundProgram, "camera_pos", cameraPosition);
  labhelper::setUniformSlow(backgroundProgram, "sunDirection", sunDirection);

  labhelper::drawFullScreenQuad();
}

void drawWater(const mat4 &viewMatrix, const mat4 &projectionMatrix) {
  glUseProgram(waterProgram);

  glActiveTexture(GL_TEXTURE0);
  glBindTexture(GL_TEXTURE_2D, fboList[0].colorTextureTarget);
  glUniform1i(glGetUniformLocation(waterProgram, "reflectionTexture"), 0);
  glActiveTexture(GL_TEXTURE1);
  glBindTexture(GL_TEXTURE_2D, fboList[1].colorTextureTarget);
  glUniform1i(glGetUniformLocation(waterProgram, "refractionTexture"), 1);
  glActiveTexture(GL_TEXTURE2);
  glBindTexture(GL_TEXTURE_2D, waterDisplacementTexture);
  glUniform1i(glGetUniformLocation(waterProgram, "displacementTexture"), 2);
  glActiveTexture(GL_TEXTURE3);
  glBindTexture(GL_TEXTURE_2D, normalWaterDisplacementTexture);
  glUniform1i(glGetUniformLocation(waterProgram, "normalDisplacementTexture"), 3);
  glUniform1f(glGetUniformLocation(waterProgram, "displacementStrength"), waterDisplacementStrength);
  glUniform1f(glGetUniformLocation(waterProgram, "waterTiling"), waterTiling);
  glUniform1f(glGetUniformLocation(waterProgram, "timeOffset"), timeOffset);
  glUniform3fv(glGetUniformLocation(waterProgram, "cameraPosition"), 1, &cameraPosition[0]);

  labhelper::setUniformSlow(waterProgram, "sunDirection", sunDirection);
  labhelper::setUniformSlow(waterProgram, "sunIntensity", sunIntensity);
  labhelper::setUniformSlow(waterProgram, "sunColor", sunColor);
  labhelper::setUniformSlow(waterProgram, "moonColor", moonColor);

  labhelper::setUniformSlow(waterProgram, "viewMatrix", viewMatrix);
  labhelper::setUniformSlow(waterProgram, "projectionMatrix", projectionMatrix);

  mat4 waterModelMatrix = translate(vec3(0.0f, waterLevel, 0.0f));
  labhelper::setUniformSlow(waterProgram, "modelMatrix", waterModelMatrix);

  glBindVertexArray(waterVAO);
  glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
  glBindVertexArray(0);
}

///////////////////////////////////////////////////////////////////////////////
/// This function is used to draw the main objects on the scene
///////////////////////////////////////////////////////////////////////////////
void drawScene(GLuint currentShaderProgram, const mat4 &viewMatrix,
               const mat4 &projectionMatrix, const vec4 &clipPlane) {
  glUseProgram(currentShaderProgram);
  // Environment
  labhelper::setUniformSlow(currentShaderProgram, "environment_multiplier",
                            environment_multiplier);

  // Bind terrain textures
  glActiveTexture(GL_TEXTURE10);
  glBindTexture(GL_TEXTURE_2D, sandTexture);
  glUniform1i(glGetUniformLocation(currentShaderProgram, "sandTexture"), 10);

  glActiveTexture(GL_TEXTURE11);
  glBindTexture(GL_TEXTURE_2D, grassTexture);
  glUniform1i(glGetUniformLocation(currentShaderProgram, "grassTexture"), 11);

  glActiveTexture(GL_TEXTURE12);
  glBindTexture(GL_TEXTURE_2D, rockTexture);
  glUniform1i(glGetUniformLocation(currentShaderProgram, "rockTexture"), 12);

  glActiveTexture(GL_TEXTURE13);
  glBindTexture(GL_TEXTURE_2D, snowTexture);
  glUniform1i(glGetUniformLocation(currentShaderProgram, "snowTexture"), 13);

  // Set texture scale
  labhelper::setUniformSlow(currentShaderProgram, "textureScale", 10.0f);

  // camera
  labhelper::setUniformSlow(currentShaderProgram, "viewInverse",
                            inverse(viewMatrix));

  // Set terrain height thresholds
  labhelper::setUniformSlow(currentShaderProgram, "sandLevel", sandLevel);
  labhelper::setUniformSlow(currentShaderProgram, "grassLevel", grassLevel);
  labhelper::setUniformSlow(currentShaderProgram, "rockLevel", rockLevel);
  labhelper::setUniformSlow(currentShaderProgram, "slopeThreshold",
                            slopeThreshold);

  labhelper::setUniformSlow(currentShaderProgram, "sunDirection", sunDirection);
  labhelper::setUniformSlow(currentShaderProgram, "sunIntensity", sunIntensity);
  labhelper::setUniformSlow(currentShaderProgram, "clipNormal",
                            vec3(clipPlane));
  labhelper::setUniformSlow(currentShaderProgram, "clipHeight", clipPlane.w);

  labhelper::setUniformSlow(currentShaderProgram, "sunColor", sunColor);
  labhelper::setUniformSlow(currentShaderProgram, "moonColor", moonColor);

  // Render terrain
  labhelper::setUniformSlow(currentShaderProgram, "modelViewProjectionMatrix",
                            projectionMatrix * viewMatrix * terrainModelMatrix);
  labhelper::setUniformSlow(currentShaderProgram, "modelViewMatrix",
                            viewMatrix * terrainModelMatrix);
  labhelper::setUniformSlow(
      currentShaderProgram, "normalMatrix",
      inverse(transpose(viewMatrix * terrainModelMatrix)));
  labhelper::setUniformSlow(currentShaderProgram, "modelMatrix",
                            terrainModelMatrix);

  glBindVertexArray(terrain->getModel()->m_vaob);
  for (auto &mesh : terrain->getModel()->m_meshes) {
    glDrawArrays(GL_TRIANGLE_STRIP, mesh.m_start_index,
                 (GLsizei)mesh.m_number_of_vertices);
  }
  glBindVertexArray(0);
}

///////////////////////////////////////////////////////////////////////////////
/// This function will be called once per frame, so the code to set up
/// the scene for rendering should go here
///////////////////////////////////////////////////////////////////////////////
void display() {
  labhelper::perf::Scope s("Display");

  int w, h;
  SDL_GetWindowSize(g_window, &w, &h);
  if (w != windowWidth || h != windowHeight) {
    windowWidth = w;
    windowHeight = h;
  }

  ///////////////////////////////////////////////////////////////////////////
  // setup matrices
  ///////////////////////////////////////////////////////////////////////////
  mat4 projMatrix = perspective(radians(45.0f),
                                static_cast<float>(windowWidth) /
                                    static_cast<float>(windowHeight),
                                5.0f, 2000.0f);
  mat4 viewMatrix =
      lookAt(cameraPosition, cameraPosition + cameraDirection, worldUp);

  ///////////////////////////////////////////////////////////////////////////
  // Bind the environment map(s) to unused texture units
  ///////////////////////////////////////////////////////////////////////////
  glActiveTexture(GL_TEXTURE6);
  glBindTexture(GL_TEXTURE_2D, environmentMap);
  glActiveTexture(GL_TEXTURE7);
  glBindTexture(GL_TEXTURE_2D, irradianceMap);
  glActiveTexture(GL_TEXTURE8);
  glBindTexture(GL_TEXTURE_2D, reflectionMap);
  glActiveTexture(GL_TEXTURE0);

  ///////////////////////////////////////////////////////////////////////////
  // Reflection FBO
  ///////////////////////////////////////////////////////////////////////////
  FboInfo &reflectionFbo = fboList[0];
  glBindFramebuffer(GL_FRAMEBUFFER, reflectionFbo.framebufferId);

  glViewport(0, 0, reflectionFbo.width, reflectionFbo.height);
  glClearColor(0.2f, 0.2f, 0.8f, 1.0f);
  glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

  vec3 underCamPos = cameraPosition;
  underCamPos.y = waterLevel - (cameraPosition.y - waterLevel);
  vec3 underCamDir = cameraDirection;
  underCamDir.y = -underCamDir.y;
  mat4 underViewMatrix =
      lookAt(underCamPos, underCamPos + underCamDir, worldUp);

  drawBackground(underViewMatrix, projMatrix);
  drawScene(shaderProgram, underViewMatrix, projMatrix,
            vec4(0.0f, 1.0f, 0.0f, -waterLevel));

  ///////////////////////////////////////////////////////////////////////////
  // Refraction FBO
  ///////////////////////////////////////////////////////////////////////////
  FboInfo &refractionFbo = fboList[1];
  glBindFramebuffer(GL_FRAMEBUFFER, refractionFbo.framebufferId);

  glViewport(0, 0, refractionFbo.width, refractionFbo.height);
  glClearColor(0.2f, 0.2f, 0.8f, -1.0f);
  glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

  drawBackground(viewMatrix, projMatrix);
  drawScene(shaderProgram, viewMatrix, projMatrix,
            vec4(0.0f, -1.0f, 0.0f, waterLevel));

  ///////////////////////////////////////////////////////////////////////////
  // Draw final scene
  ///////////////////////////////////////////////////////////////////////////
  glBindFramebuffer(GL_FRAMEBUFFER, 0);
  glViewport(0, 0, windowWidth, windowHeight);
  glClearColor(0.2f, .2f, .8f, 1.f);
  glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

  {
    labhelper::perf::Scope s("Background");
    drawBackground(viewMatrix, projMatrix);
  }
  {
    labhelper::perf::Scope s("Scene");
    drawScene(shaderProgram, viewMatrix, projMatrix,
              vec4(0.0f, -1.0f, 0.0f, 1000));
  }
  // Wave offset
  timeOffset = fmod(timeOffset + waveSpeed * deltaTime, 1.0f);
  {
    labhelper::perf::Scope s("Water");
    drawWater(viewMatrix, projMatrix);
  }

  // Time progression for sundirection
  if (timeProgress) {
    timeOfDay += deltaTime * sunSpeedMultiplier;
    timeOfDay = fmod(timeOfDay, 24.0f);
  }
  float verticalRadians = glm::radians((timeOfDay / 24.0f) * 360.0f - 90.0f);
  sunDirection =
      normalize(vec3(cos(verticalRadians), sin(verticalRadians), 0.0f));
  
  // Varies between 0 and 1
  float timeFactor = (sin(verticalRadians) + 1.0f) * 0.5f;
  float smoothFactor = smoothstep(0.0f, 1.0f, timeFactor);
  // Blend between light values for day and night
  environment_multiplier = mix(0.3f, 1.25f, smoothFactor) * lighting_multiplier;
}

///////////////////////////////////////////////////////////////////////////////
/// This function is used to update the scene according to user input
///////////////////////////////////////////////////////////////////////////////
bool handleEvents() {
  // check events (keyboard among other)
  SDL_Event event;
  bool quitEvent = false;
  while (SDL_PollEvent(&event)) {
    labhelper::processEvent(&event);

    if (event.type == SDL_QUIT ||
        (event.type == SDL_KEYUP && event.key.keysym.sym == SDLK_ESCAPE)) {
      quitEvent = true;
    }
    if (event.type == SDL_KEYUP && event.key.keysym.sym == SDLK_g) {
      if (labhelper::isGUIvisible()) {
        labhelper::hideGUI();
      } else {
        labhelper::showGUI();
      }
    }
    if (event.type == SDL_MOUSEBUTTONDOWN &&
        event.button.button == SDL_BUTTON_LEFT &&
        (!labhelper::isGUIvisible() || !ImGui::GetIO().WantCaptureMouse)) {
      g_isMouseDragging = true;
      int x;
      int y;
      SDL_GetMouseState(&x, &y);
      g_prevMouseCoords.x = x;
      g_prevMouseCoords.y = y;
    }

    if (!(SDL_GetMouseState(nullptr, nullptr) & SDL_BUTTON(SDL_BUTTON_LEFT))) {
      g_isMouseDragging = false;
    }

    if (event.type == SDL_MOUSEMOTION && g_isMouseDragging) {
      // More info at https://wiki.libsdl.org/SDL_MouseMotionEvent
      int delta_x = event.motion.x - g_prevMouseCoords.x;
      int delta_y = event.motion.y - g_prevMouseCoords.y;
      float rotationSpeed = 0.1f;
      mat4 yaw = rotate(rotationSpeed * deltaTime * -delta_x, worldUp);
      mat4 pitch = rotate(rotationSpeed * deltaTime * -delta_y,
                          normalize(cross(cameraDirection, worldUp)));
      cameraDirection = vec3(pitch * yaw * vec4(cameraDirection, 0.0f));
      g_prevMouseCoords.x = event.motion.x;
      g_prevMouseCoords.y = event.motion.y;
    }
  }

  // check keyboard state (which keys are still pressed)
  const uint8_t *state = SDL_GetKeyboardState(nullptr);
  vec3 cameraRight = cross(cameraDirection, worldUp);

  if (state[SDL_SCANCODE_W]) {
    cameraPosition += cameraSpeed * deltaTime * cameraDirection;
  }
  if (state[SDL_SCANCODE_S]) {
    cameraPosition -= cameraSpeed * deltaTime * cameraDirection;
  }
  if (state[SDL_SCANCODE_A]) {
    cameraPosition -= cameraSpeed * deltaTime * cameraRight;
  }
  if (state[SDL_SCANCODE_D]) {
    cameraPosition += cameraSpeed * deltaTime * cameraRight;
  }
  if (state[SDL_SCANCODE_Q] || state[SDL_SCANCODE_LCTRL]) {
    cameraPosition -= cameraSpeed * deltaTime * worldUp;
  }
  if (state[SDL_SCANCODE_E] || state[SDL_SCANCODE_SPACE]) {
    cameraPosition += cameraSpeed * deltaTime * worldUp;
  }
  if (state[SDL_SCANCODE_LSHIFT]) {
    cameraSpeed = 40.f;
  } else {
    cameraSpeed = 20.f;
  }
  return quitEvent;
}

///////////////////////////////////////////////////////////////////////////////
/// This function is to hold the general GUI logic
///////////////////////////////////////////////////////////////////////////////
void gui() {
  ImGui::SetNextWindowSize(ImVec2(400, 600), ImGuiCond_FirstUseEver);
  ImGui::Begin("Controls");
  ImGui::Text("Application average %.3f ms/frame (%.1f FPS)",
              1000.0f / ImGui::GetIO().Framerate, ImGui::GetIO().Framerate);

  if (ImGui::CollapsingHeader("Terrain Thresholds")) {
    ImGui::SliderFloat("Water Level", &waterLevel, -10.0f, 0.0f);
    ImGui::SliderFloat("Sand Level", &sandLevel, waterLevel, 5.0f);
    ImGui::SliderFloat("Grass Level", &grassLevel, sandLevel, 10.0f);
    ImGui::SliderFloat("Rock Level", &rockLevel, grassLevel, 20.0f);
    ImGui::SliderFloat("Slope Threshold", &slopeThreshold, 0.0f, 1.0f);
  }

  if (ImGui::CollapsingHeader("Terrain Generation")) {
    if (ImGui::SliderInt("Terrain Size", &terrainParams.size, 100, 1000) ||
        ImGui::SliderFloat("Terrain Height Scale", &terrainParams.heightScale,
                           5.0f, 50.0f) ||
        ImGui::Checkbox("Apply Erosion", &terrainParams.erosion) ||
        ImGui::SliderInt("Erosion Iterations", &terrainParams.erosionIterations,
                         1, 100) ||
        ImGui::SliderFloat("Talus Angle", &terrainParams.talusAngle, 0.0f,
                           2.0f) ||
        ImGui::SliderInt("Noise Octaves", &terrainParams.noiseOctaves, 1, 16) ||
        ImGui::SliderFloat("Noise Amplitude", &terrainParams.amplitude, 0.1f,
                           20.0f) ||
        ImGui::SliderFloat("Noise Frequency", &terrainParams.frequency, 0.001f,
                           0.2f) ||
        ImGui::SliderFloat("Noise Persistance", &terrainParams.persistance,
                           0.0f, 1.0f) ||
        ImGui::SliderFloat("Noise Lacunarity", &terrainParams.lacunarity, 1.0f,
                           4.0f)) {
      delete terrain;
      terrain = new Terrain(terrainParams);

      glBindTexture(GL_TEXTURE_2D, heightmapTexture);

      std::vector<float> normalizedHeightmap;
      normalizedHeightmap.reserve(terrainParams.size * terrainParams.size * 3);

      float minHeight = FLT_MAX;
      float maxHeight = -FLT_MAX;

      auto heightMap = terrain->getHeightMap();
      for (const auto &row : heightMap) {
        for (float height : row) {
          minHeight = std::min(minHeight, height);
          maxHeight = std::max(maxHeight, height);
        }
      }

      for (const auto &row : heightMap) {
        for (float height : row) {
          float normalizedHeight =
              (height - minHeight) / (maxHeight - minHeight);
          normalizedHeightmap.push_back(normalizedHeight);
          normalizedHeightmap.push_back(normalizedHeight);
          normalizedHeightmap.push_back(normalizedHeight);
        }
      }

      glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, terrainParams.size,
                   terrainParams.size, 0, GL_RGB, GL_FLOAT,
                   normalizedHeightmap.data());
      glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
      glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    }

    const char *noiseTypes[] = {"Perlin", "Simplex", "Value"};
    int currentNoiseType = static_cast<int>(terrainParams.noiseType);
    if (ImGui::Combo("Noise Type", &currentNoiseType, noiseTypes,
                     sizeof(noiseTypes) / sizeof(noiseTypes[0]))) {
      terrainParams.noiseType = static_cast<NoiseType>(currentNoiseType);
      delete terrain;
      terrain = new Terrain(terrainParams);

      glBindTexture(GL_TEXTURE_2D, heightmapTexture);

      std::vector<float> normalizedHeightmap;
      normalizedHeightmap.reserve(terrainParams.size * terrainParams.size * 3);

      float minHeight = FLT_MAX;
      float maxHeight = -FLT_MAX;

      auto heightMap = terrain->getHeightMap();
      for (const auto &row : heightMap) {
        for (float height : row) {
          minHeight = std::min(minHeight, height);
          maxHeight = std::max(maxHeight, height);
        }
      }

      for (const auto &row : heightMap) {
        for (float height : row) {
          float normalizedHeight =
              (height - minHeight) / (maxHeight - minHeight);
          normalizedHeightmap.push_back(normalizedHeight);
          normalizedHeightmap.push_back(normalizedHeight);
          normalizedHeightmap.push_back(normalizedHeight);
        }
      }

      glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, terrainParams.size,
                   terrainParams.size, 0, GL_RGB, GL_FLOAT,
                   normalizedHeightmap.data());
      glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
      glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    }

    ImGui::Text("Noise Map");
    ImGui::Image((void *)(intptr_t)heightmapTexture, ImVec2(100, 100));

    ImGui::InputInt("Seed", (int *)&terrainParams.seed);
    if (ImGui::Button("Random Seed")) {
      terrainParams.seed = rand();
      delete terrain;
      terrain = new Terrain(terrainParams);

      float halfSize = terrainParams.size / 2.0f;
      float waterVertices[] = {-halfSize, 0.0f, -halfSize, 0.0f, 0.0f,
                               -halfSize, 0.0f, halfSize,  0.0f, 1.0f,
                               halfSize,  0.0f, -halfSize, 1.0f, 0.0f,
                               halfSize,  0.0f, halfSize,  1.0f, 1.0f};

      glBindBuffer(GL_ARRAY_BUFFER, waterVBO);
      glBufferData(GL_ARRAY_BUFFER, sizeof(waterVertices), waterVertices,
                   GL_STATIC_DRAW);
    }
  }

  if (ImGui::CollapsingHeader("Sun and Lighting")) {
    ImGui::SliderFloat("Environment Multiplier", &lighting_multiplier, 0.0f, 5.0f);
    ImGui::SliderFloat("Sun Intensity", &sunIntensity, 0.0f, 5.0f);
    ImGui::ColorEdit3("Sun Color", &sunColor[0]);
    ImGui::ColorEdit3("Moon Color", &moonColor[0]);

    int hour = static_cast<int>(timeOfDay);
    int minute = static_cast<int>((timeOfDay - hour) * 60);
    char timeLabel[16];
    snprintf(timeLabel, sizeof(timeLabel), "%02d:%02d", hour, minute);
    ImGui::SliderFloat("Time of Day", &timeOfDay, 0.0f, 24.0f, timeLabel);

    ImGui::SliderFloat("Sun Speed Multiplier", &sunSpeedMultiplier, 1.0f,
                       50.0f);
    ImGui::Checkbox("Time Progression", &timeProgress);
  }

  if (ImGui::CollapsingHeader("Water Settings")) {
    ImGui::SliderFloat("Displacement Strength", &waterDisplacementStrength, 0.0f, 0.1f);
    ImGui::SliderFloat("Water Tiling", &waterTiling, 0.1f, 32.0f);
    ImGui::SliderFloat("Wave Speed", &waveSpeed, 0.0f, 1.0f);
  }

  ImGui::End();
}

int main(int argc, char *argv[]) {
  g_window = labhelper::init_window_SDL("OpenGL Project");
  SDL_GetWindowSize(g_window, &windowWidth, &windowHeight);
  initialize();

  bool stopRendering = false;
  auto startTime = std::chrono::system_clock::now();

  while (!stopRendering) {
    // update currentTime
    std::chrono::duration<float> timeSinceStart =
        std::chrono::system_clock::now() - startTime;
    previousTime = currentTime;
    currentTime = timeSinceStart.count();
    deltaTime = currentTime - previousTime;

    // check events (keyboard among other)
    stopRendering = handleEvents();

    // Inform imgui of new frame
    labhelper::newFrame(g_window);

    // render to window
    display();

    // Render overlay GUI.
    gui();

    // Finish the frame and render the GUI
    labhelper::finishFrame();

    // Swap front and back buffer. This frame will now been displayed.
    SDL_GL_SwapWindow(g_window);
  }
  // Free Models
  delete terrain;

  // Shut down everything. This includes the window and all other subsystems.
  labhelper::shutDown(g_window);
  return 0;
}
