#define MAIN
#include "common.h"

int main(int argc, char **argv)
{
    pthread_setname_np(pthread_self(), "sdr_main");

    // xxx
    if (setlocale(LC_ALL, "") == NULL) {
        FATAL("setlocale failed, %m\n");
    }
    if (MB_CUR_MAX <= 1) {
        FATAL("MB_CUR_MAX = %ld\n", MB_CUR_MAX);
    }

    // get options
    // xxx more options
    // - which device
    // - sample rate
    // - help
    // - ...
    while (true) {
       static struct option options[] = {
           {"list", no_argument,       NULL,  'l' },
           {"test", no_argument,       NULL,  't' },
           {0,      0,                 NULL,  0   } };

        int c = getopt_long(argc, argv, "lt", options, NULL);
        if (c == -1) {
            break;
        }
        DEBUG("option = %c\n", c);

        switch (c) {
        case 'l':
            sdr_list_devices();
            return 0;
        case 't':
            sdr_test(0, SDR_SAMPLE_RATE);  // xxx opt needed for idx
            return 0;
        case '?':
            return 1;
        default:
            FATAL("getoption returned 0x%x\n", c);
            break;
        }
    }

    // initialization
    sdr_init(0, SDR_SAMPLE_RATE);  // xxx opt for idx
    config_init();
    audio_init();
    radio_init();
    display_init();

    // runtime
    display_handler();

    // program terminating
    NOTICE("program terminating\n");

    return 0;
}
