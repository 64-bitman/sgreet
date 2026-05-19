#pragma once

#include <json.h>
#include <stddef.h>

#ifndef OK
#    define OK 0
#endif
#define FAIL (OK - 1)

#ifdef __GNUC__
#    define PRINTFLIKE(n, m) __attribute__((format(printf, n, m)))
#else
#    define PRINTFLIKE(n, m)
#endif

#define NUL '\0'

void  sgreet_log(const char *fmt, ...) PRINTFLIKE(1, 2);
char *sgreet_strdup_printf(const char *fmt, ...);
void  add_string_to_json_object_len(
    struct json_object *obj, const char *key, const char *val, int len
);
void add_string_to_json_object(
    struct json_object *obj, const char *key, const char *val
);
const char *
get_string_from_json_object(struct json_object *obj, const char *key);
int64_t timespec_diff_ms(const struct timespec *a, const struct timespec *b);
