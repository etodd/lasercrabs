/*
    Network Next: v2.14-667-g7e8ad02d
    Copyright © 2017 - 2018 Network Next, Inc. All rights reserved.
*/

#include "next_internal.h"

#if NEXT_PLATFORM == NEXT_PLATFORM_MAC || NEXT_PLATFORM == NEXT_PLATFORM_IOS || NEXT_PLATFORM == NEXT_PLATFORM_UNIX

#include <netdb.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <ifaddrs.h>
#include <net/if.h>
#include <fcntl.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <errno.h>

// threads

bool next_thread_create( next_thread_t * thread, next_thread_return_t (*fn)(void *), void * arg )
{
    return pthread_create( &thread->handle, NULL, fn, arg ) == 0;
}

void next_thread_join( next_thread_t * thread )
{
    pthread_join( thread->handle, NULL );
}

bool next_mutex_init( next_mutex_t * mutex )
{
    pthread_mutexattr_t attr;
    pthread_mutexattr_init(&attr);
    pthread_mutexattr_settype( &attr, PTHREAD_MUTEX_RECURSIVE );
    bool success = pthread_mutex_init( &mutex->handle, &attr ) == 0;
    pthread_mutexattr_destroy( &attr );
    return success;
}

void _next_mutex_acquire( next_mutex_t * mutex )
{
    pthread_mutex_lock( &mutex->handle );
    mutex->level++;
}

void _next_mutex_release( next_mutex_t * mutex )
{
    mutex->level--;
    pthread_mutex_unlock( &mutex->handle );
}

void next_mutex_destroy( next_mutex_t * mutex )
{
    pthread_mutex_destroy( &mutex->handle );
}

// time

#if NEXT_PLATFORM == NEXT_PLATFORM_MAC || NEXT_PLATFORM == NEXT_PLATFORM_IOS

    #include <unistd.h>
    #include <mach/mach.h>
    #include <mach/mach_time.h>

    void next_sleep( double time )
    {
        usleep( (int) ( time * 1000000 ) );
    }

    static uint64_t time_start = 0;
    static mach_timebase_info_data_t timebase_info;

    bool next_platform_init()
    {
        mach_timebase_info( &timebase_info );
        time_start = mach_absolute_time();
        if ( !next_curl_init() )
            return false;
        return true;
    }

    double next_platform_time()
    {
        uint64_t current = mach_absolute_time();
        return ( (double) ( current - time_start ) ) * ( (double) timebase_info.numer ) / ( (double) timebase_info.denom ) / 1000000000.0;
    }

#elif NEXT_PLATFORM == NEXT_PLATFORM_UNIX

    #include <unistd.h>

    void next_sleep( double time )
    {
        usleep( (int) ( time * 1000000 ) );
    }

    static double time_start;

    bool next_platform_init()
    {
        timespec ts;
        clock_gettime( CLOCK_MONOTONIC_RAW, &ts );
        time_start = ts.tv_sec + ( (double) ( ts.tv_nsec ) ) / 1000000000.0;
        if ( !next_curl_init() )
            return false;
        return true;
    }

    double next_platform_time()
    {
        timespec ts;
        clock_gettime( CLOCK_MONOTONIC_RAW, &ts );
        double current = ts.tv_sec + ( (double) ( ts.tv_nsec ) ) / 1000000000.0;
        return current - time_start;
    }

#endif

void next_platform_term()
{
}

// sockets

bool next_inet_pton4( const char * address_string, uint32_t * address_out )
{
    sockaddr_in sockaddr4;
    bool success = inet_pton( AF_INET, address_string, &sockaddr4.sin_addr ) == 1;
    *address_out = sockaddr4.sin_addr.s_addr;
    return success;
}

// address_out should be a uint16_t[8]
bool next_inet_pton6( const char * address_string, uint16_t * address_out )
{
    return inet_pton( AF_INET6, address_string, address_out ) == 1;
}

// address should be a uint16_t[8]
bool next_inet_ntop6( const uint16_t * address, char * address_string, size_t address_string_size )
{
    return inet_ntop( AF_INET6, (void*)address, address_string, socklen_t( address_string_size ) ) != NULL;
}

// TIMEOUT
// 0  - socket will be non-blocking
// >0 - socket will be blocking with the receive timeout set to the given value (in ms)
// <0 - socket will be blocking with no timeout
int next_socket_create( next_socket_t * s, const next_address_t * address, int timeout, int send_buffer_size, int receive_buffer_size, int flags )
{
    next_assert( s );
    next_assert( address );

    next_assert( address->type != NEXT_ADDRESS_NONE );

    s->address = *address;

    // create socket

    s->handle = socket( ( address->type == NEXT_ADDRESS_IPV6 ) ? AF_INET6 : AF_INET, SOCK_DGRAM, IPPROTO_UDP );

    if ( s->handle < 0 )
    {
        next_printf( NEXT_LOG_LEVEL_ERROR, "failed to create socket" );
        return NEXT_SOCKET_ERROR_CREATE_FAILED;
    }

    // force IPv6 only if necessary

    if ( address->type == NEXT_ADDRESS_IPV6 )
    {
        int yes = 1;
        if ( setsockopt( s->handle, IPPROTO_IPV6, IPV6_V6ONLY, (char*)( &yes ), sizeof( yes ) ) != 0 )
        {
            next_printf( NEXT_LOG_LEVEL_ERROR, "failed to set socket ipv6 only" );
            next_socket_destroy( s );
            return NEXT_SOCKET_ERROR_SOCKOPT_IPV6_ONLY_FAILED;
        }
    }

    // increase socket send and receive buffer sizes

    if ( setsockopt( s->handle, SOL_SOCKET, SO_SNDBUF, (char*)( &send_buffer_size ), sizeof( int ) ) != 0 )
    {
        next_printf( NEXT_LOG_LEVEL_ERROR, "failed to set socket send buffer size" );
        next_socket_destroy( s );
        return NEXT_SOCKET_ERROR_SOCKOPT_SNDBUF_FAILED;
    }

    if ( setsockopt( s->handle, SOL_SOCKET, SO_RCVBUF, (char*)( &receive_buffer_size ), sizeof( int ) ) != 0 )
    {
        next_printf( NEXT_LOG_LEVEL_ERROR, "failed to set socket receive buffer size" );
        next_socket_destroy( s );
        return NEXT_SOCKET_ERROR_SOCKOPT_RCVBUF_FAILED;
    }

    // flags
    if ( flags & NEXT_SOCKET_FLAG_REUSEPORT )
    {
        int value = 1;
        setsockopt( s->handle, SOL_SOCKET, SO_REUSEPORT, &value, sizeof( value ) );
    }

    // bind to port

    if ( address->type == NEXT_ADDRESS_IPV6 )
    {
        sockaddr_in6 socket_address;
        memset( &socket_address, 0, sizeof( sockaddr_in6 ) );
        socket_address.sin6_family = AF_INET6;
        for ( int i = 0; i < 8; ++i )
        {
            ( (uint16_t*) &socket_address.sin6_addr ) [i] = next_htons( address->data.ipv6[i] );
        }
        socket_address.sin6_port = next_htons( address->port );

        if ( bind( s->handle, (sockaddr*) &socket_address, sizeof( socket_address ) ) < 0 )
        {
            next_printf( NEXT_LOG_LEVEL_ERROR, "failed to bind socket (ipv6)" );
            next_socket_destroy( s );
            return NEXT_SOCKET_ERROR_BIND_IPV6_FAILED;
        }
    }
    else
    {
        sockaddr_in socket_address;
        memset( &socket_address, 0, sizeof( socket_address ) );
        socket_address.sin_family = AF_INET;
        socket_address.sin_addr.s_addr = ( ( (uint32_t) address->data.ipv4[0] ) )      | 
                                         ( ( (uint32_t) address->data.ipv4[1] ) << 8 )  | 
                                         ( ( (uint32_t) address->data.ipv4[2] ) << 16 ) | 
                                         ( ( (uint32_t) address->data.ipv4[3] ) << 24 );
        socket_address.sin_port = next_htons( address->port );

        if ( bind( s->handle, (sockaddr*) &socket_address, sizeof( socket_address ) ) < 0 )
        {
            next_printf( NEXT_LOG_LEVEL_ERROR, "failed to bind socket (ipv4)" );
            next_socket_destroy( s );
            return NEXT_SOCKET_ERROR_BIND_IPV4_FAILED;
        }
    }

    // if bound to port 0 find the actual port we got

    if ( address->port == 0 )
    {
        if ( address->type == NEXT_ADDRESS_IPV6 )
        {
            sockaddr_in6 sin;
            socklen_t len = sizeof( sin );
            if ( getsockname( s->handle, (sockaddr*)( &sin ), &len ) == -1 )
            {
                next_printf( NEXT_LOG_LEVEL_ERROR, "failed to get socket port (ipv6)" );
                next_socket_destroy( s );
                return NEXT_SOCKET_ERROR_GET_SOCKNAME_IPV6_FAILED;
            }
            s->address.port = next_ntohs( sin.sin6_port );
        }
        else
        {
            sockaddr_in sin;
            socklen_t len = sizeof( sin );
            if ( getsockname( s->handle, (sockaddr*)( &sin ), &len ) == -1 )
            {
                next_printf( NEXT_LOG_LEVEL_ERROR, "failed to get socket port (ipv4)" );
                next_socket_destroy( s );
                return NEXT_SOCKET_ERROR_GET_SOCKNAME_IPV4_FAILED;
            }
            s->address.port = next_ntohs( sin.sin_port );
        }
    }

    // set non-blocking io

    if ( timeout == 0 )
    {
        if ( fcntl( s->handle, F_SETFL, O_NONBLOCK, 1 ) == -1 )
        {
            next_socket_destroy( s );
            return NEXT_SOCKET_ERROR_SET_NON_BLOCKING_FAILED;
        }
    }
    else if ( timeout > 0 )
    {
        // set receive timeout
        struct timeval tv;
        tv.tv_sec = 0;
        tv.tv_usec = timeout * 1000;
        if ( setsockopt( s->handle, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof( tv ) ) < 0 )
            return NEXT_SOCKET_ERROR_SOCKOPT_RCVTIMEO_FAILED;
    }
    else
    {
        // timeout < 0, socket is blocking with no timeout
    }

    return NEXT_SOCKET_ERROR_NONE;
}

void next_socket_destroy( next_socket_t * socket )
{
    next_assert( socket );

    if ( socket->handle != 0 )
    {
        close( socket->handle );
        socket->handle = 0;
    }
    memset( socket, 0, sizeof( *socket ) );
}

void next_socket_send_packet( next_socket_t * socket, const next_address_t * to, void * packet_data, int packet_bytes )
{
    next_assert( socket );
    next_assert( to );
    next_assert( to->type == NEXT_ADDRESS_IPV6 || to->type == NEXT_ADDRESS_IPV4 );
    next_assert( packet_data );
    next_assert( packet_bytes > 0 );

    if ( to->type == NEXT_ADDRESS_IPV6 )
    {
        sockaddr_in6 socket_address;
        memset( &socket_address, 0, sizeof( socket_address ) );
        socket_address.sin6_family = AF_INET6;
        for ( int i = 0; i < 8; ++i )
        {
            ( (uint16_t*) &socket_address.sin6_addr ) [i] = next_htons( to->data.ipv6[i] );
        }
        socket_address.sin6_port = next_htons( to->port );
        int result = int( sendto( socket->handle, (char*)( packet_data ), packet_bytes, 0, (sockaddr*)( &socket_address ), sizeof(sockaddr_in6) ) );
        if ( result < 0 )
        {
            char address_string[NEXT_MAX_ADDRESS_STRING_LENGTH];
            next_address_to_string( to, address_string );
            next_printf( NEXT_LOG_LEVEL_ERROR, "sendto (%s) failed: %s", address_string, strerror( errno ) );
        }
    }
    else if ( to->type == NEXT_ADDRESS_IPV4 )
    {
        sockaddr_in socket_address;
        memset( &socket_address, 0, sizeof( socket_address ) );
        socket_address.sin_family = AF_INET;
        socket_address.sin_addr.s_addr = ( ( (uint32_t) to->data.ipv4[0] ) )        | 
                                         ( ( (uint32_t) to->data.ipv4[1] ) << 8 )   | 
                                         ( ( (uint32_t) to->data.ipv4[2] ) << 16 )  | 
                                         ( ( (uint32_t) to->data.ipv4[3] ) << 24 );
        socket_address.sin_port = next_htons( to->port );
        int result = int( sendto( socket->handle, (const char*)( packet_data ), packet_bytes, 0, (sockaddr*)( &socket_address ), sizeof(sockaddr_in) ) );
        if ( result < 0 )
        {
            char address_string[NEXT_MAX_ADDRESS_STRING_LENGTH];
            next_address_to_string( to, address_string );
            next_printf( NEXT_LOG_LEVEL_ERROR, "sendto (%s) failed: %s", address_string, strerror( errno ) );
        }
    }
    else
    {
        next_printf( NEXT_LOG_LEVEL_ERROR, "invalid address type. could not send packet" );
    }
}

int next_socket_receive_packet( next_socket_t * socket, next_address_t * from, void * packet_data, int max_packet_size )
{
    next_assert( socket );
    next_assert( from );
    next_assert( packet_data );
    next_assert( max_packet_size > 0 );

    sockaddr_storage sockaddr_from;
    socklen_t from_length = sizeof( sockaddr_from );

    int result = int( recvfrom( socket->handle, (char*) packet_data, max_packet_size, 0, (sockaddr*) &sockaddr_from, &from_length ) );

    if ( result <= 0 )
    {
        if ( errno == EAGAIN || errno == EINTR )
        {
            return 0;
        }

        next_printf( NEXT_LOG_LEVEL_ERROR, "recvfrom failed with error %d", errno );
        
        return 0;
    }

    if ( sockaddr_from.ss_family == AF_INET6 )
    {
        sockaddr_in6 * addr_ipv6 = (sockaddr_in6*) &sockaddr_from;
        from->type = NEXT_ADDRESS_IPV6;
        for ( int i = 0; i < 8; ++i )
        {
            from->data.ipv6[i] = next_ntohs( ( (uint16_t*) &addr_ipv6->sin6_addr ) [i] );
        }
        from->port = next_ntohs( addr_ipv6->sin6_port );
    }
    else if ( sockaddr_from.ss_family == AF_INET )
    {
        sockaddr_in * addr_ipv4 = (sockaddr_in*) &sockaddr_from;
        from->type = NEXT_ADDRESS_IPV4;
        from->data.ipv4[0] = (uint8_t) ( ( addr_ipv4->sin_addr.s_addr & 0x000000FF ) );
        from->data.ipv4[1] = (uint8_t) ( ( addr_ipv4->sin_addr.s_addr & 0x0000FF00 ) >> 8 );
        from->data.ipv4[2] = (uint8_t) ( ( addr_ipv4->sin_addr.s_addr & 0x00FF0000 ) >> 16 );
        from->data.ipv4[3] = (uint8_t) ( ( addr_ipv4->sin_addr.s_addr & 0xFF000000 ) >> 24 );
        from->port = next_ntohs( addr_ipv4->sin_port );
    }
    else
    {
        next_assert( 0 );
        return 0;
    }
  
    next_assert( result >= 0 );

    return result;

}

// http

void next_platform_curl_easy_init( CURL * )
{
}

#endif
