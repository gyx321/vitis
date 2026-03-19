#ifndef CORRECTION_H
#define CORRECTION_H

#include <stdint.h>
#include <complex.h>

#define u16 unsigned short
#define u8 unsigned char
#define u32 unsigned int
#define ElemType double
#define PI 3.14159265358979323846264338327950288419716939937510
#define PI2 6.28318530717958647692528676655900576839433879875021
#define MAX_SAMPLES 65536
#define FFT_SIZE 2048

#define NUM_CHANNELS 8

typedef struct {
    float amplitude;
    float phase;
} CorrectionCoefficients;

typedef struct {
    CorrectionCoefficients coeff[NUM_CHANNELS];
    float frequency;
    uint8_t ana_filter_sel;
} StoredCoefficients;

typedef struct {
    int16_t i;
    int16_t q;
} IQSample;

// 声明全局变量为 extern
extern StoredCoefficients *stored_coeffs;
extern int stored_count;

// 声明相关函数
void store_coefficients(float current_lo_freq, uint8_t current_filter_sel);
void free_stored_coefficients();

#endif // CORRECTION_H
