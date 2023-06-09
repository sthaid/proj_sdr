#include "common.h"

#define FILENAME "sdr.config"

#define MAX_DEMODS (sizeof(demods) / sizeof(demods[0]))

static struct {
    char *name;
    int demod;
} demods[] = {
    { "AM", DEMOD_AM },
    { "FM", DEMOD_FM },
    { "LSB", DEMOD_LSB },
    { "USB", DEMOD_USB },
        };

static void exit_hndlr(void);
static void config_dump(void) ATTRIB_UNUSED;

// -------------------------------------------------------------

void config_init(void)
{
    FILE          *fp;
    int            i, cnt, line_num=0;
    char           s[200];
    struct band_s *b=NULL;

    #define BAD_CONFIG_FILE_LINE \
        do { \
            FATAL("sdr.config line %d: '%s'\n", line_num, s); \
        } while (0)

    // open config file
    fp = fopen(FILENAME, "r");
    if (fp == NULL) {
        FATAL("failed to open %s", FILENAME);
    }

    // read lines from file
    while (fgets(s, sizeof(s), fp) != NULL) {
        line_num++;
        remove_trailing_newline(s);
        remove_leading_whitespace(s);

        if (s[0] == '\0' || s[0] == '#') {
            continue;
        }

        if (strncmp(s, "BAND ", 5) == 0) {
            double f_min, f_max, f_snap_intvl, f_snap_offset;
            char   *name_str, *demod_str;

            if (max_band == MAX_BAND) {
                BAD_CONFIG_FILE_LINE;
            }

            b = calloc(sizeof(band_t), 1);
            band[max_band] = b;

            cnt = sscanf(s+5, "%ms %lf %lf %lf %lf %ms",
                         &name_str, 
                         &f_min, &f_max, &f_snap_intvl, &f_snap_offset,
                         &demod_str);
            if (cnt != 6) {
                BAD_CONFIG_FILE_LINE;
            }

            b->name          = name_str;
            b->f_min         = nearbyint(f_min * MHZ);
            b->f_max         = nearbyint(f_max * MHZ);
            b->f_snap_intvl  = nearbyint(f_snap_intvl * MHZ);
            b->f_snap_offset = nearbyint(f_snap_offset * MHZ);
            for (i = 0; i < MAX_DEMODS; i++) {
                if (strcmp(demods[i].name, demod_str) == 0) {
                    b->demod = demods[i].demod;
                    break;
                }
            }

            b->idx    = max_band;
            b->f_span = b->f_max - b->f_min;
            b->f_play = (b->f_max + b->f_min) / 2;
            b->sim    = (strncmp(b->name, "SIM", 3) == 0);
            b->width  = 1;

            if (i == MAX_DEMODS) {
                BAD_CONFIG_FILE_LINE;
            }

            //if (strncmp(b->name, "SIM", 3) == 0) b->selected = true; // xxx temp
            b->selected = true; //xxx temp

            max_band++;
#if 0
        } else if (strncmp(s, "STATION ", 8) == 0) {
            if (b == NULL) {
                BAD_CONFIG_FILE_LINE;
            }

            cnt = sscanf(s+8, "%lf %ms",
                        &f,
                        &b->station[b->max_station].name);
            b->station[b->max_station].f = nearbyint(f * MHZ);
            if (cnt != 2) {
                BAD_CONFIG_FILE_LINE;
            }

            b->max_station++;
#endif
        }
    }

    // close file
    fclose(fp);

    // must have at least one band
    if (max_band == 0) {
        FATAL("no bands defined\n");
    }

    // register exit_hndlr
    atexit(exit_hndlr);

    // xxx print the config
    // xxx maybe add a debug mode to getopt to enable this among other debug level
    //config_dump();
}

static void exit_hndlr(void)
{
    NOTICE("config exit_hndlr\n");

    // xxx tbd
}

static void config_dump(void)
{
    int i, j;
    struct band_s *b;

    NOTICE("-------- CONFIG DUMP --------\n");

    for (i = 0; i < max_band; i++) {
        b = band[i];

        // xxx print all fields
        NOTICE("BAND %s %f %f\n",
               b->name, 
               (double)b->f_min / MHZ,
               (double)b->f_max / MHZ);
        for (j = 0; j < b->max_station; j++) {
            NOTICE("STATION %10f %s\n",
                   (double)b->station[j].f / MHZ,
                   b->station[j].name);
        }
        BLANKLINE;
    }
}


#if 0
// xxx save
static struct {
    char *name;
    int  *value;
} vars[] = {
    { NULL,         NULL        },
            };

        } else if (strncmp(s, "VAR ", 4) == 0) {
            char name[100];
            int  value;

            cnt = sscanf(s+4, "%s %d", name, &value);
            if (cnt != 2) {
                BAD_CONFIG_FILE_LINE;
            }

            for (i = 0; vars[i].name; i++) {
                if (strcmp(name, vars[i].name) == 0) {
                    *vars[i].value = value;
                }
            }


    for (i = 0; vars[i].name; i++) {
        fprintf(fp, "VAR %-10s %d\n",
                vars[i].name, *vars[i].value);
    }
#endif
