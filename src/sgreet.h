#pragma once

#include "ui.h"
#include <stdbool.h>
#include <stdio.h>
#include <sys/utsname.h>

struct desktop_entry
{
    char *path;
    char *name;
    char *exec;
};

// State that the greeter is currently in/showing
enum sgreet_state
{
    SGREET_STATE_DESKTOPENTRY,
    SGREET_STATE_USERNAME,
    SGREET_STATE_AUTH,
};

struct sgreet
{
    FILE *logfile;

    char          *tty_name;
    struct utsname uts;

    struct desktop_entry *entries;
    int                   entries_len;
    int                   cur_entry;

    enum sgreet_state state;

    struct ui_label issue;
    struct ui_label entry; // Current desktop entry to be used
    struct ui_label msg;   // Used for errors or info responses
#define MSG_DELAY 2000
    int               msg_remain; // Remaining time in ms to display msg
    struct ui_textbox username;

    int                auth_len;
    struct ui_textbox *auth; // Array of auth prompts from greetd. NULL if there
                             // are none.

    int bot_row;             // Bottommost row

    int sock_fd; // File descriptor for greetd socket

    bool persist;
    bool asterisks;
};

extern struct sgreet SGREET;

int  sgreet_init(const char **session_dirs, int session_dirs_len);
void sgreet_uninit(void);
int  sgreet_run(void);
