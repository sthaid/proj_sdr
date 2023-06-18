#define MAIN

#include "common.h"

int main(int argc, char **argv)
{
    NOTICE("program starting\n");

    // xxx
    if (setlocale(LC_ALL, "") == NULL) {
        FATAL("setlocale failed, %m\n");
    }
    NOTICE("MB_CUR_MAX = %ld\n", MB_CUR_MAX);
    BLANKLINE;

    // initialization
    config_init();
    //sdr_init();
    //audio_init();
    //fft_init();
    display_init();

    // runtime
    display_handler();

    // program terminating
    //config_write();
    NOTICE("program terminating\n");
}

