/*
    Network Next: v2.14-667-g7e8ad02d
    Copyright © 2017 - 2018 Network Next, Inc. All rights reserved.
*/

#ifndef NEXT_H
#define NEXT_H

#include <stdint.h>
#include <stddef.h>

#define NEXT_VERSION_FULL              "v2.14-667-g7e8ad02d"
#define NEXT_VERSION_MAJOR            "v2"
#define NEXT_VERSION_MINOR            14
#define NEXT_VERSION_GITHUB          "7e8ad02d"

#define NEXT_OK                                             0
#define NEXT_ERROR                                          1
#define NEXT_ERROR_INVALID_PARAMETER                        2
#define NEXT_ERROR_CLIENT_INSECURE_SESSION_FAILED           3
#define NEXT_ERROR_CLIENT_FAILED_TO_LOCATE                  4
#define NEXT_ERROR_CLIENT_INVALID_ROUTE                     5
#define NEXT_ERROR_CLIENT_BAD_SERVER_ADDRESS                6
#define NEXT_ERROR_CLIENT_ROUTE_TIMED_OUT                   7
#define NEXT_ERROR_CLIENT_TIMED_OUT                         8
#define NEXT_ERROR_CLIENT_NEXT_ONLY                         9

#define NEXT_MTU                                         1100

#define NEXT_PUBLIC_KEY_BYTES                              32
#define NEXT_PRIVATE_KEY_BYTES                             32

#define NEXT_LOG_LEVEL_NONE                                 0
#define NEXT_LOG_LEVEL_ERROR                                1
#define NEXT_LOG_LEVEL_WARN                                 2
#define NEXT_LOG_LEVEL_INFO                                 3
#define NEXT_LOG_LEVEL_DEBUG                                4

#define NEXT_ADDRESS_NONE                                   0
#define NEXT_ADDRESS_IPV4                                   1
#define NEXT_ADDRESS_IPV6                                   2

#define NEXT_MAX_ADDRESS_STRING_LENGTH                    256

#define NEXT_CLIENT_STATE_STOPPED                           0
#define NEXT_CLIENT_STATE_LOCATING                          1
#define NEXT_CLIENT_STATE_READY                             2
#define NEXT_CLIENT_STATE_INSECURE_REQUESTING               3
#define NEXT_CLIENT_STATE_REQUESTING                        4
#define NEXT_CLIENT_STATE_ESTABLISHED                       5
#define NEXT_CLIENT_STATE_DIRECT                            6

#define NEXT_CLIENT_MODE_AUTO                               0
#define NEXT_CLIENT_MODE_FORCE_DIRECT                       1
#define NEXT_CLIENT_MODE_FORCE_NEXT                         2

#define NEXT_SERVER_STATE_STOPPED                           0
#define NEXT_SERVER_STATE_LISTENING                         1

#define NEXT_MAX_STATS_SAMPLES                             60

#if defined(_WIN32)
#define NOMINMAX
#endif

#if defined( NEXT_SHARED )
    #if defined(_WIN32)
        #ifdef NEXT_EXPORT
            #define NEXT_EXPORT_FUNC extern "C" __declspec(dllexport)
        #else
            #define NEXT_EXPORT_FUNC extern "C" __declspec(dllimport)
        #endif
    #else
        #define NEXT_EXPORT_FUNC extern "C"
    #endif
#else
    #define NEXT_EXPORT_FUNC extern
#endif

struct next_client_t;
struct next_server_t;

// -----------------------------------------

NEXT_EXPORT_FUNC int next_init( const char * hostname );        // eg: "https://v2.networknext.com"

NEXT_EXPORT_FUNC void next_term();

// -----------------------------------------

struct next_address_t
{
    union { uint8_t ipv4[4]; uint16_t ipv6[8]; } data;
    uint16_t port;
    uint8_t type;
};

NEXT_EXPORT_FUNC int next_address_parse( next_address_t * address, const char * address_string_in );

NEXT_EXPORT_FUNC char * next_address_to_string( const next_address_t * address, char * buffer );

NEXT_EXPORT_FUNC int next_address_equal( const next_address_t * a, const next_address_t * b );

// -----------------------------------------

struct next_client_config_t
{
    void * context;
    void (*packet_received_callback)( next_client_t * client, void * context, uint8_t * packet_data, int packet_bytes );
    float session_timeout_seconds;
    int stats_mode;
    bool direct_only;
    bool network_next_only;
    bool disable_cant_beat_direct;
};

struct next_client_insecure_session_data_t
{
    const uint8_t * customer_private_key;
    const char * server_public_key_base64;
    const char * server_address;
    const char * max_price_per_gig;
    const char * direct_price_per_gig;
    uint64_t customer_id;
    uint64_t user_id;
    uint64_t dest_relay;
    uint32_t kbps_up;
    uint32_t kbps_down;
    float acceptable_latency;
    float acceptable_jitter;
    float acceptable_packet_loss;
    uint8_t platform_id;
};

NEXT_EXPORT_FUNC next_client_t * next_client_create( next_client_config_t * config );

NEXT_EXPORT_FUNC void next_client_destroy( next_client_t * client );

NEXT_EXPORT_FUNC uint8_t * next_client_info_create( next_client_t * client, size_t * size );

NEXT_EXPORT_FUNC void next_client_info_destroy( uint8_t * client_info );

NEXT_EXPORT_FUNC int next_client_open_session( next_client_t * client, uint8_t * route_data, int route_bytes );

NEXT_EXPORT_FUNC int next_client_open_session_direct( next_client_t * client, const char * server_address );

NEXT_EXPORT_FUNC int next_client_open_session_insecure( next_client_t * client, next_client_insecure_session_data_t * session_data );

NEXT_EXPORT_FUNC void next_client_close_session( next_client_t * client );

NEXT_EXPORT_FUNC void next_client_update( next_client_t * client );

NEXT_EXPORT_FUNC void next_client_mode( next_client_t * client, int mode );

NEXT_EXPORT_FUNC void next_client_send_packet( next_client_t * client, uint8_t * packet_data, int packet_bytes );

NEXT_EXPORT_FUNC int next_client_error( next_client_t * client );

NEXT_EXPORT_FUNC int next_client_state( next_client_t * client );

NEXT_EXPORT_FUNC uint64_t next_client_id( next_client_t * client );

struct next_client_stats_sample_t
{
    double time;
    float next_rtt;
    float next_jitter;
    float next_packet_loss;
    float direct_rtt;
    float direct_jitter;
    float direct_packet_loss;
};

struct next_client_stats_t
{
    float next_rtt;
    float next_jitter;
    float next_packet_loss;
    float direct_rtt;
    float direct_jitter;
    float direct_packet_loss;
    int num_samples;
    next_client_stats_sample_t samples[NEXT_MAX_STATS_SAMPLES];
};

NEXT_EXPORT_FUNC void next_client_stats( next_client_t * client, next_client_stats_t * stats );

// -----------------------------------------

struct next_server_config_t
{
    void * context;
    void (*packet_received_callback)( next_server_t * server, void * context, uint64_t session_id, next_address_t * address, uint8_t * packet_data, int packet_bytes );
    int max_sessions;
    float session_timeout_seconds;
    uint8_t public_key[NEXT_PUBLIC_KEY_BYTES];
    uint8_t private_key[NEXT_PRIVATE_KEY_BYTES];
};

NEXT_EXPORT_FUNC next_server_t * next_server_create( next_server_config_t * config, const char * bind_address );

NEXT_EXPORT_FUNC void next_server_destroy( next_server_t * server );

NEXT_EXPORT_FUNC void next_server_update( next_server_t * server );

NEXT_EXPORT_FUNC void next_server_send_packet( next_server_t * server, uint64_t to_session_id, uint8_t * packet_data, int packet_bytes );

NEXT_EXPORT_FUNC void next_server_send_packet_to_address( next_server_t * server, next_address_t * to_address, uint8_t * packet_data, int packet_bytes );

NEXT_EXPORT_FUNC const uint8_t * next_server_public_key( next_server_t * server );

// -----------------------------------------

NEXT_EXPORT_FUNC double next_time();      // seconds

NEXT_EXPORT_FUNC void next_sleep( double time_seconds );

NEXT_EXPORT_FUNC void next_set_log_level( int level );

NEXT_EXPORT_FUNC void next_set_assert_function( void (*function)( const char *, const char *, const char * file, int line ) );

NEXT_EXPORT_FUNC void next_set_print_function( void (*function)( int level, const char *, ... ) );

NEXT_EXPORT_FUNC void next_set_allocator( void * (*alloc_function)(size_t), void * (*realloc_function)(void*,size_t), void (*free_function)(void*) );

NEXT_EXPORT_FUNC void next_printf( int level, const char * format, ... );

NEXT_EXPORT_FUNC void next_generate_keypair( uint8_t * public_key, uint8_t * private_key );

NEXT_EXPORT_FUNC uint64_t next_relay_id( const char * );

// -----------------------------------------

#endif // #ifndef NEXT_H
