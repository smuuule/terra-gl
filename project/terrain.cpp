#include "terrain.h"
#include "labhelper.h"
#include "noise.h"
#include <cmath>
#include <iostream>
#include <random>
#include <vector>

Terrain::Terrain(const TerrainParams &params)
    : params(params), terrainModel(nullptr) {
  std::cout << "Generating terrain with size: " << params.size
            << ", heightScale: " << params.heightScale << std::endl;
  generateTerrain();
}

void Terrain::applyErosion(int iterations, float talusAngle) {
  for (int iter = 0; iter < iterations; iter++) {
    for (int z = 1; z < params.size - 1; z++) {
      for (int x = 1; x < params.size - 1; x++) {
        float currentHeight = heightMap[z][x];
        for (int dz = -1; dz <= 1; dz++) {
          for (int dx = -1; dx <= 1; dx++) {
            if (dx == 0 && dz == 0)
              continue;

            float neighborHeight = heightMap[z + dz][x + dx];
            float heightDiff = currentHeight - neighborHeight;

            if (heightDiff > talusAngle) {
              float sediment = (heightDiff - talusAngle) * 0.5f;
              heightMap[z][x] -= sediment;
              heightMap[z + dz][x + dx] += sediment;
            }
          }
        }
      }
    }
  }
}

float Terrain::noise(int seed, float x, float z) {
  switch (params.noiseType) {
  case NoiseType::Perlin:
    return fractal(seed, perlin, x, z, params);
  case NoiseType::Simplex:
    return fractal(seed, simplex, x, z, params);
  case NoiseType::Value:
    return valueNoise(x, z, whiteNoise, params);
  default:
    return 0.0f;
  }
}

void Terrain::generateTerrain() {
  terrainModel = new labhelper::Model();
  terrainModel->m_name = "Terrain";
  terrainModel->m_filename = "generated_terrain";

  std::vector<std::vector<glm::vec3>> normalMap(
      params.size, std::vector<glm::vec3>(params.size));

  srand(params.seed);
  whiteNoise.resize(params.size, std::vector<float>(params.size));
  for (int x = 0; x < params.size; ++x) {
    for (int z = 0; z < params.size; ++z) {
      whiteNoise[x][z] = static_cast<float>(rand()) / RAND_MAX;
    }
  }

  heightMap.resize(params.size, std::vector<float>(params.size));

  for (int z = 0; z < params.size; z++) {
    for (int x = 0; x < params.size; x++) {
      heightMap[z][x] = noise(params.seed, x, z) * params.heightScale;
    }
  }

  if (params.erosion)
    applyErosion(params.erosionIterations, params.talusAngle);

  for (int z = 0; z < params.size; z++) {
    for (int x = 0; x < params.size; x++) {
      glm::vec3 normal(0.0f, 1.0f, 0.0f);
      if (x > 0 && x < params.size - 1 && z > 0 && z < params.size - 1) {
        float slopeX = heightMap[z][x + 1] - heightMap[z][x - 1];
        float slopeY = heightMap[z + 1][x] - heightMap[z - 1][x];
        glm::vec3 tangentX(2.0f, slopeX, 0.0f);
        glm::vec3 tangentZ(0.0f, slopeY, 2.0f);
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
  for (int z = 0; z < params.size - 1; z++) {
    if (z > 0) {
      terrainModel->m_positions[vertexIndex] =
          terrainModel->m_positions[vertexIndex - 1];
      terrainModel->m_normals[vertexIndex] =
          terrainModel->m_normals[vertexIndex - 1];
      vertexIndex++;

      float xPos = (-params.size / 2.0f);
      float zPos = (z - params.size / 2.0f);
      terrainModel->m_positions[vertexIndex] =
          glm::vec3(xPos, heightMap[z][0], zPos);
      terrainModel->m_normals[vertexIndex] = normalMap[z][0];
      vertexIndex++;
    }

    for (int x = 0; x < params.size; x++) {
      float xPos = (x - params.size / 2.0f);
      float zPos = (z - params.size / 2.0f);
      terrainModel->m_positions[vertexIndex] =
          glm::vec3(xPos, heightMap[z][x], zPos);
      terrainModel->m_normals[vertexIndex] = normalMap[z][x];
      vertexIndex++;

      xPos = (x - params.size / 2.0f);
      zPos = ((z + 1) - params.size / 2.0f);
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
std::vector<std::vector<float>> Terrain::getHeightMap() const {
  return heightMap;
}
