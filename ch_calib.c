#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <complex.h>
#include <stdint.h>
#include <string.h>
#include <dma_utilities.h>
#include <ch_calib.h>

CorrectionCoefficients coeff[NUM_CHANNELS];
StoredCoefficients *stored_coeffs = NULL;
//IQSample buffer_apply[samp_cfg.samp_num * NUM_CHANNELS];
int stored_count = 0;

// 读取 DMA 数据
void read_dma_data(IQSample *buffer, uint32_t samp_num, int OFFSET) {
    memcpy(buffer, (void *)RX_BUFFER_BASE + OFFSET, samp_num * sizeof(IQSample) * NUM_CHANNELS);
}

void write_ddr_fast(IQSample *src, int samp_points)
{
   // IQSample *dst = (IQSample *)TX_BUFFER_BASE;
    int total_bytes = samp_points * NUM_CHANNELS * sizeof(IQSample);

    memcpy((UINTPTR)RX_BUFFER_BASE, src, total_bytes);

 //   Xil_DCacheFlushRange((UINTPTR)(RX_BUFFER_BASE+total_bytes), total_bytes);
  //  Xil_DCacheFlushRange((UINTPTR)dst, total_bytes);
}


// 计算大于等于 n 的最小 2 的幂
int next_power_of_two(int n) {
    int power = 1;
    while (power < n) {
        power <<= 1;
    }
    return power;
}

void fft_recursive(double complex *input, double complex *output, int n, int step) {
    if (n == 1) {
        output[0] = input[0];
        return;
    }

    double complex *even = (double complex *)malloc(n/2 * sizeof(double complex));
    double complex *odd = (double complex *)malloc(n/2 * sizeof(double complex));

    for (int i = 0; i < n/2; i++) {
        even[i] = input[i * 2 * step];
        odd[i] = input[(i * 2 + 1) * step];
    }

    fft_recursive(even, output, n/2, 1);
    fft_recursive(odd, output + n/2, n/2, 1);

    for (int k = 0; k < n/2; k++) {
        double complex t = output[k];
        double complex twiddle = cexp(-I * 2 * M_PI * k / n);
        double complex temp = twiddle * output[n/2 + k];
        output[k] = t + temp;
        output[n/2 + k] = t - temp;
    }

    free(even);
    free(odd);
}

void fft_analysis(float *i_data, float *q_data, float *fft_amplitude, float *fft_phase, int samp_num, int padded_samp_num) {
    double complex *input = (double complex *)malloc(padded_samp_num * sizeof(double complex));
    for (int i = 0; i < samp_num; i++) {
        input[i] = i_data[i] + I * q_data[i];
    }
    for (int i = samp_num; i < padded_samp_num; i++) {
        input[i] = 0;
    }

    double complex *output = (double complex *)malloc(padded_samp_num * sizeof(double complex));

    fft_recursive(input, output, padded_samp_num, 1);

    for (int i = 0; i < padded_samp_num; i++) {
        double amplitude = cabs(output[i]);
        double phase = carg(output[i]);

        fft_amplitude[i] = (float)amplitude;
        fft_phase[i] = (float)(phase * (180 / M_PI));
    }

    free(input);
    free(output);
}

void find_peak(float *fft_amplitude, float *fft_phase, int samp_num, float *peak_amplitude, float *peak_phase, int padded_samp_num) {
    float max_amplitude = 0.0f;
    int peak_index = 0;
    for (int j = 0; j < samp_num; j++) {
        if (fft_amplitude[j] > max_amplitude) {
            max_amplitude = fft_amplitude[j];
            peak_index = j;
        }
    }
    *peak_amplitude = max_amplitude;
    *peak_phase = fft_phase[peak_index];
}

void store_coefficients(float current_lo_freq, uint8_t current_filter_sel) {
    for (int i = 0; i < stored_count; i++) {
        if ((stored_coeffs[i].frequency == current_lo_freq)&&((stored_coeffs[i].ana_filter_sel == current_filter_sel))) {
            for (int j = 0; j < NUM_CHANNELS; j++) {
                stored_coeffs[i].coeff[j].amplitude = coeff[j].amplitude;
                stored_coeffs[i].coeff[j].phase = coeff[j].phase;
            }
            return;
        }
    }

    stored_coeffs = (StoredCoefficients *)realloc(stored_coeffs, (stored_count + 1) * sizeof(StoredCoefficients));
    if (stored_coeffs == NULL) {
        printf("Memory allocation failed\n");
        return;
    }

    stored_coeffs[stored_count].frequency = current_lo_freq;
	//printf("Channel %d: frequency = %.6f, lo_freq = %.6f\n", 456, stored_coeffs[stored_count].frequency, current_lo_freq);
    stored_coeffs[stored_count].ana_filter_sel = current_filter_sel;
    for (int j = 0; j < NUM_CHANNELS; j++) {
        stored_coeffs[stored_count].coeff[j].amplitude = coeff[j].amplitude;
        stored_coeffs[stored_count].coeff[j].phase = coeff[j].phase;
    }
    stored_count++;
}

void perform_phase_and_amplitude_correction(uint32_t samp_num, uint32_t current_lo_freq, int OFFSET, uint8_t ana_filter_sel) {
    int samp_points = (samp_num < FFT_SIZE) ? samp_num : FFT_SIZE;
    IQSample buffer[samp_points * NUM_CHANNELS];
    read_dma_data(buffer, samp_points, OFFSET);

    float i_data[NUM_CHANNELS][samp_points];
    float q_data[NUM_CHANNELS][samp_points];
    for (int i = 0; i < NUM_CHANNELS; i++) {
        for (int j = 0; j < samp_points; j++) {
            i_data[i][j] = (float)buffer[i + j * NUM_CHANNELS].i;
            q_data[i][j] = (float)buffer[i + j * NUM_CHANNELS].q;
        }
    }

    int padded_samp_num = next_power_of_two(samp_points);
    float fft_amplitude[NUM_CHANNELS][padded_samp_num];
    float fft_phase[NUM_CHANNELS][padded_samp_num];
    for (int i = 0; i < NUM_CHANNELS; i++) {
        fft_analysis(i_data[i], q_data[i], fft_amplitude[i], fft_phase[i], samp_points, padded_samp_num);
    }

    float reference_amplitude = 0.0f;
    float reference_phase = 0.0f;

    find_peak(fft_amplitude[0], fft_phase[0], samp_points, &reference_amplitude, &reference_phase, padded_samp_num);

    coeff[0].amplitude = 1.0f;
    coeff[0].phase = 0.0f;

    for (int i = 1; i < NUM_CHANNELS; i++) {
        float peak_amplitude, peak_phase;
        find_peak(fft_amplitude[i], fft_phase[i], samp_points, &peak_amplitude, &peak_phase, padded_samp_num);
        coeff[i].amplitude = reference_amplitude / peak_amplitude;

        float phase_diff = reference_phase - peak_phase;
        if (phase_diff > 180.0f) {
            phase_diff -= 360.0f;
        } else if (phase_diff <= -180.0f) {
            phase_diff += 360.0f;
        }

        coeff[i].phase = phase_diff;
    }

    store_coefficients(current_lo_freq / 2, ana_filter_sel);

    printf("Correction coefficients for frequency band %d MHz:\n Correction coefficients for filter %d \n", current_lo_freq / 2 , ana_filter_sel);
    for (int i = 0; i < NUM_CHANNELS; i++) {
        printf("Channel %d: Amplitude Correction = %.6f, Phase Correction = %.6f\n", i, coeff[i].amplitude, coeff[i].phase);
    }
}

void apply_correction(int16_t *i, int16_t *q, int samp_num, uint32_t current_lo_freq) {

    int samp_points = (samp_num < MAX_SAMPLES) ? samp_num : MAX_SAMPLES;
    IQSample buffer_apply[samp_points * NUM_CHANNELS];

    read_dma_data(buffer_apply, samp_points, 0);
    printf("buffer_apply[0] = %d\n", buffer_apply[0].i);
 //   float i_data[NUM_CHANNELS][samp_points];
 //   float q_data[NUM_CHANNELS][samp_points];
 //   for (int i = 0; i < NUM_CHANNELS; i++) {
 //       for (int j = 0; j < samp_points; j++) {
  //          i_data[i][j] = (float)buffer[i + j * NUM_CHANNELS].i;
  //          q_data[i][j] = (float)buffer[i + j * NUM_CHANNELS].q;
  //      }
  //  }

    StoredCoefficients *matching_coeffs = NULL;
    for (int i = 0; i < stored_count; i++) {
    	 printf(" frequency %d MHz:\n stored_coeffs[i].frequency %f \n", current_lo_freq  , stored_coeffs[i].frequency);
        if (fabs(stored_coeffs[i].frequency - current_lo_freq) < 1e-6) {
            matching_coeffs = &stored_coeffs[i];
            break;
        }
    }
    if (matching_coeffs != NULL) {
        for (int i_chan = 0; i_chan < NUM_CHANNELS; i_chan++) {
            for (int j = 0; j < samp_points; j++) {
                float current_i = buffer_apply[i_chan+NUM_CHANNELS* j].i;
                float current_q = buffer_apply[i_chan+NUM_CHANNELS* j].q;

                float corrected_i = current_i * matching_coeffs->coeff[i_chan].amplitude;
                float corrected_q = current_q * matching_coeffs->coeff[i_chan].amplitude;

                float phase = matching_coeffs->coeff[i_chan].phase;
                phase = phase * PI / 180.0f;

                float temp_i = corrected_i * cosf(phase) - corrected_q * sinf(phase);
                float temp_q = corrected_i * sinf(phase) + corrected_q * cosf(phase);

                buffer_apply[i_chan+NUM_CHANNELS* j].i = (int16_t)temp_i;
                buffer_apply[i_chan+NUM_CHANNELS* j].q = (int16_t)temp_q;
            }
        }
    } else {
        printf("No matching correction coefficients found for frequency %.2f MHz. Using default values.\n", current_lo_freq);
        for (int i_chan = 0; i_chan < NUM_CHANNELS; i_chan++) {
            for (int j = 0; j < samp_points; j++) {
                float current_i = buffer_apply[i_chan+NUM_CHANNELS* j].i;
                float current_q = buffer_apply[i_chan+NUM_CHANNELS* j].q;

                float corrected_i = current_i * coeff[i_chan].amplitude;
                float corrected_q = current_q * coeff[i_chan].amplitude;

                float phase = coeff[i_chan].phase;
                phase = phase * PI / 180.0f;
                float temp_i = corrected_i * cosf(phase) - corrected_q * sinf(phase);
                float temp_q = corrected_i * sinf(phase) + corrected_q * cosf(phase);

                buffer_apply[i_chan+NUM_CHANNELS* j].i = (int16_t)temp_i;
                buffer_apply[i_chan+NUM_CHANNELS* j].q = (int16_t)temp_q;
            }
        }
    }
    printf("after buffer_apply[0] = %d\n", buffer_apply[0].i);
    memcpy((UINTPTR)TCP_BUFFER_BASE, (UINTPTR)buffer_apply,  samp_points * NUM_CHANNELS * sizeof(IQSample));
   // printf(" after buffer[0] = %d\n", buffer[0].i);
   //write_ddr_fast(buffer_apply, samp_points);


}//需要根据MUSIC进行应用更改

void free_stored_coefficients() {
    if (stored_coeffs != NULL) {
        free(stored_coeffs);
        stored_coeffs = NULL;
        stored_count = 0;
    }
}
