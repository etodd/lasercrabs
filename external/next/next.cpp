/*
    Network Next: v2.14-667-g7e8ad02d
    Copyright © 2017 - 2018 Network Next, Inc. All rights reserved.
*/

#include "next.h"
#include "next_internal.h"
#include <stdlib.h>
#include <memory.h>
#include <stdio.h>
#include <errno.h>
#include <string.h>
#include <math.h>
#include <time.h>

#define NEXT_DEFAULT_CLIENT_SESSION_TIMEOUT_SECONDS 5.0f
#define NEXT_DEFAULT_SERVER_SESSION_TIMEOUT_SECONDS 10.0f
#define NEXT_INTERVAL_PING_RELAYS 0.1
#define NEXT_INTERVAL_PING_SERVER 0.1
#define NEXT_INTERVAL_SAMPLE_STATS 1.0
#define NEXT_CLIENT_STATS_WINDOW 5.0f
#define NEXT_CLIENT_LATENCY_THRESHOLD 1.0f
#define NEXT_CLIENT_MAX_LATENCY_FAILURES 25
#define NEXT_MIGRATE_PACKET_SEND_COUNT 10
#define NEXT_DESTROY_PACKET_SEND_COUNT 10

#define NEXT_CLIENT_COUNTER_NO_NEAR_RELAYS                  0
#define NEXT_CLIENT_COUNTER_OPEN_SESSION                    1
#define NEXT_CLIENT_COUNTER_OPEN_SESSION_DIRECT             2
#define NEXT_CLIENT_COUNTER_CLOSE_SESSION                   3
#define NEXT_CLIENT_COUNTER_FALLBACK_TO_DIRECT              4
#define NEXT_CLIENT_COUNTER_CANT_BEAT_DIRECT                5
#define NEXT_CLIENT_COUNTER_ROUTE_UPDATE_TIMEOUT            6
#define NEXT_CLIENT_COUNTER_SERVER_TO_CLIENT_TIMEOUT        7
#define NEXT_CLIENT_NUM_COUNTERS                            8

#define NEXT_ROUTE_REQUEST_SENDING_NONE 0
#define NEXT_ROUTE_REQUEST_SENDING_INITIAL 1
#define NEXT_ROUTE_REQUEST_SENDING_UPDATE 2
#define NEXT_ROUTE_REQUEST_SENDING_CONTINUE 3

#define NEXT_HTTP_REQUEST_SENDING_NONE 0
#define NEXT_HTTP_REQUEST_SENDING_NEAR 1
#define NEXT_HTTP_REQUEST_SENDING_ROUTE_UPDATE 2

#define NEXT_PING_HISTORY_RELAY_COUNT ( NEXT_MAX_NEAR_RELAYS * 2 )
#define NEXT_PING_HISTORY_ENTRY_COUNT 512

// -------------------------------------------------------------

static void * (*next_alloc_function)( size_t );
static void * (*next_realloc_function)( void*, size_t );
static void (*next_free_function)( void* );

#if NEXT_ENABLE_TESTS
static int canary = NEXT_CANARY_DISABLED;
#endif // #if NEXT_ENABLE_TESTS

void next_set_allocator( void * (*alloc_function)(size_t), void * (*realloc_function)(void*, size_t), void (*free_function)(void*) )
{
    next_assert( alloc_function );
    next_assert( realloc_function );
    next_assert( free_function );
    next_alloc_function = alloc_function;
    next_realloc_function = realloc_function;
    next_free_function = free_function;
}

void * next_realloc( void * p, size_t bytes )
{
    next_assert( next_realloc_function );
    return next_realloc_function( p, bytes );
}

void * next_alloc( size_t bytes )
{
    next_assert( next_alloc_function );
    return next_alloc_function( bytes );
}

void next_free( void * p )
{
    next_assert( next_alloc_function );
    return next_free_function( p );
}

int next_init( const char * hostname )
{
    next_check_initialized( 0 );

    if ( next_alloc_function == NULL )
    {
        next_alloc_function = malloc;
    }

    if ( next_realloc_function == NULL )
    {
        next_realloc_function = realloc;
    }

    if ( next_free_function == NULL )
    {
        next_free_function = free;
    }

    if ( !next_internal_init( hostname ) )
    {
        return NEXT_ERROR;
    }

    if ( sodium_init() == -1 )
    {
        return NEXT_ERROR;
    }

    return NEXT_OK;
}

void next_term()
{
    next_check_initialized( 1 );

    next_internal_term();
}

// ---------------------------------------------------------------

struct next_ping_history_entry_t
{
    uint64_t sequence;
    double time_ping_sent;
    double time_pong_received;
};

struct next_ping_history_t
{
    next_ping_history_entry_t entries[NEXT_PING_HISTORY_ENTRY_COUNT];
    uint64_t sequence_current;
    int index_current;
};

static uint64_t ping_history_insert( next_ping_history_t * history, double time )
{
    next_ping_history_entry_t * entry = &history->entries[history->index_current];
    entry->sequence = history->sequence_current;
    entry->time_ping_sent = time;
    entry->time_pong_received = 0.0;
    history->sequence_current++;
    history->index_current = ( history->index_current + 1 ) % NEXT_PING_HISTORY_ENTRY_COUNT;
    return entry->sequence;
}

static void ping_history_pong_received( next_ping_history_t * history, uint64_t sequence, double time )
{
    for ( int i = 0; i < NEXT_PING_HISTORY_ENTRY_COUNT; i++ )
    {
        next_ping_history_entry_t * entry = &history->entries[i];
        if ( entry->time_ping_sent > 0.0 && entry->sequence == sequence )
        {
            entry->time_pong_received = time;
            break;
        }
    }
}

struct next_ping_history_relay_t
{
    uint64_t relay_id;
    next_address_t address;
    next_ping_history_t history;
};

struct next_client_route_t
{
    next_flow_token_t flow_token;                       // flow token corresponding to client node
    uint64_t sequence;                                  // sequence number for packets sent to next relay
    double time_last_packet_received;                   // timestamp last time a server to client packet was received
    next_replay_protection_t replay_protection;         // protects against replay attacks
    next_ping_history_t ping_history_server;
    int route_relay_count;
};

struct next_relay_stats_t
{
    uint64_t id;
    next_route_stats_t route;
};

struct next_incoming_packet_t
{
    double timestamp;
    next_address_t from;
    int length;
    uint8_t data[NEXT_MAX_PACKET_SIZE];
};

struct next_client_stats_history_t
{
    int index;
    next_client_stats_sample_t samples[NEXT_MAX_STATS_SAMPLES];
};

static next_client_stats_sample_t * stats_history_insert( next_client_stats_history_t * history )
{
    next_client_stats_sample_t * sample = &history->samples[history->index];
    history->index = ( history->index + 1 ) % NEXT_MAX_STATS_SAMPLES;
    return sample;
}

typedef next_vector_t<next_incoming_packet_t> next_packet_queue_t;

struct next_client_t
{
    next_client_config_t config;

    next_http_t http;

    double route_request_last;
    double migrate_packet_last;
    double ping_relays_last;
    double route_update_next;
    double route_update_last;
    double route_changed_last;
    double near_update_last;
    double counter_check_last;
    double counter_post_last;
    int mode;
	int error;
    int route_request_bytes;
    int route_state_bytes;
    int route_request_sending;
    int http_request_sending;
    int ping_history_relay_index;
    int next_rtt_worse_than_direct_count;
    int locating_retries;
    bool migrate_packet_sending;
    bool backup_flow;
    bool force_route;

    bool override_location;
    char override_isp[256];
    float override_latitude;
    float override_longitude;

    next_client_route_t route_current;
    next_client_route_t route_previous;
    next_thread_t thread_listen;
    next_address_t server_address;
    next_nearest_relays_t nearest_relays;

    // fields that are shared between threads
    struct
    {
        next_packet_queue_t packet_queue_a;
        next_packet_queue_t packet_queue_b;
        next_socket_t socket;
        next_mutex_t mutex;
        int state;
        int packet_queue_active;
    }
    shared;

    next_ping_history_relay_t ping_history_relay[NEXT_PING_HISTORY_RELAY_COUNT];

    uint8_t private_key[NEXT_PRIVATE_KEY_BYTES];
    uint8_t public_key[NEXT_PUBLIC_KEY_BYTES];
    uint8_t route_state[NEXT_ENCRYPTED_ROUTE_STATE_MAX_BYTES];
    uint8_t route_request[NEXT_MAX_ROUTE_REQUEST_BYTES + 1];
    uint8_t server_token[NEXT_ENCRYPTED_SERVER_TOKEN_BYTES];

    next_ip2location_t ip2location;

    struct
    {
        double last_ping;
        double last_sample;
        next_client_stats_history_t history;
        next_ping_history_t ping_history_direct;
    }
    server_stats;

    uint64_t counters[NEXT_CLIENT_NUM_COUNTERS];
};

static next_packet_queue_t * client_packet_queue_active( next_client_t * client )
{
    return client->shared.packet_queue_active ? &client->shared.packet_queue_b : &client->shared.packet_queue_a;
}

static void client_route_reset( next_client_route_t * route )
{
    next_assert( route );
    memset( route, 0, sizeof(next_client_route_t) );
    next_replay_protection_reset( &route->replay_protection );
}

static void client_set_state( next_client_t * client, int value )
{
    client->shared.state = value;
}

static void client_set_error( next_client_t * client, int value )
{
    client->error = value;
}

static next_thread_return_t NEXT_THREAD_FUNC client_thread_listen( void * arg );

next_client_t * next_client_create( next_client_config_t * config )
{
    next_check_initialized( 1 );

    next_address_t bind_address;
    memset( &bind_address, 0, sizeof(bind_address) );
    bind_address.type = NEXT_ADDRESS_IPV4;
    bind_address.port = 0;
    next_socket_t socket;
    if ( next_socket_create( &socket, &bind_address, 100, NEXT_SOCKET_SNDBUF_SIZE, NEXT_SOCKET_RCVBUF_SIZE, 0 ) != NEXT_ERROR_SOCKET_NONE )
        return NULL;

    next_client_t * client = (next_client_t*) next_alloc( sizeof(next_client_t) );
    if ( !client )
        return NULL;

    memset( client, 0, sizeof(next_client_t) );

    client->shared.socket = socket;

    memcpy( &client->config, config, sizeof(next_client_config_t) );
    if ( client->config.session_timeout_seconds <= 0.0f )
    {
        client->config.session_timeout_seconds = NEXT_DEFAULT_CLIENT_SESSION_TIMEOUT_SECONDS;
    }

    memset( &client->route_current, 0, sizeof( next_client_route_t ) );
    memset( &client->route_previous, 0, sizeof( next_client_route_t ) );

    memset( &client->nearest_relays, 0, sizeof( next_nearest_relays_t ) );
    memset( &client->ping_history_relay, 0, sizeof( client->ping_history_relay ) );
    client->ping_history_relay_index = 0;
    next_generate_keypair( client->public_key, client->private_key );

    memset( &client->server_stats, 0, sizeof( client->server_stats ) );

    client->near_update_last = -100.0;

    client->counter_post_last = -100.0;

    if ( !next_mutex_init( &client->shared.mutex ) )
    {
        next_socket_destroy( &client->shared.socket );
        next_free( client );
        return NULL;
    }

    next_http_create( &client->http, next_master_address() );

    if ( client->config.direct_only )
    {
        client_set_state( client, NEXT_CLIENT_STATE_READY );
    }
    else
    {
        client_set_state( client, NEXT_CLIENT_STATE_LOCATING );
    }

    if ( !next_thread_create( &client->thread_listen, client_thread_listen, client ) )
    {
        next_socket_destroy( &client->shared.socket );
        next_mutex_destroy( &client->shared.mutex );
        next_free( client );
        return NULL;
    }

    return client;
}

void next_client_destroy( next_client_t * client )
{
    next_check_initialized( 1 );

    next_assert( client );

    next_client_close_session( client );

    client_set_state( client, NEXT_CLIENT_STATE_STOPPED );

    next_socket_destroy( &client->shared.socket );

    next_thread_join( &client->thread_listen );

    next_mutex_destroy( &client->shared.mutex );

    client->shared.packet_queue_a.clear();
    client->shared.packet_queue_b.clear();

    next_http_destroy( &client->http );

    memset( client, 0, sizeof(next_client_t) );

    next_free( client );
}

static void client_route_deep_copy( next_client_route_t * dst, next_client_route_t * src )
{
    client_route_reset( dst );
    memcpy( dst, src, sizeof( next_client_route_t ) );
}

uint64_t next_relay_id( const char * name )
{
    next_fnv_t fnv;
    next_fnv_init( &fnv );
    next_fnv_write( &fnv, (uint8_t *)( name ), strlen( name ) );
    return next_fnv_finalize( &fnv );
}

static bool client_insecure_callback( int response_code, const char * response, void * user_data )
{
    next_client_t * client = (next_client_t *)( user_data );

    bool success = false;
    if ( response_code == 200 )
    {
        next_json_document_t doc;
        doc.Parse( response );
        if ( doc.HasMember( "RouteRelays" ) && doc["RouteRelays"].IsString() )
        {
            next_printf( NEXT_LOG_LEVEL_INFO, "insecure session: initial route: %s", doc["RouteRelays"].GetString() );
        }
        if ( doc.HasMember( "RouteData" ) && doc["RouteData"].IsString() )
        {
            const next_json_value_t & value = doc["RouteData"];
            uint8_t * route_data = (uint8_t *)( next_alloc( value.GetStringLength() ) );
            int route_data_length = next_base64_decode_data( value.GetString(), route_data, value.GetStringLength() );
            next_client_open_session( client, route_data, route_data_length );
            next_free( route_data );
            success = true;
        }
    }
    else if ( response_code == 404 && !client->config.network_next_only )
    {
        // no routes found: go direct instead
        next_printf( NEXT_LOG_LEVEL_INFO, "insecure session: no routes found, going direct" );
        char server_address_string[NEXT_MAX_ADDRESS_STRING_LENGTH];
        next_address_to_string( &client->server_address, server_address_string );
        next_client_open_session_direct( client, server_address_string );
        success = true;
    }
    else
    {
        next_printf( NEXT_LOG_LEVEL_ERROR, "insecure session: initial route request error: HTTP %d %s", response_code, response );
    }

    if ( !success )
    {
        next_printf( NEXT_LOG_LEVEL_ERROR, "client insecure session failed" );
		client_set_error( client, NEXT_ERROR_CLIENT_INSECURE_SESSION_FAILED );
        client_set_state( client, NEXT_CLIENT_STATE_READY );
    }

    return true;
}

int next_client_open_session_insecure( next_client_t * client, next_client_insecure_session_data_t * session_data )
{
    next_assert( client );

    if ( !client )
        return NEXT_ERROR_INVALID_PARAMETER;

    next_printf( NEXT_LOG_LEVEL_DEBUG, "opening insecure session. don't ship with this!" );

    next_client_close_session( client );

    next_json_string_buffer_t wrapper_buffer;
    {
        next_json_string_buffer_t request_buffer;
        char request_base64[2048];
        int request_base64_size;

        // request
        {
            next_json_document_t doc;
            doc.SetObject();

            next_json_allocator_t& allocator = doc.GetAllocator();

            next_json_value_t value;
            value.SetUint( session_data->kbps_up );
            doc.AddMember( "KbpsUp", value, allocator );

            value.SetUint( session_data->kbps_down );
            doc.AddMember( "KbpsDown", value, allocator );

            value.SetUint64( session_data->user_id );
            doc.AddMember( "UserId", value, allocator );

            value.SetUint( uint32_t( session_data->platform_id ) );
            doc.AddMember( "PlatformId", value, allocator );

            value.SetDouble( session_data->acceptable_latency );
            doc.AddMember( "AcceptableLatency", value, allocator );

            value.SetDouble( session_data->acceptable_jitter );
            doc.AddMember( "AcceptableJitter", value, allocator );

            value.SetDouble( session_data->acceptable_packet_loss );
            doc.AddMember( "AcceptablePacketLoss", value, allocator );

            value.SetUint64( session_data->dest_relay );
            doc.AddMember( "DestRelay", value, allocator );

            value.SetString( session_data->server_public_key_base64, next_json_size_t( strlen( session_data->server_public_key_base64 ) ), allocator );
            doc.AddMember( "ServerPublicKey", value, allocator );

            if ( session_data->max_price_per_gig )
            {
                value.SetString( session_data->max_price_per_gig, next_json_size_t( strlen( session_data->max_price_per_gig ) ), allocator );
                doc.AddMember( "MaxPricePerGig", value, allocator );
            }

            if ( session_data->direct_price_per_gig )
            {
                value.SetString( session_data->direct_price_per_gig, next_json_size_t( strlen( session_data->direct_price_per_gig ) ), allocator );
                doc.AddMember( "DirectPricePerGig", value, allocator );
            }

            char server_address_base64[512];
            next_base64_encode_string( session_data->server_address, server_address_base64, sizeof( server_address_base64 ) );
            next_address_t server_address;
            if ( next_address_parse( &server_address, session_data->server_address ) != NEXT_OK )
            {
                next_printf( NEXT_LOG_LEVEL_ERROR, "bad server address: %s", session_data->server_address );
                return NEXT_ERROR_CLIENT_BAD_SERVER_ADDRESS;
            }
            client->server_address = server_address;
            doc.AddMember( "Mode", client->mode, allocator );

            value.SetString( server_address_base64, next_json_size_t( strlen( server_address_base64 ) ), allocator );
            doc.AddMember( "ServerAddress", value, allocator );

            {
                size_t client_info_size;
                uint8_t * client_info = next_client_info_create( client, &client_info_size );

                char client_info_base64[2048];
                int client_info_base64_size = next_base64_encode_data( client_info, client_info_size, client_info_base64, sizeof( client_info_base64 ) );
                next_assert( client_info_base64_size > 0 );

                next_client_info_destroy( client_info );

                value.SetString( client_info_base64, client_info_base64_size, allocator );
                doc.AddMember( "ClientInfo", value, allocator );
            }

            next_json_writer_t writer( request_buffer );
            doc.Accept( writer );

            request_base64_size = next_base64_encode_string( request_buffer.GetString(), request_base64, sizeof( request_base64 ) );
        }

        // wrapper
        {
            next_json_document_t doc;
            doc.SetObject();

            next_json_allocator_t& allocator = doc.GetAllocator();

            next_json_value_t value;
            value.SetUint64( session_data->customer_id );
            doc.AddMember( "CustomerId", value, allocator );

            uint8_t signature[crypto_sign_BYTES];
            crypto_sign_detached( signature, NULL, (unsigned char *)( request_buffer.GetString() ), request_buffer.GetSize(), session_data->customer_private_key );

            char signature_base64[crypto_sign_BYTES * 2];
            int signature_base64_size = next_base64_encode_data( signature, sizeof( signature ), signature_base64, sizeof( signature_base64 ) );
            value.SetString( signature_base64, signature_base64_size, allocator );
            doc.AddMember( "HMAC", value, allocator );

            value.SetString( request_base64, request_base64_size, allocator );
            doc.AddMember( "RouteRequest", value, allocator );

            next_json_writer_t writer( wrapper_buffer );
            doc.Accept( writer );
        }
    }

    client_set_state( client, NEXT_CLIENT_STATE_INSECURE_REQUESTING );

    next_http_nonblock_post_json( &client->http, "/v2/router/route", wrapper_buffer.GetString(), client_insecure_callback, client, 10000 );

	return NEXT_OK;
}

static bool client_backup_flow_if_possible( next_client_t * client )
{
    if ( ( !client->config.network_next_only
        && client->mode != NEXT_CLIENT_MODE_FORCE_NEXT
        && !client->force_route
        && client->server_address.type != NEXT_ADDRESS_NONE )
#if NEXT_ENABLE_TESTS
        || canary == NEXT_CANARY_BACKUP_FLOW
#endif
        )
    {
        if ( !client->backup_flow )
        {
            const uint64_t flow_id = client->route_current.flow_token.flow_id;
            const uint8_t flow_version = client->route_current.flow_token.flow_version;
            next_flow_log( NEXT_LOG_LEVEL_WARN, flow_id, flow_version, "fallback to direct" );
            client->counters[NEXT_CLIENT_COUNTER_FALLBACK_TO_DIRECT]++;
            client->backup_flow = true;
        }
        return true;
    }
    return false;
}

int next_client_open_session( next_client_t * client, uint8_t * route_data, int route_data_bytes )
{
    next_assert( client );
    next_assert( route_data );
    next_assert( route_data_bytes > 0 );

    if ( !client )
        return NEXT_ERROR_INVALID_PARAMETER;

    next_client_close_session( client );

    next_route_prefix_t prefix;

    uint8_t * p = route_data;

    if ( next_read_route_prefix( &prefix, &p, route_data_bytes ) != NEXT_OK )
    {
        next_printf( NEXT_LOG_LEVEL_ERROR, "route data is invalid. bad route prefix." );
        return NEXT_ERROR_CLIENT_INVALID_ROUTE;
    }

    memset( &client->server_address, 0, sizeof( next_address_t ) );
    if ( prefix.prefix_type == NEXT_ROUTE_PREFIX_TYPE_SERVER_ADDRESS )
    {
        memcpy( &client->server_address, prefix.prefix_value, sizeof( next_address_t ) );
    }
    else if ( prefix.prefix_type == NEXT_ROUTE_PREFIX_TYPE_DIRECT )
    {
        next_address_parse( &client->server_address, (const char*)( prefix.prefix_value ) );
    }
    else if ( prefix.prefix_type == NEXT_ROUTE_PREFIX_TYPE_FORCED_ROUTE )
    {
        client->force_route = true;
    }

    bool valid_next_route = true;

    uint32_t route_state_bytes = 0;

    if ( valid_next_route && size_t( route_data_bytes ) < prefix.prefix_length + sizeof(uint32_t) )
    {
        valid_next_route = false;
    }

    if ( valid_next_route )
    {
        route_state_bytes = next_read_uint32( &p );

        if ( route_state_bytes > NEXT_ENCRYPTED_ROUTE_STATE_MAX_BYTES )
        {
            valid_next_route = false;
            next_printf( NEXT_LOG_LEVEL_ERROR, "route data is invalid. expected no more than %d bytes, got %u", NEXT_ENCRYPTED_ROUTE_STATE_MAX_BYTES, route_state_bytes );
        }

        uint32_t min_length  = prefix.prefix_length + sizeof(uint32_t) + route_state_bytes + NEXT_ENCRYPTED_FLOW_TOKEN_BYTES;
        if ( uint32_t( route_data_bytes ) < min_length )
        {
            valid_next_route = false;
            next_printf( NEXT_LOG_LEVEL_ERROR, "route data is invalid. expected at least %u bytes, got %u", min_length, route_data_bytes );
        }
    }

    if ( valid_next_route && route_data_bytes > int( NEXT_ROUTE_PREFIX_BYTES_MAX + route_state_bytes + NEXT_ENCRYPTED_FLOW_TOKEN_BYTES + NEXT_MAX_ROUTE_REQUEST_BYTES ) )
    {
        valid_next_route = false;
        next_printf( NEXT_LOG_LEVEL_ERROR, "route data is invalid. too many bytes." );
    }

    uint8_t route_state[NEXT_ENCRYPTED_ROUTE_STATE_MAX_BYTES];
    next_flow_token_t flow_token;

    if ( valid_next_route )
    {
        next_read_bytes( &p, route_state, route_state_bytes );
        if ( next_read_encrypted_flow_token( &p, &flow_token, NEXT_KEY_MASTER, client->private_key ) != NEXT_OK )
        {
            valid_next_route = false;
            next_printf( NEXT_LOG_LEVEL_ERROR, "route data is invalid. failed to decrypt token." );
        }
    }
    else
    {
        memset( route_state, 0, sizeof(route_state) );
        memset( &flow_token, 0, sizeof(flow_token) );
    }

    client->route_state_bytes = int( route_state_bytes );

    if ( valid_next_route )
    {
        next_client_route_t * route = &client->route_current;
        route->time_last_packet_received = next_time();
        route->sequence = 1;
        next_replay_protection_reset( &route->replay_protection );

        route->flow_token = flow_token;

        memcpy( client->route_state, route_state, sizeof( client->route_state ) );

        next_flow_log( NEXT_LOG_LEVEL_INFO, flow_token.flow_id, flow_token.flow_version, "client flow requested" );

        int route_request_offset = int( p - route_data );
        client->route_request_bytes = 1 + route_data_bytes - route_request_offset;
        next_read_bytes( &p, &client->route_request[1], client->route_request_bytes - 1 );
        client->route_request[0] = NEXT_PACKET_TYPE_V2_ROUTE_REQUEST;

        int flow_token_count = ( client->route_request_bytes - 1 ) / NEXT_ENCRYPTED_FLOW_TOKEN_BYTES;
        route->route_relay_count = flow_token_count - 1;

        client_set_state( client, NEXT_CLIENT_STATE_REQUESTING );

        double time = next_time();

        client->route_request_sending = NEXT_ROUTE_REQUEST_SENDING_INITIAL;
        client->route_changed_last = time;
        client->route_update_next = time + NEXT_BILLING_SLICE_SECONDS;

        client_route_deep_copy( &client->route_previous, route );

        client->counters[NEXT_CLIENT_COUNTER_OPEN_SESSION]++;
    }
    else
    {
        // direct route

        if ( client->config.network_next_only )
        {
            next_printf( NEXT_LOG_LEVEL_ERROR, "received direct route, but network_next_only is set in config" );
            return NEXT_ERROR_CLIENT_INVALID_ROUTE ;
        }

        if ( prefix.prefix_type == NEXT_ROUTE_PREFIX_TYPE_SERVER_ADDRESS )
        {
            char server_address_string[NEXT_MAX_ADDRESS_STRING_LENGTH];
            next_address_to_string( (next_address_t *)prefix.prefix_value, server_address_string );
            next_client_open_session_direct( client, server_address_string );
        }
        else if ( prefix.prefix_type == NEXT_ROUTE_PREFIX_TYPE_DIRECT )
        {
            next_client_open_session_direct( client, (const char *)( prefix.prefix_value ) );
        }
        else
        {
            next_printf( NEXT_LOG_LEVEL_ERROR, "route data is invalid. invalid route prefix (%hhu)", prefix.prefix_type );
            return NEXT_ERROR_CLIENT_BAD_SERVER_ADDRESS;
        }
    }

	return NEXT_OK;
}

int next_client_open_session_direct( next_client_t * client, const char * server_address_string )
{
    next_assert( client );

    if ( !client )
        return NEXT_ERROR_INVALID_PARAMETER;

    if ( client->config.network_next_only )
    {
        next_printf( NEXT_LOG_LEVEL_ERROR, "can't open direct session. network_next_only is set in config" );
        return NEXT_ERROR_CLIENT_NEXT_ONLY;
    }

    if ( client->mode == NEXT_CLIENT_MODE_FORCE_NEXT )
    {
        next_printf( NEXT_LOG_LEVEL_ERROR, "can't open direct session. client is in FORCE_NEXT mode" );
        return NEXT_ERROR_CLIENT_NEXT_ONLY;
    }

    next_address_t server_address;
    if ( next_address_parse( &server_address, server_address_string ) != NEXT_OK )
    {
        next_printf( NEXT_LOG_LEVEL_ERROR, "bad server address: %s", server_address_string );
        return NEXT_ERROR_CLIENT_BAD_SERVER_ADDRESS;
    }

    next_client_close_session( client );

    next_printf( NEXT_LOG_LEVEL_INFO, "opening direct session to %s", server_address_string );

    client->server_address = server_address;
    client->route_current.flow_token.next_address = server_address;
    client->route_current.time_last_packet_received = next_time();
    client_set_state( client, NEXT_CLIENT_STATE_DIRECT );
    client->route_request_sending = NEXT_ROUTE_REQUEST_SENDING_NONE;

    client->counters[NEXT_CLIENT_COUNTER_OPEN_SESSION_DIRECT]++;

	return NEXT_OK;
}

static next_client_route_t * client_get_send_route( next_client_t * client )
{
    if ( client->route_request_sending == NEXT_ROUTE_REQUEST_SENDING_INITIAL || client->route_request_sending == NEXT_ROUTE_REQUEST_SENDING_UPDATE )
    {
        return &client->route_previous;
    }
    else
    {
        return &client->route_current;
    }
}

static void client_send_migrate_packet( next_socket_t * socket, next_flow_token_t * flow_token, uint64_t sequence )
{
    uint8_t packet[NEXT_HEADER_BYTES];
    if ( next_write_header( NEXT_PACKET_TYPE_V2_MIGRATE,
           sequence,
           flow_token->flow_id,
           flow_token->flow_version,
           flow_token->flow_flags,
           flow_token->private_key,
           packet,
           sizeof(packet) ) != NEXT_OK )
    {
        next_flow_log( NEXT_LOG_LEVEL_ERROR, flow_token->flow_id, flow_token->flow_version, "failed to write migrate packet header" );
        return;
    }

    char address_string[NEXT_MAX_ADDRESS_STRING_LENGTH];
    next_address_to_string( &flow_token->next_address, address_string );
    next_socket_send_packet( socket, &flow_token->next_address, packet, sizeof( packet ) );

    next_flow_log( NEXT_LOG_LEVEL_DEBUG, flow_token->flow_id, flow_token->flow_version, "sent migrate packet to %s", address_string );
}

static void client_send_destroy_packet( next_client_t * client, next_flow_token_t * flow_token, uint64_t sequence )
{
    uint8_t packet[NEXT_HEADER_BYTES];
    if ( next_write_header( NEXT_PACKET_TYPE_V2_DESTROY,
           sequence,
           flow_token->flow_id,
           flow_token->flow_version,
           flow_token->flow_flags,
           flow_token->private_key,
           packet,
           sizeof(packet) ) != NEXT_OK )
    {
        next_flow_log( NEXT_LOG_LEVEL_ERROR, flow_token->flow_id, flow_token->flow_version, "failed to write destroy packet header" );
        return;
    }

    char address_string[NEXT_MAX_ADDRESS_STRING_LENGTH];
    next_address_to_string( &flow_token->next_address, address_string );
    next_socket_send_packet( &client->shared.socket, &flow_token->next_address, packet, sizeof( packet ) );

    next_flow_log( NEXT_LOG_LEVEL_DEBUG, flow_token->flow_id, flow_token->flow_version, "sent destroy packet to %s", address_string );
}

void next_client_close_session( next_client_t * client )
{
    next_assert( client );

    if ( !client )
        return;

    if ( client->shared.state == NEXT_CLIENT_STATE_ESTABLISHED )
    {
        client->counters[NEXT_CLIENT_COUNTER_CLOSE_SESSION]++;

        if ( client->migrate_packet_sending )
        {
            for ( int i = 0; i < NEXT_MIGRATE_PACKET_SEND_COUNT; i++ )
            {
                client_send_migrate_packet( &client->shared.socket, &client->route_previous.flow_token, client->route_previous.sequence++ );
            }
            client->migrate_packet_sending = false;
        }

        for ( int i = 0; i < NEXT_DESTROY_PACKET_SEND_COUNT; i++ )
        {
            client_send_destroy_packet( client, &client->route_current.flow_token, client->route_current.sequence++ );
        }
    }

	client_set_error( client, NEXT_OK );

    if ( client->shared.state > NEXT_CLIENT_STATE_READY )
    {
        // client was connected in some fashion; need to bring it back to READY or LOCATING state
        if ( client->backup_flow || client->shared.state == NEXT_CLIENT_STATE_DIRECT )
        {
            // client was in direct mode, so hasn't been updating near relays
            if ( client->config.direct_only )
            {
                // we'll only ever go direct, so just go straight to READY
                client_set_state( client, NEXT_CLIENT_STATE_READY );
            }
            else
            {
                // we may go over network next soon, so we need to update near relays.
                client->nearest_relays.relay_count = 0;
                client_set_state( client, NEXT_CLIENT_STATE_LOCATING );
            }
        }
        else
        {
            // we were already on network next; no need to update near relays
            client_set_state( client, NEXT_CLIENT_STATE_READY );
        }
    }

    client->route_update_last = 0.0;
    client->http_request_sending = NEXT_HTTP_REQUEST_SENDING_NONE;
    client->route_request_sending = NEXT_ROUTE_REQUEST_SENDING_NONE;
    client_route_reset( &client->route_current );
    client_route_reset( &client->route_previous );
    memset( &client->server_address, 0, sizeof( client->server_address ) );
    client->route_request_bytes = 0;
    memset( client->server_token, 0, sizeof( client->server_token ) );
    memset( &client->server_stats, 0, sizeof( client->server_stats ) );
    client->backup_flow = false;
    client->force_route = false;
    client->next_rtt_worse_than_direct_count = 0;
    client->locating_retries = 0;
    next_http_cancel_all( &client->http );

    next_mutex_acquire( &client->shared.mutex );
    client->shared.packet_queue_a.length = 0;
    client->shared.packet_queue_b.length = 0;
    next_mutex_release( &client->shared.mutex );
}

void next_client_force_route_session( next_client_t * client )
{
    client->force_route = true;
    client_set_state(client, NEXT_CLIENT_STATE_READY);
}

static next_ping_history_relay_t * client_ping_history_for_relay_id( next_ping_history_relay_t * relays, uint64_t relay_id )
{
    for ( int i = 0; i < NEXT_PING_HISTORY_RELAY_COUNT; i++ )
    {
        next_ping_history_relay_t * h = &relays[i];
        if ( h->relay_id == relay_id )
            return h;
    }
    return NULL;
}

static void stats_from_ping_history( const next_ping_history_t * history, double start, double end, next_route_stats_t * stats )
{
    double rtt_min = 1000000.0;

    for ( int i = 0; i < NEXT_PING_HISTORY_ENTRY_COUNT; i++ )
    {
        const next_ping_history_entry_t * entry = &history->entries[i];
        if ( entry->time_ping_sent > start && entry->time_pong_received > entry->time_ping_sent )
        {
            // pong received
            double rtt = 1000.0 * ( entry->time_pong_received - entry->time_ping_sent );
            if ( rtt < rtt_min )
            {
                rtt_min = rtt;
            }
        }
    }

    int packet_count_sent = 0;
    int packet_count_received = 0;
    double stddev_rtt = 0.0;

    for ( int i = 0; i < NEXT_PING_HISTORY_ENTRY_COUNT; i++ )
    {
        const next_ping_history_entry_t * entry = &history->entries[i];
        if ( entry->time_ping_sent > start )
        {
            if ( entry->time_pong_received > entry->time_ping_sent )
            {
                // pong received
                double rtt = 1000.0 * ( entry->time_pong_received - entry->time_ping_sent );
                double error = rtt - rtt_min;
                stddev_rtt += error * error;
                packet_count_sent++;
                packet_count_received++;
            }
            else if ( entry->time_ping_sent > 0.0 )
            {
                // ping sent but pong not received
                if ( entry->time_ping_sent < end - 1.0 ) // it's had enough time; count it as dropped
                    packet_count_sent++;
            }
        }
    }

    if ( packet_count_received > 0 )
    {
        stats->jitter = 3.0f * sqrtf( float( stddev_rtt / packet_count_received ) );
        stats->rtt = float( rtt_min );
        stats->packet_loss = 100.0f * ( 1.0f - ( float( packet_count_received ) / float( packet_count_sent ) ) );
    }
    else
    {
        stats->jitter = -1.0f;
        stats->rtt = -1.0f;
        stats->packet_loss = -1.0f;
    }
}

static void relay_stats_from_ping_history( const next_ping_history_relay_t * history, uint64_t relay_id, next_relay_stats_t * stats, double time )
{
    stats->id = relay_id;
    stats_from_ping_history( &history->history, 0.0, time, &stats->route );
}

static bool client_route_over_network_next( next_client_t * client, next_client_route_t * route )
{
    return !( client->shared.state == NEXT_CLIENT_STATE_DIRECT || route->route_relay_count == 0 || client->backup_flow );
}

static void client_stats_direct( next_client_t * client, next_route_stats_t * stats, double time )
{
    stats_from_ping_history( &client->server_stats.ping_history_direct, time - NEXT_CLIENT_STATS_WINDOW, time, stats );
}

static void client_stats_next( next_client_t * client, next_route_stats_t * stats, double time )
{
    next_client_route_t * route = NULL;

    if ( client_route_over_network_next( client, &client->route_current ) )
    {
        if ( time - client->route_changed_last > 2.0 )
        {
            route = &client->route_current;
        }
        else
        {
            // current route is too new; no stats
            if ( client_route_over_network_next( client, &client->route_previous ) )
            {
                route = &client->route_previous;
            }
        }
    }

    if ( route )
    {
        stats_from_ping_history( &route->ping_history_server, time - NEXT_CLIENT_STATS_WINDOW, time, stats );
    }
    else
    {
        stats->rtt = -1.0f;
        stats->jitter = -1.0f;
        stats->packet_loss = -1.0f;
    }
}

enum
{
    NEXT_CLIENT_INFO_ROUTE_REQUEST,
    NEXT_CLIENT_INFO_ROUTE_UPDATE,
};

static uint8_t * client_info_create( next_client_t * client, int mode, size_t * size )
{
    int relay_count = 0;
    next_relay_stats_t relay_stats[NEXT_MAX_NEAR_RELAYS];

    double time = next_time();

    next_route_stats_t stats_next;
    next_route_stats_t stats_direct;

    client_stats_next( client, &stats_next, time );
    client_stats_direct( client, &stats_direct, time );

    for ( int i = 0; i < client->nearest_relays.relay_count; i++ )
    {
        uint64_t relay_id = client->nearest_relays.relays[i].id;

        next_relay_stats_t * stats = &relay_stats[relay_count];

        next_ping_history_relay_t * history = client_ping_history_for_relay_id( client->ping_history_relay, relay_id );

        if ( history )
        {
            relay_stats_from_ping_history( history, relay_id, stats, time );
            if ( stats->route.rtt > 0.0f )
            {
                relay_count++;
            }
        }
    }

    *size = 4 + 8 + 4 + 4 + 4 + 4 + 4 + 4 + 4 + relay_count * ( 8 + 4 + 4 + 4 ) + NEXT_ADDRESS_BYTES;
    if ( mode == NEXT_CLIENT_INFO_ROUTE_REQUEST )
        *size += NEXT_PUBLIC_KEY_BYTES;

    uint8_t * info = (uint8_t*) next_alloc( *size );
    if ( !info )
        return NULL;

    memset( info, 0, sizeof(*size) );

    uint8_t * b = info;

    next_write_uint32( &b, NEXT_CLIENT_INFO_VERSION );
    next_write_float64( &b, time );

    next_write_float32( &b, stats_next.rtt );
    next_write_float32( &b, stats_next.jitter );
    next_write_float32( &b, stats_next.packet_loss );

    next_write_float32( &b, stats_direct.rtt );
    next_write_float32( &b, stats_direct.jitter );
    next_write_float32( &b, stats_direct.packet_loss );

    next_write_uint32( &b, relay_count);

    for ( int i = 0; i < relay_count; i++ )
    {
        const next_relay_stats_t * stats = &relay_stats[i];
        next_write_uint64( &b, stats->id );
        next_write_float32( &b, stats->route.rtt );
        next_write_float32( &b, stats->route.jitter );
        next_write_float32( &b, stats->route.packet_loss );
    }

    next_write_address( &b, &client->ip2location.ip );

    if ( mode == NEXT_CLIENT_INFO_ROUTE_REQUEST )
    {
        next_write_bytes( &b, client->public_key, NEXT_PUBLIC_KEY_BYTES );
    }

    next_assert( b - info == int64_t( *size ) );

    return info;
}

uint8_t * next_client_info_create( next_client_t * client, size_t * size )
{
    next_assert( client );
    return client_info_create( client, NEXT_CLIENT_INFO_ROUTE_REQUEST, size );
}

#if NEXT_ENABLE_TESTS
void next_canary( int mode )
{
    canary = mode;
}
#endif // #if NEXT_ENABLE_TESTS

void next_client_info_destroy( uint8_t * info )
{
    next_free( info );
}

static int client_route_data_read(
    const char * string,
    next_flow_token_t * current_flow_token,
    next_route_prefix_t * prefix,
    uint8_t * route_state,
    uint32_t * route_state_bytes,
    uint8_t * tokens,
    uint32_t * token_bytes
    )
{
    uint8_t data
    [
        NEXT_ROUTE_PREFIX_BYTES_MAX
        + sizeof(uint32_t)
        + NEXT_ENCRYPTED_ROUTE_STATE_MAX_BYTES
        + NEXT_ENCRYPTED_FLOW_TOKEN_BYTES
        + NEXT_MAX_ROUTE_REQUEST_BYTES
    ];

    uint32_t data_length;
    if ( ( data_length = uint32_t( next_base64_decode_data( string, &data[0], sizeof( data ) ) ) ) == uint32_t( -1 ) )
    {
        next_flow_log( NEXT_LOG_LEVEL_ERROR, current_flow_token->flow_id, current_flow_token->flow_version, "failed to base64 decode continue data: %s", string );
        return NEXT_ERROR;
    }

    uint8_t * p = data;
    if ( next_read_route_prefix( prefix, &p, data_length ) != NEXT_OK )
    {
        next_flow_log( NEXT_LOG_LEVEL_ERROR, current_flow_token->flow_id, current_flow_token->flow_version, "invalid route data prefix: type %d, size %d", prefix->prefix_type, prefix->prefix_length );
        return NEXT_ERROR;
    }

    uint32_t min_length = uint32_t( p - data ) + sizeof( uint32_t );
    if ( data_length < min_length )
    {
        next_flow_log( NEXT_LOG_LEVEL_ERROR, current_flow_token->flow_id, current_flow_token->flow_version, "invalid route data; expected at least %d bytes, got %d", min_length, data_length );
        return NEXT_ERROR;
    }

    uint32_t route_state_length = next_read_uint32( &p );
    if ( route_state_length > *route_state_bytes )
    {
        next_flow_log( NEXT_LOG_LEVEL_ERROR, current_flow_token->flow_id, current_flow_token->flow_version, "invalid route data. route state too large: expected %d bytes, got %d", *route_state_bytes, route_state_length );
        return NEXT_ERROR;
    }

    *route_state_bytes = route_state_length;

    min_length = uint32_t( p - data ) + *route_state_bytes;
    if ( data_length < min_length )
    {
        next_flow_log( NEXT_LOG_LEVEL_ERROR, current_flow_token->flow_id, current_flow_token->flow_version, "invalid route data. not enough bytes. expected %d, got %d", min_length, data_length );
        return NEXT_ERROR;
    }

    next_read_bytes( &p, route_state, int( *route_state_bytes ) );

    uint32_t remaining_bytes = data_length - uint32_t( p - data );

    if ( remaining_bytes > *token_bytes )
    {
        next_flow_log( NEXT_LOG_LEVEL_ERROR, current_flow_token->flow_id, current_flow_token->flow_version, "invalid route data. expected no more than %d bytes, got %d", *token_bytes, remaining_bytes );
        return NEXT_ERROR;
    }

    *token_bytes = remaining_bytes;

    if ( *token_bytes > 0 )
    {
        memcpy( tokens, p, *token_bytes );
    }

    return NEXT_OK;
}

static void client_route_update( next_client_t * client, const char * response )
{
    next_flow_token_t * current_flow_token = &client->route_current.flow_token;

    const uint64_t flow_id = current_flow_token->flow_id;
    const uint8_t flow_version = current_flow_token->flow_version;

    next_json_document_t doc;
    doc.Parse( response );

    if ( !client->force_route )
    {
        if ( next_nearest_relays_parse_document( doc, &client->nearest_relays, NULL ) == NEXT_OK )
        {
            next_flow_log( NEXT_LOG_LEVEL_INFO, flow_id, flow_version, "received %d near relays", client->nearest_relays.relay_count );
        }
        else
        {
            next_flow_log( NEXT_LOG_LEVEL_ERROR, flow_id, flow_version, "failed to parse near relays from route response" );
        }
    }

    const char * route_relays = 0;
    if ( doc.HasMember( "RouteRelays" ) && doc["RouteRelays"].IsString() )
    {
        route_relays = doc["RouteRelays"].GetString();
    }

    next_route_stats_t predicted;
    memset( &predicted, 0, sizeof( predicted ) );

    if ( doc.HasMember("Predicted") )
    {
        const next_json_value_t & value = doc["Predicted"];
        if ( value.HasMember("RTT") && value["RTT"].IsFloat() )
        {
            predicted.rtt = value["RTT"].GetFloat();
        }
        if ( value.HasMember("Jitter") && value["Jitter"].IsFloat() )
        {
            predicted.jitter = value["Jitter"].GetFloat();
        }
        if ( value.HasMember("PacketLoss") && value["PacketLoss"].IsFloat() )
        {
            predicted.packet_loss = value["PacketLoss"].GetFloat();
        }
    }

    if ( doc.HasMember("ContinueData") && doc["ContinueData"].IsString() && doc["ContinueData"].GetStringLength() > 0 )
    {
        // continue route

        const next_json_value_t & value = doc["ContinueData"];

        next_route_prefix_t prefix;
        uint8_t route_state[NEXT_ENCRYPTED_ROUTE_STATE_MAX_BYTES];
        uint32_t route_state_bytes = sizeof(route_state);
        uint8_t tokens[NEXT_ENCRYPTED_CONTINUE_TOKEN_BYTES * NEXT_MAX_FLOW_TOKENS];
        uint32_t token_bytes = sizeof(tokens);

        if ( client_route_data_read( value.GetString(), current_flow_token, &prefix, route_state, &route_state_bytes, tokens, &token_bytes ) != NEXT_OK )
        {
            next_flow_log( NEXT_LOG_LEVEL_ERROR, flow_id, flow_version, "could not read route data" );
            return;
        }

        if ( token_bytes < NEXT_ENCRYPTED_CONTINUE_TOKEN_BYTES )
        {
            next_flow_log( NEXT_LOG_LEVEL_ERROR, flow_id, flow_version, "not enough bytes to read encrypted continue token" );
            return;
        }

        uint8_t * p = tokens;

        next_continue_token_t continue_token;
        if ( next_read_encrypted_continue_token( &p, &continue_token, NEXT_KEY_MASTER, client->private_key ) != NEXT_OK )
        {
            next_flow_log( NEXT_LOG_LEVEL_ERROR, flow_id, flow_version, "failed to decrypt continue token" );
            return;
        }

        if ( client->shared.state == NEXT_CLIENT_STATE_ESTABLISHED )
        {
            if ( continue_token.flow_id == flow_id && continue_token.flow_version == flow_version )
            {
                if ( route_relays )
                {
                    next_flow_log( NEXT_LOG_LEVEL_INFO, flow_id, flow_version, "holding route: %s", route_relays );
                }
                else
                {
                    next_flow_log( NEXT_LOG_LEVEL_INFO, flow_id, flow_version, "holding route" );
                }

                client->route_state_bytes = route_state_bytes;
                memcpy( client->route_state, route_state, route_state_bytes );

                client->route_request_sending = NEXT_ROUTE_REQUEST_SENDING_CONTINUE;

                client->route_request_bytes = 1 + int( token_bytes ) - NEXT_ENCRYPTED_CONTINUE_TOKEN_BYTES;
                next_read_bytes( &p, &client->route_request[1], client->route_request_bytes - 1 );
                client->route_request[0] = NEXT_PACKET_TYPE_V2_CONTINUE_REQUEST;
                client->route_update_next += NEXT_BILLING_SLICE_SECONDS;
            }
            else
            {
                next_flow_log( NEXT_LOG_LEVEL_ERROR, flow_id, flow_version, "received invalid continue token" );
            }
        }
    }
    else if ( doc.HasMember("RouteData") && doc["RouteData"].IsString() )
    {
        // update route

        const next_json_value_t & value = doc["RouteData"];

        next_route_prefix_t prefix;
        uint8_t route_state[NEXT_ENCRYPTED_ROUTE_STATE_MAX_BYTES];
        uint32_t route_state_bytes = sizeof(route_state);
        uint8_t tokens[NEXT_ENCRYPTED_FLOW_TOKEN_BYTES * NEXT_MAX_FLOW_TOKENS];
        uint32_t token_bytes = sizeof(tokens);

        if ( client_route_data_read( value.GetString(), current_flow_token, &prefix, route_state, &route_state_bytes, tokens, &token_bytes ) != NEXT_OK )
        {
            next_flow_log( NEXT_LOG_LEVEL_ERROR, current_flow_token->flow_id, current_flow_token->flow_version, "could not read route data" );
            return;
        }

        if ( token_bytes < NEXT_ENCRYPTED_FLOW_TOKEN_BYTES )
        {
            next_flow_log( NEXT_LOG_LEVEL_ERROR, current_flow_token->flow_id, current_flow_token->flow_version, "not enough bytes to read encrypted flow token" );
            return;
        }

        uint8_t * p = tokens;
        next_flow_token_t flow_token;
        if ( next_read_encrypted_flow_token( &p, &flow_token, NEXT_KEY_MASTER, client->private_key ) != NEXT_OK )
        {
            next_flow_log( NEXT_LOG_LEVEL_ERROR, current_flow_token->flow_id, current_flow_token->flow_version, "failed to decrypt flow token from route data." );
            return;
        }

        if ( client->shared.state >= NEXT_CLIENT_STATE_REQUESTING )
        {
            if ( route_relays )
            {
                next_flow_log( NEXT_LOG_LEVEL_INFO, flow_token.flow_id, flow_token.flow_version, "new route: %s", route_relays );
            }
            else
            {
                next_flow_log( NEXT_LOG_LEVEL_INFO, flow_token.flow_id, flow_token.flow_version, "new route" );
            }

            client->route_state_bytes = route_state_bytes;
            memcpy( client->route_state, route_state, route_state_bytes );

            client->route_request_sending = NEXT_ROUTE_REQUEST_SENDING_UPDATE;

            client->migrate_packet_sending = true;

            client_route_deep_copy( &client->route_previous, &client->route_current );
            next_replay_protection_reset( &client->route_current.replay_protection );
            client->route_current.sequence = 1;
            client->route_current.flow_token = flow_token;
            memset( &client->route_current.ping_history_server, 0, sizeof( client->route_current.ping_history_server ) );

            client->route_request_bytes = 1 + int( token_bytes ) - NEXT_ENCRYPTED_FLOW_TOKEN_BYTES;
            next_read_bytes( &p, &client->route_request[1], client->route_request_bytes - 1 );
            client->route_request[0] = NEXT_PACKET_TYPE_V2_ROUTE_REQUEST;
            client->route_changed_last = next_time();
            client->route_update_next += NEXT_BILLING_SLICE_SECONDS;

            client->route_current.route_relay_count = int( token_bytes / NEXT_ENCRYPTED_FLOW_TOKEN_BYTES ) - 2;
        }
    }
}

static bool client_route_update_callback( int response_code, const char * response, void * user_data )
{
    next_client_t * client = (next_client_t*) user_data;

    client->http_request_sending = NEXT_HTTP_REQUEST_SENDING_NONE;

    const uint64_t flow_id = client->route_current.flow_token.flow_id;
    const uint8_t flow_version = client->route_current.flow_token.flow_version;

    if ( response_code == 200 )
    {
        client_route_update( client, response );
    }
    else
    {
        next_flow_log( NEXT_LOG_LEVEL_WARN, flow_id, flow_version, "route update failed: %d", response_code );
    }

    return true;
}

static void client_update_stats( next_client_t * client, double time )
{
    if ( time - client->server_stats.last_ping >= NEXT_INTERVAL_PING_SERVER )
    {
        client->server_stats.last_ping = time;

        // ping server directly
        if ( client->server_address.type != NEXT_ADDRESS_NONE )
        {
            uint64_t sequence = ping_history_insert( &client->server_stats.ping_history_direct, time );

            uint8_t packet[NEXT_PACKET_V2_PING_PONG_BYTES] = { 0 };
            uint8_t * p = packet;
            next_write_uint8( &p, NEXT_PACKET_TYPE_V2_DIRECT_SERVER_PING );
            next_write_uint64( &p, client->route_current.flow_token.flow_id );
            next_write_uint64( &p, sequence );

            next_socket_send_packet( &client->shared.socket, &client->server_address, packet, sizeof( packet ) );
        }

        // ping server through network next
        if ( client->shared.state == NEXT_CLIENT_STATE_ESTABLISHED && !client->backup_flow )
        {
            next_client_route_t * route = client_get_send_route( client );
            next_flow_token_t * flow_token = &route->flow_token;

            uint8_t packet[NEXT_HEADER_BYTES + NEXT_PACKET_V2_PING_PONG_BYTES] = { 0 };
            if ( next_write_header( NEXT_PACKET_TYPE_V2_NEXT_SERVER_PING,
                   route->sequence,
                   flow_token->flow_id,
                   flow_token->flow_version,
                   flow_token->flow_flags,
                   flow_token->private_key,
                   packet,
                   sizeof(packet) ) != NEXT_OK )
            {
                next_flow_log( NEXT_LOG_LEVEL_ERROR, flow_token->flow_id, flow_token->flow_version, "failed to write next server ping packet header" );
            }
            else
            {
                route->sequence++;

                uint64_t ping_sequence = ping_history_insert( &route->ping_history_server, time );

                uint8_t * p = packet + NEXT_HEADER_BYTES;
                next_write_uint64( &p, flow_token->flow_id );
                next_write_uint64( &p, ping_sequence );
                next_socket_send_packet( &client->shared.socket, &flow_token->next_address, packet, sizeof( packet ) );
            }
        }
    }

    // sample stats
    if ( time - client->server_stats.last_sample >= NEXT_INTERVAL_SAMPLE_STATS )
    {
        client->server_stats.last_sample = time;

        next_client_stats_sample_t * sample = stats_history_insert( &client->server_stats.history );
        sample->time = time;

        next_route_stats_t direct;
        next_route_stats_t next;

        client_stats_direct( client, &direct, time );
        client_stats_next( client, &next, time );

        sample->direct_rtt = direct.rtt;
        sample->direct_jitter = direct.jitter;
        sample->direct_packet_loss = direct.packet_loss;

        sample->next_rtt = next.rtt;
        sample->next_jitter = next.jitter;
        sample->next_packet_loss = next.packet_loss;

        client->next_rtt_worse_than_direct_count = ( ( ( sample->direct_rtt - NEXT_CLIENT_LATENCY_THRESHOLD ) <= sample->next_rtt ) && !client->backup_flow ) ?
        client->next_rtt_worse_than_direct_count + 1 : 0;
    }

    if ( !client->config.network_next_only
        && !client->config.disable_cant_beat_direct
        && client->next_rtt_worse_than_direct_count >= NEXT_CLIENT_MAX_LATENCY_FAILURES
        && !client->backup_flow
        && !client->force_route
        && client->shared.state == NEXT_CLIENT_STATE_ESTABLISHED
        && client->server_address.type != NEXT_ADDRESS_NONE
        && !client->config.network_next_only
        && client->mode != NEXT_CLIENT_MODE_FORCE_NEXT )
    {
        next_flow_log( NEXT_LOG_LEVEL_INFO, client->route_current.flow_token.flow_id, client->route_current.flow_token.flow_version, "can't beat direct" );
        client->counters[NEXT_CLIENT_COUNTER_CANT_BEAT_DIRECT]++;
        client->backup_flow = true;
    }
}

static void client_done_locating( next_client_t * client )
{
    if ( client->nearest_relays.relay_count == 0 )
    {
        client->counters[NEXT_CLIENT_COUNTER_NO_NEAR_RELAYS]++;

        // no near relays
        if ( client->config.network_next_only )
        {
            next_printf( NEXT_LOG_LEVEL_ERROR, "no near relays" );
            client_set_error( client, NEXT_ERROR_CLIENT_FAILED_TO_LOCATE );
        }
        else
        {
            // near failed and we can go direct; let's do that
            next_printf( NEXT_LOG_LEVEL_WARN, "no near relays, falling back to direct" );
            if ( client->shared.state > NEXT_CLIENT_STATE_STOPPED )
            {
                client_set_state( client, NEXT_CLIENT_STATE_READY );
            }
        }
    }
}

static bool nearest_relays_callback( int response_code, const char * response, void * user_data )
{
    next_client_t * client = (next_client_t *)( user_data );

    client->http_request_sending = NEXT_HTTP_REQUEST_SENDING_NONE;

#if NEXT_ENABLE_TESTS
    if ( canary == NEXT_CANARY_NEAR_FAIL )
    {
        next_printf( NEXT_LOG_LEVEL_WARN, "canary forced near relays to fail" );
        return true;
    }
#endif // #if NEXT_ENABLE_TESTS

    next_ip2location_t ip2location;
    next_nearest_relays_t nearest_relays;

    if ( response_code != 200 )
    {
        next_printf( NEXT_LOG_LEVEL_WARN, "failed to get near relays: status code %d", response_code );
        return true;
    }

    if ( next_nearest_relays_parse( response, &nearest_relays, &ip2location ) != NEXT_OK )
    {
        next_printf( NEXT_LOG_LEVEL_WARN, "failed to parse near relays response" );
        return true;
    }

    next_printf( NEXT_LOG_LEVEL_INFO, "received %d near relays", nearest_relays.relay_count );

    client->ip2location = ip2location;
    client->nearest_relays = nearest_relays;

    if ( client->shared.state == NEXT_CLIENT_STATE_LOCATING )
    {
        char address_string[NEXT_MAX_ADDRESS_STRING_LENGTH];
        next_address_to_string( &client->ip2location.ip, address_string );
        if ( client->override_location )
        {
            strcpy( ip2location.country_code, "OVR" );
            strcpy( ip2location.country, "Override" );
            strcpy( ip2location.region, "Override" );
            strcpy( ip2location.city, "Override" );
            strcpy( ip2location.isp, client->override_isp );
            sprintf( ip2location.latitude, "%.4f", client->override_latitude );
            sprintf( ip2location.longitude, "%.4f", client->override_longitude );

            next_printf( NEXT_LOG_LEVEL_INFO, "ip2location override: %s, %s, %s, %s",
                address_string,
                ip2location.latitude,
                ip2location.longitude,
                ip2location.isp );
        }
        else
        {
            next_printf( NEXT_LOG_LEVEL_INFO, "ip2location: %s, %s, %s, %s, %s, %s, %s, %s",
                address_string,
                ip2location.country_code,
                ip2location.country,
                ip2location.region,
                ip2location.city,
                ip2location.latitude,
                ip2location.longitude,
                ip2location.isp );
        }
        
        client_done_locating( client );
    }

    return true;
}

static void client_get_near_relays( next_client_t * client )
{
    // get nearest relays
    client->http_request_sending = NEXT_HTTP_REQUEST_SENDING_NEAR;
    if ( client->override_location )
    {
        next_printf( NEXT_LOG_LEVEL_INFO, "requesting near relays (ip2location override)" );
        char latitude[32];
        char longitude[32];
        snprintf( latitude, sizeof(latitude), "%0.4f", client->override_latitude );
        snprintf( longitude, sizeof(longitude), "%0.4f", client->override_longitude );
        next_nearest_relays_ip2location_override( &client->http, latitude, longitude, nearest_relays_callback, client );
    }
    else
    {
        next_printf( NEXT_LOG_LEVEL_INFO, "requesting near relays" );
        next_nearest_relays( &client->http, nearest_relays_callback, client );
    }
}

static void client_update_location( next_client_t * client, double time )
{
    if ( client->backup_flow || client->config.direct_only || client->force_route )
        return;

    if ( client->shared.state == NEXT_CLIENT_STATE_LOCATING )
    {
        if ( client->nearest_relays.relay_count == 0 )
        {
            // still getting near relays
            if ( client->http_request_sending == NEXT_HTTP_REQUEST_SENDING_NONE
                && time - client->near_update_last > 1.0 )
            {
                const int num_retries = 4;

                if ( client->locating_retries < num_retries )
                {
                    client->near_update_last = time;
                    client->locating_retries++;
                    client_get_near_relays( client );
                }
                else
                {
                    // too many retries; fail
                    client_done_locating( client );
                }
            }
        }
        else
        {
            // we have near relays, but stay in STATE_LOCATE for a couple seconds
            // and ping the relays before going to STATE_READY
            if ( time - client->near_update_last > 2.0 )
            {
                client_set_state( client, NEXT_CLIENT_STATE_READY );
            }
        }
    }
    else if ( client->shared.state == NEXT_CLIENT_STATE_READY )
    {
        // we're waiting to connect, so occasionally update near relays
        // we only need to do this in the READY state, not ESTABLISHED,
        // because ESTABLISHED does route updates, which include near relays
        if ( time - client->near_update_last > 10.0 )
        {
            client->near_update_last = time;
            client_get_near_relays( client );
        }
    }
}

static void client_ping_near_relays( next_client_t * client, double time )
{
    if ( client->backup_flow || client->config.direct_only )
        return;

    if ( client->shared.state == NEXT_CLIENT_STATE_DIRECT || client->shared.state < NEXT_CLIENT_STATE_LOCATING )
        return;

    if ( time - client->ping_relays_last >= NEXT_INTERVAL_PING_RELAYS )
    {
        client->ping_relays_last = time;
        for ( int i = 0; i < client->nearest_relays.relay_count; i++ )
        {
            next_near_relay_t * relay = &client->nearest_relays.relays[i];

            next_ping_history_relay_t * history = client_ping_history_for_relay_id( client->ping_history_relay, relay->id );

            if ( !history )
            {
                // didn't find history. create one
                history = &client->ping_history_relay[client->ping_history_relay_index];
                memset( history, 0, sizeof( *history ) );
                history->relay_id = relay->id;
                history->address = relay->address;

                client->ping_history_relay_index = ( client->ping_history_relay_index + 1 ) % NEXT_PING_HISTORY_RELAY_COUNT;
            }

            uint64_t sequence = ping_history_insert( &history->history, time );

            uint8_t packet[NEXT_PACKET_V2_PING_PONG_BYTES];
            uint8_t * p = packet;
            next_write_uint8( &p, NEXT_PACKET_TYPE_V2_CLIENT_RELAY_PING );
            next_write_uint64( &p, client->route_current.flow_token.flow_id );
            next_write_uint64( &p, sequence );

            next_socket_send_packet( &client->shared.socket, &relay->address, packet, sizeof( packet ) );
        }
    }
}

static void client_update_session( next_client_t * client, double time )
{
    if ( client->backup_flow || client->config.direct_only )
        return;
    
    if ( client->shared.state == NEXT_CLIENT_STATE_DIRECT || client->shared.state < NEXT_CLIENT_STATE_LOCATING )
        return;

    if ( time >= client->route_update_next
        && time > client->route_update_last + 1.0
        && client->http_request_sending == NEXT_HTTP_REQUEST_SENDING_NONE )
    {
        client->route_update_last = time;
#if NEXT_ENABLE_TESTS
        if ( canary == NEXT_CANARY_BACKUP_FLOW && !client_route_over_network_next( client, &client->route_current ) )
        {
            next_printf( NEXT_LOG_LEVEL_DEBUG, "want to request route update, but canary says no" );
        }
        else
#endif // NEXT_ENABLE_TESTS
        if ( client->shared.state != NEXT_CLIENT_STATE_ESTABLISHED )
        {
            next_printf( NEXT_LOG_LEVEL_DEBUG, "want to request route update, but can't because client state is not established (%d)", client->shared.state );
        }
        else
        {
            if ( client->route_request_sending != NEXT_ROUTE_REQUEST_SENDING_NONE )
            {
                next_flow_log( NEXT_LOG_LEVEL_WARN, client->route_current.flow_token.flow_id, client->route_current.flow_token.flow_version, "performing a route update, but route request sending is still active (%d)", client->route_request_sending );
            }

            bool build_update_succeeded = true;

            next_json_string_buffer_t request_buffer;

            next_json_document_t doc;
            doc.SetObject();

            next_json_allocator_t & allocator = doc.GetAllocator();

            char string[2048];
            int string_length = 0;
            if ( ( string_length = next_base64_encode_data( client->route_state, client->route_state_bytes, string, sizeof( string ) ) ) == -1 )
            {
                next_printf( NEXT_LOG_LEVEL_ERROR, "failed to base64 encode route data" );
                build_update_succeeded = false;
            }

            next_json_value_t value;
            value.SetString( string, string_length, allocator );
            doc.AddMember( "RouteState", value, allocator );

            if ( ( string_length = next_base64_encode_data( client->server_token, sizeof( client->server_token ), string, sizeof( string ) ) ) == -1 )
            {
                next_printf( NEXT_LOG_LEVEL_ERROR, "failed to base64 encode server token" );
                build_update_succeeded = false;
            }

            value.SetString( string, string_length, allocator );
            doc.AddMember( "ServerToken", value, allocator );

            size_t client_info_size;
            uint8_t * client_info = client_info_create( client, NEXT_CLIENT_INFO_ROUTE_UPDATE, &client_info_size);

            if ( ( string_length = next_base64_encode_data( client_info, int( client_info_size ), string, sizeof( string ) ) ) == -1 )
            {
                next_printf( NEXT_LOG_LEVEL_ERROR, "failed to base64 encode client info" );
                build_update_succeeded = false;
            }

            next_client_info_destroy( client_info );

            value.SetString( string, string_length, allocator );
            doc.AddMember( "ClientInfo", value, allocator );

            if ( client->override_location )
            {
                value.SetString( client->override_isp, next_json_size_t( strlen( client->override_isp ) ), allocator );
                doc.AddMember( "OverrideISP", value, allocator );

                value.SetDouble( client->override_latitude );
                doc.AddMember( "OverrideLatitude", value, allocator );

                value.SetDouble( client->override_longitude );
                doc.AddMember( "OverrideLongitude", value, allocator );
            }
            
            doc.AddMember( "Mode", client->mode, allocator );

            next_json_writer_t writer( request_buffer );
            doc.Accept( writer );

            if ( build_update_succeeded )
            {
                client->http_request_sending = NEXT_HTTP_REQUEST_SENDING_ROUTE_UPDATE;
                next_flow_token_t * token = &client->route_current.flow_token;
                next_flow_log( NEXT_LOG_LEVEL_INFO, token->flow_id, token->flow_version, "requesting route update" );
                next_http_nonblock_post_json( &client->http, "/v2/router/update", request_buffer.GetString(), client_route_update_callback, client, NEXT_HTTP_TIMEOUT_UPDATE_SESSION );
            }
        }
    }

    // send migrate packet to old relay

    if ( client->migrate_packet_sending && time - client->migrate_packet_last >= 0.1 )
    {
        client->migrate_packet_last = time;
        client_send_migrate_packet( &client->shared.socket, &client->route_previous.flow_token, client->route_previous.sequence );
        client->route_previous.sequence++;
    }

    // send route request to next relay

    if ( client->route_request_sending && time - client->route_request_last >= 0.1 )
    {
        client->route_request_last = time;

        next_flow_token_t * current_flow_token = &client->route_current.flow_token;
        next_socket_send_packet( &client->shared.socket, &current_flow_token->next_address, client->route_request, client->route_request_bytes );

        char address_string[NEXT_MAX_ADDRESS_STRING_LENGTH];
        next_address_to_string( &current_flow_token->next_address, address_string );
        next_flow_log( NEXT_LOG_LEVEL_DEBUG, current_flow_token->flow_id, current_flow_token->flow_version, "sent route request to %s", address_string );
    }
}

static bool client_route_read_packet_header( next_client_t * client, next_client_route_t * route, double time, uint8_t * packet_data, int packet_bytes )
{
    if ( route->time_last_packet_received + client->config.session_timeout_seconds < time )
    {
        next_printf( NEXT_LOG_LEVEL_DEBUG, "ignored packet because session timed out" );
        return false;
    }

    uint64_t sequence;
    uint8_t type;
    uint64_t flow_id;
    uint8_t flow_version;
    uint8_t flow_flags;
    if ( next_read_header( &type,
                           &sequence,
                           &flow_id,
                           &flow_version,
                           &flow_flags,
                           route->flow_token.private_key,
                           packet_data,
                           packet_bytes ) != NEXT_OK )
    {
        next_printf( NEXT_LOG_LEVEL_DEBUG, "client ignored packet. failed to read header" );
        return false;
    }

    if ( flow_id != route->flow_token.flow_id )
    {
        next_flow_log( NEXT_LOG_LEVEL_DEBUG, route->flow_token.flow_id, route->flow_token.flow_version, "client ignored packet from server. wrong flow id %" PRIx64, flow_id );
        return false;
    }

    if ( flow_version != route->flow_token.flow_version )
    {
        next_printf( NEXT_LOG_LEVEL_DEBUG, "client ignored packet. bad flow version" );
        return false;
    }

    if ( next_replay_protection_packet_already_received( &route->replay_protection, sequence ) )
    {
        next_printf( NEXT_LOG_LEVEL_DEBUG, "client ignored packet. packet already received" );
        return false;
    }

    return true;
}

static next_client_route_t * client_read_packet_header( next_client_t * client, double time, uint8_t * packet_data, int packet_bytes )
{
    if ( client_route_read_packet_header( client, &client->route_current, time, packet_data, packet_bytes ) )
    {
        return &client->route_current;
    }
    else if ( client_route_read_packet_header( client, &client->route_previous, time, packet_data, packet_bytes ) )
    {
        return &client->route_previous;
    }
    else
    {
        return NULL;
    }
}

static void client_process_direct_packet( next_client_t * client, next_incoming_packet_t * packet )
{
    if ( !next_address_equal( &packet->from, &client->server_address ) )
    {
        next_printf( NEXT_LOG_LEVEL_DEBUG, "ignored direct packet. not from server address" );
        return;
    }

    client->config.packet_received_callback( client, client->config.context, packet->data + 1, packet->length - 1 );

    client->route_current.time_last_packet_received = packet->timestamp;
}

static void client_process_backup_packet( next_client_t * client, next_incoming_packet_t * packet )
{
    if ( !next_address_equal( &packet->from, &client->server_address ) )
    {
        next_printf( NEXT_LOG_LEVEL_DEBUG, "ignored backup packet. not from server address" );
        return;
    }

    if ( packet->length <= NEXT_BACKUP_FLOW_BYTES )
    {
        next_printf( NEXT_LOG_LEVEL_DEBUG, "ignored backup packet. not enough bytes" );
        return;
    }

    next_client_route_t * route = &client->route_current;

    if ( route->time_last_packet_received + client->config.session_timeout_seconds < packet->timestamp )
    {
        next_printf( NEXT_LOG_LEVEL_DEBUG, "ignored backup packet. session timed out" );
        return;
    }

    uint8_t * p = &packet->data[1];
    uint64_t flow_id = next_read_uint64( &p );
    if ( flow_id != route->flow_token.flow_id )
    {
        next_printf( NEXT_LOG_LEVEL_DEBUG, "ignored backup packet. got flow id %" PRIx64 " expected flow id %" PRIx64" ", flow_id, route->flow_token.flow_id );
        return;
    }

    client->config.packet_received_callback( client, client->config.context, packet->data + NEXT_BACKUP_FLOW_BYTES, packet->length - NEXT_BACKUP_FLOW_BYTES );

    route->time_last_packet_received = packet->timestamp;
}

static void client_process_direct_server_pong_packet( next_client_t * client, next_incoming_packet_t * packet )
{
    if ( !next_address_equal( &packet->from, &client->server_address ) )
    {
        next_printf( NEXT_LOG_LEVEL_DEBUG, "ignored direct server pong packet. not from server address" );
        return;
    }

    if ( packet->length != NEXT_PACKET_V2_PING_PONG_BYTES )
    {
        next_printf( NEXT_LOG_LEVEL_DEBUG, "ignored direct server pong packet. bad packet length" );
        return;
    }

    if ( !next_address_equal( &packet->from, &client->server_address ) )
    {
        next_printf( NEXT_LOG_LEVEL_DEBUG, "ignored direct server pong packet. not from server address" );
        return;
    }

    uint8_t * p = &packet->data[1];
    uint64_t session = next_read_uint64( &p );
    uint64_t sequence = next_read_uint64( &p );

    if ( session != client->route_current.flow_token.flow_id )
    {
        next_printf( NEXT_LOG_LEVEL_DEBUG, "ignored direct server pong packet. session id mismatch" );
        return;
    }

    ping_history_pong_received( &client->server_stats.ping_history_direct, sequence, packet->timestamp );

    client->route_current.time_last_packet_received = packet->timestamp;
}

static void client_process_client_relay_pong_packet( next_client_t * client, next_incoming_packet_t * packet )
{
    if ( packet->length != NEXT_PACKET_V2_PING_PONG_BYTES )
    {
        next_printf( NEXT_LOG_LEVEL_DEBUG, "ignored client relay pong packet. expected %d bytes, got %d", NEXT_PACKET_V2_PING_PONG_BYTES, packet->length );
        return;
    }

    uint8_t * p = &packet->data[1];
    uint64_t session_id = next_read_uint64(&p);
    next_ping_history_relay_t * history = NULL;
    if ( client->route_current.flow_token.flow_id == session_id )
    {
        for ( int i = 0; i < NEXT_PING_HISTORY_RELAY_COUNT; i++ )
        {
            if ( next_address_equal( &packet->from, &client->ping_history_relay[i].address ) )
            {
                history = &client->ping_history_relay[i];
                break;
            }
        }
    }

    if ( !history )
    {
        next_printf( NEXT_LOG_LEVEL_DEBUG, "ignored client relay pong packet. could not find ping history" );
        return;
    }

    uint64_t sequence = next_read_uint64( &p );

    ping_history_pong_received( &history->history, sequence, packet->timestamp );
}

static void client_process_next_server_pong_packet( next_client_t * client, next_incoming_packet_t * packet )
{
    if ( !next_address_equal( &packet->from, &client->route_current.flow_token.next_address ) &&
         !next_address_equal( &packet->from, &client->route_previous.flow_token.next_address ) )
    {
        next_printf( NEXT_LOG_LEVEL_DEBUG, "ignored packet next server packet. from address does not match expected next address" );
        return;
    }

    if ( packet->length != NEXT_HEADER_BYTES + NEXT_PACKET_V2_PING_PONG_BYTES )
    {
        next_printf( NEXT_LOG_LEVEL_DEBUG, "ignored next server pong response packet. expected %d bytes, got %d", NEXT_HEADER_BYTES + NEXT_PACKET_V2_PING_PONG_BYTES, packet->length );
        return;
    }

    next_client_route_t * route = client_read_packet_header( client, packet->timestamp, packet->data, packet->length );
    if ( !route )
    {
        next_printf( NEXT_LOG_LEVEL_DEBUG, "ignored next server pong response packet. could not find session" );
        return;
    }

    uint8_t * p = &packet->data[NEXT_HEADER_BYTES];
    uint64_t pong_session = next_read_uint64( &p );
    if ( pong_session != route->flow_token.flow_id )
    {
        next_printf( NEXT_LOG_LEVEL_DEBUG, "ignored next server pong response packet. session id mismatch" );
        return;
    }

    uint64_t pong_sequence = next_read_uint64( &p );

    ping_history_pong_received( &route->ping_history_server, pong_sequence, packet->timestamp );

    route->time_last_packet_received = packet->timestamp;
}

static void client_process_migrate_response_packet( next_client_t * client, next_incoming_packet_t * packet )
{
    if ( !next_address_equal( &packet->from, &client->route_current.flow_token.next_address ) &&
         !next_address_equal( &packet->from, &client->route_previous.flow_token.next_address ) )
    {
        next_printf( NEXT_LOG_LEVEL_WARN, "ignored migrate response packet. from address does not match expected next address" );
        return;
    }

    if ( !client_route_read_packet_header( client, &client->route_previous, packet->timestamp, packet->data, packet->length ) )
    {
        next_printf( NEXT_LOG_LEVEL_WARN, "ignored migrate response packet. could not read packet header" );
        return;
    }

    next_flow_log( NEXT_LOG_LEVEL_DEBUG, client->route_previous.flow_token.flow_id, client->route_previous.flow_token.flow_version, "received migrate response." );

    client->migrate_packet_sending = false;
}

static void client_process_route_response_packet( next_client_t * client, next_incoming_packet_t * packet )
{
    if ( !next_address_equal( &packet->from, &client->route_current.flow_token.next_address ) )
    {
        next_printf( NEXT_LOG_LEVEL_WARN, "ignored route response packet. from address does not match expected next address" );
        return;
    }

    if ( packet->length != NEXT_HEADER_BYTES + NEXT_ENCRYPTED_SERVER_TOKEN_BYTES )
    {
        next_printf( NEXT_LOG_LEVEL_WARN, "ignored route response packet. incorrect packet size (expected %d, got %d)", NEXT_HEADER_BYTES + NEXT_ENCRYPTED_SERVER_TOKEN_BYTES, packet->length );
        return;
    }

    if ( !client_route_read_packet_header( client, &client->route_current, packet->timestamp, packet->data, packet->length ) )
    {
        next_printf( NEXT_LOG_LEVEL_WARN, "ignored route response packet. failed to read packet header" );
        return;
    }

    const uint64_t flow_id = client->route_current.flow_token.flow_id;
    const uint8_t flow_version = client->route_current.flow_token.flow_version;

    if ( client->shared.state == NEXT_CLIENT_STATE_REQUESTING )
    {
        next_flow_log( NEXT_LOG_LEVEL_INFO, flow_id, flow_version, "client flow established" );
        client_set_state( client, NEXT_CLIENT_STATE_ESTABLISHED );
        memcpy( client->server_token, &packet->data[NEXT_HEADER_BYTES], NEXT_ENCRYPTED_SERVER_TOKEN_BYTES );
        client->route_request_sending = NEXT_ROUTE_REQUEST_SENDING_NONE;
    }
    else if ( client->shared.state == NEXT_CLIENT_STATE_ESTABLISHED && client->route_request_sending == NEXT_ROUTE_REQUEST_SENDING_UPDATE )
    {
        next_flow_log( NEXT_LOG_LEVEL_INFO, flow_id, flow_version, "received route update response");
        memcpy( client->server_token, &packet->data[NEXT_HEADER_BYTES], NEXT_ENCRYPTED_SERVER_TOKEN_BYTES );
        client->route_request_sending = NEXT_ROUTE_REQUEST_SENDING_NONE;
    }
    else
    {
        next_flow_log( NEXT_LOG_LEVEL_DEBUG, flow_id, flow_version, "ignored route response packet. client not sending route request" );
    }
}

static void client_process_continue_response_packet( next_client_t * client, next_incoming_packet_t * packet )
{
    if ( !next_address_equal( &packet->from, &client->route_current.flow_token.next_address ) )
    {
        next_printf( NEXT_LOG_LEVEL_WARN, "ignored continue response packet. from address does not match expected next address" );
        return;
    }

    if ( packet->length != NEXT_HEADER_BYTES + NEXT_ENCRYPTED_SERVER_TOKEN_BYTES )
    {
        next_printf( NEXT_LOG_LEVEL_WARN, "ignored continue response packet. incorrect packet size (expected %d, got %d)", NEXT_HEADER_BYTES + NEXT_ENCRYPTED_SERVER_TOKEN_BYTES, packet->length );
        return;
    }

    if ( !client_route_read_packet_header( client, &client->route_current, packet->timestamp, packet->data, packet->length ) )
    {
        next_printf( NEXT_LOG_LEVEL_WARN, "ignored continue response packet. failed to read packet header" );
        return;
    }

    const uint64_t flow_id = client->route_current.flow_token.flow_id;
    const uint8_t flow_version = client->route_current.flow_token.flow_version;

    if ( client->route_request_sending != NEXT_ROUTE_REQUEST_SENDING_CONTINUE
        || client->shared.state != NEXT_CLIENT_STATE_ESTABLISHED )
    {
        next_flow_log( NEXT_LOG_LEVEL_DEBUG, flow_id, flow_version, "ignored continue response packet. client not sending continue" );
        return;
    }

    next_flow_log( NEXT_LOG_LEVEL_INFO, flow_id, flow_version, "received continue response");
    memcpy( client->server_token, &packet->data[NEXT_HEADER_BYTES], NEXT_ENCRYPTED_SERVER_TOKEN_BYTES );
    client->route_request_sending = NEXT_ROUTE_REQUEST_SENDING_NONE;
}

static void client_process_server_to_client_packet( next_client_t * client, next_incoming_packet_t * packet )
{
    if ( !next_address_equal( &packet->from, &client->route_current.flow_token.next_address ) &&
         !next_address_equal( &packet->from, &client->route_previous.flow_token.next_address ) )
    {
        next_printf( NEXT_LOG_LEVEL_DEBUG, "ignored server to client packet. from address does not match expected next address" );
        return;
    }

    if ( packet->length <= NEXT_HEADER_BYTES )
    {
        next_printf( NEXT_LOG_LEVEL_DEBUG, "ignored server to client packet. not enough bytes" );
        return;
    }

    if ( packet->length > NEXT_HEADER_BYTES + NEXT_MTU )
    {
        next_printf( NEXT_LOG_LEVEL_DEBUG, "ignored server to client packet. too many bytes" );
        return;
    }

    next_client_route_t * route = client_read_packet_header( client, packet->timestamp, packet->data, packet->length );

    if ( !route )
    {
        next_printf( NEXT_LOG_LEVEL_DEBUG, "ignored server to client packet. could not read packet header" );
        return;
    }

    next_flow_log( NEXT_LOG_LEVEL_DEBUG, route->flow_token.flow_id, route->flow_token.flow_version, "received server to client packet" );

    client->config.packet_received_callback( client, client->config.context, packet->data + NEXT_HEADER_BYTES, packet->length - NEXT_HEADER_BYTES );

    route->time_last_packet_received = packet->timestamp;
}

static void client_process_incoming_packet( next_client_t * client, next_incoming_packet_t * packet )
{
    if ( packet->length < 1 )
        return;

    switch ( packet->data[0] )
    {
        case NEXT_PACKET_TYPE_V2_DIRECT:
            client_process_direct_packet( client, packet );
            break;

        case NEXT_PACKET_TYPE_V2_BACKUP:
            client_process_backup_packet( client, packet );
            break;

        case NEXT_PACKET_TYPE_V2_DIRECT_SERVER_PONG:
            client_process_direct_server_pong_packet( client, packet );
            break;

        case NEXT_PACKET_TYPE_V2_CLIENT_RELAY_PONG:
            client_process_client_relay_pong_packet( client, packet );
            break;

        case NEXT_PACKET_TYPE_V2_NEXT_SERVER_PONG:
            client_process_next_server_pong_packet( client, packet );
            break;

        case NEXT_PACKET_TYPE_V2_MIGRATE_RESPONSE:
            client_process_migrate_response_packet( client, packet );
            break;

        case NEXT_PACKET_TYPE_V2_ROUTE_RESPONSE:
            client_process_route_response_packet( client, packet );
            break;

        case NEXT_PACKET_TYPE_V2_CONTINUE_RESPONSE:
            client_process_continue_response_packet( client, packet );
            break;

        case NEXT_PACKET_TYPE_V2_SERVER_TO_CLIENT:
            client_process_server_to_client_packet( client, packet );
            break;

        default:
            break;
    }
}

static void client_upload_counters( next_client_t * client )
{
#ifndef NEXT_CANARY_ENABLED
    int num_counters = 0;
    for ( int i = 0; i < NEXT_CLIENT_NUM_COUNTERS; ++i )
    {
        if ( client->counters[i] != 0 )
        {
            num_counters++;
        }
    }
    if ( num_counters == 0 )
        return;

    next_printf( NEXT_LOG_LEVEL_DEBUG, "uploading client counters" );

    next_json_document_t doc;
    doc.SetObject();
    
    next_json_allocator_t & allocator = doc.GetAllocator();
    next_json_value_t array;
    array.SetArray();
    
    for ( int j = 0; j < NEXT_CLIENT_NUM_COUNTERS; j++ )
    {
        next_json_value_t value;
        value.SetUint64( client->counters[j] );
        array.PushBack( value, allocator );
    }
    
    doc.AddMember( "Counters", array, allocator );
    
    next_json_string_buffer_t string_buffer;
    next_json_writer_t writer( string_buffer );
    doc.Accept( writer );
    
    next_http_nonblock_post_json( &client->http, "/v2/stats/counters", string_buffer.GetString(), NULL, NULL, 10000 );
    memset( client->counters, 0, sizeof(client->counters) );
    client->counter_post_last = next_time();
#else 
    (void)(client);
#endif
}

static next_thread_return_t NEXT_THREAD_FUNC client_thread_listen( void * arg )
{
    next_client_t * client = (next_client_t*)( arg );

    next_assert( client );

    next_socket_t client_socket = client->shared.socket;

    while ( true )
    {
        next_address_t from;
        memset( &from, 0, sizeof( next_address_t ) );
        uint8_t packet_data[NEXT_MAX_PACKET_SIZE];

        int packet_bytes = next_socket_receive_packet( &client_socket, &from, packet_data, NEXT_MAX_PACKET_SIZE );

        double timestamp = next_time();

        next_mutex_acquire( &client->shared.mutex );
        {
            if ( client->shared.state <= NEXT_CLIENT_STATE_STOPPED )
            {
                next_printf( NEXT_LOG_LEVEL_INFO, "client listen thread stopped (%d)", client->shared.state );
                next_mutex_release( &client->shared.mutex );
                break;
            }

            if ( packet_bytes > 0 )
            {
                next_packet_queue_t * queue = client_packet_queue_active( client );
                next_incoming_packet_t * packet = queue->add();
                packet->timestamp = timestamp;
                packet->from = from;
                packet->length = packet_bytes;
                memcpy( packet->data, packet_data, packet->length );
            }
        }
        next_mutex_release( &client->shared.mutex );
    }

    next_mutex_acquire( &client->shared.mutex );
    client_upload_counters( client );
    next_mutex_release( &client->shared.mutex );

#if NEXT_HTTP_LOG
    for ( int i = 0; i < 250; ++i )
    {
        next_http_nonblock_update( &client->http );
        next_internal_update();
        next_sleep( 0.01f );
    }
#endif // #if NEXT_HTTP_LOG

    NEXT_THREAD_RETURN();
}

static void client_update_packet_queue( next_client_t * client, double time )
{
    (void) time;
    next_mutex_acquire( &client->shared.mutex );
    next_packet_queue_t * queue = client_packet_queue_active( client );
    client->shared.packet_queue_active = !client->shared.packet_queue_active;
    next_mutex_release( &client->shared.mutex );

    for ( int i = 0; i < queue->length; i++ )
    {
        next_incoming_packet_t * packet = &(*queue)[i];
        client_process_incoming_packet( client, packet );
    }

    queue->length = 0;
}

void client_update_counters( next_client_t * client, double time )
{
    if ( client->counter_check_last + 1.0f < time && client->counter_post_last + 10.0f < time )
    {
        for ( int i = 0; i < NEXT_CLIENT_NUM_COUNTERS; ++i )
        {
            if ( client->counters[i] != 0 )
            {
                client_upload_counters( client );
                break;
            }
        }
        client->counter_check_last = time;
    }
}

static void client_update_timeouts( next_client_t * client, double time )
{
    const uint64_t flow_id = client->route_current.flow_token.flow_id;
    const uint8_t flow_version = client->route_current.flow_token.flow_version;

    if ( ( client->shared.state == NEXT_CLIENT_STATE_ESTABLISHED || client->shared.state == NEXT_CLIENT_STATE_REQUESTING )
		&& !client->backup_flow
		&& client->route_update_next + 5.0 < time )
    {
        if ( !client_backup_flow_if_possible( client ) )
        {
            next_flow_log( NEXT_LOG_LEVEL_ERROR, flow_id, flow_version, "route update timed out" );
            client_set_error( client, NEXT_ERROR_CLIENT_ROUTE_TIMED_OUT );
			client_set_state( client, NEXT_CLIENT_STATE_READY );
            client->counters[NEXT_CLIENT_COUNTER_ROUTE_UPDATE_TIMEOUT]++;
            return;
        }
    }

    if ( ( client->shared.state == NEXT_CLIENT_STATE_ESTABLISHED || client->shared.state == NEXT_CLIENT_STATE_REQUESTING
		|| client->shared.state == NEXT_CLIENT_STATE_DIRECT || client->backup_flow )
		&& client->route_current.time_last_packet_received + client->config.session_timeout_seconds <= time )
    {
        next_flow_log( NEXT_LOG_LEVEL_ERROR, flow_id, flow_version, "client timed out" );
        client_set_error( client, NEXT_ERROR_CLIENT_TIMED_OUT );
		client_set_state( client, NEXT_CLIENT_STATE_READY );
        client->counters[NEXT_CLIENT_COUNTER_SERVER_TO_CLIENT_TIMEOUT]++;
        return;
    }
}

void next_client_update( next_client_t * client )
{
    next_assert( client );

    if ( !client )
        return;

    double time = next_time();

    client_update_timeouts( client, time );

    client_update_location( client, time );

    client_update_session( client, time );

    client_ping_near_relays( client, time );

    client_update_packet_queue( client, time );

    client_update_stats( client, time );

    client_update_counters( client, time );

    next_http_nonblock_update( &client->http );

    next_internal_update();
}

void next_client_mode( next_client_t * client, int mode )
{
    next_assert( client );
    if ( client )
        client->mode = mode;
}

void next_client_send_packet( next_client_t * client, uint8_t * user_packet_data, int user_packet_bytes )
{
    next_assert( client );
    next_assert( user_packet_data );
    next_assert( user_packet_bytes >= 0 );
    next_assert( user_packet_bytes <= NEXT_MTU );

    if ( !client )
        return;

    if ( !user_packet_data )
        return;

    if ( user_packet_bytes < 0 )
        return;

    if ( user_packet_bytes > NEXT_MTU )
    {
        next_printf( NEXT_LOG_LEVEL_WARN, "next_client_send_packet dropped packet because it's larger than MTU (%d bytes)", NEXT_MTU );
        return;
    }

    if ( client->shared.state == NEXT_CLIENT_STATE_DIRECT )
    {
        uint8_t * direct_packet_data = (uint8_t*)( alloca( user_packet_bytes + 1 ) );

        direct_packet_data[0] = NEXT_PACKET_TYPE_V2_DIRECT;

        memcpy( direct_packet_data + 1, user_packet_data, user_packet_bytes );

        next_socket_send_packet( &client->shared.socket, &client->route_current.flow_token.next_address, direct_packet_data, user_packet_bytes + 1 );
    }
    else if ( client->backup_flow )
    {
        uint8_t * packet_data = (uint8_t*)( alloca( NEXT_BACKUP_FLOW_BYTES + user_packet_bytes ) );
        uint8_t * p = packet_data;

        client->route_current.sequence++;

        next_write_uint8( &p, NEXT_PACKET_TYPE_V2_BACKUP );
        next_write_uint64( &p, client->route_current.flow_token.flow_id );

        memcpy( p, user_packet_data, user_packet_bytes );

        next_socket_send_packet( &client->shared.socket, &client->server_address, packet_data, NEXT_BACKUP_FLOW_BYTES + user_packet_bytes );
    }
    else if ( client->shared.state == NEXT_CLIENT_STATE_REQUESTING || client->shared.state == NEXT_CLIENT_STATE_ESTABLISHED )
    {
        next_client_route_t * route = client_get_send_route( client );

        uint8_t * packet_data = (uint8_t*)( alloca( NEXT_HEADER_BYTES + user_packet_bytes ) );

        int packet_bytes = NEXT_HEADER_BYTES + user_packet_bytes;

        if ( next_write_header( NEXT_PACKET_TYPE_V2_CLIENT_TO_SERVER, 
                                route->sequence,
                                route->flow_token.flow_id,
                                route->flow_token.flow_version,
                                route->flow_token.flow_flags,
                                route->flow_token.private_key,
                                packet_data, NEXT_HEADER_BYTES + user_packet_bytes ) != NEXT_OK )
        {
            next_flow_log( NEXT_LOG_LEVEL_ERROR, route->flow_token.flow_id, route->flow_token.flow_version, "client failed to send packet to server. failed to write header" );
            return;
        }

        route->sequence++;

        memcpy( packet_data + NEXT_HEADER_BYTES, user_packet_data, user_packet_bytes );

        next_socket_send_packet( &client->shared.socket, &route->flow_token.next_address, packet_data, packet_bytes );
    }
}

int next_client_state( next_client_t * client )
{
    next_assert( client );

    if ( !client )
        return NEXT_CLIENT_STATE_STOPPED;
    
    next_mutex_acquire( &client->shared.mutex );
    int value = client->shared.state;
    next_mutex_release( &client->shared.mutex );
    
    return value;
}

int next_client_error( next_client_t * client )
{
    next_assert( client );
    return client ? client->error : NEXT_OK;
}

uint64_t next_client_id( next_client_t * client )
{
    next_assert( client );
    return client ? client->route_current.flow_token.flow_id : 0;
}

void next_client_stats( next_client_t * client, next_client_stats_t * stats )
{
    double time = next_time();

    next_route_stats_t direct;
    next_route_stats_t next;

    client_stats_direct( client, &direct, time );
    client_stats_next( client, &next, time );

    stats->direct_rtt = direct.rtt;
    stats->direct_jitter = direct.jitter;
    stats->direct_packet_loss = direct.packet_loss;

    stats->next_rtt = next.rtt;
    stats->next_jitter = next.jitter;
    stats->next_packet_loss = next.packet_loss;

    next_client_stats_history_t * history = &client->server_stats.history;

    if ( history->samples[history->index].time == 0.0 )
    {
        stats->num_samples = history->index;
    }
    else
    {
        stats->num_samples = NEXT_MAX_STATS_SAMPLES;
    }

    int start = history->index - stats->num_samples;
    if ( start < 0 )
        start += NEXT_MAX_STATS_SAMPLES;

    int index_read = start;
    int index_write = 0;
    while ( index_write < stats->num_samples )
    {
        stats->samples[index_write] = history->samples[index_read];
        index_write++;
        index_read = ( index_read + 1 ) % NEXT_MAX_STATS_SAMPLES;
    }
}

void next_client_override_location( next_client_t * client, const char * isp, float latitude, float longitude )
{
    next_assert( client );
    client->override_location = true;
    strncpy( client->override_isp, isp, sizeof(client->override_isp) - 1 );
    client->override_latitude = latitude;
    client->override_longitude = longitude;
}

void next_client_location_info( next_client_t * client, char * isp, float * latitude, float * longitude )
{
    next_assert( client );
    next_assert( isp );
    next_assert( latitude );
    next_assert( longitude );
    if ( client->override_location )
    {
        strcpy( isp, client->override_isp );
        *latitude = client->override_latitude;
        *longitude = client->override_longitude;
    }
    else
    {
        strcpy( isp, client->ip2location.isp );
        *latitude = float( atof(client->ip2location.latitude ) );
        *longitude = float( atof( client->ip2location.longitude ) );
    }
}

void next_client_near_relay_stats( next_client_t * client, next_client_near_relay_stats_t * stats )
{
    next_assert( client );
    next_assert( stats );
    double time = next_time();
    memset( stats, 0, sizeof(next_client_near_relay_stats_t) );
    for ( int i = 0; i < client->nearest_relays.relay_count; i++ )
    {
        const int num_relays = stats->num_relays;
        const uint64_t relay_id = stats->relay_id[stats->num_relays] = client->nearest_relays.relays[i].id;
        next_ping_history_relay_t * history = client_ping_history_for_relay_id( client->ping_history_relay, relay_id );
        if ( history )
        {
            next_relay_stats_t relay_stats;
            relay_stats_from_ping_history( history, relay_id, &relay_stats, time );
            if ( relay_stats.route.rtt > 0.0f )
            {
                stats->relay_id[num_relays] = relay_id;
                stats->relay_rtt[num_relays] = relay_stats.route.rtt;
                stats->num_relays++;
            }
        }
    }
    next_assert( stats->num_relays >= 0 );
    next_assert( stats->num_relays <= NEXT_MAX_NEAR_RELAYS );
}

// ---------------------------------------------------------------

struct next_server_route_t
{
    uint64_t packet_sequence;
    next_replay_protection_t replay_protection;
    uint32_t kbps_up;
    uint32_t kbps_down;
    next_address_t prev_address;
    uint8_t flow_version;
    uint8_t flow_flags;
    uint8_t private_key[NEXT_PRIVATE_KEY_BYTES];
};

struct next_server_session_data_t
{
    uint64_t flow_id;
    double last_packet_receive_time;
    next_server_route_t route_current;
    next_server_route_t route_previous;
    uint8_t session_sequence;
    bool backup_flow;
};

struct next_server_incoming_packet_t
{
    uint64_t from_flow_id;
    next_address_t from_address;
    int length;
    uint8_t data[NEXT_MAX_PACKET_SIZE];
};

struct next_server_t
{
    next_server_config_t config;
    int state;
    next_socket_t socket;
    uint64_t * session_ids;
    next_server_session_data_t * session_data;
    next_thread_t thread_listen;
    next_mutex_t mutex;
    next_vector_t<next_server_incoming_packet_t> incoming_packet_queue;
};

void next_server_validate_config( next_server_config_t * config )
{
    if ( config->max_sessions <= 0 )
    {
        config->max_sessions = 256;
    }

    if ( config->session_timeout_seconds <= 0.0f )
    {
        config->session_timeout_seconds = NEXT_DEFAULT_SERVER_SESSION_TIMEOUT_SECONDS;
    }

    uint8_t zero_private_key[NEXT_PRIVATE_KEY_BYTES];
    uint8_t zero_public_key[NEXT_PUBLIC_KEY_BYTES];
    memset( zero_private_key, 0, NEXT_PRIVATE_KEY_BYTES );
    memset( zero_public_key, 0, NEXT_PUBLIC_KEY_BYTES );
    
    if ( memcmp( config->private_key, zero_private_key, NEXT_PRIVATE_KEY_BYTES ) == 0 &&
         memcmp( config->public_key, zero_public_key, NEXT_PUBLIC_KEY_BYTES ) == 0 )
    {
        const uint8_t DefaultServerPublicKey[] = {0xc6, 0xa4, 0xa2, 0x41, 0x24, 0x9b, 0x02, 0x88, 0x4e, 0xdf, 0x43, 0xf9, 0x22, 0xa9, 0x87, 0x5e, 0xcb, 0xa7, 0x56, 0xbd, 0x38, 0x82, 0x9c, 0xb0, 0x58, 0x5f, 0x19, 0x9e, 0xfc, 0x2b, 0x6a, 0x1d};
        const uint8_t DefaultServerPrivateKey[] = {0xac, 0xe2, 0x42, 0x88, 0x55, 0x48, 0x3c, 0x32, 0xf2, 0x67, 0x50, 0x5d, 0x1b, 0x7a, 0xdb, 0x07, 0x11, 0xb4, 0x63, 0xfe, 0xd3, 0x13, 0x0f, 0x96, 0xfd, 0xa1, 0x63, 0x19, 0x23, 0x87, 0x77, 0xd0};
        next_printf( NEXT_LOG_LEVEL_DEBUG, "using default server public/private keypair. don't ship with this!" );
        memcpy( config->public_key, DefaultServerPublicKey, NEXT_PUBLIC_KEY_BYTES );
        memcpy( config->private_key, DefaultServerPrivateKey, NEXT_PRIVATE_KEY_BYTES );
    }
}

static next_thread_return_t NEXT_THREAD_FUNC server_thread_listen( void * arg );

next_server_t * next_server_create( next_server_config_t * config, const char * bind_address_string )
{
    next_check_initialized( 1 );

    next_address_t bind_address;
    memset( &bind_address, 0, sizeof( bind_address ) );
    if ( next_address_parse( &bind_address, bind_address_string ) != NEXT_OK )
        return NULL;

    next_socket_t socket;
    if ( next_socket_create( &socket, &bind_address, 100, NEXT_SOCKET_SNDBUF_SIZE, NEXT_SOCKET_RCVBUF_SIZE, 0 ) != NEXT_ERROR_SOCKET_NONE )
        return NULL;

    next_server_t * server = (next_server_t*) next_alloc( sizeof(next_server_t) );
    if ( !server )
        return NULL;

    memset( server, 0, sizeof(next_server_t) );

    server->socket = socket;

    memcpy( &server->config, config, sizeof(next_server_config_t) );
    next_server_validate_config( &server->config );

    server->state = NEXT_SERVER_STATE_LISTENING;

    server->session_ids = (uint64_t*)( next_alloc( sizeof(uint64_t) * server->config.max_sessions ) );
    memset( server->session_ids, 0, sizeof(uint64_t) * server->config.max_sessions );

    server->session_data = (next_server_session_data_t*)( next_alloc( sizeof(next_server_session_data_t) * server->config.max_sessions ) );
    memset( server->session_data, 0, sizeof(next_server_session_data_t) * server->config.max_sessions );

    if ( !next_mutex_init( &server->mutex ) )
    {
        next_socket_destroy( &server->socket );
        next_free( server );
        return NULL;
    }

    if ( !next_thread_create( &server->thread_listen, server_thread_listen, server ) )
    {
        next_socket_destroy( &server->socket );
        next_mutex_destroy( &server->mutex );
        next_free( server );
        return NULL;
    }

    return server;
}

void next_server_destroy( next_server_t * server )
{
    next_check_initialized( 1 );

    next_assert( server );

    if ( !server )
        return;

    next_mutex_acquire( &server->mutex );
    server->state = NEXT_SERVER_STATE_STOPPED;
    next_socket_destroy( &server->socket );
    next_mutex_release( &server->mutex );

    next_thread_join( &server->thread_listen );

    next_mutex_destroy( &server->mutex );

    next_free( server->session_data );
    next_free( server->session_ids );

    server->incoming_packet_queue.clear();

    memset( server, 0, sizeof(next_server_t) );

    next_free( server );
}

static void server_send_packet( next_server_t * server, next_address_t * to, uint8_t * packet_data, int packet_bytes )
{
    next_socket_send_packet( &server->socket, to, packet_data, packet_bytes );
}

static int server_find_session( next_server_t * server, uint64_t session_id )
{
    next_assert( server );
    next_assert( server->session_ids );
    for ( int i = 0; i < server->config.max_sessions; ++i )
    {
        if ( server->session_ids[i] == session_id )
        {
            return i;
        }
    }
    return -1;
}

static int server_find_free_session( next_server_t * server )
{
    next_assert( server );
    next_assert( server->session_ids );
    for ( int i = 0; i < server->config.max_sessions; ++i )
    {
        if ( server->session_ids[i] == 0 )
        {
            return i;
        }
    }
    return -1;
}

static void server_add_session( next_server_t * server, next_address_t * from, int session_index, next_flow_token_t * flow_token )
{
    next_assert( server );
    next_assert( server->session_ids );
    next_assert( session_index >= 0 );
    next_assert( session_index < server->config.max_sessions );
    next_assert( flow_token );
    next_assert( server->session_ids[session_index] == 0 );

    server->session_ids[session_index] = flow_token->flow_id;

    next_server_session_data_t * session_data = &server->session_data[session_index];

    session_data->route_current.flow_version = flow_token->flow_version;
    session_data->route_current.flow_flags = flow_token->flow_flags;
    session_data->route_current.kbps_up = flow_token->kbps_up;
    session_data->route_current.kbps_down = flow_token->kbps_down;
    session_data->route_current.packet_sequence = 1;
    session_data->route_current.prev_address = *from;
    next_replay_protection_reset( &session_data->route_current.replay_protection );
    memcpy( session_data->route_current.private_key, flow_token->private_key, NEXT_SYMMETRIC_KEY_BYTES );

    session_data->flow_id = flow_token->flow_id;
    session_data->last_packet_receive_time = next_time();
    session_data->route_previous = session_data->route_current;

    session_data->session_sequence++;
}

static void server_update_session( next_server_t * server, next_address_t * from, int session_index, next_flow_token_t * flow_token )
{
    next_assert( server );
    next_assert( server->session_ids );
    next_assert( session_index >= 0 );
    next_assert( session_index < server->config.max_sessions );
    next_assert( flow_token );
    next_assert( server->session_ids[session_index] == flow_token->flow_id );

    next_server_session_data_t * session_data = &server->session_data[session_index];

    session_data->route_previous = session_data->route_current;

    session_data->route_current.flow_version = flow_token->flow_version;
    session_data->route_current.flow_flags = flow_token->flow_flags;
    session_data->route_current.packet_sequence = 1;
    session_data->route_current.prev_address = *from;
    memcpy( session_data->route_current.private_key, flow_token->private_key, NEXT_SYMMETRIC_KEY_BYTES );
    next_replay_protection_reset( &session_data->route_current.replay_protection );
}

static void server_remove_session( next_server_t * server, int session_index )
{
    next_assert( server );
    next_assert( server->session_ids );
    next_assert( server->session_data );
    next_assert( session_index >= 0 );
    next_assert( session_index < server->config.max_sessions );
    server->session_ids[session_index] = 0;
    memset( &server->session_data[session_index], 0, sizeof(next_server_session_data_t) );
}

static void server_process_route_request_packet( next_server_t * server, next_address_t * from, uint8_t * packet_data, int packet_bytes )
{
    next_assert( packet_data );
    next_assert( packet_bytes >= 1 );
    next_assert( packet_bytes <= NEXT_MAX_PACKET_SIZE );

    next_assert( server );

    if ( packet_bytes != 1 + NEXT_ENCRYPTED_FLOW_TOKEN_BYTES )
    {
        next_printf( NEXT_LOG_LEVEL_ERROR, "server ignored route request packet. incorrect packet size. expected %d bytes, got %d", 1 + NEXT_ENCRYPTED_FLOW_TOKEN_BYTES, packet_bytes );
        return;
    }

    uint8_t * buffer = &packet_data[1];

    next_flow_token_t flow_token;
    if ( next_read_encrypted_flow_token( &buffer, &flow_token, NEXT_KEY_MASTER, server->config.private_key ) != NEXT_OK )
    {
        next_printf( NEXT_LOG_LEVEL_ERROR, "server ignored route request packet. could not read encrypted token" );
        return;
    }

    uint64_t session_id = flow_token.flow_id;
    int session_index = server_find_session( server, session_id );
    if ( session_index == -1 )
    {
        session_index = server_find_free_session( server );
        if ( session_index == -1 )
        {
            next_printf( NEXT_LOG_LEVEL_ERROR, "server ignored route request packet. no free sessions" );
            return;
        }

        next_flow_log( NEXT_LOG_LEVEL_INFO, flow_token.flow_id, flow_token.flow_version, "session created" );

        server_add_session( server, from, session_index, &flow_token );
    }
    else if ( next_sequence_greater_than( flow_token.flow_version, server->session_data[session_index].route_current.flow_version ) )
    {
        // next_flow_log( NEXT_LOG_LEVEL_INFO, flow_token.flow_id, server->session_data[session_index].route_current.flow_version, "server updated session (flow version: %hu->%hu)", server->session_data[session_index].route_current.flow_version, flow_token.flow_version );

        server_update_session( server, from, session_index, &flow_token );
    }

    uint8_t response_data[NEXT_MAX_PACKET_SIZE];
    if ( next_write_header( NEXT_PACKET_TYPE_V2_ROUTE_RESPONSE,
                            server->session_data[session_index].route_current.packet_sequence++,
                            flow_token.flow_id,
                            flow_token.flow_version,
                            flow_token.flow_flags,
                            flow_token.private_key,
                            response_data,
                            sizeof( response_data ) ) != NEXT_OK )
    {
        next_flow_log( NEXT_LOG_LEVEL_ERROR, flow_token.flow_id, flow_token.flow_version, "server ignored route request packet. failed to write route response header" );
        return;
    }

    next_server_token_t server_token;
    server_token.flow_id = flow_token.flow_id;
    server_token.flow_version = flow_token.flow_version;
    server_token.flow_flags = flow_token.flow_flags;
    server_token.expire_timestamp = flow_token.expire_timestamp;

    uint8_t * p = response_data + NEXT_HEADER_BYTES;

    if ( next_write_encrypted_server_token( &p, &server_token, server->config.private_key, NEXT_KEY_MASTER ) != NEXT_OK )
    {
        next_flow_log( NEXT_LOG_LEVEL_ERROR, flow_token.flow_id, flow_token.flow_version, "server ignored route request packet. failed to write encrypted server token" );
        return;
    }

    next_flow_log( NEXT_LOG_LEVEL_DEBUG, flow_token.flow_id, flow_token.flow_version, "server received route request packet" );

    server_send_packet( server, from, response_data, NEXT_HEADER_BYTES + NEXT_ENCRYPTED_SERVER_TOKEN_BYTES );
}

static void server_process_continue_request_packet( next_server_t * server, next_address_t * from, uint8_t * packet_data, int packet_bytes )
{
    next_assert( server );
    next_assert( packet_data );
    next_assert( packet_bytes >= 1 );
    next_assert( packet_bytes <= NEXT_MAX_PACKET_SIZE );

    if ( packet_bytes != 1 + NEXT_ENCRYPTED_CONTINUE_TOKEN_BYTES )
    {
        next_printf( NEXT_LOG_LEVEL_ERROR, "server ignored continue packet. incorrect packet size. expected %d bytes, got %d", 1 + NEXT_ENCRYPTED_CONTINUE_TOKEN_BYTES, packet_bytes );
        return;
    }

    uint8_t * buffer = &packet_data[1];

    next_continue_token_t continue_token;
    if ( next_read_encrypted_continue_token( &buffer, &continue_token, NEXT_KEY_MASTER, server->config.private_key ) != NEXT_OK )
    {
        next_printf( NEXT_LOG_LEVEL_ERROR, "server ignored continue request packet. could not read encrypted token" );
        return;
    }

    int session_index = server_find_session( server, continue_token.flow_id );
    if ( session_index == -1 )
    {
        next_flow_log( NEXT_LOG_LEVEL_ERROR, continue_token.flow_id, continue_token.flow_version, "server ignored continue request packet. could not find session to continue" );
        return;
    }

    uint8_t response_data[NEXT_MAX_PACKET_SIZE];
    if ( next_write_header( NEXT_PACKET_TYPE_V2_CONTINUE_RESPONSE,
                            server->session_data[session_index].route_current.packet_sequence++,
                            continue_token.flow_id,
                            continue_token.flow_version,
                            continue_token.flow_flags,
                            server->session_data[session_index].route_current.private_key,
                            response_data,
                            sizeof( response_data ) ) != NEXT_OK )
    {
        next_flow_log( NEXT_LOG_LEVEL_ERROR, continue_token.flow_id, continue_token.flow_version, "server ignored continue request packet. failed to write continue response header" );
        return;
    }

    next_server_token_t server_token;
    server_token.flow_id = continue_token.flow_id;
    server_token.flow_version = continue_token.flow_version;
    server_token.flow_flags = continue_token.flow_flags;
    server_token.expire_timestamp = continue_token.expire_timestamp;

    uint8_t * p = response_data + NEXT_HEADER_BYTES;

    if ( next_write_encrypted_server_token( &p, &server_token, server->config.private_key, NEXT_KEY_MASTER ) != NEXT_OK )
    {
        next_flow_log( NEXT_LOG_LEVEL_ERROR, continue_token.flow_id, continue_token.flow_version, "server ignored continue request packet. failed to write encrypted server token" );
        return;
    }

    next_flow_log( NEXT_LOG_LEVEL_DEBUG, continue_token.flow_id, continue_token.flow_version, "server received continue request packet" );

    server_send_packet( server, from, response_data, NEXT_HEADER_BYTES + NEXT_ENCRYPTED_SERVER_TOKEN_BYTES );
}

static int server_session_for_packet( next_server_t * server, uint8_t * packet_data, int packet_bytes, int * out_session_index, next_server_route_t ** out_route )
{
    uint8_t type = 0;
    uint64_t sequence = 0;
    uint64_t flow_id = 0;
    uint8_t flow_version = 0;
    uint8_t flow_flags = 0;
    if ( next_peek_header( &type, &sequence, &flow_id, &flow_version, &flow_flags, packet_data, packet_bytes ) != NEXT_OK )
    {
        next_printf( NEXT_LOG_LEVEL_DEBUG, "server ignored packet. failed to peek header" );
        return NEXT_ERROR;
    }

    int session_index = server_find_session( server, flow_id );
    if ( session_index == -1 )
    {
        next_flow_log( NEXT_LOG_LEVEL_DEBUG, flow_id, flow_version, "server ignored packet (type %hhu). could not find session", type );
        return NEXT_ERROR;
    }

    *out_session_index = session_index;

    next_server_session_data_t * session_data = &server->session_data[session_index];

    if ( flow_version == session_data->route_current.flow_version )
    {
        *out_route = &session_data->route_current;
        return NEXT_OK;
    }
    else if ( flow_version == session_data->route_previous.flow_version )
    {
        *out_route = &session_data->route_previous;
        return NEXT_OK;
    }
    else
    {
        next_flow_log( NEXT_LOG_LEVEL_DEBUG, flow_id, flow_version, "server ignored packet (type %hhu). flow version mismatch" );
        return NEXT_ERROR;
    }
}

static int server_route_process_packet( next_server_route_t * route, next_address_t * from, uint8_t * packet_data, int packet_bytes )
{
    uint8_t type = 0;
    uint64_t sequence = 0;
    uint64_t flow_id = 0;
    uint8_t flow_version = 0;
    uint8_t flow_flags = 0;

    if ( next_read_header( &type, &sequence, &flow_id, &flow_version, &flow_flags, route->private_key, packet_data, packet_bytes ) != NEXT_OK )
    {
        next_flow_log( NEXT_LOG_LEVEL_DEBUG, flow_id, flow_version, "server ignored packet (type %hhu). failed to read header", type );
        return NEXT_ERROR;
    }

    if ( flow_version != route->flow_version )
    {
        next_flow_log( NEXT_LOG_LEVEL_DEBUG, flow_id, route->flow_version, "server ignored packet (type %hhu). flow version mismatch: %hu", type, flow_version );
        return NEXT_ERROR;
    }

    if ( next_replay_protection_packet_already_received( &route->replay_protection, sequence ) )
    {
        next_printf( NEXT_LOG_LEVEL_DEBUG, "server ignored packet (type %hhu). sequence number already received: %" PRId64, type, sequence );
        return NEXT_ERROR;
    }

    route->prev_address = *from;

    return NEXT_OK;
}

static void server_process_client_to_server_packet( next_server_t * server, next_address_t * from, uint8_t * packet_data, int packet_bytes )
{
    next_assert( server );
    next_assert( packet_data );
    next_assert( packet_bytes >= 1 );
    next_assert( packet_bytes <= NEXT_MAX_PACKET_SIZE );

    (void) from;

    if ( packet_bytes < NEXT_HEADER_BYTES )
    {
        next_printf( NEXT_LOG_LEVEL_WARN, "server ignored client to server packet. packet is too small (%d bytes)", packet_bytes );
        return;
    }

    int session_index;
    next_server_route_t * route;
    if ( server_session_for_packet( server, packet_data, packet_bytes, &session_index, &route ) != NEXT_OK )
        return;

    if ( server_route_process_packet( route, from, packet_data, packet_bytes ) != NEXT_OK )
        return;

    next_server_session_data_t * session_data = &server->session_data[session_index];

    server->session_data[session_index].last_packet_receive_time = next_time();

    if ( server->config.packet_received_callback )
    {
        next_address_t session_address;
        next_session_to_address( session_index, session_data->session_sequence, &session_address );

        next_server_incoming_packet_t * packet = server->incoming_packet_queue.add();
        packet->from_flow_id = session_data->flow_id;
        memcpy( &packet->from_address, &session_address, sizeof( packet->from_address ) );
        packet->length = packet_bytes - NEXT_HEADER_BYTES;
        memcpy( packet->data, packet_data + NEXT_HEADER_BYTES, packet->length );
        next_flow_log( NEXT_LOG_LEVEL_DEBUG, session_data->flow_id, route->flow_version, "server received client to server packet" );
    }
    else
    {
        next_flow_log( NEXT_LOG_LEVEL_ERROR, session_data->flow_id, route->flow_version, "server ignored client to server packet. packet_received_callback is NULL" );
    }
}

static void server_process_migrate_packet( next_server_t * server, next_address_t * from, uint8_t * packet_data, int packet_bytes )
{
    next_assert( server );
    next_assert( packet_data );
    next_assert( packet_bytes >= 1 );
    next_assert( packet_bytes <= NEXT_MAX_PACKET_SIZE );

    if ( packet_bytes != NEXT_HEADER_BYTES )
    {
        next_printf( NEXT_LOG_LEVEL_ERROR, "server ignored migrate packet. packet has wrong size (%d bytes)", packet_bytes );
        return;
    }

    int session_index;
    next_server_route_t * route;
    if ( server_session_for_packet( server, packet_data, packet_bytes, &session_index, &route ) != NEXT_OK )
        return;

    next_server_session_data_t * session_data = &server->session_data[session_index];

    if ( route != &session_data->route_previous )
        return;

    if ( server_route_process_packet( &session_data->route_previous, from, packet_data, packet_bytes ) != NEXT_OK )
        return;

    uint8_t next_packet_data[NEXT_HEADER_BYTES];
    if ( next_write_header( NEXT_PACKET_TYPE_V2_MIGRATE_RESPONSE,
            session_data->route_previous.packet_sequence++,
            session_data->flow_id,
            session_data->route_previous.flow_version,
            session_data->route_previous.flow_flags,
            session_data->route_previous.private_key,
            next_packet_data,
            sizeof(next_packet_data) ) != NEXT_OK )
    {
        next_flow_log( NEXT_LOG_LEVEL_ERROR, session_data->flow_id, session_data->route_previous.flow_version, "server ignored migrate packet. failed to write response packet header" );
        return;
    }

    int next_packet_bytes = NEXT_HEADER_BYTES;

    next_flow_log( NEXT_LOG_LEVEL_DEBUG, session_data->flow_id, session_data->route_previous.flow_version, "server received migrate packet" );

    server_send_packet( server, &session_data->route_previous.prev_address, next_packet_data, next_packet_bytes );
}

static void server_process_destroy_packet( next_server_t * server, next_address_t * from, uint8_t * packet_data, int packet_bytes )
{
    next_assert( server );
    next_assert( packet_data );
    next_assert( packet_bytes >= 1 );
    next_assert( packet_bytes <= NEXT_MAX_PACKET_SIZE );

    if ( packet_bytes != NEXT_HEADER_BYTES )
    {
        next_printf( NEXT_LOG_LEVEL_ERROR, "server ignored destroy packet. packet has wrong size (%d bytes)", packet_bytes );
        return;
    }

    int session_index;
    next_server_route_t * route;
    if ( server_session_for_packet( server, packet_data, packet_bytes, &session_index, &route ) != NEXT_OK )
        return;

    next_server_session_data_t * session_data = &server->session_data[session_index];

    if ( route != &session_data->route_current )
        return;

    if ( server_route_process_packet( route, from, packet_data, packet_bytes ) != NEXT_OK )
        return;

    next_flow_log( NEXT_LOG_LEVEL_INFO, server->session_ids[session_index], server->session_data[session_index].route_current.flow_version, "session destroyed" );

    server_remove_session( server, session_index );
}

static void server_process_backup_packet( next_server_t * server, next_address_t * from, uint8_t * packet_data, int packet_bytes )
{
    next_assert( server );
    next_assert( packet_data );
    next_assert( packet_bytes >= NEXT_BACKUP_FLOW_BYTES );
    next_assert( packet_bytes <= NEXT_MAX_PACKET_SIZE );

    if ( packet_bytes < NEXT_BACKUP_FLOW_BYTES )
    {
        next_printf( NEXT_LOG_LEVEL_DEBUG, "server ignored backup packet. too small" );
        return;
    }

    uint8_t * p = &packet_data[1];
    uint64_t flow_id = next_read_uint64( &p );

    int session_index = server_find_session( server, flow_id );
    if ( session_index == -1 )
    {
        next_printf( NEXT_LOG_LEVEL_DEBUG, "server ignored backup packet. could not find session for flow id %" PRIx64 "", flow_id );
        return;
    }

    if ( !server->session_data[session_index].backup_flow )
    {
        server->session_data[session_index].backup_flow = true;
        server->session_data[session_index].route_current.prev_address = *from;
        next_flow_log( NEXT_LOG_LEVEL_INFO, server->session_data[session_index].flow_id, server->session_data[session_index].route_current.flow_version, "session fell to backup flow" );
    }

    if ( server->config.packet_received_callback )
    {
        next_server_incoming_packet_t * packet = server->incoming_packet_queue.add();
        packet->from_flow_id = flow_id;
        memcpy( &packet->from_address, from, sizeof( packet->from_address ) );
        packet->length = packet_bytes - NEXT_BACKUP_FLOW_BYTES;
        memcpy( packet->data, packet_data + NEXT_BACKUP_FLOW_BYTES, packet->length );
        next_flow_log( NEXT_LOG_LEVEL_DEBUG, server->session_data[session_index].flow_id, server->session_data[session_index].route_current.flow_version, "server received backup packet" );
    }
    else
    {
        next_printf( NEXT_LOG_LEVEL_ERROR, "server ignored backup packet. packet_received_callback is NULL" );
    }
   server->session_data[session_index].last_packet_receive_time = next_time();
}

static void server_process_direct_packet( next_server_t * server, next_address_t * from, uint8_t * packet_data, int packet_bytes )
{
    next_assert( server );
    next_assert( packet_data );
    next_assert( packet_bytes >= 1 );
    next_assert( packet_bytes <= NEXT_MAX_PACKET_SIZE );

    if ( packet_bytes == 1 )
    {
        next_printf( NEXT_LOG_LEVEL_WARN, "server ignored direct packet. too small" );
        return;
    }

    if ( server->config.packet_received_callback )
    {
        uint64_t flow_id = next_direct_address_to_flow_id( from );

        next_assert( flow_id != 0 );
        next_assert( flow_id & (1ULL<<63) );

        next_server_incoming_packet_t * packet = server->incoming_packet_queue.add();
        packet->from_flow_id = flow_id;
        memcpy( &packet->from_address, from, sizeof( packet->from_address ) );
        packet->length = packet_bytes - 1;
        memcpy( packet->data, packet_data + 1, packet->length );
        next_printf( NEXT_LOG_LEVEL_DEBUG, "server received direct packet" );
    }
    else
    {
        next_printf( NEXT_LOG_LEVEL_ERROR, "server ignored direct packet. packet_received_callback is NULL" );
    }
}

static void server_process_direct_server_ping_packet( next_server_t * server, next_address_t * from, uint8_t * packet_data, int packet_bytes )
{
    next_assert( server );
    next_assert( packet_data );

    if ( packet_bytes != NEXT_PACKET_V2_PING_PONG_BYTES )
    {
        next_printf( NEXT_LOG_LEVEL_WARN, "server ignored direct server ping. expected %d byte packet, got %d bytes", NEXT_PACKET_V2_PING_PONG_BYTES, packet_bytes );
        return;
    }

    uint8_t * p = &packet_data[1];
    uint64_t session = next_read_uint64( &p );
    uint64_t sequence = next_read_uint64( &p );

    uint8_t response_data[256];
    p = &response_data[0];
    next_write_uint8( &p, NEXT_PACKET_TYPE_V2_DIRECT_SERVER_PONG );
    next_write_uint64( &p, session );
    next_write_uint64( &p, sequence );
    server_send_packet( server, from, response_data, NEXT_PACKET_V2_PING_PONG_BYTES );
}

static void server_process_next_server_ping_packet( next_server_t * server, next_address_t * from, uint8_t * packet_data, int packet_bytes )
{
    next_assert( server );
    next_assert( packet_data );
    next_assert( packet_bytes >= 1 );
    next_assert( packet_bytes <= NEXT_MAX_PACKET_SIZE );

    if ( packet_bytes != NEXT_HEADER_BYTES + NEXT_PACKET_V2_PING_PONG_BYTES )
    {
        next_printf( NEXT_LOG_LEVEL_WARN, "server ignored next server ping packet. expected %d bytes, got %d", NEXT_HEADER_BYTES + NEXT_PACKET_V2_PING_PONG_BYTES, packet_bytes );
        return;
    }

    int session_index;
    next_server_route_t * route;
    if ( server_session_for_packet( server, packet_data, packet_bytes, &session_index, &route ) != NEXT_OK )
        return;

    if ( server_route_process_packet( route, from, packet_data, packet_bytes ) != NEXT_OK )
        return;

    next_server_session_data_t * session_data = &server->session_data[session_index];

    uint8_t * p = packet_data + NEXT_HEADER_BYTES;
    uint64_t ping_session = next_read_uint64( &p );
    uint64_t ping_sequence = next_read_uint64( &p );

    uint8_t next_packet_data[NEXT_HEADER_BYTES + NEXT_PACKET_V2_PING_PONG_BYTES];

    if ( next_write_header( NEXT_PACKET_TYPE_V2_NEXT_SERVER_PONG,
        route->packet_sequence++,
        session_data->flow_id,
        route->flow_version,
        route->flow_flags,
        route->private_key,
        next_packet_data,
        sizeof(next_packet_data) ) != NEXT_OK )
    {
        next_flow_log( NEXT_LOG_LEVEL_ERROR, session_data->flow_id, route->flow_version, "server ignored next server ping packet. failed to write response packet header" );
        return;
    }

    p = next_packet_data + NEXT_HEADER_BYTES;
    next_write_uint64( &p, ping_session );
    next_write_uint64( &p, ping_sequence );

    session_data->last_packet_receive_time = next_time();

    int next_packet_bytes = NEXT_HEADER_BYTES + NEXT_PACKET_V2_PING_PONG_BYTES;

    next_flow_log( NEXT_LOG_LEVEL_DEBUG, session_data->flow_id, route->flow_version, "server received next server ping packet" );

    server_send_packet( server, &route->prev_address, next_packet_data, next_packet_bytes );
}

static void server_process_packet( next_server_t * server, next_address_t * from, uint8_t * packet_data, int packet_bytes )
{
    if ( packet_bytes < 1 )
        return;

    uint8_t packet_type = packet_data[0];

    switch ( packet_type )
    {
        case NEXT_PACKET_TYPE_V2_ROUTE_REQUEST:
        {
            server_process_route_request_packet( server, from, packet_data, packet_bytes );
        }
        break;

        case NEXT_PACKET_TYPE_V2_CONTINUE_REQUEST:
        {
            server_process_continue_request_packet( server, from, packet_data, packet_bytes );
        }
        break;

        case NEXT_PACKET_TYPE_V2_CLIENT_TO_SERVER:
        {
            server_process_client_to_server_packet( server, from, packet_data, packet_bytes );
        }
        break;

        case NEXT_PACKET_TYPE_V2_MIGRATE:
        {
            server_process_migrate_packet( server, from, packet_data, packet_bytes );
        }
        break;

        case NEXT_PACKET_TYPE_V2_DESTROY:
        {
            server_process_destroy_packet( server, from, packet_data, packet_bytes );
        }
        break;

        case NEXT_PACKET_TYPE_V2_BACKUP:
        {
            server_process_backup_packet( server, from, packet_data, packet_bytes );
        }
        break;

        case NEXT_PACKET_TYPE_V2_DIRECT:
        {
            server_process_direct_packet( server, from, packet_data, packet_bytes );
        }
        break;

        case NEXT_PACKET_TYPE_V2_DIRECT_SERVER_PING:
        {
            server_process_direct_server_ping_packet( server, from, packet_data, packet_bytes );
        }
        break;

        case NEXT_PACKET_TYPE_V2_NEXT_SERVER_PING:
        {
            server_process_next_server_ping_packet( server, from, packet_data, packet_bytes );
        }
        break;

        default: break;
    }
}

static next_thread_return_t NEXT_THREAD_FUNC server_thread_listen( void * arg )
{
    next_server_t * server = (next_server_t*) arg;

    next_assert( server );

    next_mutex_acquire( &server->mutex );
    next_socket_t server_socket = server->socket;
    next_mutex_release( &server->mutex );

    while ( true )
    {
        next_address_t from;
        memset( &from, 0, sizeof( next_address_t ) );
        uint8_t packet_data[NEXT_MAX_PACKET_SIZE];

        int packet_bytes = next_socket_receive_packet( &server_socket, &from, packet_data, NEXT_MAX_PACKET_SIZE );

        next_mutex_acquire( &server->mutex );
        {
            if ( server->state <= NEXT_SERVER_STATE_STOPPED )
            {
                next_printf( NEXT_LOG_LEVEL_INFO, "server listen thread stopped (%d)", server->state );
                next_mutex_release( &server->mutex );
                break;
            }

            server_process_packet( server, &from, packet_data, packet_bytes );
        }
        next_mutex_release( &server->mutex );
    }

    NEXT_THREAD_RETURN();
}

void server_check_for_timeouts( next_server_t * server, double time )
{
    next_assert( server );

    for ( int i = 0; i < server->config.max_sessions; ++i )
    {
        if ( server->session_ids[i] == 0 )
            continue;

        if ( server->session_data[i].last_packet_receive_time + server->config.session_timeout_seconds < time )
        {
            next_flow_log( NEXT_LOG_LEVEL_WARN, server->session_ids[i], server->session_data[i].route_current.flow_version, "session timed out" );
            server_remove_session( server, i );
        }
    }
}

void next_server_update( next_server_t * server )
{
    next_assert( server );

    if ( !server )
        return;

    double time = next_time();

    next_internal_update();

    next_mutex_acquire( &server->mutex );

    for ( int i = 0; i < server->incoming_packet_queue.length; i++ )
    {
        next_server_incoming_packet_t * packet = &server->incoming_packet_queue[i];
        server->config.packet_received_callback( server, server->config.context, packet->from_flow_id, &packet->from_address, packet->data, packet->length );
    }

    server->incoming_packet_queue.length = 0;

    server_check_for_timeouts( server, time );

    next_mutex_release( &server->mutex );
}

void next_server_send_packet( next_server_t * server, uint64_t to_session_id, uint8_t * packet_data, int packet_bytes )
{
    next_assert( server );
    next_assert( packet_data );
    next_assert( packet_bytes >= 0 );
    next_assert( packet_bytes <= NEXT_MTU );

    if ( !server )
        return;

    if ( !packet_data )
        return;

    if ( packet_bytes < 0 )
        return;

    if ( packet_bytes > NEXT_MTU )
    {
        next_printf( NEXT_LOG_LEVEL_WARN, "next_server_send_packet dropped packet because it's larger than MTU (%d bytes)", NEXT_MTU );
        return;
    }

    next_address_t direct_address;
    next_direct_address_from_flow_id( to_session_id, &direct_address );
    if ( direct_address.type != NEXT_ADDRESS_NONE )
    {
        next_server_send_packet_to_address( server, &direct_address, packet_data, packet_bytes );
        return;
    }

    next_mutex_acquire( &server->mutex );

    int session_index = server_find_session( server, to_session_id );
    if ( session_index < 0 )
    {
        next_mutex_release( &server->mutex );
        return;
    }

    next_assert( session_index >= 0 );
    next_assert( session_index < server->config.max_sessions );

    if ( server->session_data[session_index].backup_flow )
    {
        uint8_t next_packet_data[NEXT_BACKUP_FLOW_BYTES + NEXT_MTU], * start = next_packet_data;

        next_write_uint8( &start, NEXT_PACKET_TYPE_V2_BACKUP );
        next_write_uint64( &start, server->session_data[session_index].flow_id );
        memcpy( start, packet_data, packet_bytes );
        server_send_packet( server, &server->session_data[session_index].route_current.prev_address, next_packet_data, NEXT_BACKUP_FLOW_BYTES + packet_bytes );
    }
    else
    {
        uint8_t next_packet_data[NEXT_HEADER_BYTES + NEXT_MTU];
        if ( next_write_header( NEXT_PACKET_TYPE_V2_SERVER_TO_CLIENT,
                server->session_data[session_index].route_current.packet_sequence++,
                server->session_ids[session_index],
                server->session_data[session_index].route_current.flow_version,
                server->session_data[session_index].route_current.flow_flags,
                server->session_data[session_index].route_current.private_key,
                next_packet_data,
                sizeof(next_packet_data) ) != NEXT_OK )
        {
            next_mutex_release( &server->mutex );
            return;
        }

        int next_packet_bytes = NEXT_HEADER_BYTES + packet_bytes;

        memcpy( next_packet_data + NEXT_HEADER_BYTES, packet_data, packet_bytes );

        server_send_packet( server, &server->session_data[session_index].route_current.prev_address, next_packet_data, next_packet_bytes );
    }

    next_mutex_release( &server->mutex );
}

void next_server_send_packet_to_address( next_server_t * server, next_address_t * to_address, uint8_t * packet_data, int packet_bytes )
{
    next_assert( server );
    next_assert( packet_data );
    next_assert( packet_bytes >= 0 );
    next_assert( packet_bytes <= NEXT_MTU );

    if ( !server )
        return;

    if ( !packet_data )
        return;

    if ( packet_bytes < 0 )
        return;

    if ( packet_bytes > NEXT_MTU )
    {
        next_printf( NEXT_LOG_LEVEL_WARN, "next_server_send_packet_to_address dropped packet because it's larger than MTU (%d bytes)", NEXT_MTU );
        return;
    }

    int session_index;
    uint8_t session_sequence;
    next_session_from_address( to_address, &session_index, &session_sequence );

    next_mutex_acquire( &server->mutex );
    {
        if ( session_index != -1 )
        {
            if ( server->session_data[session_index].session_sequence == session_sequence )
            {
                next_server_send_packet( server, server->session_ids[session_index], packet_data, packet_bytes );
            }
            next_mutex_release( &server->mutex );
            return;
        }

        uint8_t next_packet_data[1 + NEXT_MTU];
        next_packet_data[0] = NEXT_PACKET_TYPE_V2_DIRECT;
        memcpy( next_packet_data + 1, packet_data, packet_bytes );

        server_send_packet( server, to_address, next_packet_data, 1 + packet_bytes );
    }
    next_mutex_release( &server->mutex );
}

const uint8_t * next_server_public_key( next_server_t * server )
{
    return server->config.public_key;
}

// -----------------------------------------------------------------------------
