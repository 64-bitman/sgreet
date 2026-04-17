#include "sgreet.h"
#include "ipc.h"
#include "util.h"
#include <dirent.h>
#include <ncurses.h>
#include <string.h>
#include <sys/types.h>
#include <time.h>
#include <unistd.h>

struct sgreet SGREET;

static void
clear_desktop_entry(struct desktop_entry *entry)
{
    sgreet_free(entry->path);
    sgreet_free(entry->name);
    sgreet_free(entry->exec);
}

/*
 * Parse the .desktop file at "path" and store it in "entry". Only entries with
 * "Name" and "Exec" fields are used. Returns OK on success and FAIL on failure.
 */
static int
parse_desktop_file(const char *path, struct desktop_entry *entry)
{
    FILE *fp = fopen(path, "r");

    if (fp == NULL)
        return FAIL;

    entry->path = sgreet_strdup(path);

    char   *buf = NULL;
    size_t  sz;
    ssize_t len;

    entry->name = NULL;
    entry->exec = NULL;

    while ((len = getline(&buf, &sz, fp)) != -1)
    {
        if (strcmp(buf, "[Desktop Entry]") == 0 || *buf == '#')
            continue;

        char *p = strchr(buf, '=');

        if (p == NULL)
            continue;

        p++; // Skip hashtag

        char **store;

        if (strncmp(buf, "Name", 4) == 0)
            store = &entry->name;
        else if (strncmp(buf, "Exec", 4) == 0)
            store = &entry->exec;
        else
            continue;

        *store = sgreet_strdup(p);
    }

    sgreet_free(buf);
    fclose(fp);

    if (entry->name == NULL || entry->exec == NULL)
    {
        sgreet_log("Invalid .desktop file '%s'", path);
        clear_desktop_entry(entry);
        return FAIL;
    }

    return OK;
}

/*
 * Parse the issue file, handling any escape codes and return final string as a
 * static buffer
 */
static char *
get_issue(const char *path)
{
    FILE *fp = fopen(path, "r");

    if (fp == NULL)
        return "";

#define BUFSIZE 128
    static char buf[BUFSIZE];
    int         len = 0;
    int         c;

    while (len < BUFSIZE && (c = fgetc(fp)) != EOF)
    {
        int max = BUFSIZE - len;

        if (c == '\\')
        {
            c = fgetc(fp);

            switch (c)
            {
            case 'd': // Current date
            {
                time_t     t = time(NULL);
                struct tm *tm = localtime(&t);

                len += strftime(buf + len, max, "%F", tm);
                break;
            }
            case 'l': // Current TTY
                len += snprintf(buf + len, max, "%s", SGREET.tty_name);
                break;
            case 's': // Operating system name
                len += snprintf(buf + len, max, "%s", SGREET.uts.sysname);
                break;
            case 'm': // Architecture identifier
                len += snprintf(buf + len, max, "%s", SGREET.uts.machine);
                break;
            case 'n': // Hostname
                len += snprintf(buf + len, max, "%s", SGREET.uts.nodename);
                break;
            case 'o': // Domain name
                len += snprintf(buf + len, max, "%s", SGREET.uts.domainname);
                break;
            case 'r': // Kernel release number
                len += snprintf(buf + len, max, "%s", SGREET.uts.release);
                break;
            }
        }
        else
            buf[len++] = c;
    }
#undef BUFSIZE

    fclose(fp);
    return buf;
}

/*
 * Initialize global state for greeter. Returns OK on success and FAIL on
 * failure.
 */
int
sgreet_init(const char **session_dirs, int session_dirs_len)
{
    struct desktop_entry *entries = NULL;
    int                   entries_len = 0;

    for (int i = 0; i < session_dirs_len; i++)
    {
        const char *dir = session_dirs[i];

        DIR           *dp = opendir(dir);
        struct dirent *de;

        if (dp == NULL)
        {
            sgreet_log("Error opening directory '%s'", dir);
            continue;
        }

        while ((de = readdir(dp)) != NULL)
        {
            if (de->d_type != DT_REG)
                continue;

            char *fullpath = sgreet_strdup_printf("%s/%s", dir, de->d_name);
            int   ret;

            entries = sgreet_realloc(
                entries, ++entries_len * sizeof(struct desktop_entry)
            );
            ret = parse_desktop_file(fullpath, entries + entries_len - 1);
            sgreet_free(fullpath);

            if (ret == FAIL)
                entries_len--;
        }

        closedir(dp);
    }

    if (entries == NULL)
    {
        fprintf(stderr, "No directory for session files provided\n");
        return FAIL;
    }

    SGREET.entries = entries;
    SGREET.entries_len = entries_len;
    SGREET.sock_fd = -1;

    if (uname(&SGREET.uts) == -1 ||
        (SGREET.tty_name = ttyname(STDIN_FILENO)) == NULL)
    {
        sgreet_uninit();
        return FAIL;
    }

    SGREET.state = SGREET_STATE_USERNAME;

    return OK;
}

/*
 * Free resources
 */
void
sgreet_uninit(void)
{
    sgreet_free(SGREET.tty_name);

    for (int i = 0; i < SGREET.entries_len; i++)
        clear_desktop_entry(SGREET.entries + i);
    sgreet_free(SGREET.entries);

    if (SGREET.logfile != NULL)
        fclose(SGREET.logfile);

    ui_textbox_clear(&SGREET.username);
    ui_label_clear(&SGREET.issue);

    if (SGREET.auth != NULL)
    {
        for (int i = 0; i < SGREET.auth_len; i++)
            ui_textbox_clear(SGREET.auth + i);
        sgreet_free(SGREET.auth);
    }

    if (SGREET.sock_fd != -1)
        close(SGREET.sock_fd);
}

static void
handle_username_state(int c)
{
    switch (c)
    {
    case KEY_LEFT:
        ui_textbox_move(&SGREET.username, -1);
        break;
    case KEY_RIGHT:
        ui_textbox_move(&SGREET.username, 1);
        break;
    case KEY_UP:
        SGREET.state = SGREET_STATE_DESKTOPENTRY;
        ui_label_highlight(&SGREET.entry, true);
        set_cursor(CURSOR_SLEEP);
        break;
    case KEY_BACKSPACE:
        ui_textbox_del(&SGREET.username, 1);
        break;
    default:
    {
        const char *str = unctrl(c);

        ui_textbox_add(&SGREET.username, str);
    }
    }
}

/*
 * Update "entry" label to match the current desktop entry.
 */
static void
apply_current_entry(void)
{
    ui_label_update(
        &SGREET.entry, "Session: %s", SGREET.entries[SGREET.cur_entry].name
    );
}

static void
handle_entry_state(int c)
{
    switch (c)
    {
    case KEY_LEFT:
        if (SGREET.cur_entry > 0)
            SGREET.cur_entry--;
        apply_current_entry();
        break;
    case KEY_RIGHT:
        if (SGREET.cur_entry < SGREET.entries_len - 1)
            SGREET.cur_entry++;
        apply_current_entry();
        break;
    case KEY_DOWN:
        SGREET.state = SGREET_STATE_USERNAME;
        ui_label_highlight(&SGREET.entry, false);
        set_cursor(CURSOR_UNSLEEP);
        break;
    }
}

static void
handle_auth_state(int c)
{
}

/*
 * Start running the greeter. Returns OK on success and FAIL on failure.
 */
int
sgreet_run(void)
{
    SGREET.sock_fd = ipc_init();
    if (SGREET.sock_fd == -1)
        return FAIL;

    initscr();

    raw();
    noecho();
    keypad(stdscr, true);
    nonl();

    ui_textbox_init(&SGREET.username, 3, 0, "%s login: ", SGREET.uts.nodename);
    ui_label_init(&SGREET.issue, 0, 0, "%s", get_issue("/etc/issue"));
    ui_label_init(&SGREET.entry, 2, 0, NULL);
    apply_current_entry();

    refresh();

    while (true)
    {
        set_cursor(CURSOR_OFF);

        ui_textbox_draw(&SGREET.username);
        ui_label_draw(&SGREET.issue);
        ui_label_draw(&SGREET.entry);

        switch (SGREET.state)
        {
        case SGREET_STATE_USERNAME:
            ui_textbox_focus(&SGREET.username);
            break;
        case SGREET_STATE_AUTH:
            break;
        default:
            break;
        }

        doupdate();
        set_cursor(CURSOR_ON);

        int c = getch();

        if (c == 3) // Ctrl-C
            break;

        switch (SGREET.state)
        {
        case SGREET_STATE_USERNAME:
            handle_username_state(c);
            break;
        case SGREET_STATE_DESKTOPENTRY:
            handle_entry_state(c);
            break;
        case SGREET_STATE_AUTH:
            handle_auth_state(c);
            break;
        }
    }

    endwin();

    return OK;
}
