#include "common.h"

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
