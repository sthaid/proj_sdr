#include "common.h"

int main(int argc, char **argv)
{
    config_init();
    sdr_init();
    audio_init();
    fft_init();
    display_init();

#if 0
    while true
        if display has changed
            update display

        while ctrl event avail
            process ctrl event

        if exit requested
            terminate
    end
#endif
}

