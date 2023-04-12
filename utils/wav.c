#include <stdlib.h>
#include <string.h>
#include <sndfile.h>

#include <misc.h>

// reference:
//   apt install libsndfile1-dev
//   file:///usr/share/doc/libsndfile1-dev/html/api.html
//   https://github.com/libsndfile/libsndfile

int read_wav_file(char *filename, float **data, int *num_chan, int *num_items, int *sample_rate)
{
    SNDFILE *file;
    SF_INFO  sfinfo;
    int      cnt, items;
    float   *d;

    // preset return values
    *data = NULL;
    *num_chan = 0;
    *num_items = 0; 
    *sample_rate = 0;

    // open wav file and get info
    memset(&sfinfo, 0, sizeof (sfinfo));
    file = sf_open(filename, SFM_READ, &sfinfo);
    if (file == NULL) {
        return -1;
    }

    // allocate memory for the data
    items = sfinfo.frames * sfinfo.channels;
    d = malloc(items*sizeof(float));

    // read the wav file data 
    cnt = sf_read_float(file, d, items);
    if (cnt != items) {
        free(d);
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

