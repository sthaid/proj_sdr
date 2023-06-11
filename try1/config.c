#include "common.h"

#define CONFIG_FILENAME "sdr.config"

#define MAX_VARS (sizeof(vars) / sizeof(vars[0]))

static struct {
    char *name;
    int  *value;
} vars[] = {
    { "zoom",       &zoom       },
    { "volume",     &volume     },
    { "mute",       &mute       },
    { "scan_intvl", &scan_intvl },
    { "help",       &help       },
            };

// -------------------------------------------------------------

void config_init(void)
{
    FILE          *fp;
    int            i, cnt, line_num=0;
    char           s[200];
    struct band_s *b=NULL;

    #define BAD_CONFIG_FILE_LINE \
        do { \
            FATAL("bad config file line %d: '%s'\n", line_num, s); \
        } while (0)

    // open config file
    fp = fopen(CONFIG_FILENAME, "r");
    if (fp == NULL) {
        FATAL("failed to open %s", CONFIG_FILENAME);
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
            if (max_band == MAX_BAND) {
                BAD_CONFIG_FILE_LINE;
            }

            b = &band[max_band];
            max_band++;

            cnt = sscanf(s+5, "%ms %lf %lf %lf %lf %d %d %d %d",
                         &b->name, 
                         &b->f_min,
                         &b->f_max,
                         &b->f_step,
                         &b->f_curr,
                         &b->demod,
                         &b->squelch,
                         &b->selected,
                         &b->active);
            if (cnt != 9) {
                BAD_CONFIG_FILE_LINE;
            }
        } else if (strncmp(s, "STATION ", 8) == 0) {
            if (b == NULL) {
                BAD_CONFIG_FILE_LINE;
            }

            cnt = sscanf(s+8, "%lf %ms",
                        &b->station[b->max_station].freq,
                        &b->station[b->max_station].name);
            if (cnt != 2) {
                BAD_CONFIG_FILE_LINE;
            }

            b->max_station++;
        } else if (strncmp(s, "VAR ", 4) == 0) {
            char name[100];
            int  value;

            cnt = sscanf(s+4, "%s %d", name, &value);
            if (cnt != 2) {
                BAD_CONFIG_FILE_LINE;
            }

            for (i = 0; i < MAX_VARS; i++) {
                if (strcmp(name, vars[i].name) == 0) {
                    *vars[i].value = value;
                }
            }
        }
    }

    // close file
    fclose(fp);
}

void config_write(void)
{
    FILE *fp = stdout;
    int i, j;
    struct band_s *b;

    for (i = 0; i < max_band; i++) {
        b = &band[i];

        fprintf(fp, "BAND %s %f %f %f %f %d %d %d %d\n",
                b->name, 
                b->f_min,
                b->f_max,
                b->f_step,
                b->f_curr,
                b->demod,
                b->squelch,
                b->selected,
                b->active);
        for (j = 0; j < b->max_station; j++) {
            fprintf(fp, "STATION %10f %s\n",
                    b->station[j].freq,
                    b->station[j].name);
        }
        fprintf(fp, "\n");
    }

    for (i = 0; i < MAX_VARS; i++) {
        fprintf(fp, "VAR %-10s %d\n",
                vars[i].name, *vars[i].value);
    }
}
