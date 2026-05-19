#include "util.h"
#include "sgreet.h"
#include <json.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

void
sgreet_log(const char *fmt, ...)
{
    if (SGREET.logfile == NULL)
        return;

    time_t     t = time(NULL);
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
    char   *str;
    va_list ap;
    int     len;

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

/*
 * Add a string value to a JSON object. Note that "key" is assumed to be a
 * static string.
 */
void
add_string_to_json_object_len(
    struct json_object *obj, const char *key, const char *val, int len
)
{
    json_object_object_add_ex(
        obj, key, json_object_new_string_len(val, len),
        JSON_C_OBJECT_ADD_CONSTANT_KEY
    );
}

void
add_string_to_json_object(
    struct json_object *obj, const char *key, const char *val
)
{
    add_string_to_json_object_len(obj, key, val, strlen(val));
}

/*
 * Return string value of an entry in a JSON object. Returns NULL if not exists.
 */
const char *
get_string_from_json_object(struct json_object *obj, const char *key)
{
    struct json_object *val;

    if (!json_object_object_get_ex(obj, key, &val))
        return NULL;

    return json_object_get_string(val);
}

/*
 * Get difference between two struct timespec in milliseconds.
 */
int64_t
timespec_diff_ms(const struct timespec *a, const struct timespec *b)
{
    int64_t sec = a->tv_sec - b->tv_sec;
    int64_t nsec = a->tv_nsec - b->tv_nsec;

    if (nsec < 0)
    {
        sec--;
        nsec += 1000000000LL;
    }

    return sec * 1000LL + nsec / 1000000LL;
}
