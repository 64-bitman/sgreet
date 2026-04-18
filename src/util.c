#include "util.h"
#include "sgreet.h"
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>

void
sgreet_log(const char *fmt, ...)
{
    if (SGREET.logfile == NULL)
        return;

    time_t t = time(NULL);
    struct tm *tm = localtime(&t);

    static char buf[128];
    strftime(buf, sizeof(buf), "%I:%M:%S", tm);

    fprintf(SGREET.logfile, "%s - ", buf);

    va_list ap;

    va_start(ap, fmt);
    vfprintf(SGREET.logfile, fmt, ap);
    fputc('\n', SGREET.logfile);
    va_end(ap);

    fflush(SGREET.logfile);
}

char *
sgreet_strdup_printf(const char *fmt, ...)
{
    char *str;
    va_list ap;
    int len;

    va_start(ap, fmt);
    len = vsnprintf(NULL, 0, fmt, ap);
    va_end(ap);

    str = malloc(len + 1);
    if (str == NULL)
        return NULL;

    va_start(ap, fmt);
    vsnprintf(str, len + 1, fmt, ap);
    va_end(ap);

    return str;
}
