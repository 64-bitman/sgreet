#pragma once

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
