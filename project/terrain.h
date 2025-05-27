#pragma once
#include "Model.h"

enum class NoiseType {
  Perlin,
  Simplex,
  Value
};

struct TerrainParams {
  int size = 500;
  float heightScale = 15.0f;
  int noiseOctaves = 8;
  unsigned int seed = 0;
  float amplitude = 1.0f;
  float frequency = 0.015f;
  float persistance = 0.5f;
  float lacunarity = 1.85f;
  bool erosion = true;
  int erosionIterations = 10;
  float talusAngle = 0.7f;
  NoiseType noiseType = NoiseType::Simplex;
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

  void generateTerrain();
  void applyErosion(int iterations, float talusAngle);
  float noise(int seed, float x, float z);
};
