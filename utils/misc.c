#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <time.h>
#include <math.h>

#include <misc.h>

// -----------------  LOGGING  --------------------------------------------

void log_msg(char *lvl, char *fmt, ...)
{
    char s[200];
    va_list ap;
    int len;

    va_start(ap, fmt);
    vsnprintf(s, sizeof(s), fmt, ap);
    va_end(ap);

    len = strlen(s);
    if (len > 0 && s[len-1] == '\n') {
        s[len-1] = '\0';
    }

    fprintf(stderr, "%s %s: %s\n", lvl, progname, s);
}

// -----------------  TIME  -----------------------------------------------

unsigned long microsec_timer(void)
{
    struct timespec ts;

    clock_gettime(CLOCK_MONOTONIC,&ts);
    return  ((unsigned long)ts.tv_sec * 1000000) + ((unsigned long)ts.tv_nsec / 1000);
}

unsigned long get_real_time_us(void)
{
    struct timespec ts;

    clock_gettime(CLOCK_REALTIME,&ts);
    return ((unsigned long)ts.tv_sec * 1000000) + ((unsigned long)ts.tv_nsec / 1000);
}

// -----------------  DATA SET OPS ---------------------

double moving_avg(double v, int n, void **cx_arg)
{
    struct {
        double sum;
        int    idx;
        double vals[0];
    } *cx;

    if (*cx_arg == NULL) {
        int len = sizeof(*cx) + n*sizeof(double);
        *cx_arg = malloc(len);
        memset(*cx_arg, 0, len);
    }
    cx = *cx_arg;

    cx->sum += (v - cx->vals[cx->idx]);
    cx->vals[cx->idx] = v;
    if (++(cx->idx) == n) cx->idx = 0;
    return cx->sum / n;
}

void average_float(float *v, int n, double *min_arg, double *max_arg, double *avg)
{
    double sum = 0;
    double min = 1e99;
    double max = -1e99;

    for (int i = 0; i < n; i++) {
        sum += v[i];
        if (v[i] < min) min = v[i];
        if (v[i] > max) max = v[i];
    }
    *avg = sum / n;
    *min_arg = min;
    *max_arg = max;
}

void average(double *v, int n, double *min_arg, double *max_arg, double *avg)
{
    double sum = 0;
    double min = 1e99;
    double max = -1e99;

    for (int i = 0; i < n; i++) {
        sum += v[i];
        if (v[i] < min) min = v[i];
        if (v[i] > max) max = v[i];
    }
    *avg = sum / n;
    *min_arg = min;
    *max_arg = max;
}

void normalize(double *v, int n, double min, double max)
{
    double span = max - min;
    double vmin, vmax, vavg, vspan;

    average(v, n, &vmin, &vmax, &vavg);
    //NOTICE("min=%f  max=%f  avg=%f\n", vmin, vmax, vavg);
    vspan = vmax - vmin;

    for (int i = 0; i < n; i++) {
        v[i] = (v[i] - vmin) * (span / vspan) + min;
    }
}

// -----------------  MISC  ----------------------------

void zero_real(double *data, int n)
{
    memset(data, 0, n*sizeof(double));
}

void zero_complex(complex *data, int n)
{
    memset(data, 0, n*sizeof(complex));
}

