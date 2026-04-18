#pragma once

enum ipc_request_type
{
    IPC_REQUEST_CREATE_SESSION,
    IPC_REQUEST_START_SESSION,
    IPC_REQUEST_POST_AUTH_MESSAGE_RESPONSE,
    IPC_REQUEST_CANCEL_SESSION
};

struct ipc_request_create_session
{
    char username[256];
};

struct ipc_request_start_session
{
    char cmd[256];
};

struct ipc_request_cancel_session
{
};

struct ipc_request_post_auth_message_response
{
    char str[256]; // May be empty string
};

struct ipc_request
{
    enum ipc_request_type type;
    union
    {
        struct ipc_request_create_session create_session;
        struct ipc_request_start_session  start_session;
        struct ipc_request_cancel_session cancel_session;
        struct ipc_request_post_auth_message_response
            post_auth_message_response;
    } body;
};

enum ipc_auth_message_type
{
    IPC_AUTH_MESSAGE_VISIBLE,
    IPC_AUTH_MESSAGE_SECRET,
    IPC_AUTH_MESSAGE_INFO,
    IPC_AUTH_MESSAGE_ERROR
};

enum ipc_error_type
{
    IPC_ERROR,
    IPC_ERROR_AUTH
};

enum ipc_response_type
{
    IPC_RESPONSE_SUCCESS,
    IPC_RESPONSE_ERROR,
    IPC_RESPONSE_AUTH_MESSAGE
};

struct ipc_response_success
{
};

struct ipc_response_error
{
    enum ipc_error_type type;
    char                description[256];
};

struct ipc_response_auth_message
{
    enum ipc_auth_message_type type;
    char                       message[256];
};

struct ipc_response
{
    enum ipc_response_type type;
    union
    {
        struct ipc_response_success      success;
        struct ipc_response_error        error;
        struct ipc_response_auth_message auth_message;
    } body;
};

int                 ipc_init(void);
struct ipc_response ipc_roundtrip(int fd, struct ipc_request *req);
