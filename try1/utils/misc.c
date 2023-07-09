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

    if (lvl[0] == '\0') {
        fprintf(stderr, "%s\n", s);
    } else {
        fprintf(stderr, "%s: %s\n", lvl, s);
    }
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

// xxx needs a free routine, or caller can free the cx
// xxx review all routines here
double moving_avg(double v, int n, void **cx_arg)
{
    struct {
        double sum;
        int    idx;
        int    n;
        double vals[0];
    } *cx;

    if (*cx_arg == NULL) {
        int len = sizeof(*cx) + n*sizeof(double);
        *cx_arg = calloc(len,1);
    }
    cx = *cx_arg;

    cx->sum += (v - cx->vals[cx->idx]);
    cx->vals[cx->idx] = v;
    if (++(cx->idx) == n) cx->idx = 0;
    if (cx->n < n) cx->n++;
    return cx->sum / cx->n;
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

// xxx normalize to the average
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

// -----------------  STRINGS  -------------------------

void remove_trailing_newline(char *s)
{
    int len = strlen(s);
    if (len > 0 && s[len-1] == '\n') {
        s[len-1] = '\0';
    }
}

void remove_leading_whitespace(char *s)
{
    int prefix_len = strspn(s, " \t");
    char *s1 = s + prefix_len;
    int s1_len = strlen(s1);
    memmove(s, s1, s1_len+1);
}

int mbstrchars(char *s)
{
    int chars=0, charlen;
    mbstate_t mbs;

    memset(&mbs, 0, sizeof(mbs));
    while (1) {
        charlen = mbrlen(s, MB_CUR_MAX, &mbs);
        if (charlen <= 0) {
            if (charlen < 0) {
                ERROR("invlid multibyte string\n");
            }
            break;
        }
        chars++;
        s += charlen;
    }

    return chars;
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

unsigned int round_up(unsigned int n, unsigned int multiple)
{
    if ((n % multiple) == 0) {
        return n;
    } else {
        return (n / multiple + 1) * multiple;
    }
}


