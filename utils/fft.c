
// xxx these are not thread safe, for now

#define NONE   0
#define DFT_1D 1
#define R2C_1D 2
#define C2R_1D 3

typedef struct {
    int type;
    int n;
    void *in;
    void *out;
    int dir;
    fftw_plan *plan;
} plan_t;

#define MAX_PLAN 20
static plan_t plan[MAX_PLAN];

static fftw_plan * get_plan(int type, int n, void *in, void *out, int dir)
{
    int i;
    
    for (i = 0; i < MAX_PLAN; i++) {
        plan_t *p = plan[i];
        if (p->type == TYPE_NONE) {
            break;
        }
        if (p->type == type && p->n == n && p->in == in && p->out == out && p->dir == dir) {
            return p->plan;
        }
    }

    if (i == MAX_PLAN) {
        FATAL("out of plan\n");
    }

    // xxx save the in array and restore it when creating the plan

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
}

void lpf_complex(complex *data, int n, double sample_rate, double f)
{
    unsigned long start, duration;
    fftw_plan *fwd, *back;

    // find/create the plans
    fwd = find_plan(DFT_1D, n, data, data, FFT_FORWARD);
    back = find_plan(DFT_1D, n, data, data, FFT_BACKWARD);
    
    // perform forward fft
    start = microsec_timer();
    fftw_execute(fwd);

    // apply filter
    // xxx check this
    ix = n / sample_rate * f;
    for (i = ix; i < n/2+1; i++) {
        data[i] = 0;
        data[n-i] = 0;
    }

    // perform backward fft
    fftw_execute(back);
    duration = microsec_timer() - start;

    // print elapsed time
    NOTICE("duration %ld ms\n", duration/1000);
}

void lpf_real(double *data_arg, int n, double sample_rate, double f)
{
    unsigned long start, duration;
    fftw_plan *fwd, *back;

    #define MAX_N 20000000

    static double *data;

    // xxx
    if (n > MAX_N) {
        FATAL("n=%d is too large\n", n);
    }

    // allocate data buffer, and copy caller's data to it
    if (data == NULL) {
        data = fft_alloc_real(2*(MAX_N/2+1));
    }
    memcpy(data, data_arg, n*sizeof(double));

    // find/create the plans
    fwd = find_plan(R2C_1D, n, data, data, FFT_FORWARD);
    back = find_plan(C2R_1D, n, data, data, FFT_BACKWARD);

    // perform forward fft
    start = microsec_timer();
    fftw_execute(fwd);

    // xxx filter
    ix = n / sample_rate * f;
    for (i = ix; i < n/2+1; i++) {
        data[i] = 0;
    }

    // perform backward fft
    fftw_execute(back);
    duration = microsec_timer() - start;

    // copy result back to caller's buffer
    memcpy(data_arg, data, n*sizeof(double));

    // print elapsed time
    NOTICE("duration %ld ms\n", duration/1000);
}
