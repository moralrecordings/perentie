#ifndef SIMPLEX_H
#define SIMPLEX_H

#include <stdlib.h>

// https://github.com/SRombauts/SimplexNoise ported to C

float simplex_noise_1d(float x);
float simplex_noise_2d(float x, float y);
float simplex_noise_3d(float x, float y, float z);
float simplex_fractal_1d(
    float frequency, float amplitude, float lacunarity, float persistence, size_t octaves, float x);
float simplex_fractal_2d(
    float frequency, float amplitude, float lacunarity, float persistence, size_t octaves, float x, float y);
float simplex_fractal_3d(
    float frequency, float amplitude, float lacunarity, float persistence, size_t octaves, float x, float y, float z);

#endif // SIMPLEX_H
