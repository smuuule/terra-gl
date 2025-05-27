#include "noise.h"
#include "terrain.h"

float valueNoise(float x, float z, std::vector<std::vector<float>> &whiteNoise, TerrainParams params) {
  x *= params.frequency;
  z *= params.frequency;

  int gridX = static_cast<int>(floor(x));
  int gridZ = static_cast<int>(floor(z));
  float fracX = smoothstep(x - gridX);
  float fracZ = smoothstep(z - gridZ);

  float bottomLeft = whiteNoise[gridX % params.size][gridZ % params.size];
  float bottomRight = whiteNoise[(gridX + 1) % params.size][gridZ % params.size];
  float topLeft = whiteNoise[gridX % params.size][(gridZ + 1) % params.size];
  float topRight = whiteNoise[(gridX + 1) % params.size][(gridZ + 1) % params.size];

  float bottom = lerp(bottomLeft, bottomRight, fracX);
  float top = lerp(topLeft, topRight, fracX);

  return lerp(bottom, top, fracZ);
}

float cornerContrib(const glm::vec2& grad, float x, float z) {
    float t = 0.5f - x * x - z * z;
    if (t < 0.0f) {
        return 0.0f;
    } else {
        t *= t;
        return t * t * glm::dot(grad, glm::vec2(x, z));
    }
}

float simplex(int seed, float x, float z) {
    float F2 = 0.5f * (sqrt(3.0f) - 1.0f);
    float G2 = (3.0f - sqrt(3.0f)) / 6.0f;

    // Skew input space
    float s = (x + z) * F2;
    float xs = x + s;
    float zs = z + s;
    int i = floor(xs);
    int j = floor(zs);

    // Unskew back to original space
    float t = (i + j) * G2;
    float x0 = x - (i - t);
    float z0 = z - (j - t);

    // Determine simplex corner offsets,
    // second corner depends if we are in upper or lower triangle
    int i1, j1;
    if (x0 > z0) {
        i1 = 1;
        j1 = 0;
    } else {
        i1 = 0;
        j1 = 1;
    }

    float x1 = x0 - i1 + G2;
    float z1 = z0 - j1 + G2;
    float x2 = x0 - 1.0f + 2.0f * G2;
    float z2 = z0 - 1.0f + 2.0f * G2;

    // Calculate the contributions from the three corners
    float n0 = cornerContrib(randGrad(seed, i, j), x0, z0);
    float n1 = cornerContrib(randGrad(seed, i + i1, j + j1), x1, z1);
    float n2 = cornerContrib(randGrad(seed, i + 1, j + 1), x2, z2);

    return 45.23065f * (n0 + n1 + n2);
}

float fractal(int seed, float (*noiseFunction)(int, float, float), float x, float z, TerrainParams params) {
  float value = 0.0f;
  float amplitude = params.amplitude;
  float frequency = params.frequency;
  float maxValue = 0.0f;

  for (int i = 0; i < params.noiseOctaves; i++) {
    value += amplitude * noiseFunction(seed, x * frequency, z * frequency);
    maxValue += amplitude;

    amplitude *= params.persistance;
    frequency *= params.lacunarity;
  }

  return value / maxValue;
}

float perlin(int seed, float x, float z) {
  int X = static_cast<int>(floor(x));
  int Z = static_cast<int>(floor(z));

  float xf = x - floor(x);
  float zf = z - floor(z);

  float u = smoothstep(xf);
  float v = smoothstep(zf);

  glm::vec2 g00 = randGrad(seed, X, Z);
  glm::vec2 g10 = randGrad(seed, X + 1, Z);
  glm::vec2 g01 = randGrad(seed, X, Z + 1);
  glm::vec2 g11 = randGrad(seed, X + 1, Z + 1);

  float n00 = glm::dot(g00, glm::vec2(xf, zf));
  float n10 = glm::dot(g10, glm::vec2(xf - 1.0f, zf));
  float n01 = glm::dot(g01, glm::vec2(xf, zf - 1.0f));
  float n11 = glm::dot(g11, glm::vec2(xf - 1.0f, zf - 1.0f));

  float nx0 = lerp(n00, n10, u);
  float nx1 = lerp(n01, n11, u);

  return lerp(nx0, nx1, v);
}

uint hash(int seed, int x, int z) {
  unsigned int h = x * 73856093 ^ z * 19349663 ^ seed;
  h = h * h * h * 60493;
  h = h ^ (h >> 13);
  return h & 0xFFFF;
}

glm::vec2 randGrad(int seed, int x, int z) {
  float angle = hash(seed, x, z) * (2.0f * M_PI / 65536.0f);
  return glm::vec2(cos(angle), sin(angle));
}

float smoothstep(float t) { return t * t * t * (t * (t * 6 - 15) + 10); }

float lerp(float a, float b, float x) { return a + x * (b - a); }
