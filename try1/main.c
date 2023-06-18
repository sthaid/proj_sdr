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

    // initialization
    config_init();
    //sdr_init();
    //audio_init();
    //fft_init();
    display_init();
    config_write();

    // runtime
    display_handler();

    // program terminating
    NOTICE("program terminating\n");
}

