// xxx 
// - work on lpf_complex
// - these are not thread safe, for now
// - check for MAX_OUT_COMPLEX whereever out_complex is used
// - save the in array and restore it when creating the plan, or comment about it not needed

#include <stdlib.h>
#include <string.h>

#include <misc.h>
#include <fft.h>

//
// defines
//

#define NONE   0
#define DFT_1D 1
#define R2C_1D 2
#define C2R_1D 3

#define TYPE_STR(t) \
    ((t) == NONE   ? "NONE" : \
     (t) == DFT_1D ? "DFT_1D" : \
     (t) == R2C_1D ? "R2C_1D" : \
     (t) == C2R_1D ? "C2R_1D" : \
                     "????")

#define DIR_STR(d) ((d) == FFTW_FORWARD ? "FWD" : "BACK")

#define MAX_OUT_COMPLEX 20000000
#define MAX_PLAN 20

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
    case DFT_1D:
        plan = fftw_plan_dft_1d(n, in, out, dir, FFTW_ESTIMATE);
        break;
    case R2C_1D:
        plan = fftw_plan_dft_r2c_1d(n, in, out, FFTW_ESTIMATE);
        break;
    case C2R_1D:
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

    p = get_plan(R2C_1D, n, in, out, FFTW_FORWARD);
    fftw_execute(p);
}

void fft_back_c2r(complex *in, double *out, int n)
{
    fftw_plan p;

    p = get_plan(C2R_1D, n, in, out, FFTW_BACKWARD);
    fftw_execute(p);
}

void fft_fwd_r2r(double *in, double *out, int n)
{
    fftw_plan p;

    p = get_plan(R2C_1D, n, in, out_complex, FFTW_FORWARD);
    fftw_execute(p);

    for (int i = 0; i < n; i++) {
        out[i] = cabs(out_complex[i]);
    }
}

// -----------------  LOW PASS FILTERS  -------------------------------

void lpf_complex(complex *data, int n, double sample_rate, double f, char *str)
{
    unsigned long start, duration;
    fftw_plan fwd, back;
    int i, ix;

    NOTICE("lpf_complex %s starting\n", str);

    // find/create the plans
    fwd = get_plan(DFT_1D, n, data, data, FFTW_FORWARD);
    back = get_plan(DFT_1D, n, data, data, FFTW_BACKWARD);
    
    // perform forward fft
    start = microsec_timer();
    fftw_execute(fwd);

    // apply filter
    ix = n / sample_rate * f;
    for (i = ix; i < n/2+1; i++) {
        data[i] = 0;
        data[n-i-1] = 0;
    }

    // perform backward fft
    fftw_execute(back);
    duration = microsec_timer() - start;

    // print elapsed time
    NOTICE("lpf_complex %s duration %ld ms\n", str, duration/1000);
}

void lpf_real(double *in, double *out, int n, double sample_rate, double f, char *str)
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
    NOTICE("lpf_real %s duration %ld ms\n", str, (microsec_timer()-start)/1000);
}
