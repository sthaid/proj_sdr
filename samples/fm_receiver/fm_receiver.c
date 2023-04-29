// xxx restore this file
/*
  Software defined radio wideband FM receiver

  Copyright (c) 2013, Cosmin Gorgovan
  All rights reserved.

  Redistribution and use in source and binary forms, with or without
  modification, are permitted provided that the following conditions are met:
      * Redistributions of source code must retain the above copyright
        notice, this list of conditions and the following disclaimer.
      * Redistributions in binary form must reproduce the above copyright
        notice, this list of conditions and the following disclaimer in the
        documentation and/or other materials provided with the distribution.
      * Neither the name of the project's author nor the
        names of its contributors may be used to endorse or promote products
        derived from this software without specific prior written permission.

  THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND
  ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
  WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
  DISCLAIMED. IN NO EVENT SHALL <COPYRIGHT HOLDER> BE LIABLE FOR ANY
  DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
  (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
  LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
  ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
  (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
  SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
*/

#include <stdlib.h>
#include <stdio.h>
#include <errno.h>
#include <getopt.h>
#include <unistd.h>
#include <string.h>
#include <assert.h>
#include <complex.h>
#include <math.h>
#include <fftw3.h>

#include "rtl-sdr.h"

//#define DEFAULT_SAMPLE_RATE   500000
#define DEFAULT_SAMPLE_RATE    1000000
#define DEFAULT_DEVICE_INDEX  0
#define BUFFER_SIZE           (1024*8)
//#define BUFFER_SIZE           (1024*4)
#define FFT_SIZE              (BUFFER_SIZE/2)
//#define DEFAULT_FREQ         97700000
//#define DEFAULT_FREQ          100700000
//#define START_FREQ               90100000
//#define START_FREQ               103500000
#define START_FREQ               100700000


unsigned int freq = START_FREQ;

FILE *record_file = NULL;
rtlsdr_dev_t *device;

typedef int (*get_samples_f)(uint8_t *buffer, int buffer_len);

// File reader
int get_samples_file(uint8_t *buffer, int buffer_len) {
  int len;
  len = fread(buffer, sizeof(uint8_t), buffer_len, record_file);
  
  return len == buffer_len;
}

// rtl-sdr reader
int get_samples_rtl_sdr(uint8_t *buffer, int buffer_len) {
  int len;
  rtlsdr_read_sync(device, buffer, buffer_len, &len);
  
  return len == buffer_len;
}

static int count;
#if 1
void receive(get_samples_f get_samples , int buffer_size) 
{
  uint8_t buffer[BUFFER_SIZE];
  double complex sample;
  double complex product;
  double complex prev_sample = 0 + 0 * I;
  
  int fft_size = buffer_size/2;
  int16_t output_buffer[fft_size];
  double fft_in[fft_size];
  double fft_out[fft_size];
  
  fftw_complex *filter_mid = (fftw_complex *) fftw_malloc(sizeof(fftw_complex) * (fft_size));
  fftw_plan plan_filter_in = fftw_plan_dft_r2c_1d(fft_size, fft_in, filter_mid, FFTW_ESTIMATE);
  
  fftw_complex *filter_mid2 = (fftw_complex *) fftw_malloc(sizeof(fftw_complex) * (fft_size));
  fftw_plan plan_filter_out = fftw_plan_dft_c2r_1d(fft_size, filter_mid2, fft_out, FFTW_ESTIMATE);
  
  while(get_samples(buffer, buffer_size) == 1) {
#if 0
    count += buffer_size/2;
    if (count > 500000) {
        count = 0;
        freq += 200000;
        rtlsdr_set_center_freq(device, freq);
        fprintf(stderr, "freq = %d\n", freq);
    }
#endif

    /* Compute amplitudes in the time domain
       from phase difference between succesive samples */
#if 1
    for (int i = 0; i < buffer_size; i+=2) {
      sample = (buffer[i] - 127) + (buffer[i+1] - 127) * I;
      product = sample * conj(prev_sample);
      fft_in[i/2] = atan2(cimag(product), creal(product));
      prev_sample = sample;
    }
#else
    for (int i = 0; i < buffer_size; i+=2) {
      sample = (buffer[i] - 127) + (buffer[i+1] - 127) * I;
      fft_in[i/2] = carg(sample) - carg(prev_sample);
      prev_sample = sample;
    }
#endif
    
#if 1
    fftw_execute(plan_filter_in);
    filter_mid2[0] = 0;
    int cutoff = (fft_size)/50;  // xxx was 14
    //fprintf(stderr, "hello %d\n", count++);
    for (int i = 1; i < cutoff;i++) {
      filter_mid2[i] = filter_mid[i];
      filter_mid2[fft_size-i] = conj(filter_mid2[i]);
    }
    for (int i = cutoff; i < fft_size/2;i++) {
      filter_mid2[i] = 0 + 0 *I;
      filter_mid2[fft_size-i] = conj(filter_mid2[i]);
    }
    
    fftw_execute(plan_filter_out);

    int b = 0;
    for (int i = 0; i < fft_size; i+=50) { //xxx
      output_buffer[b++] = (short)(creal(fft_out[i]) * 10000.0 / (double)fft_size );
      //if (count++ < 2000) {
        //fprintf(stderr, "%d\n", output_buffer[b-1]);
      //}
    }
#else
    int b = 0;
    for (int i = 0; i < fft_size; i+=50) { //xxx
      //output_buffer[b++] = (short)(creal(fft_in[i]) * 10000.0 / (double)fft_size );
      output_buffer[b++] = (short)(creal(fft_in[i]) * 10000.0);
      //if (count++ < 2000) {
        //fprintf(stderr, "%d\n", output_buffer[b-1]);
      //}
    }
#endif
    
    fwrite(output_buffer, sizeof(int16_t), b, stdout);
  }
}
#else
void receive(get_samples_f get_samples , int buffer_size) 
{
    uint8_t buffer[BUFFER_SIZE];
    int16_t out[BUFFER_SIZE];
    int i, b;

    while (get_samples(buffer, buffer_size) == 1) {
        b = 0;
        for (i = 0; i < buffer_size; i += 200) {
            out[b++] = (buffer[i] - 127) * 100;;

            if (count++ < 2000) {
                fprintf(stderr, "%d\n", out[b-1]);
            }
        }

        fwrite(out, 1, b, stdout);
        fprintf(stderr, "b %d\n", b);
    }
}
#endif

void print_usage() {
  fprintf(stderr,
"Usage: fm_receiver [OPTIONS]\n\n\
Valid options:\n\
  -r <file>         Use recorded data from <file> instead of an rtl-sdr device\n\
  -f <frequency>    Frequency to tune to, in Hz (default: %.2f MHz)\n\
  -d <device_index> Rtl-sdr device index (default: 0)\n\
  -h                Show this\n",
  START_FREQ/1000000.0);
}

//https://github.com/keenerd/rtl-sdr-misc/blob/master/rtl-sdl/rtl_power_lite.c
void gain_default(void)
{
    int count;
    int* gains;
    count = rtlsdr_get_tuner_gains(device, NULL);
    fprintf(stderr, "count %d\n", count);
    if (count <= 0)
        {return;}
    gains = malloc(sizeof(int) * count);
    count = rtlsdr_get_tuner_gains(device, gains);
    rtlsdr_set_tuner_gain(device, gains[count-1]);
    fprintf(stderr, "GAIN %d\n", gains[count-1]);
    free(gains);
}

int main(int argc, char **argv) 
{
  int opt;
  char *end_ptr;

  /* SDR settings */
  uint32_t device_index = DEFAULT_DEVICE_INDEX;
  uint32_t sample_rate = DEFAULT_SAMPLE_RATE;
  int buffer_size = BUFFER_SIZE;

  freq = START_FREQ; 

  /* Parse command line arguments */
  errno = 0;
  while ((opt = getopt(argc, argv, "f:d:r:h")) != -1) {
    switch (opt) {
      case 'r':
        record_file = fopen(optarg, "r");
        if (record_file == NULL) {
          fprintf(stderr, "Error opening record file: %s\n", strerror(errno));
          exit(EXIT_FAILURE);
        }
        break;
      case 'f':
        freq = strtol(optarg, &end_ptr, 10);
        if (errno != 0 || *end_ptr != '\0') {
          fprintf(stderr, "Invalid frequency specified.\n");
          print_usage();
          exit(EXIT_FAILURE);
        }
        break;
      case 'd':
        device_index = strtol(optarg, &end_ptr, 10);
        if (errno != 0 || *end_ptr != '\0') {
          fprintf(stderr, "Invalid device index specified.\n");
          print_usage();
          exit(EXIT_FAILURE);
        }
        break;
      case 'h':
        print_usage();
        exit(EXIT_SUCCESS);
        break;
      default:
        print_usage();
        exit(EXIT_FAILURE);
    }
  }

  if (record_file != NULL) {
    receive(get_samples_file, buffer_size);
  } else {
    /* Open device and set it up */
    if (rtlsdr_get_device_count() - 1 < device_index) {
      fprintf(stderr, "Device %d not found\n", device_index);
      print_usage();
      exit(EXIT_FAILURE);
    }
    
    if (rtlsdr_open(&device, device_index) != 0) {
      fprintf(stderr, "Error opening device: %s", strerror(errno));
      print_usage();
      exit(EXIT_FAILURE);
    }

    // Tuner (ie. E4K/R820T/etc) auto gain
    //rtlsdr_set_tuner_gain_mode(device, 1);
    //rtlsdr_set_tuner_gain(device, 420);
    // RTL2832 auto gain off

    rtlsdr_reset_buffer(device);
    rtlsdr_set_center_freq(device, freq);
    rtlsdr_set_sample_rate(device, sample_rate);
#if 1
    rtlsdr_set_tuner_gain_mode(device, 1);
    gain_default();
#else
    rtlsdr_set_tuner_gain_mode(device, 0);
    rtlsdr_set_agc_mode(device, 1);
#endif

    //rtlsdr_set_agc_mode(device, 0);
    //rtlsdr_set_freq_correction(device, 0);

    fprintf(stderr, "Frequency set to %d\n", rtlsdr_get_center_freq(device));
    
    // Flush the buffer
    rtlsdr_reset_buffer(device);
    
    receive(get_samples_rtl_sdr, buffer_size);
  }
}
