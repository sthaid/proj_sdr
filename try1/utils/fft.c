// xxx 
// - work on lpf_complex
// - these are not thread safe, for now
// - save the in array and restore it when creating the plan, or comment about it not needed

#include <stdlib.h>
#include <stdbool.h>
#include <string.h>

#include <misc.h>
#include <fft.h>

//
// defines
//

#define NONE  0
#define C2C   1
#define R2C   2
#define C2R   3

#define TYPE_STR(t) \
    ((t) == NONE  ? "NONE" : \
     (t) == C2C   ? "C2C" : \
     (t) == R2C   ? "R2C" : \
     (t) == C2R   ? "C2R" : \
                    "????")

#define DIR_STR(d) ((d) == FFTW_FORWARD ? "FWD" : "BACK")

#define MAX_PLAN 100

//
// typedefs
//

typedef struct {
    int type;
    int n;
    void *in;
    void *out;
    int dir;
    fftw_plan plan;
} plan_t;

//
// variables
//

static plan_t Plan[MAX_PLAN];

// -----------------  GET PLAN  ---------------------------------------

static fftw_plan get_plan(int type, int n, void *in, void *out, int dir)
{
    int i;
    plan_t *p;
    fftw_plan plan;
    
    for (i = 0; i < MAX_PLAN; i++) {
        p = &Plan[i];
        if (p->type == NONE) {
            break;
        }
        if (p->type == type && p->n == n && p->in == in && p->out == out && p->dir == dir) {
            return p->plan;
        }
    }

    if (i == MAX_PLAN) {
        FATAL("out of plan\n");
    }

    NOTICE("creating fftw_plan %s n=%d in=%p out=%p dir=%s\n",
           TYPE_STR(type), n, in, out, DIR_STR(dir));
    switch (type) {
    case C2C:
        plan = fftw_plan_dft_1d(n, in, out, dir, FFTW_ESTIMATE);
        break;
    case R2C:
        plan = fftw_plan_dft_r2c_1d(n, in, out, FFTW_ESTIMATE);
        break;
    case C2R:
        plan = fftw_plan_dft_c2r_1d(n, in, out, FFTW_ESTIMATE);
        break;
    default:
        FATAL("invalid type %d\n", type);
        break;
    }

    p->type = type;
    p->n    = n;
    p->in   = in;
    p->out  = out;
    p->dir  = dir;
    p->plan = plan;

    return p->plan;
}

// -----------------  ALLOC FFT BUFFERS  ------------------------------

complex * fft_alloc_complex(int n)
{
    return fftw_alloc_complex(n);
}

double * fft_alloc_real(int n)
{
    return fftw_alloc_real(n);
}

// -----------------  EXECUTE FFT  ------------------------------------

// note that for fft_fwd_r2c and fft_back_c2r:
// - real array has n elements
// - complex array has n/2+1 elements

void fft_fwd_r2c(double *in, complex *out, int n)
{
    fftw_plan p;

    p = get_plan(R2C, n, in, out, FFTW_FORWARD);
    fftw_execute(p);
}

void fft_back_c2r(complex *in, double *out, int n, bool normalize)
{
    fftw_plan p;

    p = get_plan(C2R, n, in, out, FFTW_BACKWARD);
    fftw_execute(p);

    if (normalize) {
        for (int i = 0; i < n; i++) {
            out[i] /= n;
        }
    }
}

void fft_fwd_c2c(complex *in, complex *out, int n)
{
    fftw_plan p;

    p = get_plan(C2C, n, in, out, FFTW_FORWARD);
    fftw_execute(p);
}

void fft_back_c2c(complex *in, complex *out, int n, bool normalize)
{
    fftw_plan p;

    p = get_plan(C2C, n, in, out, FFTW_BACKWARD);
    fftw_execute(p);

    if (normalize) {
        for (int i = 0; i < n; i++) {
            out[i] /= n;
        }
    }
}

// -----------------  FILTERS  ----------------------------------------

void fft_bpf_complex(complex *in, complex *out, int n, double sample_rate, double f_low, double f_high)
{
    unsigned long start=microsec_timer();
    int           i, ix1, ix2;

    // perform forward fft
    fft_fwd_c2c(in, out, n);

    // apply filter
    ix1 = n / sample_rate * f_low;
    ix2 = n / sample_rate * f_high;
    for (i = 0; i < n - (ix2-ix1); i++) {
        int xx = i + ix2;
        if (xx < 0) xx += n;
        else if (xx >= n) xx -= n;
        out[xx] = 0;
    }

    // perform backward fft, and normalize
    fft_back_c2c(out, out, n, true);

    // print elapsed time
    DEBUG("fft_lpf_complex duration %ld ms\n", (microsec_timer()-start)/1000);
}

// note: the number of elements in out must be n+2
void fft_lpf_real(double *in, double *out, int n, double sample_rate, double f_cutoff)
{
    unsigned long   start = microsec_timer();
    int             i, ix;
    complex        *out_complex = (complex*)out;

    // perform forward fft
    fft_fwd_r2c(in, out_complex, n);

    // apply filter
    ix = n / sample_rate * f_cutoff;
    for (i = ix; i < n/2+1; i++) {
        out_complex[i] = 0;
    }

    // perform backward fft, and normalize
    fft_back_c2r(out_complex, out, n, true);

    // print elapsed time
    DEBUG("fft_lpf_real duration %ld ms\n", (microsec_timer()-start)/1000);
}
