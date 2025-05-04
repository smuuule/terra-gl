#pragma once
#include "Model.h"

struct TerrainParams
{
    int size = 100;
    float scale = 1.0f;
    float heightScale = 1.0f;
    int noiseOctaves = 4;
    unsigned int seed = 0;
};

class Terrain
{
public:
    Terrain(const TerrainParams &params);
    labhelper::Model *getModel() const;
    std::vector<std::vector<float>> getHeightMap() const;

private:
    TerrainParams params;
    labhelper::Model *terrainModel;
    std::vector<std::vector<float>> heightMap;

    void generateTerrain();
    float getTerrainHeight(float x, float z);
    float fbm(float x, float z);
    float noise2D(float x, float z);
    glm::vec2 randomGradient(int ix, int iy);
    float smoothstep(float t);
    float lerp(float a, float b, float t);
};
