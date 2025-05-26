#pragma once
#include "Model.h"

enum class NoiseType {
  Perlin,
  Simplex,
  Value
};

struct TerrainParams {
  int size = 500;
  float heightScale = 10.0f;
  int noiseOctaves = 8;
  unsigned int seed = 0;
  float amplitude = 1.0f;
  float frequency = 0.015f;
  float persistance = 0.6f;
  bool erosion = true;
  int erosionIterations = 10;
  float talusAngle = 0.7f;
  NoiseType noiseType = NoiseType::Perlin;
};

class Terrain {
public:
  Terrain(const TerrainParams &params);
  labhelper::Model *getModel() const;
  std::vector<std::vector<float>> getHeightMap() const;

private:
  TerrainParams params;
  labhelper::Model *terrainModel;
  std::vector<std::vector<float>> heightMap;
  std::vector<std::vector<float>> whiteNoise;
  static const int p[512]; // Permutation table for Perlin noise

  void generateTerrain();
  void applyErosion(int iterations, float talusAngle);
  float noise(int seed, float x, float z);
  float valueNoise(float x, float z);
  float simplexNoise(int seed, float x, float z);
  float perlinOctaves(int seed, float x, float y);
  float perlin(int seed, float x, float y);
  glm::vec2 grad(int seed, int x, int y);
  float smoothstep(float t);
  float lerp(float a, float b, float t);
};
