#include "persist.h"
#include "sgreet.h"
#include "util.h"
#include <errno.h>
#include <json.h>
#include <string.h>

/*
 * Try saving the current state in the persist file, if any.
 */
void
persist_save(void)
{
    struct json_object *root;
    const char         *err;

    if (SGREET.persist == 0)
        return;

    sgreet_log("Saving state to persist file...");

    root = json_object_new_object();
    if (root == NULL)
        return;

    if (SGREET.persist & PERSIST_SESSION)
        add_string_to_json_object(
            root, "session", SGREET.entries[SGREET.cur_entry].path
        );

    if (SGREET.persist & PERSIST_USER)
        add_string_to_json_object_len(
            root, "user", SGREET.username.content, SGREET.username.len
        );

    json_object_to_file(SGREET.persist_path, root);
    json_object_put(root);

    err = json_util_get_last_err();
    if (err != NULL)
        sgreet_log("Error writing to persist file: %s", err);
}

/*
 * Try reading from the persist file and update the current state. Should be
 * called before reading the session files.
 */
void
persist_read(void)
{
    FILE *fp;

    if (SGREET.persist == 0)
        return;

    sgreet_log("Attempting read from persist file...");

    fp = fopen(SGREET.persist_path, "r");
    if (fp == NULL)
    {
        sgreet_log("Error opening persist file: %s", strerror(errno));
        return;
    }

    long   size;
    size_t r;
    char  *buf = NULL;

    if (fseek(fp, 0, SEEK_END) == -1 || (size = ftell(fp)) == -1 ||
        fseek(fp, 0, SEEK_SET) == -1 || (buf = malloc(size + 1)) == NULL)
    {
        sgreet_log("Error reading persist file: %s", strerror(errno));
        free(buf);
        goto exit;
    }
    if ((r = fread(buf, 1, size, fp)) != (size_t)size)
    {
        free(buf);
        goto exit;
    }

    buf[size] = NUL;

    struct json_object *root = json_tokener_parse(buf);

    free(buf);
    if (root == NULL)
    {
        sgreet_log("Error parsing persist file");
        goto exit;
    }

    const char *session;
    const char *user;

    if (SGREET.persist & PERSIST_SESSION &&
        (session = get_string_from_json_object(root, "session")) != NULL)
    {
        SGREET.last_session = strdup(session);
        sgreet_log("Last session: %s", session);
    }

    if (SGREET.persist & PERSIST_USER &&
        (user = get_string_from_json_object(root, "user")) != NULL)
    {
        SGREET.last_user = strdup(user);
        sgreet_log("Last user: %s", user);
    }

    json_object_put(root);

exit:
    fclose(fp);
}
