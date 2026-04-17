#include "ipc.h"
#include "util.h"
#include <json.h>
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

    return fd;
}

/*
 * Add a string value to a JSON object. Note that "key" is assumed to be a
 * static string.
 */
static inline void
add_string_to_json_object(
    struct json_object *obj, const char *key, const char *val
)
{
    json_object_object_add_ex(
        obj, key, json_object_new_string(val), JSON_C_OBJECT_ADD_CONSTANT_KEY
    );
}

/*
 * Write "obj" to "fd" in greetd serialization format. Returns OK on success and
 * FAIL on failure.
 */
static int
write_request(int fd, struct json_object *obj)
{
    size_t      buflen;
    const char *buf = json_object_to_json_string_length(
        obj, JSON_C_TO_STRING_PLAIN, &buflen
    );

    if (buf == NULL)
        return FAIL;

    uint32_t sz = (uint32_t)buflen;
    size_t remain = sizeof(uint32_t);
    ssize_t w;

    // Write payload size
    while (remain > 0 && (w = write(fd, (char *)&sz, remain)) != -1)
        remain -= w;

    if (w == -1)
        return FAIL;

    // Write payload
    remain = buflen;
    while (remain > 0 && (w = write(fd, buf, remain)) != -1)
        remain -= w;

    return OK;
}

/*
 * Send the request and wait for a response, returning it.
 */
struct ipc_response
ipc_roundtrip(int fd, struct ipc_request *request)
{
    struct json_object *j_req = json_object_new_object();

    switch (request->type)
    {
    case IPC_REQUEST_CREATE_SESSION:
    {
        struct ipc_request_create_session *req =
            (struct ipc_request_create_session *)request;

        add_string_to_json_object(j_req, "type", "create_session");
        add_string_to_json_object(j_req, "username", req->username);
        break;
    }
    case IPC_REQUEST_POST_AUTH_MESSAGE_RESPONSE:
    {
        struct ipc_request_post_auth_message_response *req =
            (struct ipc_request_post_auth_message_response *)request;

        add_string_to_json_object(j_req, "type", "post_auth_message_response");
        if (req->str != NULL)
            add_string_to_json_object(j_req, "response", req->str);
        break;
    }
    case IPC_REQUEST_START_SESSION:
    {
        struct ipc_request_start_session *req =
            (struct ipc_request_start_session *)request;

        add_string_to_json_object(j_req, "type", "start_session");

        struct json_object *j_cmd = json_object_new_array_ext(1);
        json_object_array_add(j_cmd, json_object_new_string(req->cmd));

        json_object_object_add_ex(
            j_req, "cmd", j_cmd, JSON_C_OBJECT_ADD_CONSTANT_KEY
        );

        break;
    }
    case IPC_REQUEST_CANCEL_SESSION:
        add_string_to_json_object(j_req, "type", "cancel_session");
        break;
    }

    write_request(fd, j_req);
    json_object_put(j_req);

    struct ipc_response resp;

    return resp;
}
