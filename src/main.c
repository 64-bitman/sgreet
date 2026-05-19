#include "sgreet.h"
#include "util.h"
#include <getopt.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static const struct option OPTIONS[] = {
    {"version", no_argument, 0, 'v'},
    {"help", no_argument, 0, 'h'},
    {"sessions", required_argument, 0, 's'},
    {"persist", required_argument, 0, 'p'},
    {"persist-path", required_argument, 0, 'f'},
    {"logfile", required_argument, 0, 'l'},
    {"asterisks", no_argument, 0, 'a'},
};

static void help_msg(void);

int
main(int argc, char **argv)
{
    int c;
    int idx;

    char **session_dirs = NULL; // Array of strings each being a path to a
                                // directory containing .desktop files.
    int session_dirs_len = 0;

    while ((c = getopt_long(argc, argv, "vhs:pl:a", OPTIONS, &idx)) != -1)
    {
        switch (c)
        {
        case 'v':
            printf("version 1.0\n");
            return EXIT_SUCCESS;
        case 'h':
            help_msg();
            return EXIT_SUCCESS;
        case 's':
            session_dirs_len++;
            session_dirs =
                realloc(session_dirs, session_dirs_len * sizeof(char *));

            if ((session_dirs[session_dirs_len - 1] = strdup(optarg)) == NULL)
                session_dirs_len--;
            break;
        case 'p':
            if (strstr(optarg, "session") != NULL)
                SGREET.persist |= PERSIST_SESSION;
            if (strstr(optarg, "user") != NULL)
                SGREET.persist |= PERSIST_USER;
            break;
        case 'f':
            SGREET.persist_path = strdup(optarg);
            break;
        case 'l':
        {
            // Clear the file first
            FILE *tmp = fopen(optarg, "w");

            if (tmp == NULL)
                break;
            fclose(tmp);
            SGREET.logfile = fopen(optarg, "a");
            break;
        }
        case 'a':
            SGREET.asterisks = true;
            break;
        default:
            fprintf(stderr, "Failed parsing command line arguments\n");
            return EXIT_FAILURE;
        }
    }

    int ret;

    ret = sgreet_init((const char **)session_dirs, session_dirs_len);
    for (int i = 0; i < session_dirs_len; i++)
        free(session_dirs[i]);
    free(session_dirs);

    if (ret == FAIL)
        return EXIT_FAILURE;

    ret = sgreet_run();
    sgreet_uninit();

    return ret == OK ? EXIT_SUCCESS : EXIT_FAILURE;
}

static void
help_msg(void)
{
    printf("Usage: sgreet [OPTIONS]\n\n");
    printf("Options:\n");
    printf("    -h,--help                   print this help message\n");
    printf("    -v,--version                print version\n");
    printf("    -s,--sessions {PATH}        session directory to search\n");
    printf("    -p,--persist session,user   persist state\n");
    printf("    -l,--logfile {PATH}         file to log messages to\n");
    printf("    -a,--asterisks              show password in asterisks\n");
}
