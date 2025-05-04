#ifdef _WIN32
extern "C" _declspec(dllexport) unsigned int NvOptimusEnablement = 0x00000001;
#endif

#include <GL/glew.h>
#include <chrono>

#include <imgui.h>
#include <labhelper.h>

#include <perf.h>

#include <glm/glm.hpp>
#include <glm/gtx/transform.hpp>
using namespace glm;

#include "hdr.h"
#include "terrain.h"
#include <Model.h>

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

///////////////////////////////////////////////////////////////////////////////
// Environment
///////////////////////////////////////////////////////////////////////////////
float environment_multiplier = 1.5f;
GLuint environmentMap, irradianceMap, reflectionMap;
const std::string envmap_base_name = "001";

///////////////////////////////////////////////////////////////////////////////
// Camera parameters.
///////////////////////////////////////////////////////////////////////////////
vec3 cameraPosition(-70.0f, 50.0f, 70.0f);
vec3 cameraDirection = normalize(vec3(0.0f) - cameraPosition);
float cameraSpeed = 10.f;

vec3 worldUp(0.0f, 1.0f, 0.0f);

///////////////////////////////////////////////////////////////////////////////
// Terrain parameters.
///////////////////////////////////////////////////////////////////////////////
TerrainParams terrainParams;

///////////////////////////////////////////////////////////////////////////////
// Models
///////////////////////////////////////////////////////////////////////////////
Terrain *terrain = nullptr;

mat4 terrainModelMatrix; // Terrain model matrix

// Add these global variables for terrain height thresholds
float waterLevel = -2.0f;
float sandLevel = -0.9f;
float grassLevel = 4.0f;
float rockLevel = 14.0f;

// Add a GLuint to store the heightmap texture
GLuint heightmapTexture;

void loadShaders(bool is_reload)
{
  GLuint shader = labhelper::loadShaderProgram(
      "../project/background.vert", "../project/background.frag", is_reload);
  if (shader != 0)
  {
    backgroundProgram = shader;
  }

  shader = labhelper::loadShaderProgram("../project/terrain.vert",
                                        "../project/terrain.frag", is_reload);
  if (shader != 0)
  {
    shaderProgram = shader;
  }
}

///////////////////////////////////////////////////////////////////////////////
/// This function is called once at the start of the program and never again
///////////////////////////////////////////////////////////////////////////////
void initialize()
{
  ENSURE_INITIALIZE_ONLY_ONCE();

  ///////////////////////////////////////////////////////////////////////
  //		Load Shaders
  ///////////////////////////////////////////////////////////////////////
  loadShaders(false);

  // Generate terrain using the Terrain class
  terrainParams.size = 100;
  terrainParams.scale = 1.0f;
  terrainParams.heightScale = 1.0f;
  terrainParams.noiseOctaves = 4;
  terrainParams.seed = std::rand();
  terrain = new Terrain(terrainParams);

  terrainModelMatrix = translate(
      vec3(0.0f, -5.0f, 0.0f)); // Position the terrain below the landing pad

  // Generate and upload the heightmap texture
  glGenTextures(1, &heightmapTexture);
  glBindTexture(GL_TEXTURE_2D, heightmapTexture);

  // Create a normalized version of the heightmap for display
  std::vector<float> normalizedHeightmap;
  normalizedHeightmap.reserve(terrainParams.size * terrainParams.size * 3); // * 3 for RGB
  float minHeight = FLT_MAX;
  float maxHeight = -FLT_MAX;

  // First find min and max heights
  auto heightMap = terrain->getHeightMap();
  for (const auto &row : heightMap)
  {
    for (float height : row)
    {
      minHeight = std::min(minHeight, height);
      maxHeight = std::max(maxHeight, height);
    }
  }

  // Normalize the heightmap data to 0-1 range and convert to RGB
  for (const auto &row : heightMap)
  {
    for (float height : row)
    {
      float normalizedHeight = (height - minHeight) / (maxHeight - minHeight);
      // Add the same value for R, G, and B to create grayscale
      normalizedHeightmap.push_back(normalizedHeight); // R
      normalizedHeightmap.push_back(normalizedHeight); // G
      normalizedHeightmap.push_back(normalizedHeight); // B
    }
  }

  // Upload the normalized heightmap data to the texture
  glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, terrainParams.size, terrainParams.size, 0, GL_RGB, GL_FLOAT, normalizedHeightmap.data());
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

  glEnable(GL_DEPTH_TEST); // enable Z-buffering
  glEnable(GL_CULL_FACE);  // enables backface culling
}

void drawBackground(const mat4 &viewMatrix, const mat4 &projectionMatrix)
{
  glUseProgram(backgroundProgram);
  labhelper::setUniformSlow(backgroundProgram, "environment_multiplier",
                            environment_multiplier);
  labhelper::setUniformSlow(backgroundProgram, "inv_PV",
                            inverse(projectionMatrix * viewMatrix));
  labhelper::setUniformSlow(backgroundProgram, "camera_pos", cameraPosition);
  labhelper::drawFullScreenQuad();
}

///////////////////////////////////////////////////////////////////////////////
/// This function is used to draw the main objects on the scene
///////////////////////////////////////////////////////////////////////////////
void drawScene(GLuint currentShaderProgram, const mat4 &viewMatrix,
               const mat4 &projectionMatrix)
{
  glUseProgram(currentShaderProgram);
  // Environment
  labhelper::setUniformSlow(currentShaderProgram, "environment_multiplier",
                            environment_multiplier);

  // camera
  labhelper::setUniformSlow(currentShaderProgram, "viewInverse",
                            inverse(viewMatrix));

  // Set terrain height thresholds
  labhelper::setUniformSlow(currentShaderProgram, "waterLevel", waterLevel);
  labhelper::setUniformSlow(currentShaderProgram, "sandLevel", sandLevel);
  labhelper::setUniformSlow(currentShaderProgram, "grassLevel", grassLevel);
  labhelper::setUniformSlow(currentShaderProgram, "rockLevel", rockLevel);

  // Render terrain
  labhelper::setUniformSlow(currentShaderProgram, "modelViewProjectionMatrix",
                            projectionMatrix * viewMatrix * terrainModelMatrix);
  labhelper::setUniformSlow(currentShaderProgram, "modelViewMatrix",
                            viewMatrix * terrainModelMatrix);
  labhelper::setUniformSlow(
      currentShaderProgram, "normalMatrix",
      inverse(transpose(viewMatrix * terrainModelMatrix)));
  // Set the model matrix for world space position calculation
  labhelper::setUniformSlow(currentShaderProgram, "modelMatrix",
                            terrainModelMatrix);

  glBindVertexArray(terrain->getModel()->m_vaob);
  for (auto &mesh : terrain->getModel()->m_meshes)
  {
    glDrawArrays(GL_TRIANGLE_STRIP, mesh.m_start_index,
                 (GLsizei)mesh.m_number_of_vertices);
  }
  glBindVertexArray(0);
}

///////////////////////////////////////////////////////////////////////////////
/// This function will be called once per frame, so the code to set up
/// the scene for rendering should go here
///////////////////////////////////////////////////////////////////////////////
void display()
{
  labhelper::perf::Scope s("Display");

  ///////////////////////////////////////////////////////////////////////////
  // Check if window size has changed and resize buffers as needed
  ///////////////////////////////////////////////////////////////////////////
  {
    int w, h;
    SDL_GetWindowSize(g_window, &w, &h);
    if (w != windowWidth || h != windowHeight)
    {
      windowWidth = w;
      windowHeight = h;
    }
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
  // Draw from camera
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
    drawScene(shaderProgram, viewMatrix, projMatrix);
  }
}

///////////////////////////////////////////////////////////////////////////////
/// This function is used to update the scene according to user input
///////////////////////////////////////////////////////////////////////////////
bool handleEvents()
{
  // check events (keyboard among other)
  SDL_Event event;
  bool quitEvent = false;
  while (SDL_PollEvent(&event))
  {
    labhelper::processEvent(&event);

    if (event.type == SDL_QUIT ||
        (event.type == SDL_KEYUP && event.key.keysym.sym == SDLK_ESCAPE))
    {
      quitEvent = true;
    }
    if (event.type == SDL_KEYUP && event.key.keysym.sym == SDLK_g)
    {
      if (labhelper::isGUIvisible())
      {
        labhelper::hideGUI();
      }
      else
      {
        labhelper::showGUI();
      }
    }
    if (event.type == SDL_MOUSEBUTTONDOWN &&
        event.button.button == SDL_BUTTON_LEFT &&
        (!labhelper::isGUIvisible() || !ImGui::GetIO().WantCaptureMouse))
    {
      g_isMouseDragging = true;
      int x;
      int y;
      SDL_GetMouseState(&x, &y);
      g_prevMouseCoords.x = x;
      g_prevMouseCoords.y = y;
    }

    if (!(SDL_GetMouseState(nullptr, nullptr) & SDL_BUTTON(SDL_BUTTON_LEFT)))
    {
      g_isMouseDragging = false;
    }

    if (event.type == SDL_MOUSEMOTION && g_isMouseDragging)
    {
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

  if (state[SDL_SCANCODE_W])
  {
    cameraPosition += cameraSpeed * deltaTime * cameraDirection;
  }
  if (state[SDL_SCANCODE_S])
  {
    cameraPosition -= cameraSpeed * deltaTime * cameraDirection;
  }
  if (state[SDL_SCANCODE_A])
  {
    cameraPosition -= cameraSpeed * deltaTime * cameraRight;
  }
  if (state[SDL_SCANCODE_D])
  {
    cameraPosition += cameraSpeed * deltaTime * cameraRight;
  }
  if (state[SDL_SCANCODE_Q])
  {
    cameraPosition -= cameraSpeed * deltaTime * worldUp;
  }
  if (state[SDL_SCANCODE_E])
  {
    cameraPosition += cameraSpeed * deltaTime * worldUp;
  }
  return quitEvent;
}

///////////////////////////////////////////////////////////////////////////////
/// This function is to hold the general GUI logic
///////////////////////////////////////////////////////////////////////////////
void gui()
{
  ImGui::Begin("Controls");
  ImGui::Text("Application average %.3f ms/frame (%.1f FPS)",
              1000.0f / ImGui::GetIO().Framerate, ImGui::GetIO().Framerate);
  ImGui::SliderFloat("Environment Multiplier", &environment_multiplier, 0.0f,
                     10.0f);

  // Terrain controls
  ImGui::Separator();
  ImGui::Text("Terrain Controls");
  ImGui::SliderInt("Terrain Size", &terrainParams.size, 10, 100);
  ImGui::SliderFloat("Terrain Scale", &terrainParams.scale, 0.1f, 10.0f);
  ImGui::SliderFloat("Terrain Height Scale",
                     &terrainParams.heightScale, 0.1f, 10.0f);

  // Add height threshold controls
  ImGui::Separator();
  ImGui::Text("Terrain Height Thresholds");
  ImGui::SliderFloat("Water Level", &waterLevel, -10.0f, 0.0f);
  ImGui::SliderFloat("Sand Level", &sandLevel, waterLevel, 5.0f);
  ImGui::SliderFloat("Grass Level", &grassLevel, sandLevel, 10.0f);
  ImGui::SliderFloat("Rock Level", &rockLevel, grassLevel, 20.0f);

  // Display the heightmap in the GUI
  ImGui::Separator();
  ImGui::Text("Heightmap Preview");
  ImGui::Image((void *)(intptr_t)heightmapTexture, ImVec2(100, 100));

  // Update the heightmap texture when regenerating the terrain
  if (ImGui::Button("Generate New Terrain"))
  {
    delete terrain;
    terrainParams.seed = std::rand();
    terrain = new Terrain(terrainParams);

    // Create a normalized version of the heightmap for display
    std::vector<float> normalizedHeightmap;
    normalizedHeightmap.reserve(terrainParams.size * terrainParams.size * 3); // * 3 for RGB
    float minHeight = FLT_MAX;
    float maxHeight = -FLT_MAX;

    // First find min and max heights
    auto heightMap = terrain->getHeightMap();
    for (const auto &row : heightMap)
    {
      for (float height : row)
      {
        minHeight = std::min(minHeight, height);
        maxHeight = std::max(maxHeight, height);
      }
    }

    // Normalize the heightmap data to 0-1 range and convert to RGB
    for (const auto &row : heightMap)
    {
      for (float height : row)
      {
        float normalizedHeight = (height - minHeight) / (maxHeight - minHeight);
        // Add the same value for R, G, and B to create grayscale
        normalizedHeightmap.push_back(normalizedHeight); // R
        normalizedHeightmap.push_back(normalizedHeight); // G
        normalizedHeightmap.push_back(normalizedHeight); // B
      }
    }

    // Update the heightmap texture
    glBindTexture(GL_TEXTURE_2D, heightmapTexture);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, terrainParams.size, terrainParams.size, 0, GL_RGB, GL_FLOAT, normalizedHeightmap.data());
    glBindTexture(GL_TEXTURE_2D, 0);
  }

  ImGui::End();
}

int main(int argc, char *argv[])
{
  g_window = labhelper::init_window_SDL("OpenGL Project");

  initialize();

  bool stopRendering = false;
  auto startTime = std::chrono::system_clock::now();

  while (!stopRendering)
  {
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
