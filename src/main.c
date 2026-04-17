#include "sgreet.h"
#include "util.h"
#include <getopt.h>
#include <stdio.h>
#include <stdlib.h>

static const struct option OPTIONS[] = {
    {"sessions", required_argument, 0, 's'},
    {"persist", no_argument, 0, 'p'},
    {"logfile", no_argument, 0, 'l'},
};

int
main(int argc, char **argv)
{
    int c;
    int idx;

    char **session_dirs = NULL; // Array of strings each being a path to a
                                // directory containing .desktop files.
    int session_dirs_len = 0;

    while ((c = getopt_long(argc, argv, "s:p", OPTIONS, &idx)) != -1)
    {
        switch (c)
        {
        case 's':
            session_dirs_len++;
            session_dirs =
                realloc(session_dirs, session_dirs_len * sizeof(char *));

            session_dirs[session_dirs_len - 1] = sgreet_strdup(optarg);
            break;
        case 'p':
            SGREET.persist = true;
            break;
        case 'l':
            SGREET.logfile = fopen(optarg, "w");
            break;
        default:
            fprintf(stderr, "Failed parsing command line arguments\n");
            return EXIT_FAILURE;
        }
    }

    int ret;

    ret = sgreet_init((const char **)session_dirs, session_dirs_len);
    for (int i = 0; i < session_dirs_len; i++)
        sgreet_free(session_dirs[i]);
    sgreet_free(session_dirs);

    if (ret == FAIL)
        return EXIT_FAILURE;

    ret = sgreet_run();
    sgreet_uninit();

    return ret == OK ? EXIT_SUCCESS : EXIT_FAILURE;
}
