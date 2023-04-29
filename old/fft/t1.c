#include "common.h"

char *progname = "t1";

char *file_name = "buf1";

#define MAX_SAMPLES (10 * 2400000)

unsigned char data_uchar[MAX_SAMPLES];
float data_float[MAX_SAMPLES];

int main(int argc, char **argv)
{
    int fd, i, n;

    // read buf
    fd = open(file_name, O_RDONLY);
    if (fd < 0) {
        fprintf(stderr, "FATAL: open %s\n", file_name);
        return 1;
    }
    n = read(fd, data_uchar, MAX_SAMPLES);
    close(fd);
    fprintf(stderr, "n = %d samples\n", n);

    // convert values to float
    for (i = 0; i < n; i++) {
        data_float[i] = (data_uchar[i] - 128) / 128.;
    }

    // remove DC bias  ?
    long double sum = 0;
    double max=0, min=0, average;
    for (i = 0; i < n; i++) {
        sum += data_float[i];
        if (data_float[i] > max) max = data_float[i];
        if (data_float[i] < min) min = data_float[i];
    }
    average = sum / n;
    fprintf(stderr, "min=%f  max=%f  average=%f\n", min, max, average);
    for (i = 0; i < n; i++) {
        data_float[i] -= average;
    }

#if 0
    // print 
    for (i = 0; i < n; i++) {
        fprintf(stderr, "%f\n", data_float[i]);
    }
#endif

#if 1
    // filter  10 .95
    for (i = 0; i < n; i++) {
        data_float[i] = lpf(data_float[i], 10, .95);
    }
#endif

    // scale up
    for (i = 0; i < n; i++) {
        data_float[i] *= 12;
    }

    // remove DC bias  ?
    sum = 0;
    max=0; min=0;
    for (i = 0; i < n; i++) {
        sum += data_float[i];
        if (data_float[i] > max) max = data_float[i];
        if (data_float[i] < min) min = data_float[i];
    }
    average = sum / n;
    fprintf(stderr, "min=%f  max=%f  average=%f\n", min, max, average);
    for (i = 0; i < n; i++) {
        data_float[i] -= average;
    }

#if 0
    // print 
    for (i = 0; i < n; i++) {
        fprintf(stderr, "%f\n", data_float[i]);
    }
#endif


    // downsample to 24000   (by factor of 1000), and
    // write to stdout
    float out[1000];
    int out_max=0, count=0;
    for (i = 0; i < n; i++) {
        if (++count == 110) {
            count = 0;
            out[out_max++] = data_float[i];
            if (out_max == 1000) {
                out_max = 0;
                fwrite(out, sizeof(float), 1000, stdout);
            }
        }
    }
}
