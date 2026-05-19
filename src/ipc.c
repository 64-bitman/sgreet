#include "ipc.h"
#include "util.h"
#include <errno.h>
#include <json.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>

/*
 * Return socket file descriptor for greetd socket. Returns -1 on failure.
 */
int
ipc_init(void)
{
    int         fd;
    const char *sockpath = getenv("GREETD_SOCK");

    if (sockpath == NULL)
    {
        fprintf(stderr, "$GREETD_SOCK not defined in environment\n");
        return -1;
    }

    fd = socket(AF_UNIX, SOCK_STREAM, 0);
    if (fd == -1)
    {
        perror("Error creating socket");
        return -1;
    }

    struct sockaddr_un addr;

    snprintf(addr.sun_path, sizeof(addr.sun_path), "%s", sockpath);
    addr.sun_family = AF_UNIX;

    if (connect(fd, (struct sockaddr *)&addr, sizeof(addr)) == -1)
    {
        perror("Error connecting to greetd socket");
        close(fd);
        return -1;
    }

    sgreet_log("Connected to socket '%s'", sockpath);

    return fd;
}

/*
 * Write "obj" to "fd" in greetd serialization format. Returns OK on success and
 * FAIL on failure.
 */
static int
write_request(int fd, struct json_object *obj, bool secret)
{
    size_t      buflen;
    const char *buf =
        json_object_to_json_string_length(obj, JSON_C_TO_STRING_PLAIN, &buflen);

    if (buf == NULL)
        return FAIL;

    uint32_t sz = (uint32_t)buflen;
    size_t   remain = sizeof(uint32_t);
    ssize_t  w;

    // Write payload size
    while (remain > 0 && (w = write(fd, (char *)&sz, remain)) > 0)
        remain -= w;

    if (w == -1)
        return FAIL;

    // Write payload
    remain = buflen;
    while (remain > 0 && (w = write(fd, buf, remain)) > 0)
        remain -= w;

    if (!secret)
        sgreet_log("Sent request of size %zu: %s", buflen, buf);
    else
        sgreet_log("Sent request");

    return OK;
}

/*
 * Read a response from greetd. Returns NULL on failure.
 */
static struct json_object *
read_response(int fd)
{
    char    *buf = malloc(sizeof(uint32_t));
    uint32_t sz;

    if (buf == NULL)
        return NULL;

    // Read payload size
    size_t  remain = sizeof(uint32_t);
    ssize_t r;

    while (remain > 0 && (r = read(fd, buf, remain)) > 0)
        remain -= r;
    if (r == -1)
    {
        free(buf);
        return NULL;
    }
    memcpy(&sz, buf, sizeof(sz));

    char *tmp = realloc(buf, sz + 1); // Include NUL
    if (tmp == NULL)
    {
        sgreet_log("Error allocating buffer of size %u", sz);
        free(buf);
        return NULL;
    }
    buf = tmp;

    // Read payload into buffer
    remain = sz;
    while (remain > 0 && (r = read(fd, buf, remain)) > 0)
        remain -= r;
    buf[sz] = NUL;

    struct json_object *resp = json_tokener_parse(buf);

    if (resp != NULL)
        sgreet_log("Read response of size %u: %s", sz, buf);
    else
        sgreet_log("Error parsing response");

    free(buf);
    return resp;
}

/*
 * Send the request and wait for a response, returning it.
 */
struct ipc_response
ipc_roundtrip(int fd, struct ipc_request *req)
{
    struct json_object *j_req = json_object_new_object();

    switch (req->type)
    {
    case IPC_REQUEST_CREATE_SESSION:
    {
        add_string_to_json_object(j_req, "type", "create_session");
        add_string_to_json_object(
            j_req, "username", req->body.create_session.username
        );
        break;
    }
    case IPC_REQUEST_POST_AUTH_MESSAGE_RESPONSE:
    {
        add_string_to_json_object(j_req, "type", "post_auth_message_response");
        if (*req->body.post_auth_message_response.str != NUL)
            add_string_to_json_object(
                j_req, "response", req->body.post_auth_message_response.str
            );
        break;
    }
    case IPC_REQUEST_START_SESSION:
    {
        add_string_to_json_object(j_req, "type", "start_session");

        struct json_object *j_cmd = json_object_new_array_ext(1);

        json_object_array_add(
            j_cmd, json_object_new_string(req->body.start_session.cmd)
        );
        json_object_object_add_ex(
            j_req, "cmd", j_cmd, JSON_C_OBJECT_ADD_CONSTANT_KEY
        );

        break;
    }
    case IPC_REQUEST_CANCEL_SESSION:
    {
        add_string_to_json_object(j_req, "type", "cancel_session");
        break;
    }
    }

    // Send request and read back response
    int ret = write_request(
        fd, j_req, req->type == IPC_REQUEST_POST_AUTH_MESSAGE_RESPONSE
    );
    struct ipc_response resp = {
        .type = IPC_RESPONSE_ERROR, .body.error.type = IPC_ERROR
    };

    snprintf(
        resp.body.error.description, sizeof(resp.body.error.description),
        "Unknown error"
    );

    json_object_put(j_req);
    if (ret == FAIL)
    {
        snprintf(
            resp.body.error.description, sizeof(resp.body.error.description),
            "Error writing request to socket: %s", strerror(errno)
        );
        return resp;
    }

    struct json_object *j_resp = read_response(fd);

    if (j_resp == NULL)
    {
        snprintf(
            resp.body.error.description, sizeof(resp.body.error.description),
            "Error reading response"
        );
        return resp;
    }

    // Parse response
    const char *type = get_string_from_json_object(j_resp, "type");

    if (type == NULL)
    {
        snprintf(
            resp.body.error.description, sizeof(resp.body.error.description),
            "Response does not have a type"
        );
        goto exit;
    }

    if (strcmp(type, "success") == 0)
    {
        resp.type = IPC_RESPONSE_SUCCESS;
    }
    else if (strcmp(type, "error") == 0)
    {
        const char *error_type =
            get_string_from_json_object(j_resp, "error_type");

        if (strcmp(error_type, "auth_error") == 0)
            resp.body.error.type = IPC_ERROR_AUTH;
        else if (strcmp(error_type, "error") != 0)
        {
            snprintf(
                resp.body.error.description,
                sizeof(resp.body.error.description),
                "Unknown error response type '%s'", error_type
            );
            goto exit;
        }

        const char *desc = get_string_from_json_object(j_resp, "description");

        if (desc != NULL)
            snprintf(
                resp.body.error.description,
                sizeof(resp.body.error.description), "%s", desc
            );
    }
    else if (strcmp(type, "auth_message") == 0)
    {
        const char *auth_type =
            get_string_from_json_object(j_resp, "auth_message_type");

        if (auth_type == NULL)
        {
            snprintf(
                resp.body.error.description,
                sizeof(resp.body.error.description), "Auth response has no type"
            );
            goto exit;
        }
        else if (strcmp(auth_type, "visible") == 0)
            resp.body.auth_message.type = IPC_AUTH_MESSAGE_VISIBLE;
        else if (strcmp(auth_type, "secret") == 0)
            resp.body.auth_message.type = IPC_AUTH_MESSAGE_SECRET;
        else if (strcmp(auth_type, "info") == 0)
            resp.body.auth_message.type = IPC_AUTH_MESSAGE_INFO;
        else if (strcmp(auth_type, "error") == 0)
            resp.body.auth_message.type = IPC_AUTH_MESSAGE_ERROR;
        else
        {
            snprintf(
                resp.body.error.description,
                sizeof(resp.body.error.description),
                "Unknown auth response type '%s'", auth_type
            );
            goto exit;
        }

        const char *auth_message =
            get_string_from_json_object(j_resp, "auth_message");

        if (auth_message != NULL)
            snprintf(
                resp.body.auth_message.message,
                sizeof(resp.body.auth_message.message), "%s", auth_message
            );

        resp.type = IPC_RESPONSE_AUTH_MESSAGE;
    }
    else
    {
        snprintf(
            resp.body.error.description, sizeof(resp.body.error.description),
            "Unknown response type '%s'", type
        );
        goto exit;
    }

    sgreet_log("Finished roundtrip");

exit:
    json_object_put(j_resp);
    return resp;
}
