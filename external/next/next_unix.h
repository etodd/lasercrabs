/*
    Network Next: v2.14-667-g7e8ad02d
    Copyright © 2017 - 2018 Network Next, Inc. All rights reserved.
*/

#include <pthread.h>
#include <unistd.h>
#include "next_curl.h"

struct next_thread_t
{
    pthread_t handle;
};

struct next_mutex_t
{
    pthread_mutex_t handle;
    int level;
};

typedef void * next_thread_return_t;

#define NEXT_THREAD_RETURN() do { return NULL; } while ( 0 )

typedef int next_socket_handle_t;

struct next_socket_t
{
    next_address_t address;
    next_socket_handle_t handle;
};

#define NEXT_THREAD_FUNC

#define NEXT_IPV6 1
