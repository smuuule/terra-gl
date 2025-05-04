#include "terrain.h"
#include "labhelper.h"
#include <cmath>
#include <random>
#include <vector>
#include <iostream>

Terrain::Terrain(const TerrainParams &params)
    : params(params), terrainModel(nullptr)
{
  std::cout << "Generating terrain with size: " << params.size
            << ", scale: " << params.scale
            << ", heightScale: " << params.heightScale
            << ", seed: " << params.seed << std::endl;
  generateTerrain();
}

void Terrain::generateTerrain()
{
  terrainModel = new labhelper::Model();
  terrainModel->m_name = "Terrain";
  terrainModel->m_filename = "generated_terrain";

  // Generate height, normal, and color maps
  std::vector<std::vector<glm::vec3>> normalMap(
      params.size, std::vector<glm::vec3>(params.size));

  heightMap.resize(params.size, std::vector<float>(params.size));

  float minHeight = FLT_MAX, maxHeight = -FLT_MAX;

  for (int z = 0; z < params.size; z++)
  {
    for (int x = 0; x < params.size; x++)
    {
      float xPos = (x - params.size / 2.0f) * params.scale;
      float zPos = (z - params.size / 2.0f) * params.scale;
      heightMap[z][x] = getTerrainHeight(xPos, zPos) * params.heightScale;
      minHeight = std::min(minHeight, heightMap[z][x]);
      maxHeight = std::max(maxHeight, heightMap[z][x]);
    }
  }

  for (int z = 0; z < params.size; z++)
  {
    for (int x = 0; x < params.size; x++)
    {
      glm::vec3 normal(0.0f, 1.0f, 0.0f);
      if (x > 0 && x < params.size - 1 && z > 0 && z < params.size - 1)
      {
        float hL = heightMap[z][x - 1], hR = heightMap[z][x + 1];
        float hD = heightMap[z - 1][x], hU = heightMap[z + 1][x];
        glm::vec3 tangentX(2.0f * params.scale, hR - hL, 0.0f);
        glm::vec3 tangentZ(0.0f, hU - hD, 2.0f * params.scale);
        normal = glm::normalize(glm::cross(tangentX, tangentZ));
      }
      normalMap[z][x] = normal;
    }
  }

  int verticesPerRow = params.size * 2;
  int numRows = params.size - 1;
  int degenerateVertices = (numRows - 1) * 2;
  int numVertices = verticesPerRow * numRows + degenerateVertices;

  terrainModel->m_positions.resize(numVertices);
  terrainModel->m_normals.resize(numVertices);
  terrainModel->m_texture_coordinates.resize(numVertices);

  int vertexIndex = 0;
  for (int z = 0; z < params.size - 1; z++)
  {
    if (z > 0)
    {
      terrainModel->m_positions[vertexIndex] =
          terrainModel->m_positions[vertexIndex - 1];
      terrainModel->m_normals[vertexIndex] =
          terrainModel->m_normals[vertexIndex - 1];
      terrainModel->m_texture_coordinates[vertexIndex] =
          terrainModel->m_texture_coordinates[vertexIndex - 1];
      vertexIndex++;

      float xPos = (-params.size / 2.0f) * params.scale;
      float zPos = (z - params.size / 2.0f) * params.scale;
      terrainModel->m_positions[vertexIndex] =
          glm::vec3(xPos, heightMap[z][0], zPos);
      terrainModel->m_normals[vertexIndex] = normalMap[z][0];
      terrainModel->m_texture_coordinates[vertexIndex] = glm::vec2(0.0f, 0.0f);
      vertexIndex++;
    }

    for (int x = 0; x < params.size; x++)
    {
      float xPos = (x - params.size / 2.0f) * params.scale;
      float zPos = (z - params.size / 2.0f) * params.scale;
      terrainModel->m_positions[vertexIndex] =
          glm::vec3(xPos, heightMap[z][x], zPos);
      terrainModel->m_normals[vertexIndex] = normalMap[z][x];
      terrainModel->m_texture_coordinates[vertexIndex] = glm::vec2(0.0f, 0.0f);
      vertexIndex++;

      xPos = (x - params.size / 2.0f) * params.scale;
      zPos = ((z + 1) - params.size / 2.0f) * params.scale;
      terrainModel->m_positions[vertexIndex] =
          glm::vec3(xPos, heightMap[z + 1][x], zPos);
      terrainModel->m_normals[vertexIndex] = normalMap[z + 1][x];
      terrainModel->m_texture_coordinates[vertexIndex] = glm::vec2(0.0f, 0.0f);
      vertexIndex++;
    }
  }

  labhelper::Mesh mesh;
  mesh.m_name = "TerrainMesh";
  mesh.m_start_index = 0;
  mesh.m_number_of_vertices = numVertices;

  glGenVertexArrays(1, &terrainModel->m_vaob);
  glBindVertexArray(terrainModel->m_vaob);

  glGenBuffers(1, &terrainModel->m_positions_bo);
  glBindBuffer(GL_ARRAY_BUFFER, terrainModel->m_positions_bo);
  glBufferData(GL_ARRAY_BUFFER,
               terrainModel->m_positions.size() * sizeof(glm::vec3),
               terrainModel->m_positions.data(), GL_STATIC_DRAW);
  glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 0, nullptr);
  glEnableVertexAttribArray(0);

  glGenBuffers(1, &terrainModel->m_normals_bo);
  glBindBuffer(GL_ARRAY_BUFFER, terrainModel->m_normals_bo);
  glBufferData(GL_ARRAY_BUFFER,
               terrainModel->m_normals.size() * sizeof(glm::vec3),
               terrainModel->m_normals.data(), GL_STATIC_DRAW);
  glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 0, nullptr);
  glEnableVertexAttribArray(1);

  glGenBuffers(1, &terrainModel->m_texture_coordinates_bo);
  glBindBuffer(GL_ARRAY_BUFFER, terrainModel->m_texture_coordinates_bo);
  glBufferData(GL_ARRAY_BUFFER,
               terrainModel->m_texture_coordinates.size() * sizeof(glm::vec2),
               terrainModel->m_texture_coordinates.data(), GL_STATIC_DRAW);
  glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, 0, nullptr);
  glEnableVertexAttribArray(2);

  glBindVertexArray(0);
  terrainModel->m_meshes.push_back(mesh);
}

labhelper::Model *Terrain::getModel() const { return terrainModel; }
std::vector<std::vector<float>> Terrain::getHeightMap() const { return heightMap; }

float Terrain::getTerrainHeight(float x, float z)
{
  float scale = 0.05f;
  x *= scale;
  z *= scale;
  float height = fbm(x, z);
  return height * 20.0f;
}

float Terrain::fbm(float x, float z)
{
  float value = 0.0f;
  float amplitude = 1.0f;
  float frequency = 1.0f;
  float maxValue = 0.0f;

  for (int i = 0; i < params.noiseOctaves; i++)
  {
    value += amplitude * noise2D(x * frequency, z * frequency);
    maxValue += amplitude;
    amplitude *= 0.5f;
    frequency *= 2.0f;
  }

  return (value / maxValue + 1.0f) * 0.5f;
}

float Terrain::noise2D(float x, float z)
{
  int ix = static_cast<int>(floor(x));
  int iz = static_cast<int>(floor(z));
  float fx = x - ix;
  float fz = z - iz;
  glm::vec2 g00 = randomGradient(ix, iz);
  glm::vec2 g10 = randomGradient(ix + 1, iz);
  glm::vec2 g01 = randomGradient(ix, iz + 1);
  glm::vec2 g11 = randomGradient(ix + 1, iz + 1);
  float n00 = g00.x * fx + g00.y * fz;
  float n10 = g10.x * (fx - 1.0f) + g10.y * fz;
  float n01 = g01.x * fx + g01.y * (fz - 1.0f);
  float n11 = g11.x * (fx - 1.0f) + g11.y * (fz - 1.0f);
  float sx = smoothstep(fx);
  float nx0 = lerp(n00, n10, sx);
  float nx1 = lerp(n01, n11, sx);
  float sz = smoothstep(fz);
  return lerp(nx0, nx1, sz) * 5.0f;
}

glm::vec2 Terrain::randomGradient(int ix, int iy)
{
  std::mt19937 generator(params.seed); // Use the seed for noise generation
  std::uniform_real_distribution<float> distribution(-1.0f, 1.0f);
  unsigned int seed = params.seed ^ (ix * 7919 + iy * 6037);
  std::mt19937 localGen(seed);
  std::uniform_real_distribution<float> localDis(-1.0f, 1.0f);
  float angle = localDis(localGen) * 2.0f * M_PI;
  return glm::vec2(cos(angle), sin(angle));
}
float Terrain::smoothstep(float t) { return t * t * (3.0f - 2.0f * t); }

float Terrain::lerp(float a, float b, float t) { return a + t * (b - a); }
