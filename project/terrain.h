#pragma once
#include "Model.h"

struct TerrainParams
{
    int size = 100;
    float scale = 1.0f;
    float heightScale = 1.0f;
    int noiseOctaves = 4;
    unsigned int seed = 0;
    float amplitude = 1.0f;
    float frequency = 0.05f;
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
    static const int p[512]; // Permutation table for Perlin noise

    void generateTerrain();
    float perlinOctaves(float x, float y, float persistance);
    float perlin(float x, float y);
    glm::vec2 grad(int x, int y);
    float fade(float t);
    float lerp(float a, float b, float t);
};
