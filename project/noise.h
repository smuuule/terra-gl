#pragma once
#include "terrain.h"

float valueNoise(float x, float z, std::vector<std::vector<float>> &whiteNoise, TerrainParams params);
float simplex(int seed, float x, float z);
float fractal(int seed, float (*noiseFunction)(int, float, float), float x, float z, TerrainParams params);
float perlin(int seed, float x, float z);
float hash(int seed, float x, float z);
glm::vec2 randGrad(int seed, int x, int z);
float smoothstep(float t);
float lerp(float a, float b, float x);
