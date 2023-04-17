// xxx 
// - work on lpf_complex
// - these are not thread safe, for now
// - check for MAX_OUT_COMPLEX whereever out_complex is used
// - save the in array and restore it when creating the plan, or comment about it not needed
// - change how out_complex is used

#include <stdlib.h>
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

#define MAX_OUT_COMPLEX 20000000
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

static plan_t   Plan[MAX_PLAN];
static complex *out_complex;

// -----------------  INIT  -------------------------------------------

void init_fft(void)
{
    out_complex = fftw_alloc_complex(MAX_OUT_COMPLEX);
    // xxx zero
}

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

// -----------------  EXECUTE FFT  ------------------------------------

void fft_fwd_r2c(double *in, complex *out, int n)
{
    fftw_plan p;

    // real array has n elements
    // complex array has n/2+1 elements

    p = get_plan(R2C, n, in, out, FFTW_FORWARD);
    fftw_execute(p);
}

void fft_back_c2r(complex *in, double *out, int n)
{
    fftw_plan p;

    // complex array has n/2+1 elements
    // real array has n elements

    p = get_plan(C2R, n, in, out, FFTW_BACKWARD);
    fftw_execute(p);
}

void fft_fwd_r2r(double *in, double *out, int n)
{
    fft_fwd_r2c(in, out_complex, n);

    for (int i = 0; i < n/2+1; i++) {
        out[i] = cabs(out_complex[i]);
    }
    for (int i = n/2+1; i < n; i++) {
        out[i] = 0;
    }
}

// - - - - - 

void fft_fwd_c2c(complex *in, complex *out, int n)
{
    fftw_plan p;

    p = get_plan(C2C, n, in, out, FFTW_FORWARD);
    fftw_execute(p);
}

void fft_back_c2c(complex *in, complex *out, int n)
{
    fftw_plan p;

    p = get_plan(C2C, n, in, out, FFTW_BACKWARD);
    fftw_execute(p);
}

// -----------------  LOW PASS FILTERS  -------------------------------

void fft_lpf_complex(complex *in, complex *out, int n, double sample_rate, double f)
{
    unsigned long start=microsec_timer();
    int           i, ix;

    // perform forward fft,
    // apply filter,
    // perform backward fft
    // xxx could avoid using out_complex here
    fft_fwd_c2c(in, out_complex, n);
    ix = n / sample_rate * f;
    for (i = ix; i < n/2+1; i++) {
        out_complex[i] = 0;
        out_complex[n-i-1] = 0;
    }
    fft_back_c2c(out_complex, out, n);

    // print elapsed time
    NOTICE("fft_lpf_complex duration %ld ms\n", (microsec_timer()-start)/1000);
}

void fft_lpf_real(double *in, double *out, int n, double sample_rate, double f)
{
    unsigned long start=microsec_timer();
    int           i, ix;

    // perform forward fft,
    // apply filter,
    // perform backward fft
    fft_fwd_r2c(in, out_complex, n);
    ix = n / sample_rate * f;
    for (i = ix; i < n/2+1; i++) {
        out_complex[i] = 0;
    }
    fft_back_c2r(out_complex, out, n);

    // print elapsed time
    NOTICE("fft_lpf_real duration %ld ms\n", (microsec_timer()-start)/1000);
}
