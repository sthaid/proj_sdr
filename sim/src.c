#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <stdbool.h>
#include <unistd.h>
#include <string.h>

#include "common.h"

#include <sndfile.h>

// reference:
//   apt install libsndfile1-dev
//   file:///usr/share/doc/libsndfile1-dev/html/api.html
//   https://github.com/libsndfile/libsndfile

// sine
// white
// file=xxx.wav

char *progname = "src";

int read_wav_file(char *filename, short **data, int *num_chan, int *num_items, int *sample_rate);

int main(int argc, char **argv)
{
    int rc, num_chan, num_items, sample_rate;
    short *data;
    char *filename = "/home/haid/Audio/super_critical.wav";

    rc = read_wav_file(filename, &data, &num_chan, &num_items, &sample_rate);
    if (rc != 0) {
        ERROR("read_wav_file %s, %s\n", filename, strerror(errno));
        exit(1);
    }

    NOTICE("%d %d %d\n", num_chan, num_items, sample_rate);

    // xxx if multi chan ...

    fwrite(data, sizeof(short), num_items, stdout);

    return 0;
}

// ------------------------------------------------

// caller must free returned data when done
int read_wav_file(char *filename, short **data, int *num_chan, int *num_items, int *sample_rate)
{
    SNDFILE *file;
    SF_INFO  sfinfo;
    int      cnt, items;
    short   *d;

    // preset return values
    *data = NULL;
    *num_chan = 0;
    *num_items = 0; 
    *sample_rate = 0;

    // open wav file and get info
    memset(&sfinfo, 0, sizeof (sfinfo));
    file = sf_open(filename, SFM_READ, &sfinfo);
    if (file == NULL) {
        ERROR("sf_open '%s'\n", filename);
        return -1;
    }

    // allocate memory for the data
    items = sfinfo.frames * sfinfo.channels;
    d = malloc(items*sizeof(short));

    // read the wav file data 
    cnt = sf_read_short(file, d, items);
    if (cnt != items) {
        ERROR("sf_read_short, cnt=%d items=%d\n", cnt, items);
        sf_close(file);
        return -1;
    }

    // close file
    sf_close(file);

    // return values
    *data        = d;
    *num_chan    = sfinfo.channels;
    *num_items   = items;
    *sample_rate = sfinfo.samplerate;
    return 0;
}


#if 0
// on input num_items is the total number of shorts in caller's data buffer
int read_wav_file2(char *filename, short *data, int *num_chan, int *num_items, int *sample_rate)
{
    SNDFILE *file;
    SF_INFO  sfinfo;
    int      cnt, items;
    int      num_items_orig = *num_items;

    // preset return values
    *num_chan = 0;
    *num_items = 0;
    *sample_rate = 0;

    // open wav file and get info
    memset(&sfinfo, 0, sizeof (sfinfo));
    file = sf_open(filename, SFM_READ, &sfinfo);
    if (file == NULL) {
        ERROR("sf_open '%s'\n", filename);
        return -1;
    }

    // limit number of items being read to not overflow caller's buffer
    items = sfinfo.frames * sfinfo.channels;
    if (items > num_items_orig) {
        items = num_items_orig;
    }

    // read the wav file data
    cnt = sf_read_short(file, data, items);
    if (cnt != items) {
        ERROR("sf_read_short, cnt=%d items=%d\n", cnt, items);
        sf_close(file);
    }

    // close file
    sf_close(file);

    // return values
    *num_chan    = sfinfo.channels;
    *num_items    = items;
    *sample_rate = sfinfo.samplerate;
    return 0;
}

#endif
