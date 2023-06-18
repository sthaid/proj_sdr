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

    // get options
    // xxx more options
    // - which device
    // - sample rate
    // - help
    // - ...
    while (true) {
       static struct option options[] = {
           {"list", no_argument,       NULL,  'l' },
           {"test", required_argument, NULL,  't' },
           {0,      0,                 NULL,  0   } };

        int c = getopt_long(argc, argv, "lt:", options, NULL);
        if (c == -1) break;

        switch (c) {
        case 'l':
            NOTICE("option = list\n");
            sdr_list_devices();
            return 0;
        case 't':
            NOTICE("option = test, arg = '%s'\n", optarg);
            break;
        case '?':
            return 1;
        default:
            FATAL("getoption returned 0x%x\n", c);
            break;
        }
    }

    // initialization
    config_init();
    //sdr_init();
    audio_init();
    //fft_init();
    display_init();

    // runtime
    display_handler();

    // program terminating
    //config_write();
    NOTICE("program terminating\n");
}

