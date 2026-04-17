#include "util.h"
#include "sgreet.h"
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

void
sgreet_log(const char *fmt, ...)
{
    if (SGREET.logfile == NULL)
        return;

    va_list ap;

    va_start(ap, fmt);
    vfprintf(SGREET.logfile, fmt, ap);
    fputc('\n', SGREET.logfile);
    va_end(ap);
}

void *
sgreet_malloc(size_t sz)
{
    void *ptr = malloc(sz);

    if (ptr == NULL)
    {
        perror("malloc() error");
        abort();
    }
    return ptr;
}

void *
sgreet_realloc(void *ptr, size_t sz)
{
    void *new = realloc(ptr, sz);

    if (new == NULL)
    {
        perror("realloc() error");
        abort();
    }
    return new;
}

char *
sgreet_strdup(const char *str)
{
    char *dup = strdup(str);

    if (dup == NULL)
    {
        perror("strdup() error");
        abort();
    }
    return dup;
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

    str = sgreet_malloc(len + 1);

    va_start(ap, fmt);
    vsnprintf(str, len + 1, fmt, ap);
    va_end(ap);

    return str;
}

void
sgreet_free(void *ptr)
{
    free(ptr);
}
