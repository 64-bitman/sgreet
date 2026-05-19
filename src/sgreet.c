#include "sgreet.h"
#include "ipc.h"
#include "persist.h"
#include "util.h"
#include <dirent.h>
#include <ncurses.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <time.h>
#include <unistd.h>

struct sgreet SGREET = {0};

static void
clear_desktop_entry(struct desktop_entry *entry)
{
    free(entry->path);
    free(entry->name);
    free(entry->exec);
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

    entry->path = strdup(path);
    if (entry->path == NULL)
    {
        fclose(fp);
        return FAIL;
    }

    char   *buf = NULL;
    size_t  sz;
    ssize_t len;

    entry->name = NULL;
    entry->exec = NULL;

    while ((len = getline(&buf, &sz, fp)) != -1)
    {
        buf[len - 1] = NUL;

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

        *store = strdup(p);
    }

    free(buf);
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
get_issue(const char *path, bool *has_time)
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
            case 't': // Current time
            {
                time_t     t = time(NULL);
                struct tm *tm = localtime(&t);

                if (c == 'd')
                    len += strftime(buf + len, max, "%F", tm);
                else
                {
                    len += strftime(buf + len, max, "%I:%M:%S %p", tm);
                    *has_time = true;
                }
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
    buf[len - 1] = NUL;

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

    if (SGREET.persist != 0 && SGREET.persist_path == NULL)
    {
        // Use default location
        SGREET.persist_path = "/var/cache/sgreet/persist.json";
        SGREET.persist |= PERSIST_STATIC;
    }
    persist_read();

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
            int   ret = FAIL;

            if (fullpath == NULL)
                continue;

            struct desktop_entry *tmp =
                realloc(entries, ++entries_len * sizeof(struct desktop_entry));

            if (tmp != NULL)
            {
                entries = tmp;
                ret = parse_desktop_file(fullpath, entries + entries_len - 1);
            }

            if (ret == FAIL)
                entries_len--;
            else if (
                SGREET.last_session != NULL &&
                strcmp(fullpath, SGREET.last_session) == 0
            )
                SGREET.cur_entry = entries_len - 1;

            free(fullpath);
        }

        closedir(dp);
    }
    free(SGREET.last_session);

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
    for (int i = 0; i < SGREET.entries_len; i++)
        clear_desktop_entry(SGREET.entries + i);
    free(SGREET.entries);

    if (SGREET.logfile != NULL)
        fclose(SGREET.logfile);

    ui_textbox_clear(&SGREET.username);
    ui_label_clear(&SGREET.issue);

    if (SGREET.auth != NULL)
    {
        for (int i = 0; i < SGREET.auth_len; i++)
            ui_textbox_clear(SGREET.auth + i);
        free(SGREET.auth);
    }

    if (!(SGREET.persist & PERSIST_STATIC))
        free(SGREET.persist_path);

    if (SGREET.sock_fd != -1)
        close(SGREET.sock_fd);
}

/*
 * Append a new auth prompt to answer auth message.
 */
static void
add_auth_prompt(const char *prompt, bool secret)
{
    struct ui_textbox *tmp =
        realloc(SGREET.auth, sizeof(struct ui_textbox) * (SGREET.auth_len + 1));

    if (tmp == NULL)
        return;

    SGREET.auth = tmp;

    struct ui_textbox *tb = SGREET.auth + SGREET.auth_len;

    if (secret)
    {
        if (SGREET.asterisks)
            tb->show = UI_TEXTBOX_SHOW_ASTERISKS;
        else
            tb->show = UI_TEXTBOX_SHOW_INVIS;
    }
    else
        tb->show = UI_TEXTBOX_SHOW_NORMAL;

    SGREET.state = SGREET_STATE_AUTH;

    ui_textbox_init(tb, SGREET.bot_row, 0, "%s", prompt);

    SGREET.bot_row++;
    SGREET.auth_len++;
}

static void
cancel_session(void)
{
    sgreet_log("Cancelling current session");

    struct ipc_request req;
    req.type = IPC_REQUEST_CANCEL_SESSION;

    ipc_roundtrip(SGREET.sock_fd, &req);

    for (int i = 0; i < SGREET.auth_len; i++)
    {
        werase(SGREET.auth[i].win);
        wrefresh(SGREET.auth[i].win);
        ui_textbox_clear(SGREET.auth + i);
    }
    free(SGREET.auth);
    SGREET.auth = NULL;
    SGREET.auth_len = 0;
    SGREET.bot_row = 4;
    SGREET.state = SGREET_STATE_USERNAME;
}

static void
handle_response(struct ipc_response *resp, bool start)
{
    switch (resp->type)
    {
    case IPC_RESPONSE_SUCCESS:
    {
        if (start)
        {
            endwin();
            persist_save();
            sgreet_uninit();
            exit(0);
        }
        struct ipc_request req;

        req.type = IPC_REQUEST_START_SESSION;
        snprintf(
            req.body.start_session.cmd, sizeof(req.body.start_session.cmd),
            "%s", SGREET.entries[SGREET.cur_entry].exec
        );

        struct ipc_response resp = ipc_roundtrip(SGREET.sock_fd, &req);

        sgreet_log("Starting session");

        handle_response(&resp, true);
        break;
    }
    case IPC_RESPONSE_ERROR:
    {
        ui_label_update(&SGREET.msg, "Error: %s", resp->body.error.description);
        SGREET.msg_remain = MSG_DELAY;
        cancel_session();
        break;
    }
    case IPC_RESPONSE_AUTH_MESSAGE:
    {
        switch (resp->body.auth_message.type)
        {
        case IPC_AUTH_MESSAGE_SECRET:
            add_auth_prompt(resp->body.auth_message.message, true);
            break;
        case IPC_AUTH_MESSAGE_VISIBLE:
            add_auth_prompt(resp->body.auth_message.message, false);
            break;
        case IPC_AUTH_MESSAGE_INFO:
            ui_label_update(
                &SGREET.msg, ">>> %s", resp->body.auth_message.message
            );
            break;
        case IPC_AUTH_MESSAGE_ERROR:
            ui_label_update(
                &SGREET.msg, "Authentication error: %s",
                resp->body.auth_message.message
            );
            break;
        }
        break;
    }
    }
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
    case KEY_ENTER:
    case '\n':
    case '\r':
        sgreet_log(
            "Creating session for '%.*s'", SGREET.username.len,
            SGREET.username.content
        );

        struct ipc_request req;

        req.type = IPC_REQUEST_CREATE_SESSION;

        snprintf(
            req.body.create_session.username,
            sizeof(req.body.create_session.username), "%.*s",
            SGREET.username.len, SGREET.username.content
        );

        struct ipc_response resp = ipc_roundtrip(SGREET.sock_fd, &req);

        handle_response(&resp, false);
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
    case 'h':
        if (SGREET.cur_entry > 0)
            SGREET.cur_entry--;
        apply_current_entry();
        break;
    case KEY_RIGHT:
    case 'l':
        if (SGREET.cur_entry < SGREET.entries_len - 1)
            SGREET.cur_entry++;
        apply_current_entry();
        break;
    case KEY_DOWN:
    case KEY_ENTER:
    case '\n':
    case '\r':
        SGREET.state = SGREET_STATE_USERNAME;
        ui_label_highlight(&SGREET.entry, false);
        set_cursor(CURSOR_UNSLEEP);
        break;
    }
}

static void
handle_auth_state(int c)
{
    struct ui_textbox *tb = SGREET.auth + SGREET.auth_len - 1;

    switch (c)
    {
    case KEY_LEFT:
        ui_textbox_move(tb, -1);
        break;
    case KEY_RIGHT:
        ui_textbox_move(tb, 1);
        break;
    case KEY_BACKSPACE:
        ui_textbox_del(tb, 1);
        break;
    case KEY_ENTER:
    case '\n':
    case '\r':
        sgreet_log("Answering auth message");

        struct ipc_request req;

        req.type = IPC_REQUEST_POST_AUTH_MESSAGE_RESPONSE;

        snprintf(
            req.body.post_auth_message_response.str,
            sizeof(req.body.post_auth_message_response.str), "%.*s", tb->len,
            tb->content
        );

        ui_label_update(&SGREET.msg, "Waiting for response...");
        ui_label_setpos(&SGREET.msg, SGREET.bot_row, 0);
        ui_label_draw(&SGREET.msg);
        doupdate();

        struct ipc_response resp = ipc_roundtrip(SGREET.sock_fd, &req);

        ui_label_update(&SGREET.msg, NULL);
        werase(SGREET.msg.win);
        wrefresh(SGREET.msg.win);

        handle_response(&resp, false);

        break;
    default:
    {
        const char *str = unctrl(c);

        ui_textbox_add(tb, str);
    }
    }
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

    SGREET.bot_row = 4;

    bool hastime = false;

    ui_textbox_init(&SGREET.username, 3, 0, "%s login: ", SGREET.uts.nodename);
    if (SGREET.last_user != NULL)
    {
        snprintf(
            SGREET.username.content, sizeof(SGREET.username.content), "%s",
            SGREET.last_user
        );
        SGREET.username.len = strlen(SGREET.last_user);
        // Set cursor position at the end
        SGREET.username.curpos = SGREET.username.len;
        free(SGREET.last_user);
    }

    ui_label_init(&SGREET.issue, 0, 0);
    ui_label_init(&SGREET.entry, 2, 0);
    ui_label_init(&SGREET.msg, SGREET.bot_row, 0);
    apply_current_entry();

    refresh();

    while (true)
    {
        set_cursor(CURSOR_OFF);

        ui_label_update(&SGREET.issue, "%s", get_issue("/etc/issue", &hastime));

        ui_textbox_draw(&SGREET.username);
        ui_label_draw(&SGREET.issue);
        ui_label_draw(&SGREET.entry);

        // Make sure message is at the bottom (under any auth prompts)
        ui_label_setpos(&SGREET.msg, SGREET.bot_row, 0);
        ui_label_draw(&SGREET.msg);

        // Draw any auth prompts
        if (SGREET.auth != NULL)
            for (int i = 0; i < SGREET.auth_len; i++)
                ui_textbox_draw(SGREET.auth + i);

        switch (SGREET.state)
        {
        case SGREET_STATE_USERNAME:
            ui_textbox_focus(&SGREET.username);
            break;
        case SGREET_STATE_AUTH:
            // Focus bottommost auth prompt (since thats the one that is active)
            ui_textbox_focus(SGREET.auth + SGREET.auth_len - 1);
            break;
        default:
            break;
        }

        doupdate();
        set_cursor(CURSOR_ON);

        int             delta = -1;
        struct timespec now;

        // Must update every second for timer if shown, or if there is a
        // message, then show it for a while.
        if (hastime)
        {
            // Make sure that we update synchronized to the clock
            clock_gettime(CLOCK_REALTIME, &now);
            delta = (1000000000LL - now.tv_nsec + 999999LL) / 1000000LL;
        }
        if (*SGREET.msg.str != NUL)
        {
            if (delta == -1 || delta > SGREET.msg_remain)
                delta = SGREET.msg_remain;

            clock_gettime(CLOCK_MONOTONIC, &now);
        }

        timeout(delta);

        int c = getch();

        // If there is a message, then check if enough time has passed
        if (*SGREET.msg.str != NUL)
        {
            struct timespec newnow;
            clock_gettime(CLOCK_MONOTONIC, &newnow);

            int elapsed = (int)timespec_diff_ms(&newnow, &now);

            SGREET.msg_remain -= elapsed;

            if (SGREET.msg_remain <= 0)
            {
                SGREET.msg_remain = MSG_DELAY;
                ui_label_update(&SGREET.msg, NULL);
            }
        }

        if (c == ERR)
            continue;

        if (c == 3) // Ctrl-C
        {
            // If we are in an auth prompt, then cancel the session instead
            if (SGREET.state == SGREET_STATE_AUTH)
            {
                cancel_session();
                continue;
            }
            else
                break;
        }

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
