/*
    Network Next: v2.14-667-g7e8ad02d
    Copyright © 2017 - 2018 Network Next, Inc. All rights reserved.
*/

#define _WINSOCKAPI_
#include <windows.h>
#include <winsock2.h>

#include "next_curl.h"

struct next_thread_t
{
    HANDLE handle;
};

struct next_mutex_t
{
    CRITICAL_SECTION handle;
    int level;
};

typedef DWORD next_thread_return_t;

#define NEXT_THREAD_RETURN() do { return 0; } while ( 0 )

#define NEXT_THREAD_FUNC WINAPI

#pragma warning(disable:4996)

#if _WIN64
    typedef uint64_t next_socket_handle_t;
#else
    typedef _W64 unsigned int next_socket_handle_t;
#endif

struct next_socket_t
{
    next_address_t address;
    next_socket_handle_t handle;
};

#if WINVER <= 0x0502
#define NEXT_IPV6 0
#else
#define NEXT_IPV6 1
#endif
