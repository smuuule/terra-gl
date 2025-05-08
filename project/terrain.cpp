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
            << ", heightScale: " << params.heightScale << std::endl;
  generateTerrain();
}

void Terrain::generateTerrain()
{
  terrainModel = new labhelper::Model();
  terrainModel->m_name = "Terrain";
  terrainModel->m_filename = "generated_terrain";

  std::vector<std::vector<glm::vec3>> normalMap(
      params.size, std::vector<glm::vec3>(params.size));

  heightMap.resize(params.size, std::vector<float>(params.size));

  for (int z = 0; z < params.size; z++)
  {
    for (int x = 0; x < params.size; x++)
    {
      float xPos = (x - params.size / 2.0f) * params.scale;
      float zPos = (z - params.size / 2.0f) * params.scale;
      heightMap[z][x] = perlinOctaves(xPos, zPos, 0.5f) * params.heightScale;
    }
  }

  for (int z = 0; z < params.size; z++)
  {
    for (int x = 0; x < params.size; x++)
    {
      glm::vec3 normal(0.0f, 1.0f, 0.0f);
      if (x > 0 && x < params.size - 1 && z > 0 && z < params.size - 1)
      {
        float slopeX = heightMap[z][x + 1] - heightMap[z][x - 1];
        float slopeY = heightMap[z + 1][x] - heightMap[z - 1][x];
        glm::vec3 tangentX(2.0f * params.scale, slopeX, 0.0f);
        glm::vec3 tangentZ(0.0f, slopeY, 2.0f * params.scale);
        normal = glm::normalize(glm::cross(tangentX, tangentZ));
      }
      normalMap[z][x] = normal;
    }
  }

  int verticesPerRow = params.size * 2;
  int numRows = params.size - 1;
  int borderVertices = (numRows - 1) * 2;
  int numVertices = verticesPerRow * numRows + borderVertices;

  terrainModel->m_positions.resize(numVertices);
  terrainModel->m_normals.resize(numVertices);

  int vertexIndex = 0;
  for (int z = 0; z < params.size - 1; z++)
  {
    if (z > 0)
    {
      terrainModel->m_positions[vertexIndex] =
          terrainModel->m_positions[vertexIndex - 1];
      terrainModel->m_normals[vertexIndex] =
          terrainModel->m_normals[vertexIndex - 1];
      vertexIndex++;

      float xPos = (-params.size / 2.0f) * params.scale;
      float zPos = (z - params.size / 2.0f) * params.scale;
      terrainModel->m_positions[vertexIndex] =
          glm::vec3(xPos, heightMap[z][0], zPos);
      terrainModel->m_normals[vertexIndex] = normalMap[z][0];
      vertexIndex++;
    }

    for (int x = 0; x < params.size; x++)
    {
      float xPos = (x - params.size / 2.0f) * params.scale;
      float zPos = (z - params.size / 2.0f) * params.scale;
      terrainModel->m_positions[vertexIndex] =
          glm::vec3(xPos, heightMap[z][x], zPos);
      terrainModel->m_normals[vertexIndex] = normalMap[z][x];
      vertexIndex++;

      xPos = (x - params.size / 2.0f) * params.scale;
      zPos = ((z + 1) - params.size / 2.0f) * params.scale;
      terrainModel->m_positions[vertexIndex] =
          glm::vec3(xPos, heightMap[z + 1][x], zPos);
      terrainModel->m_normals[vertexIndex] = normalMap[z + 1][x];
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

  glBindVertexArray(0);
  terrainModel->m_meshes.push_back(mesh);
}

labhelper::Model *Terrain::getModel() const { return terrainModel; }
std::vector<std::vector<float>> Terrain::getHeightMap() const { return heightMap; }

float Terrain::perlinOctaves(float x, float z, float persistance)
{
  float value = 0.0f;
  float amplitude = params.amplitude;
  float frequency = params.frequency;
  float maxValue = 0.0f;

  for (int i = 0; i < params.noiseOctaves; i++)
  {
    value += amplitude * perlin(x * frequency, z * frequency);
    maxValue += amplitude;

    amplitude *= persistance;
    frequency *= 2.0f;
  }

  return value / maxValue;
}

float Terrain::perlin(float x, float y)
{
  int X = static_cast<int>(floor(x));
  int Y = static_cast<int>(floor(y));

  float xf = x - floor(x);
  float yf = y - floor(y);

  float u = fade(xf);
  float v = fade(yf);

  glm::vec2 g00 = grad(X, Y);
  glm::vec2 g10 = grad(X + 1, Y);
  glm::vec2 g01 = grad(X, Y + 1);
  glm::vec2 g11 = grad(X + 1, Y + 1);

  float n00 = glm::dot(g00, glm::vec2(xf, yf));
  float n10 = glm::dot(g10, glm::vec2(xf - 1.0f, yf));
  float n01 = glm::dot(g01, glm::vec2(xf, yf - 1.0f));
  float n11 = glm::dot(g11, glm::vec2(xf - 1.0f, yf - 1.0f));

  float nx0 = lerp(n00, n10, u);
  float nx1 = lerp(n01, n11, u);
  float res = lerp(nx0, nx1, v);

  return res * 2.0f;
}

glm::vec2 Terrain::grad(int x, int y)
{
  unsigned int h = x * 73856093 ^ y * 19349663 ^ params.seed;
  h = h * h * h * 60493;
  h = h ^ (h >> 13);

  float angle = (h & 0xFFFF) * (2.0f * M_PI / 65536.0f);

  return glm::vec2(cos(angle), sin(angle));
}

float Terrain::fade(float t) { return t * t * t * (t * (t * 6 - 15) + 10); }

float Terrain::lerp(float a, float b, float x) { return a + x * (b - a); }
