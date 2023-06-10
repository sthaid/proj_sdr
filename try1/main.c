#define MAIN

#include "common.h"

//char *progname = "sdr";
//band_t band[MAX_BAND];
//int max_band;

int main(int argc, char **argv)
{
    progname = "sdr";

    config_init();
    //sdr_init();
    //audio_init();
    //fft_init();
    //display_init();

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

    config_write();
}

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

    fprintf(stderr, "%s %s: %s\n", lvl, progname, s);
}

