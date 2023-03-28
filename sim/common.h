#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <stdbool.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>

// -----------------  LOGGING  --------------------

#define NOTICE(fmt, args...) log_msg("NOTICE", fmt, ## args);
#define WARN(fmt, args...) log_msg("WARN", fmt, ## args);
#define ERROR(fmt, args...) log_msg("ERROR", fmt, ## args);

extern char *progname;

void log_msg(char *lvl, char *fmt, ...);

