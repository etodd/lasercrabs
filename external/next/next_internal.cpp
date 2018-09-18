/*
    Network Next: v2.14-667-g7e8ad02d
    Copyright © 2017 - 2018 Network Next, Inc. All rights reserved.
*/

#include "next_internal.h"
#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <memory.h>
#include <ctime>

#if defined(linux) || defined(__linux) || defined(__linux__) || defined(__APPLE__)
#include <signal.h> // for asserts
#endif

#if NEXT_HTTP_LOG && NEXT_PLATFORM != NEXT_PLATFORM_WINDOWS && NEXT_PLATFORM != NEXT_PLATFORM_MAC && NEXT_PLATFORM != NEXT_PLATFORM_UNIX
#error NEXT_HTTP_LOG not supported on this platform
#endif

uint8_t NEXT_KEY_MASTER[] = {0x49, 0x2e, 0x79, 0x74, 0x49, 0x7d, 0x9d, 0x34, 0xa7, 0x55, 0x50, 0xeb, 0xab, 0x03, 0xde, 0xa9, 0x1b, 0xff, 0x61, 0xc6, 0x0e, 0x65, 0x92, 0xd7, 0x09, 0x64, 0xe9, 0x34, 0x12, 0x32, 0x5f, 0x46};

struct next_t
{
    int initialized;
    char address[1024];
#if NEXT_HTTP_LOG
    next_http_t http_log;
    next_mutex_t http_log_mutex;
    char hostname[128];
#endif // #if NEXT_HTTP_LOG
};

static next_t next;

void next_check_initialized( int initialized )
{
    (void)initialized;
    next_assert( next.initialized == initialized );
}

bool next_internal_init( const char * address )
{
    next_check_initialized( 0 );

    if ( !next_platform_init() )
    {
        return false;
    }

#if NEXT_HTTP_LOG
    gethostname( next.hostname, sizeof( next.hostname ) );
    if ( !next_mutex_init( &next.http_log_mutex ) )
    {
        next_printf( NEXT_LOG_LEVEL_ERROR, "failed to initialize http log mutex" );
        return false;
    }
    next_http_create( &next.http_log, address );
#endif // #if NEXT_HTTP_LOG

    strncpy( next.address, address, sizeof( next.address ) - 1 );

    next.initialized = 1;

    return true;
}

void next_internal_update()
{
#if NEXT_HTTP_LOG
    next_mutex_acquire( &next.http_log_mutex );
    next_http_nonblock_update( &next.http_log );
    next_mutex_release( &next.http_log_mutex );
#endif // #if NEXT_HTTP_LOG
}

const char * next_master_address()
{
    return next.address;
}

void next_internal_term()
{
    next_check_initialized( 1 );

#if NEXT_HTTP_LOG
    next_mutex_acquire( &next.http_log_mutex );
    next_http_destroy( &next.http_log );
    next_mutex_release( &next.http_log_mutex );
    next_mutex_destroy( &next.http_log_mutex );
#endif // #if NEXT_HTTP_LOG

    next_http_term();

    next_platform_term();

    next.initialized = 0;
}

static void next_default_assert_function( const char * condition, const char * function, const char * file, int line )
{
    printf( "assert failed: ( %s ), function %s, file %s, line %d\n", condition, function, file, line );
    fflush( stdout );
    #if defined(_MSC_VER)
        __debugbreak();
    #elif defined(__ORBIS__)
        __builtin_trap();
    #elif defined(__clang__)
        __builtin_debugtrap();
    #elif defined(__GNUC__)
        __builtin_trap();
    #elif defined(linux) || defined(__linux) || defined(__linux__) || defined(__APPLE__)
        raise(SIGTRAP);
    #else
        #error "asserts not supported on this platform!"
    #endif
}

static int log_level = 0;

int next_log_level()
{
    return log_level;
}

static const char * next_log_level_str( int level )
{
    if ( level == NEXT_LOG_LEVEL_DEBUG )
        return "debug";
    else if ( level == NEXT_LOG_LEVEL_INFO )
        return "info";
    else if ( level == NEXT_LOG_LEVEL_ERROR )
        return "error";
    else if ( level == NEXT_LOG_LEVEL_WARN )
        return "warning";
    else
        return "log";
}

void next_default_print_function( int level, const char * format, ... ) 
{
    va_list args;
    va_start( args, format );
    char buffer[1024];
    vsnprintf( buffer, sizeof( buffer ), format, args );
    const char * level_str = next_log_level_str( level );
    printf( "%0.2f %s: %s", next_time(), level_str, buffer );
    va_end( args );
    fflush( stdout );
}

static void (*next_print_function)( int level, const char *, ... ) = next_default_print_function;

void (*next_assert_function)( const char *, const char *, const char * file, int line ) = next_default_assert_function;

void next_set_log_level( int level )
{
    log_level = level;
}

void next_set_print_function( void (*function)( int level, const char *, ... ) )
{
    next_assert( function );
    next_print_function = function;
}

void next_set_assert_function( void (*function)( const char *, const char *, const char * file, int line ) )
{
    next_assert_function = function;
}

#if NEXT_ENABLE_LOGGING

void next_flow_log( int level, uint64_t flow_id, uint8_t flow_version, const char * format, ... )
{
    if ( level > log_level )
        return;
    va_list args;
    va_start( args, format );
    char buffer[1024];
    vsnprintf( buffer, sizeof( buffer ), format, args );
    va_end( args );
    next_print_function( level, "[%" PRIx64 ":%hhu] %s\n", flow_id, flow_version, buffer );
#if NEXT_HTTP_LOG
    if ( level < NEXT_LOG_LEVEL_INFO )
    {
        char buffer2[4*1024];
        snprintf( buffer2, sizeof( buffer ), "node=%s msg=\"%s\" level=%s flow_id=%" PRIx64 " flow_version=%hhu", next.hostname, buffer, next_log_level_str( level ), flow_id, flow_version );
        next_mutex_acquire( &next.http_log_mutex );
        next_http_nonblock_post_json( &next.http_log, "/v2/stats/log", buffer2, NULL, NULL, 10000 );
        next_mutex_release( &next.http_log_mutex );
    }
#endif // #if NEXT_HTTP_LOG
}

void next_printf( int level, const char * format, ... ) 
{
    if ( level > log_level )
        return;
    va_list args;
    va_start( args, format );
    char buffer[1024];
    vsnprintf( buffer, sizeof( buffer ), format, args );
    next_print_function( level, "%s\n", buffer );
    va_end( args );
#if NEXT_HTTP_LOG
    if ( level < NEXT_LOG_LEVEL_INFO )
    {
        char buffer2[4*1024];
        snprintf( buffer2, sizeof( buffer ), "node=%s msg=\"%s\" level=%s", next.hostname, buffer, next_log_level_str( level ) );
        next_mutex_acquire( &next.http_log_mutex );
        next_http_nonblock_post_json( &next.http_log, "/v2/stats/log", buffer2, NULL, NULL, 10000 );
        next_mutex_release( &next.http_log_mutex );
    }
#endif // #if NEXT_HTTP_LOG
}

#else // #if NEXT_ENABLE_LOGGING

void next_flow_log( int level, uint64_t flow_id, uint8_t flow_version, uint8_t flow_flags, const char * format, ... )
{
    (void) level;
    (void) flow_id;
    (void) flow_version;
    (void) flow_flags;
    (void) format;
}

void next_printf( int level, const char * format, ... ) 
{
    (void) level;
    (void) format;
}

#endif // #if NEXT_ENABLE_LOGGING

// ------------------------------------------------------------------

#if NEXT_ENABLE_TESTS
static double next_test_time = -1.0;
#endif // #if NEXT_ENABLE_TESTS

double next_time()
{
#if NEXT_ENABLE_TESTS
    if ( next_test_time >= 0.0 )
    {
        return next_test_time;
    }
#endif // #if NEXT_ENABLE_TESTS

    return next_platform_time();
}

uint16_t next_ntohs( uint16_t in )
{
    return (uint16_t)( ( ( in << 8 ) & 0xFF00 ) | ( ( in >> 8 ) & 0x00FF ) );
}

uint16_t next_htons( uint16_t in )
{
    return (uint16_t)( ( ( in << 8 ) & 0xFF00 ) | ( ( in >> 8 ) & 0x00FF ) );
}

// ----------------------------------------------------------------

int next_address_parse( next_address_t * address, const char * address_string_in )
{
    next_assert( address );
    next_assert( address_string_in );

    if ( !address )
        return NEXT_ERROR;

    if ( !address_string_in )
        return NEXT_ERROR;

    memset( address, 0, sizeof( next_address_t ) );

    // first try to parse the string as an IPv6 address:
    // 1. if the first character is '[' then it's probably an ipv6 in form "[addr6]:portnum"
    // 2. otherwise try to parse as a raw IPv6 address using inet_pton

    char buffer[NEXT_MAX_ADDRESS_STRING_LENGTH + NEXT_ADDRESS_BUFFER_SAFETY*2];

    char * address_string = buffer + NEXT_ADDRESS_BUFFER_SAFETY;
    strncpy( address_string, address_string_in, NEXT_MAX_ADDRESS_STRING_LENGTH - 1 );
    address_string[NEXT_MAX_ADDRESS_STRING_LENGTH-1] = '\0';

    int address_string_length = (int) strlen( address_string );

    if ( address_string[0] == '[' )
    {
        const int base_index = address_string_length - 1;
        
        // note: no need to search past 6 characters as ":65535" is longest possible port value
        for ( int i = 0; i < 6; ++i )
        {
            const int index = base_index - i;
            if ( index < 3 )
            {
                return NEXT_ERROR;
            }
            if ( address_string[index] == ':' )
            {
                address->port = (uint16_t) ( atoi( &address_string[index + 1] ) );
                address_string[index-1] = '\0';
            }
        }
        address_string += 1;
    }

    uint16_t addr6[8];
    if ( next_inet_pton6( address_string, addr6 ) )
    {
        address->type = NEXT_ADDRESS_IPV6;
        for ( int i = 0; i < 8; ++i )
        {
            address->data.ipv6[i] = next_ntohs( addr6[i] );
        }
        return NEXT_OK;
    }

    // otherwise it's probably an IPv4 address:
    // 1. look for ":portnum", if found save the portnum and strip it out
    // 2. parse remaining ipv4 address via inet_pton

    address_string_length = (int) strlen( address_string );
    const int base_index = address_string_length - 1;
    for ( int i = 0; i < 6; ++i )
    {
        const int index = base_index - i;
        if ( index < 0 )
            break;
        if ( address_string[index] == ':' )
        {
            address->port = (uint16_t)( atoi( &address_string[index + 1] ) );
            address_string[index] = '\0';
        }
    }

    uint32_t addr4;
    if ( next_inet_pton4( address_string, &addr4 ) )
    {
        address->type = NEXT_ADDRESS_IPV4;
        address->data.ipv4[3] = (uint8_t) ( ( addr4 & 0xFF000000 ) >> 24 );
        address->data.ipv4[2] = (uint8_t) ( ( addr4 & 0x00FF0000 ) >> 16 );
        address->data.ipv4[1] = (uint8_t) ( ( addr4 & 0x0000FF00 ) >> 8  );
        address->data.ipv4[0] = (uint8_t) ( ( addr4 & 0x000000FF )     );
        return NEXT_OK;
    }

    return NEXT_ERROR;
}

char * next_address_to_string( const next_address_t * address, char * buffer )
{
    next_assert( buffer );

    if ( address->type == NEXT_ADDRESS_IPV6 )
    {
#if defined(WINVER) && WINVER <= 0x0502
        // ipv6 not supported
        buffer[0] = '\0';
        return buffer;
#else
        uint16_t ipv6_network_order[8];
        for ( int i = 0; i < 8; ++i )
            ipv6_network_order[i] = next_htons( address->data.ipv6[i] );
        char address_string[NEXT_MAX_ADDRESS_STRING_LENGTH];
        next_inet_ntop6( ipv6_network_order, address_string, sizeof( address_string ) );
        if ( address->port == 0 )
        {
            strncpy( buffer, address_string, NEXT_MAX_ADDRESS_STRING_LENGTH );
            return buffer;
        }
        else
        {
            snprintf( buffer, NEXT_MAX_ADDRESS_STRING_LENGTH, "[%s]:%d", address_string, address->port );
            return buffer;
        }
#endif
    }
    else if ( address->type == NEXT_ADDRESS_IPV4 )
    {
        if ( address->port != 0 )
        {
            snprintf( buffer, 
                      NEXT_MAX_ADDRESS_STRING_LENGTH, 
                      "%d.%d.%d.%d:%d", 
                      address->data.ipv4[0], 
                      address->data.ipv4[1], 
                      address->data.ipv4[2], 
                      address->data.ipv4[3], 
                      address->port );
        }
        else
        {
            snprintf( buffer, 
                      NEXT_MAX_ADDRESS_STRING_LENGTH, 
                      "%d.%d.%d.%d", 
                      address->data.ipv4[0], 
                      address->data.ipv4[1], 
                      address->data.ipv4[2], 
                      address->data.ipv4[3] );
        }
        return buffer;
    }
    else
    {
        snprintf( buffer, NEXT_MAX_ADDRESS_STRING_LENGTH, "%s", "NONE" );
        return buffer;
    }
}

int next_address_equal( const next_address_t * a, const next_address_t * b )
{
    next_assert( a );
    next_assert( b );

    if ( a->type != b->type )
        return 0;

    if ( a->port != b->port )
        return 0;

    if ( a->type == NEXT_ADDRESS_IPV4 )
    {
        for ( int i = 0; i < 4; ++i )
        {
            if ( a->data.ipv4[i] != b->data.ipv4[i] )
                return 0;
        }
    }
    else if ( a->type == NEXT_ADDRESS_IPV6 )
    {
        for ( int i = 0; i < 8; ++i )
        {
            if ( a->data.ipv6[i] != b->data.ipv6[i] )
                return 0;
        }
    }
    else
    {
        return 0;
    }

    return 1;
}

void next_print_bytes( const char * label, const uint8_t * data, int data_bytes )
{
    printf( "%s: ", label );
    for ( int i = 0; i < data_bytes; ++i )
    {
        printf( "0x%02x,", (int) data[i] );
    }
    printf( " (%d bytes)\n", data_bytes );
}

// ----------------------------------------------------------------

void next_write_uint8( uint8_t ** p, uint8_t value )
{
    **p = value;
    ++(*p);
}

void next_write_uint16( uint8_t ** p, uint16_t value )
{
    (*p)[0] = value & 0xFF;
    (*p)[1] = value >> 8;
    *p += 2;
}

void next_write_uint32( uint8_t ** p, uint32_t value )
{
    (*p)[0] = value & 0xFF;
    (*p)[1] = ( value >> 8  ) & 0xFF;
    (*p)[2] = ( value >> 16 ) & 0xFF;
    (*p)[3] = value >> 24;
    *p += 4;
}

void next_write_float32( uint8_t ** p, float value )
{
    uint32_t value_int;
    char * p_value = (char *)(&value);
    char * p_value_int = (char *)(&value_int);
    memcpy(p_value_int, p_value, sizeof(uint32_t));
    next_write_uint32( p, value_int);
}

void next_write_uint64( uint8_t ** p, uint64_t value )
{
    (*p)[0] = value & 0xFF;
    (*p)[1] = ( value >> 8  ) & 0xFF;
    (*p)[2] = ( value >> 16 ) & 0xFF;
    (*p)[3] = ( value >> 24 ) & 0xFF;
    (*p)[4] = ( value >> 32 ) & 0xFF;
    (*p)[5] = ( value >> 40 ) & 0xFF;
    (*p)[6] = ( value >> 48 ) & 0xFF;
    (*p)[7] = value >> 56;
    *p += 8;
}

void next_write_float64( uint8_t ** p, double value )
{
    uint64_t value_int;
    char * p_value = (char *)(&value);
    char * p_value_int = (char *)(&value_int);
    memcpy(p_value_int, p_value, sizeof(uint64_t));
    next_write_uint64( p, value_int);
}

void next_write_bytes( uint8_t ** p, uint8_t * byte_array, int num_bytes )
{
    for ( int i = 0; i < num_bytes; ++i )
    {
        next_write_uint8( p, byte_array[i] );
    }
}

uint8_t next_read_uint8( uint8_t ** p )
{
    uint8_t value = **p;
    ++(*p);
    return value;
}

uint16_t next_read_uint16( uint8_t ** p )
{
    uint16_t value;
    value = (*p)[0];
    value |= ( ( (uint16_t)( (*p)[1] ) ) << 8 );
    *p += 2;
    return value;
}

uint32_t next_read_uint32( uint8_t ** p )
{
    uint32_t value;
    value  = (*p)[0];
    value |= ( ( (uint32_t)( (*p)[1] ) ) << 8 );
    value |= ( ( (uint32_t)( (*p)[2] ) ) << 16 );
    value |= ( ( (uint32_t)( (*p)[3] ) ) << 24 );
    *p += 4;
    return value;
}

uint64_t next_read_uint64( uint8_t ** p )
{
    uint64_t value;
    value  = (*p)[0];
    value |= ( ( (uint64_t)( (*p)[1] ) ) << 8  );
    value |= ( ( (uint64_t)( (*p)[2] ) ) << 16 );
    value |= ( ( (uint64_t)( (*p)[3] ) ) << 24 );
    value |= ( ( (uint64_t)( (*p)[4] ) ) << 32 );
    value |= ( ( (uint64_t)( (*p)[5] ) ) << 40 );
    value |= ( ( (uint64_t)( (*p)[6] ) ) << 48 );
    value |= ( ( (uint64_t)( (*p)[7] ) ) << 56 );
    *p += 8;
    return value;
}

float next_read_float32( uint8_t ** p )
{
    uint32_t value_int = next_read_uint32( p );
    float value_float;
    uint8_t * pointer_int = (uint8_t *)( &value_int );
    uint8_t * pointer_float = (uint8_t *)( &value_float );
    memcpy(pointer_float, pointer_int, sizeof( value_int ) );
    return value_float;
}

double next_read_float64( uint8_t ** p )
{
    uint64_t value_int = next_read_uint64( p );
    double value_float;
    uint8_t * pointer_int = (uint8_t *)( &value_int );
    uint8_t * pointer_float = (uint8_t *)( &value_float );
    memcpy(pointer_float, pointer_int, sizeof( value_int ) );
    return value_float;
}

void next_read_bytes( uint8_t ** p, uint8_t * byte_array, int num_bytes )
{
    for ( int i = 0; i < num_bytes; ++i )
    {
        byte_array[i] = next_read_uint8( p );
    }
}

// ----------------------------------------------------------------

#if SODIUM_LIBRARY_VERSION_MAJOR > 7 || ( SODIUM_LIBRARY_VERSION_MAJOR && SODIUM_LIBRARY_VERSION_MINOR >= 3 )
#define SODIUM_SUPPORTS_OVERLAPPING_BUFFERS 1
#endif

void next_generate_key( uint8_t * key )
{
    next_assert( key );
    randombytes_buf( key, NEXT_SYMMETRIC_KEY_BYTES );
}

void next_generate_keypair( uint8_t * public_key, uint8_t * private_key )
{
    next_assert( public_key );
    next_assert( private_key );
    crypto_box_keypair( public_key, private_key );
}

void next_random_bytes( uint8_t * data, int bytes )
{
    next_assert( data );
    next_assert( bytes > 0 );
    randombytes_buf( data, bytes );
}

int next_encrypt_aead( uint8_t * message, uint64_t message_length, 
                       uint8_t * additional, uint64_t additional_length,
                       uint8_t * nonce,
                       uint8_t * key )
{
    unsigned long long encrypted_length;

    #if SODIUM_SUPPORTS_OVERLAPPING_BUFFERS

        int result = crypto_aead_chacha20poly1305_ietf_encrypt( message, &encrypted_length,
                                                                message, (unsigned long long) message_length,
                                                                additional, (unsigned long long) additional_length,
                                                                NULL, nonce, key );
    
        if ( result != 0 )
            return NEXT_ERROR;

    #else // #if SODIUM_SUPPORTS_OVERLAPPING_BUFFERS

        uint8_t * temp = (uint8_t *)( alloca( message_length + NEXT_SYMMETRIC_MAC_BYTES ) );

        int result = crypto_aead_chacha20poly1305_ietf_encrypt( temp, &encrypted_length,
                                                                message, (unsigned long long) message_length,
                                                                additional, (unsigned long long) additional_length,
                                                                NULL, nonce, key );
        
        if ( result == 0 )
        {
            memcpy( message, temp, message_length + NEXT_SYMMETRIC_MAC_BYTES );
        }
        else
        {
            return NEXT_ERROR;
        }
    

    #endif // #if SODIUM_SUPPORTS_OVERLAPPING_BUFFERS

    next_assert( encrypted_length == message_length + NEXT_SYMMETRIC_MAC_BYTES );

    return NEXT_OK;
}

int next_decrypt_aead( uint8_t * message, uint64_t message_length, 
                       uint8_t * additional, uint64_t additional_length,
                       uint8_t * nonce,
                       uint8_t * key )
{
    unsigned long long decrypted_length;

    #if SODIUM_SUPPORTS_OVERLAPPING_BUFFERS

        int result = crypto_aead_chacha20poly1305_ietf_decrypt( message, &decrypted_length,
                                                                NULL,
                                                                message, (unsigned long long) message_length,
                                                                additional, (unsigned long long) additional_length,
                                                                nonce, key );

        if ( result != 0 )
            return NEXT_ERROR;

    #else // #if SODIUM_SUPPORTS_OVERLAPPING_BUFFERS

        uint8_t * temp = (uint8_t *)( alloca( message_length ) );

        int result = crypto_aead_chacha20poly1305_ietf_decrypt( temp, &decrypted_length,
                                                                NULL,
                                                                message, (unsigned long long) message_length,
                                                                additional, (unsigned long long) additional_length,
                                                                nonce, key );
        
        if ( result == 0 )
        {
            memcpy( message, temp, decrypted_length );
        }
        else
        {
            return NEXT_ERROR;
        }
    

    #endif // #if SODIUM_SUPPORTS_OVERLAPPING_BUFFERS

    next_assert( decrypted_length == message_length - NEXT_SYMMETRIC_MAC_BYTES );

    return NEXT_OK;
}

// ---------------------------------------------------------------

void next_write_address( uint8_t ** buffer, next_address_t * address )
{
    next_assert( buffer );
    next_assert( *buffer );
    next_assert( address );

    uint8_t * start = *buffer;

    (void) buffer;

    if ( address->type == NEXT_ADDRESS_IPV4 )
    {
        next_write_uint8( buffer, NEXT_ADDRESS_IPV4 );
        for ( int i = 0; i < 4; ++i )
        {
            next_write_uint8( buffer, address->data.ipv4[i] );
        }
        next_write_uint16( buffer, address->port );
        for ( int i = 0; i < 12; ++i )
        {
            next_write_uint8( buffer, 0 );
        }
    }
    else if ( address->type == NEXT_ADDRESS_IPV6 )
    {
        next_write_uint8( buffer, NEXT_ADDRESS_IPV6 );
        for ( int i = 0; i < 8; ++i )
        {
            next_write_uint16( buffer, address->data.ipv6[i] );
        }
        next_write_uint16( buffer, address->port );
    }
    else
    {
        for ( int i = 0; i < NEXT_ADDRESS_BYTES; ++i )
        {
            next_write_uint8( buffer, 0 );
        }
    }

    (void) start;

    next_assert( *buffer - start == NEXT_ADDRESS_BYTES );
}

void next_read_address( uint8_t ** buffer, next_address_t * address )
{
    uint8_t * start = *buffer;

    address->type = next_read_uint8( buffer );

    if ( address->type == NEXT_ADDRESS_IPV4 )
    {
        for ( int j = 0; j < 4; ++j )
        {
            address->data.ipv4[j] = next_read_uint8( buffer );
        }
        address->port = next_read_uint16( buffer );
        for ( int i = 0; i < 12; ++i )
        {
            uint8_t dummy = next_read_uint8( buffer ); (void) dummy;
        }
    }
    else if ( address->type == NEXT_ADDRESS_IPV6 )
    {
        for ( int j = 0; j < 8; ++j )
        {
            address->data.ipv6[j] = next_read_uint16( buffer );
        }
        address->port = next_read_uint16( buffer );
    }
    else
    {
        for ( int i = 0; i < NEXT_ADDRESS_BYTES - 1; ++i )
        {
            uint8_t dummy = next_read_uint8( buffer ); (void) dummy;
        }
    }

    (void) start;

    next_assert( *buffer - start == NEXT_ADDRESS_BYTES );
}

// -----------------------------------------------------------

void next_write_flow_token( next_flow_token_t * token, uint8_t * buffer, int buffer_length )
{
    (void) buffer_length;

    next_assert( token );
    next_assert( buffer );
    next_assert( buffer_length >= NEXT_FLOW_TOKEN_BYTES );

    uint8_t * start = buffer;

    (void) start;

    next_write_uint64( &buffer, token->expire_timestamp );
    next_write_uint64( &buffer, token->flow_id );
    next_write_uint8( &buffer, token->flow_version );
    next_write_uint8( &buffer, token->flow_flags );
    next_write_uint32( &buffer, token->kbps_up );
    next_write_uint32( &buffer, token->kbps_down );
    next_write_address( &buffer, &token->next_address );
    next_write_bytes( &buffer, token->private_key, NEXT_SYMMETRIC_KEY_BYTES );

    next_assert( buffer - start == NEXT_FLOW_TOKEN_BYTES );
}

void next_read_flow_token( next_flow_token_t * token, uint8_t * buffer )
{
    next_assert( token );
    next_assert( buffer );

    uint8_t * start = buffer;

    (void) start;

    token->expire_timestamp = next_read_uint64( &buffer );
    token->flow_id = next_read_uint64( &buffer );
    token->flow_version = next_read_uint8( &buffer );
    token->flow_flags = next_read_uint8( &buffer );
    token->kbps_up = next_read_uint32( &buffer );
    token->kbps_down = next_read_uint32( &buffer );
    next_read_address( &buffer, &token->next_address );
    next_read_bytes( &buffer, token->private_key, NEXT_SYMMETRIC_KEY_BYTES );
    next_assert( buffer - start == NEXT_FLOW_TOKEN_BYTES );
}

int next_encrypt_flow_token( uint8_t * sender_private_key, uint8_t * receiver_public_key, uint8_t * nonce, uint8_t * buffer, int buffer_length )
{
    next_assert( sender_private_key );
    next_assert( receiver_public_key );
    next_assert( buffer );
    next_assert( buffer_length >= (int) ( NEXT_FLOW_TOKEN_BYTES + crypto_box_MACBYTES ) );

    (void) buffer_length;

#if SODIUM_SUPPORTS_OVERLAPPING_BUFFERS

    if ( crypto_box_easy( buffer, buffer, NEXT_FLOW_TOKEN_BYTES, nonce, receiver_public_key, sender_private_key ) != 0 )
    {
        return NEXT_ERROR;
    }

#else // #if SODIUM_SUPPORTS_OVERLAPPING_BUFFERS

    #error this version of sodium does not suppert overlapping buffers. please upgrade your libsodium!

#endif // #if SODIUM_SUPPORTS_OVERLAPPING_BUFFERS

    return NEXT_OK;
}

int next_decrypt_flow_token( uint8_t * sender_public_key, uint8_t * receiver_private_key, uint8_t * nonce, uint8_t * buffer )
{
    next_assert( sender_public_key );
    next_assert( receiver_private_key );
    next_assert( buffer );

#if SODIUM_SUPPORTS_OVERLAPPING_BUFFERS

    if ( crypto_box_open_easy( buffer, buffer, NEXT_FLOW_TOKEN_BYTES + crypto_box_MACBYTES, nonce, sender_public_key, receiver_private_key ) != 0 )
    {
        return NEXT_ERROR;
    }

#else // #if SODIUM_SUPPORTS_OVERLAPPING_BUFFERS

    #error this version of sodium does not suppert overlapping buffers. please upgrade your libsodium!

#endif // #if SODIUM_SUPPORTS_OVERLAPPING_BUFFERS

    return NEXT_OK;
}

int next_write_encrypted_flow_token( uint8_t ** buffer, next_flow_token_t * token, uint8_t * sender_private_key, uint8_t * receiver_public_key )
{
    next_assert( buffer );
    next_assert( token );
    next_assert( sender_private_key );
    next_assert( receiver_public_key );

    unsigned char nonce[crypto_box_NONCEBYTES];
    next_random_bytes( nonce, crypto_box_NONCEBYTES );

    uint8_t * start = *buffer;

    next_write_bytes( buffer, nonce, crypto_box_NONCEBYTES );

    next_write_flow_token( token, *buffer, NEXT_FLOW_TOKEN_BYTES );

    if ( next_encrypt_flow_token( sender_private_key, receiver_public_key, nonce, *buffer, NEXT_FLOW_TOKEN_BYTES + crypto_box_NONCEBYTES ) != NEXT_OK )
        return NEXT_ERROR;

    *buffer += NEXT_FLOW_TOKEN_BYTES + crypto_box_MACBYTES;

    (void) start;

    next_assert( ( *buffer - start ) == NEXT_ENCRYPTED_FLOW_TOKEN_BYTES );

    return NEXT_OK;
}

int next_read_encrypted_flow_token( uint8_t ** buffer, next_flow_token_t * token, uint8_t * sender_public_key, uint8_t * receiver_private_key )
{
    next_assert( buffer );
    next_assert( token );
    next_assert( sender_public_key );
    next_assert( receiver_private_key );

    uint8_t * nonce = *buffer;

    *buffer += crypto_box_NONCEBYTES;

    if ( next_decrypt_flow_token( sender_public_key, receiver_private_key, nonce, *buffer ) != NEXT_OK )
    {
        return NEXT_ERROR;
    }

    next_read_flow_token( token, *buffer );

    *buffer += NEXT_FLOW_TOKEN_BYTES + crypto_box_MACBYTES;

    return NEXT_OK;
}

// -----------------------------------------------------------

void next_write_continue_token( next_continue_token_t * token, uint8_t * buffer, int buffer_length )
{
    (void) buffer_length;

    next_assert( token );
    next_assert( buffer );
    next_assert( buffer_length >= NEXT_CONTINUE_TOKEN_BYTES );

    uint8_t * start = buffer;

    (void) start;

    next_write_uint64( &buffer, token->expire_timestamp );
    next_write_uint64( &buffer, token->flow_id );
    next_write_uint8( &buffer, token->flow_version );
    next_write_uint8( &buffer, token->flow_flags );

    next_assert( buffer - start == NEXT_CONTINUE_TOKEN_BYTES );
}

void next_read_continue_token( next_continue_token_t * token, uint8_t * buffer )
{
    next_assert( token );
    next_assert( buffer );

    uint8_t * start = buffer;

    (void) start;

    token->expire_timestamp = next_read_uint64( &buffer );
    token->flow_id = next_read_uint64( &buffer );
    token->flow_version = next_read_uint8( &buffer );
    token->flow_flags = next_read_uint8( &buffer );

    next_assert( buffer - start == NEXT_CONTINUE_TOKEN_BYTES );
}

int next_encrypt_continue_token( uint8_t * sender_private_key, uint8_t * receiver_public_key, uint8_t * nonce, uint8_t * buffer, int buffer_length )
{
    next_assert( sender_private_key );
    next_assert( receiver_public_key );
    next_assert( buffer );
    next_assert( buffer_length >= (int) ( NEXT_CONTINUE_TOKEN_BYTES + crypto_box_MACBYTES ) );

    (void) buffer_length;

#if SODIUM_SUPPORTS_OVERLAPPING_BUFFERS

    if ( crypto_box_easy( buffer, buffer, NEXT_CONTINUE_TOKEN_BYTES, nonce, receiver_public_key, sender_private_key ) != 0 )
    {
        return NEXT_ERROR;
    }

#else // #if SODIUM_SUPPORTS_OVERLAPPING_BUFFERS

    #error this version of sodium does not suppert overlapping buffers. please upgrade your libsodium!

#endif // #if SODIUM_SUPPORTS_OVERLAPPING_BUFFERS

    return NEXT_OK;
}

int next_decrypt_continue_token( uint8_t * sender_public_key, uint8_t * receiver_private_key, uint8_t * nonce, uint8_t * buffer )
{
    next_assert( sender_public_key );
    next_assert( receiver_private_key );
    next_assert( buffer );

#if SODIUM_SUPPORTS_OVERLAPPING_BUFFERS

    if ( crypto_box_open_easy( buffer, buffer, NEXT_CONTINUE_TOKEN_BYTES + crypto_box_MACBYTES, nonce, sender_public_key, receiver_private_key ) != 0 )
    {
        return NEXT_ERROR;
    }

#else // #if SODIUM_SUPPORTS_OVERLAPPING_BUFFERS

    #error this version of sodium does not suppert overlapping buffers. please upgrade your libsodium!

#endif // #if SODIUM_SUPPORTS_OVERLAPPING_BUFFERS

    return NEXT_OK;
}

int next_write_encrypted_continue_token( uint8_t ** buffer, next_continue_token_t * token, uint8_t * sender_private_key, uint8_t * receiver_public_key )
{
    next_assert( buffer );
    next_assert( token );
    next_assert( sender_private_key );
    next_assert( receiver_public_key );

    unsigned char nonce[crypto_box_NONCEBYTES];
    next_random_bytes( nonce, crypto_box_NONCEBYTES );

    uint8_t * start = *buffer;

    next_write_bytes( buffer, nonce, crypto_box_NONCEBYTES );

    next_write_continue_token( token, *buffer, NEXT_CONTINUE_TOKEN_BYTES );

    if ( next_encrypt_continue_token( sender_private_key, receiver_public_key, nonce, *buffer, NEXT_CONTINUE_TOKEN_BYTES + crypto_box_NONCEBYTES ) != NEXT_OK )
        return NEXT_ERROR;

    *buffer += NEXT_CONTINUE_TOKEN_BYTES + crypto_box_MACBYTES;

    (void) start;

    next_assert( ( *buffer - start ) == NEXT_ENCRYPTED_CONTINUE_TOKEN_BYTES );

    return NEXT_OK;
}

int next_read_encrypted_continue_token( uint8_t ** buffer, next_continue_token_t * token, uint8_t * sender_public_key, uint8_t * receiver_private_key )
{
    next_assert( buffer );
    next_assert( token );
    next_assert( sender_public_key );
    next_assert( receiver_private_key );

    uint8_t * nonce = *buffer;

    *buffer += crypto_box_NONCEBYTES;

    if ( next_decrypt_continue_token( sender_public_key, receiver_private_key, nonce, *buffer ) != NEXT_OK )
    {
        return NEXT_ERROR;
    }

    next_read_continue_token( token, *buffer );

    *buffer += NEXT_CONTINUE_TOKEN_BYTES + crypto_box_MACBYTES;

    return NEXT_OK;
}

// -----------------------------------------------------------

void next_write_server_token( next_server_token_t * token, uint8_t * buffer, int buffer_length )
{
    (void) buffer_length;

    next_assert( token );
    next_assert( buffer );
    next_assert( buffer_length >= (int) NEXT_SERVER_TOKEN_BYTES );

    uint8_t * start = buffer;

    (void) start;

    next_write_uint64( &buffer, token->expire_timestamp );
    next_write_uint64( &buffer, token->flow_id );
    next_write_uint8( &buffer, token->flow_version );
    next_write_uint8( &buffer, token->flow_flags );

    next_assert( buffer - start == NEXT_SERVER_TOKEN_BYTES );
}

void next_read_server_token( next_server_token_t * token, uint8_t * buffer )
{
    next_assert( token );
    next_assert( buffer );

    uint8_t * start = buffer;

    (void) start;

    token->expire_timestamp = next_read_uint64( &buffer );
    token->flow_id = next_read_uint64( &buffer );
    token->flow_version = next_read_uint8( &buffer );
    token->flow_flags = next_read_uint8( &buffer );

    next_assert( buffer - start == NEXT_SERVER_TOKEN_BYTES );
}

int next_encrypt_server_token( uint8_t * sender_private_key, uint8_t * receiver_public_key, uint8_t * nonce, uint8_t * buffer, int buffer_length )
{
    next_assert( sender_private_key );
    next_assert( receiver_public_key );
    next_assert( buffer );
    next_assert( buffer_length >= (int) ( NEXT_SERVER_TOKEN_BYTES + crypto_box_MACBYTES ) );

    (void) buffer_length;

#if SODIUM_SUPPORTS_OVERLAPPING_BUFFERS

    if ( crypto_box_easy( buffer, buffer, NEXT_SERVER_TOKEN_BYTES, nonce, receiver_public_key, sender_private_key ) != 0 )
    {
        return NEXT_ERROR;
    }

#else // #if SODIUM_SUPPORTS_OVERLAPPING_BUFFERS

    #error this version of sodium does not suppert overlapping buffers. please upgrade your libsodium!

#endif // #if SODIUM_SUPPORTS_OVERLAPPING_BUFFERS

    return NEXT_OK;
}

int next_decrypt_server_token( uint8_t * sender_public_key, uint8_t * receiver_private_key, uint8_t * nonce, uint8_t * buffer )
{
    next_assert( sender_public_key );
    next_assert( receiver_private_key );
    next_assert( buffer );

#if SODIUM_SUPPORTS_OVERLAPPING_BUFFERS

    if ( crypto_box_open_easy( buffer, buffer, NEXT_SERVER_TOKEN_BYTES + crypto_box_MACBYTES, nonce, sender_public_key, receiver_private_key ) != 0 )
    {
        return NEXT_ERROR;
    }

#else // #if SODIUM_SUPPORTS_OVERLAPPING_BUFFERS

    #error this version of sodium does not suppert overlapping buffers. please upgrade your libsodium!

#endif // #if SODIUM_SUPPORTS_OVERLAPPING_BUFFERS

    return NEXT_OK;
}

int next_write_encrypted_server_token( uint8_t ** buffer, next_server_token_t * token, uint8_t * sender_private_key, uint8_t * receiver_public_key )
{
    next_assert( buffer );
    next_assert( token );
    next_assert( sender_private_key );
    next_assert( receiver_public_key );

    unsigned char nonce[crypto_box_NONCEBYTES];
    next_random_bytes( nonce, crypto_box_NONCEBYTES );

    uint8_t * start = *buffer;

    next_write_bytes( buffer, nonce, crypto_box_NONCEBYTES );

    next_write_server_token( token, *buffer, NEXT_SERVER_TOKEN_BYTES );

    if ( next_encrypt_server_token( sender_private_key, receiver_public_key, nonce, *buffer, NEXT_SERVER_TOKEN_BYTES + crypto_box_NONCEBYTES ) != NEXT_OK )
        return NEXT_ERROR;

    *buffer += NEXT_CONTINUE_TOKEN_BYTES + crypto_box_MACBYTES;

    (void) start;

    next_assert( ( *buffer - start ) == NEXT_ENCRYPTED_SERVER_TOKEN_BYTES );

    return NEXT_OK;
}

int next_read_encrypted_server_token( uint8_t ** buffer, next_server_token_t * token, uint8_t * sender_public_key, uint8_t * receiver_private_key )
{
    next_assert( buffer );
    next_assert( token );
    next_assert( sender_public_key );
    next_assert( receiver_private_key );

    uint8_t * nonce = *buffer;

    *buffer += crypto_box_NONCEBYTES;

    if ( next_decrypt_continue_token( sender_public_key, receiver_private_key, nonce, *buffer ) != NEXT_OK )
    {
        return NEXT_ERROR;
    }

    next_read_server_token( token, *buffer );

    *buffer += NEXT_SERVER_TOKEN_BYTES + crypto_box_MACBYTES;

    return NEXT_OK;
}

// -----------------------------------------------------------------------------

static int next_packet_is_server_to_client ( uint8_t type )
{
    return type == NEXT_PACKET_TYPE_V2_ROUTE_RESPONSE
        || type == NEXT_PACKET_TYPE_V2_SERVER_TO_CLIENT
        || type == NEXT_PACKET_TYPE_V2_CONTINUE_RESPONSE
        || type == NEXT_PACKET_TYPE_V2_MIGRATE_RESPONSE
        || type == NEXT_PACKET_TYPE_V2_NEXT_SERVER_PONG;
}

static int next_packet_is_client_to_server ( uint8_t type )
{
    return type == NEXT_PACKET_TYPE_V2_CLIENT_TO_SERVER
        || type == NEXT_PACKET_TYPE_V2_MIGRATE
        || type == NEXT_PACKET_TYPE_V2_DESTROY
        || type == NEXT_PACKET_TYPE_V2_NEXT_SERVER_PING;
}

int next_write_header( uint8_t type, 
                       uint64_t sequence, 
                       uint64_t flow_id, 
                       uint8_t flow_version,
                       uint8_t flow_flags,
                       uint8_t * private_key, 
                       uint8_t * buffer, 
                       int buffer_length )
{
    next_assert( private_key );
    next_assert( buffer );
    next_assert( NEXT_HEADER_BYTES <= buffer_length );

    (void) buffer_length;

    uint8_t * start = buffer;

    if ( next_packet_is_server_to_client( type ) )
    {
        // high bit must be set
        sequence |= 1ULL << 63;
    }
    else if ( next_packet_is_client_to_server( type ) )
    {
        // high bit must be clear
        sequence &= ~(1ULL << 63);
    }

    next_write_uint8( &buffer, type );
    next_write_uint64( &buffer, sequence );

    uint8_t * additional = buffer;

    next_write_uint64( &buffer, flow_id );
    next_write_uint8( &buffer, flow_version );
    next_write_uint8( &buffer, flow_flags );

    const int additional_length = 8 + 2;

    uint8_t nonce[12];
    {
        uint8_t * p = nonce;
        next_write_uint32( &p, 0 );
        next_write_uint64( &p, sequence );
    }

    if ( next_encrypt_aead( buffer, 0, additional, additional_length, nonce, private_key ) != NEXT_OK )
        return NEXT_ERROR;

    buffer += NEXT_SYMMETRIC_MAC_BYTES;

    int bytes = (int) ( buffer - start );

    next_assert( bytes == NEXT_HEADER_BYTES );

    (void) bytes;

    return NEXT_OK;
}

int next_packet_type_has_header( uint8_t type )
{
    return type == NEXT_PACKET_TYPE_V2_MIGRATE
        || type == NEXT_PACKET_TYPE_V2_MIGRATE_RESPONSE
        || type == NEXT_PACKET_TYPE_V2_DESTROY
        || type == NEXT_PACKET_TYPE_V2_CONTINUE_RESPONSE
        || type == NEXT_PACKET_TYPE_V2_ROUTE_RESPONSE
        || type == NEXT_PACKET_TYPE_V2_CLIENT_TO_SERVER
        || type == NEXT_PACKET_TYPE_V2_SERVER_TO_CLIENT
        || type == NEXT_PACKET_TYPE_V2_NEXT_SERVER_PING
        || type == NEXT_PACKET_TYPE_V2_NEXT_SERVER_PONG;
}

int next_peek_header( uint8_t * type, 
                      uint64_t * sequence, 
                      uint64_t * flow_id, 
                      uint8_t * flow_version, 
                      uint8_t * flow_flags, 
                      uint8_t * buffer, 
                      int buffer_length )
{
    uint8_t _type;
    uint64_t _sequence;

    next_assert( buffer );

    if ( buffer_length < NEXT_HEADER_BYTES )
        return NEXT_ERROR;

    _type = next_read_uint8( &buffer );

    if ( !next_packet_type_has_header( _type ) )
        return NEXT_ERROR;

    _sequence = next_read_uint64( &buffer );

    if ( next_packet_is_server_to_client( _type ) )
    {
        // high bit must be set
        if ( !( _sequence & ( 1ULL << 63 ) ) )
            return NEXT_ERROR;

        // okay now don't worry about it any more
        _sequence &= ~( 1ULL << 63 );
    }
    else if ( next_packet_is_client_to_server( _type ) )
    {
        // high bit must be clear
        if ( _sequence & ( 1ULL << 63 ) )
            return NEXT_ERROR;
    }

    *type = _type;
    *sequence = _sequence;
    *flow_id = next_read_uint64( &buffer );
    *flow_version = next_read_uint8( &buffer );
    *flow_flags = next_read_uint8( &buffer );

    return NEXT_OK;
}

int next_read_header( uint8_t * type, 
                      uint64_t * sequence, 
                      uint64_t * flow_id, 
                      uint8_t * flow_version, 
                      uint8_t * flow_flags, 
                      uint8_t * private_key, 
                      uint8_t * buffer, 
                      int buffer_length )
{
    next_assert( private_key );
    next_assert( buffer );

    if ( buffer_length < NEXT_HEADER_BYTES )
        return NEXT_ERROR;

    uint8_t * start = buffer;

    uint8_t _type;
    uint64_t _sequence;
    uint64_t _flow_id;
    uint8_t _flow_version;
    uint8_t _flow_flags;

    _type = next_read_uint8( &buffer );

    if ( !next_packet_type_has_header( _type ) )
        return NEXT_ERROR;

    _sequence = next_read_uint64( &buffer );

    uint8_t * additional = buffer;

    const int additional_length = 8 + 2;

    _flow_id = next_read_uint64( &buffer );
    _flow_version = next_read_uint8( &buffer );
    _flow_flags = next_read_uint8( &buffer );

    uint8_t nonce[12];
    {
        uint8_t * p = nonce;
        next_write_uint32( &p, 0 );
        next_write_uint64( &p, _sequence );
    }

    if ( next_decrypt_aead( buffer, NEXT_SYMMETRIC_MAC_BYTES, additional, additional_length, nonce, private_key ) != NEXT_OK )
        return NEXT_ERROR;

    if ( next_packet_is_server_to_client( _type ) )
    {
        // high bit must be set
        if ( !( _sequence & ( 1ULL <<  63) ) )
            return NEXT_ERROR;

        // okay now don't worry about high bit any more
        _sequence &= ~( 1ULL << 63 );
    }
    else if ( next_packet_is_client_to_server( _type ) )
    {
        // high bit must be clear
        if ( _sequence & ( 1ULL << 63 ) )
            return NEXT_ERROR;
    }

    buffer += NEXT_SYMMETRIC_MAC_BYTES;

    int bytes = (int) ( buffer - start );

    next_assert( bytes == NEXT_HEADER_BYTES );

    *type = _type;
    *sequence = _sequence;
    *flow_id = _flow_id;
    *flow_version = _flow_version;
    *flow_flags = _flow_flags;

    (void) bytes;

    return NEXT_OK;
}

// ---------------------------------------------------------------

int next_read_route_prefix( next_route_prefix_t * prefix, uint8_t ** buffer, int buffer_length )
{
    // note: route prefix must be the first thing you read out of the buffer

    next_assert( prefix );
    next_assert( buffer );

    uint8_t * p = *buffer;

    if ( buffer_length < NEXT_ROUTE_PREFIX_BYTES )
        return 1;

    prefix->prefix_type = next_read_uint8( &p );
    prefix->prefix_length = next_read_uint32( &p );
    switch ( prefix->prefix_type )
    {
        case NEXT_ROUTE_PREFIX_TYPE_SERVER_ADDRESS:
        {
            if ( prefix->prefix_length != NEXT_ROUTE_PREFIX_TYPE_SERVER_ADDRESS_BYTES )
                return 1;
            next_read_address( &p, (next_address_t *)( &prefix->prefix_value[0] ) );
        }
        break;

        case NEXT_ROUTE_PREFIX_TYPE_NULL:
        case NEXT_ROUTE_PREFIX_TYPE_FORCED_ROUTE:
        {
            if ( prefix->prefix_length != NEXT_ROUTE_PREFIX_TYPE_NULL_BYTES )
                return 1;
        }
        break;

        case NEXT_ROUTE_PREFIX_TYPE_DIRECT:
        {
            if ( prefix->prefix_length > NEXT_ROUTE_PREFIX_BYTES + NEXT_MAX_ADDRESS_STRING_LENGTH )
                return 1;
            next_read_bytes( &p, prefix->prefix_value, prefix->prefix_length - NEXT_ROUTE_PREFIX_BYTES );
            prefix->prefix_value[prefix->prefix_length - NEXT_ROUTE_PREFIX_BYTES] = '\0'; // null-terminate server address string
        }
        break;

        default:
        {
            return 1;
        }
    }

    *buffer += prefix->prefix_length;
    return 0;
}

// ---------------------------------------------------------------

void next_replay_protection_reset( next_replay_protection_t * replay_protection )
{
    next_assert( replay_protection );
    replay_protection->most_recent_sequence = 0;
    memset( replay_protection->received_packet, 0xFF, sizeof( replay_protection->received_packet ) );
}

int next_replay_protection_packet_already_received( next_replay_protection_t * replay_protection, uint64_t sequence )
{
    next_assert( replay_protection );

    if ( sequence + NEXT_REPLAY_PROTECTION_BUFFER_SIZE <= replay_protection->most_recent_sequence )
        return 1;
    
    if ( sequence > replay_protection->most_recent_sequence )
        replay_protection->most_recent_sequence = sequence;

    const int index = (int) ( sequence % NEXT_REPLAY_PROTECTION_BUFFER_SIZE );

    if ( replay_protection->received_packet[index] == 0xFFFFFFFFFFFFFFFFLL )
    {
        replay_protection->received_packet[index] = sequence;
        return 0;
    }

    if ( replay_protection->received_packet[index] >= sequence )
        return 1;
    
    replay_protection->received_packet[index] = sequence;

    return 0;
}

// fnv1a64

void next_fnv_init( next_fnv_t * fnv )
{
    *fnv = 0xCBF29CE484222325;
}

void next_fnv_write( next_fnv_t * fnv, const uint8_t * data, size_t size )
{
    for ( size_t i = 0; i < size; i++ )
    {
        (*fnv) ^= data[i];
        (*fnv) *= 0x00000100000001B3;
    }
}

uint64_t next_fnv_finalize( next_fnv_t * fnv )
{
    return *fnv;
}

int next_base64_encode_string( const char * input, char * output, size_t output_size )
{
    next_assert( input );
    next_assert( output );
    next_assert( output_size > 0 );

    return next_base64_encode_data( (const uint8_t *)( input ), strlen( input ), output, output_size );
}

int next_base64_decode_string( const char * input, char * output, size_t output_size )
{
    next_assert( input );
    next_assert( output );
    next_assert( output_size > 0 );

    int output_length = next_base64_decode_data( input, (uint8_t *)( output ), output_size - 1 );
    if ( output_length < 0 )
    {
        return output_length;
    }

    output[output_length] = '\0';

    return output_length;
}

static const unsigned char base64_table_encode[65] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";

int next_base64_encode_data( const uint8_t * input, size_t input_length, char * output, size_t output_size )
{
    next_assert( input );
    next_assert( output );
    next_assert( output_size > 0 );

    char * pos;
    const uint8_t * end;
    const uint8_t * in;

    size_t output_length = 4 * ( ( input_length + 2 ) / 3 ); // 3-byte blocks to 4-byte

    if ( output_length < input_length )
    {
        return -1; // integer overflow
    }

    if ( output_length >= output_size )
    {
        return -1; // not enough room in output buffer
    }

    end = input + input_length;
    in = input;
    pos = output;
    while ( end - in >= 3 )
    {
        *pos++ = base64_table_encode[in[0] >> 2];
        *pos++ = base64_table_encode[( ( in[0] & 0x03 ) << 4 ) | ( in[1] >> 4 )];
        *pos++ = base64_table_encode[( ( in[1] & 0x0f ) << 2 ) | ( in[2] >> 6 )];
        *pos++ = base64_table_encode[in[2] & 0x3f];
        in += 3;
    }

    if (end - in)
    {
        *pos++ = base64_table_encode[in[0] >> 2];
        if (end - in == 1)
        {
            *pos++ = base64_table_encode[(in[0] & 0x03) << 4];
            *pos++ = '=';
        }
        else
        {
            *pos++ = base64_table_encode[((in[0] & 0x03) << 4) | (in[1] >> 4)];
            *pos++ = base64_table_encode[(in[1] & 0x0f) << 2];
        }
        *pos++ = '=';
    }

    output[output_length] = '\0';

    return int( output_length );
}

static const int base64_table_decode[256] =
{
    0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,
    0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,
    0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0, 62, 63, 62, 62, 63, 52, 53, 54, 55,
    56, 57, 58, 59, 60, 61,  0,  0,  0,  0,  0,  0,  0,  0,  1,  2,  3,  4,  5,  6,
    7,  8,  9, 10, 11, 12, 13, 14, 15, 16, 17, 18, 19, 20, 21, 22, 23, 24, 25,  0,
    0,  0,  0, 63,  0, 26, 27, 28, 29, 30, 31, 32, 33, 34, 35, 36, 37, 38, 39, 40,
    41, 42, 43, 44, 45, 46, 47, 48, 49, 50, 51,
};

int next_base64_decode_data( const char * input, uint8_t * output, size_t output_size )
{
    next_assert( input );
    next_assert( output );
    next_assert( output_size > 0 );

    size_t input_length = strlen( input );
    int pad = input_length > 0 && ( input_length % 4 || input[input_length - 1] == '=' );
    size_t L = ( ( input_length + 3 ) / 4 - pad ) * 4;
    size_t output_length = L / 4 * 3 + pad;

    if ( output_length > output_size )
    {
        return -1;
    }

    for ( size_t i = 0, j = 0; i < L; i += 4 )
    {
        int n = base64_table_decode[int( input[i] )] << 18 | base64_table_decode[int( input[i + 1] )] << 12 | base64_table_decode[int( input[i + 2] )] << 6 | base64_table_decode[int( input[i + 3] )];
        output[j++] = uint8_t( n >> 16 );
        output[j++] = uint8_t( n >> 8 & 0xFF );
        output[j++] = uint8_t( n & 0xFF );
    }

    if (pad)
    {
        int n = base64_table_decode[int( input[L] )] << 18 | base64_table_decode[int( input[L + 1] )] << 12;
        output[output_length - 1] = uint8_t( n >> 16 );

        if (input_length > L + 2 && input[L + 2] != '=')
        {
            n |= base64_table_decode[int( input[L + 2] )] << 6;
            output_length += 1;
            if ( output_length > output_size )
            {
                return -1;
            }
            output[output_length - 1] = uint8_t( n >> 8 & 0xFF );
        }
    }

    return int( output_length );
}

void next_write_uint64( uint8_t * p, uint64_t value )
{
    p[0] = value & 0xFF;
    p[1] = ( value >> 8  ) & 0xFF;
    p[2] = ( value >> 16 ) & 0xFF;
    p[3] = ( value >> 24 ) & 0xFF;
    p[4] = ( value >> 32 ) & 0xFF;
    p[5] = ( value >> 40 ) & 0xFF;
    p[6] = ( value >> 48 ) & 0xFF;
    p[7] = value >> 56;
}

uint64_t next_read_uint64( uint8_t * p )
{
    uint64_t value;
    value  = p[0];
    value |= ( ( (uint64_t)( p[1] ) ) << 8  );
    value |= ( ( (uint64_t)( p[2] ) ) << 16 );
    value |= ( ( (uint64_t)( p[3] ) ) << 24 );
    value |= ( ( (uint64_t)( p[4] ) ) << 32 );
    value |= ( ( (uint64_t)( p[5] ) ) << 40 );
    value |= ( ( (uint64_t)( p[6] ) ) << 48 );
    value |= ( ( (uint64_t)( p[7] ) ) << 56 );
    return value;
}

void next_sleep_until_next_frame( double start_frame_time, double frame_delta_time )
{
    double next_frame_time = start_frame_time + frame_delta_time;
    double current_time = next_time();
    if ( current_time > next_frame_time )
    {
        next_printf( NEXT_LOG_LEVEL_DEBUG, "dropped frame: %f > %f", current_time, next_frame_time ); 
        return;
    }
    double time_remaining = next_frame_time - current_time;
    double sleep_resolution = 1.0 / 1000.0;
    double sleep_time = time_remaining - sleep_resolution;
    if ( sleep_time > 0 )
    {
        next_sleep( sleep_time );
    }
    current_time = next_time();
    if ( current_time > next_frame_time )
    {
        next_printf( NEXT_LOG_LEVEL_DEBUG, "slept past frame: %f > %f", current_time, next_frame_time ); 
        return;
    }
    while ( true )
    {
        current_time = next_time();
        if ( current_time >= next_frame_time )
        {
            break;
        }
    }
}

// --------------------------------------------------

bool next_ping_token_read( next_ping_token_t * token, uint8_t * data )
{
    next_assert( token );
    next_assert( data );
    uint8_t * p = data;
    token->sequence = next_read_uint64( &p );
    token->create_timestamp = next_read_uint64( &p );
    token->expire_timestamp = next_read_uint64( &p );
    next_read_address( &p, &token->relay_address );
    if ( token->relay_address.type == NEXT_ADDRESS_NONE )
    {
        return false;
    }
    next_read_bytes( &p, token->private_data, NEXT_PING_TOKEN_PRIVATE_BYTES );
    return true;
}

bool next_nearest_relays_parse( const char * input, next_nearest_relays_t * output, next_ip2location_t * ip2location )
{
    next_assert( input );

    next_json_document_t document;
    document.Parse( input );
    return next_nearest_relays_parse_document( document, output, ip2location );
}

bool next_nearest_relays_parse_document( next_json_document_t & document, next_nearest_relays_t * output, next_ip2location_t * ip2location )
{
    if ( !document.IsObject() || !document.HasMember( "Relays" ) )
    {
        next_printf( NEXT_LOG_LEVEL_DEBUG, "invalid root in near response json" );
        return false;
    }

    const next_json_value_t & relays = document["Relays"];
    if ( !relays.IsArray() ) 
    {
        next_printf( NEXT_LOG_LEVEL_DEBUG, "could not find relays array in near response json" );
        return false;
    }

    if ( ip2location )
    {
        memset( ip2location, 0, sizeof( *ip2location ) );
        if ( document.HasMember( "IP2Location" ) )
        {
            next_json_value_t & value = document["IP2Location"];
            if ( value.GetStringLength() > 0
                && !next_ip2location_parse( value.GetString(), ip2location ) )
            {
                next_printf( NEXT_LOG_LEVEL_DEBUG, "could not parse ip2location in near response json" );
                return false;
            }
        }
        if ( document.HasMember( "IP" ) )
        {
            next_json_value_t & value = document["IP"];
            if ( value.GetStringLength() > 0 )
            {
                // client IP address
                char address_str[1024];
                if ( next_base64_decode_string( value.GetString(), address_str, sizeof( address_str ) ) == -1 )
                {
                    next_printf( NEXT_LOG_LEVEL_DEBUG, "failed to base64 decode IP address from near response json: %s", value.GetString() );
                    return false;
                }
                
                if ( next_address_parse( &ip2location->ip, address_str ) != NEXT_OK )
                {
                    next_printf( NEXT_LOG_LEVEL_DEBUG, "could not parse IP address from near response json", address_str );
                    return false;
                }
            }
        }
    }

    output->relay_count = 0;

    for ( next_json_value_t::ConstValueIterator itor = relays.Begin(); itor != relays.End(); ++itor ) 
    {
        if ( output->relay_count >= NEXT_MAX_NEAR_RELAYS )
            break;

        const next_json_value_t & id = (*itor)["Id"];
        const next_json_value_t & token = (*itor)["Token"];
        const next_json_value_t & address = (*itor)["Address"];

        if ( id.GetType() != rapidjson::kNumberType )
        {
            next_printf( NEXT_LOG_LEVEL_DEBUG, "relay id should be number type" );
            return false;
        }

        if ( token.GetType() != rapidjson::kStringType )
        {
            next_printf( NEXT_LOG_LEVEL_DEBUG, "relay token should be string type" );
            return false;
        }

        if ( address.GetType() != rapidjson::kStringType )
        {
            next_printf( NEXT_LOG_LEVEL_DEBUG, "relay address should be string type" );
            return false;
        }

        next_near_relay_t * relay = &output->relays[output->relay_count];

        relay->id = id.GetUint64();

        {
            // token
            uint8_t token_data[NEXT_PING_TOKEN_BYTES*2];
            int bytes = next_base64_decode_data( token.GetString(), token_data, sizeof( token_data ) );
            if ( bytes != NEXT_PING_TOKEN_BYTES )
            {
                next_printf( NEXT_LOG_LEVEL_DEBUG, "failed to decode ping token base64" );
                return false;
            }

            if ( !next_ping_token_read( &relay->ping_token, token_data ) )
            {
                next_printf( NEXT_LOG_LEVEL_DEBUG, "failed to read ping token" );
                return false;
            }
        }

        {
            // address
            char address_str[1024];
            if ( next_base64_decode_string( address.GetString(), address_str, sizeof( address_str ) ) == -1 )
            {
                next_printf( NEXT_LOG_LEVEL_DEBUG, "failed to base64 decode relay address: %s", address.GetString() );
                return false;
            }
            if ( next_address_parse( &relay->address, (char *)( address_str ) ) != NEXT_OK )
            {
                next_printf( NEXT_LOG_LEVEL_DEBUG, "failed to parse relay address string: %s", address_str );
                return false;
            }
        }

        output->relay_count++;
    }

    return true;
}

void next_nearest_relays_ip2location_override( next_http_t * context, const char * latitude, const char * longitude, next_http_callback_t * callback, void * user_data )
{
    next_assert( context );
    next_assert( latitude );
    next_assert( longitude );
    next_assert( callback );

    char path[1024];
    snprintf( path, sizeof( path ), "/v2/near/%s/%s", latitude, longitude );

    next_http_nonblock_get( context, path, callback, user_data, NEXT_HTTP_TIMEOUT_NEAR_RELAYS );
}

void next_nearest_relays( next_http_t * context, next_http_callback_t * callback, void * user_data )
{
    next_assert( context );
    next_assert( callback );

    next_http_nonblock_get( context, "/v2/near/auto", callback, user_data, NEXT_HTTP_TIMEOUT_NEAR_RELAYS );
}

static const char * ip2location_token( char * output, size_t output_size, const char * input, char delimiter )
{
    if ( input[0] == '\0' )
        return NULL;

    int index_output = 0;
    int index_input = 0;
    while ( true )
    {
        if ( input[index_input] == delimiter )
        {
            output[index_output] = '\0';
            return &input[index_input + 1];
        }

        if ( input[index_input] == '\0' )
        {
            output[index_output] = '\0';
            return &input[index_input];
        }

        if ( index_output < int( output_size - 1 ) )
        {
            output[index_output] = input[index_input];
            index_output++;
        }

        index_input++;
    }

    return NULL;
}

bool next_ip2location_parse( const char * input, next_ip2location_t * data )
{
    next_assert( input );
    next_assert( data );
    
    const char * token = ip2location_token( data->country_code, sizeof(data->country_code), input, ';' );
    if ( !token )
        return false;

    token = ip2location_token( data->country, sizeof(data->country), token, ';' );
    if ( !token )
        return false;

    token = ip2location_token( data->region, sizeof(data->region), token, ';' );
    if ( !token )
        return false;

    token = ip2location_token( data->city, sizeof(data->city), token, ';' );
    if ( !token )
        return false;

    token = ip2location_token( data->latitude, sizeof(data->latitude), token, ';' );
    if ( !token )
        return false;

    token = ip2location_token( data->longitude, sizeof(data->longitude), token, ';' );
    if ( !token )
        return false;

    token = ip2location_token( data->isp, sizeof(data->isp), token, ';' );
    if ( !token )
        return false;

    return true;
}

// ------------------------------------------------------------

uint64_t next_direct_address_to_flow_id( next_address_t * direct_address )
{
    next_assert( direct_address );
    next_assert( direct_address->type == NEXT_ADDRESS_IPV4 );

    if ( direct_address->type != NEXT_ADDRESS_IPV4 )
        return 0;

    uint64_t flow_id = 0;
    
    flow_id |= direct_address->data.ipv4[0];
    flow_id <<= 8;

    flow_id |= direct_address->data.ipv4[1];
    flow_id <<= 8;

    flow_id |= direct_address->data.ipv4[2];
    flow_id <<= 8;

    flow_id |= direct_address->data.ipv4[3];
    flow_id <<= 8;

    flow_id |= ( direct_address->port >> 8 );
    flow_id <<= 8;

    flow_id |= direct_address->port & 0xFF;

    flow_id |= (1ULL<<63);

    return flow_id;
}

void next_direct_address_from_flow_id( uint64_t flow_id, next_address_t * direct_address )
{
    next_assert( direct_address );

    if ( ( flow_id & (1ULL<<63) ) == 0 )
    {
        direct_address->type = NEXT_ADDRESS_NONE;
        return;
    }

    direct_address->type = NEXT_ADDRESS_IPV4;

    direct_address->port = uint16_t( flow_id & 0xFF );
    direct_address->port |= uint16_t( flow_id & 0xFF00 );
    flow_id >>= 16;

    direct_address->data.ipv4[3] = uint8_t( flow_id & 0xFF );
    flow_id >>= 8;

    direct_address->data.ipv4[2] = uint8_t( flow_id & 0xFF );
    flow_id >>= 8;

    direct_address->data.ipv4[1] = uint8_t( flow_id & 0xFF );
    flow_id >>= 8;

    direct_address->data.ipv4[0] = uint8_t( flow_id & 0xFF );
}

void next_session_to_address( int session_index, uint8_t session_sequence, next_address_t * session_address )
{
    next_assert( session_address );

    next_assert( session_index >= 0 );
    next_assert( session_index <= 65535 );

    session_address->type = NEXT_ADDRESS_IPV4;

    session_address->data.ipv4[0] = 224;
    session_address->data.ipv4[1] = 0;
    session_address->data.ipv4[2] = 0;
    session_address->data.ipv4[3] = session_sequence;
    session_address->port = (uint16_t) session_index;
}

void next_session_from_address( next_address_t * session_address, int * session_index, uint8_t * session_sequence )
{
    next_assert( session_address );
    next_assert( session_index );
    next_assert( session_sequence );

    if ( session_address->type != NEXT_ADDRESS_IPV4 || 
         session_address->data.ipv4[0] != 224 || 
         session_address->data.ipv4[1] != 0 || 
         session_address->data.ipv4[2] != 0 )
    {
        *session_index = -1;
        *session_sequence = 0;
        return;
    }

    *session_index = session_address->port;
    *session_sequence = session_address->data.ipv4[3];
}
