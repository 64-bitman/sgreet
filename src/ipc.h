#pragma once

enum ipc_request_type
{
    IPC_REQUEST_CREATE_SESSION,
    IPC_REQUEST_START_SESSION,
    IPC_REQUEST_POST_AUTH_MESSAGE_RESPONSE,
    IPC_REQUEST_CANCEL_SESSION
};

struct ipc_request
{
    enum ipc_request_type type;
};

#define CREATE_SESSION_USERNAME_MAX 64
struct ipc_request_create_session
{
    struct ipc_request base;
    char              *username;
};

struct ipc_request_start_session
{
    struct ipc_request base;
    char              *cmd;
};

struct ipc_request_cancel_session
{
    struct ipc_request base;
};

struct ipc_request_post_auth_message_response
{
    struct ipc_request base;
    char              *str; // May be NULL
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

struct ipc_response
{
    enum ipc_response_type type;
};

struct ipc_response_success
{
    struct ipc_response base;
};

struct ipc_response_error
{
    struct ipc_response base;
    enum ipc_error_type type;
    char               *description;
};

struct ipc_response_auth_message
{
    struct ipc_response        base;
    enum ipc_auth_message_type type;
    char                      *message;
};

int ipc_init(void);
