/*
    Network Next: v2.14-667-g7e8ad02d
    Copyright © 2017 - 2018 Network Next, Inc. All rights reserved.
*/

#include "next_internal.h"

#if NEXT_ENABLE_TESTS && !NEXT_SHARED

#include <stdio.h>
#include <stdlib.h>
#include <memory.h>
#include <time.h>

void next_check_handler( const char * condition, 
                         const char * function,
                         const char * file,
                         int line )
{
    printf( "check failed: ( %s ), function %s, file %s, line %d\n", condition, function, file, line );
    fflush( stdout );
#ifndef NDEBUG
    #if defined( __GNUC__ )
        __builtin_trap();
    #elif defined( _MSC_VER )
        __debugbreak();
    #endif
#endif
    exit( 1 );
}

#define check( condition )                                                                                  \
do                                                                                                          \
{                                                                                                           \
    if ( !(condition) )                                                                                     \
    {                                                                                                       \
        next_check_handler( #condition, (const char*) __FUNCTION__, (const char*) __FILE__, __LINE__ );     \
    }                                                                                                       \
} while(0)

struct dummy_t
{
    uint32_t id;
    char placeholder[256];
};

static void test_vector()
{
    next_vector_t<dummy_t> vector;
    check( vector.length == 0 );

    dummy_t a;
    a.id = 1337;
    vector.add( a );
    check( vector.length == 1 );
    check( vector[0].id == 1337 );

    dummy_t b;
    b.id = 0xdeadbeef;
    vector.add( b );
    check( vector.length == 2 );
    check( vector[1].id == 0xdeadbeef );

    dummy_t c;
    c.id = 0xbad;
    vector.add( c );
    check( vector.length == 3 );

    vector.remove( 0 );
    check( vector.length == 2 );
    check( vector[0].id == 0xbad );

    vector.add( a );
    check( vector.length == 3 );
    check( vector[2].id == 1337 );

    vector.remove_ordered( 0 );
    check( vector.length == 2 );
    check( vector[0].id == 0xdeadbeef );
    check( vector[1].id == 1337 );
    
    vector.clear();
    check( vector.length == 0 );

    for ( int i = 0; i < 1000; i++ )
    {
        vector.add( a );
    }
    check( vector.length == 1000 );
    check( size_t( &vector[999] ) - size_t( &vector[998] ) == sizeof(dummy_t) );
    vector.~next_vector_t();
    check( vector.length == 0 );
}

static void test_address()
{
    {
        struct next_address_t address;
        check( next_address_parse( &address, "" ) == NEXT_ERROR );
        check( next_address_parse( &address, "[" ) == NEXT_ERROR );
        check( next_address_parse( &address, "[]" ) == NEXT_ERROR );
        check( next_address_parse( &address, "[]:" ) == NEXT_ERROR );
        check( next_address_parse( &address, ":" ) == NEXT_ERROR );
#if !defined(WINVER) || WINVER > 0x502 // windows xp sucks
        check( next_address_parse( &address, "1" ) == NEXT_ERROR );
        check( next_address_parse( &address, "12" ) == NEXT_ERROR );
        check( next_address_parse( &address, "123" ) == NEXT_ERROR );
        check( next_address_parse( &address, "1234" ) == NEXT_ERROR );
#endif
        check( next_address_parse( &address, "1234.0.12313.0000" ) == NEXT_ERROR );
        check( next_address_parse( &address, "1234.0.12313.0000.0.0.0.0.0" ) == NEXT_ERROR );
        check( next_address_parse( &address, "1312313:123131:1312313:123131:1312313:123131:1312313:123131:1312313:123131:1312313:123131" ) == NEXT_ERROR );
        check( next_address_parse( &address, "." ) == NEXT_ERROR );
        check( next_address_parse( &address, ".." ) == NEXT_ERROR );
        check( next_address_parse( &address, "..." ) == NEXT_ERROR );
        check( next_address_parse( &address, "...." ) == NEXT_ERROR );
        check( next_address_parse( &address, "....." ) == NEXT_ERROR );
    }

    {
        struct next_address_t address;
        check( next_address_parse( &address, "107.77.207.77" ) == NEXT_OK );
        check( address.type == NEXT_ADDRESS_IPV4 );
        check( address.port == 0 );
        check( address.data.ipv4[0] == 107 );
        check( address.data.ipv4[1] == 77 );
        check( address.data.ipv4[2] == 207 );
        check( address.data.ipv4[3] == 77 );
    }

    {
        struct next_address_t address;
        check( next_address_parse( &address, "127.0.0.1" ) == NEXT_OK );
        check( address.type == NEXT_ADDRESS_IPV4 );
        check( address.port == 0 );
        check( address.data.ipv4[0] == 127 );
        check( address.data.ipv4[1] == 0 );
        check( address.data.ipv4[2] == 0 );
        check( address.data.ipv4[3] == 1 );
    }

    {
        struct next_address_t address;
        check( next_address_parse( &address, "107.77.207.77:40000" ) == NEXT_OK );
        check( address.type == NEXT_ADDRESS_IPV4 );
        check( address.port == 40000 );
        check( address.data.ipv4[0] == 107 );
        check( address.data.ipv4[1] == 77 );
        check( address.data.ipv4[2] == 207 );
        check( address.data.ipv4[3] == 77 );
    }

    {
        struct next_address_t address;
        check( next_address_parse( &address, "127.0.0.1:40000" ) == NEXT_OK );
        check( address.type == NEXT_ADDRESS_IPV4 );
        check( address.port == 40000 );
        check( address.data.ipv4[0] == 127 );
        check( address.data.ipv4[1] == 0 );
        check( address.data.ipv4[2] == 0 );
        check( address.data.ipv4[3] == 1 );
    }

#if NEXT_IPV6
    {
        struct next_address_t address;
        check( next_address_parse( &address, "fe80::202:b3ff:fe1e:8329" ) == NEXT_OK );
        check( address.type == NEXT_ADDRESS_IPV6 );
        check( address.port == 0 );
        check( address.data.ipv6[0] == 0xfe80 );
        check( address.data.ipv6[1] == 0x0000 );
        check( address.data.ipv6[2] == 0x0000 );
        check( address.data.ipv6[3] == 0x0000 );
        check( address.data.ipv6[4] == 0x0202 );
        check( address.data.ipv6[5] == 0xb3ff );
        check( address.data.ipv6[6] == 0xfe1e );
        check( address.data.ipv6[7] == 0x8329 );
    }

    {
        struct next_address_t address;
        check( next_address_parse( &address, "::" ) == NEXT_OK );
        check( address.type == NEXT_ADDRESS_IPV6 );
        check( address.port == 0 );
        check( address.data.ipv6[0] == 0x0000 );
        check( address.data.ipv6[1] == 0x0000 );
        check( address.data.ipv6[2] == 0x0000 );
        check( address.data.ipv6[3] == 0x0000 );
        check( address.data.ipv6[4] == 0x0000 );
        check( address.data.ipv6[5] == 0x0000 );
        check( address.data.ipv6[6] == 0x0000 );
        check( address.data.ipv6[7] == 0x0000 );
    }

    {
        struct next_address_t address;
        check( next_address_parse( &address, "::1" ) == NEXT_OK );
        check( address.type == NEXT_ADDRESS_IPV6 );
        check( address.port == 0 );
        check( address.data.ipv6[0] == 0x0000 );
        check( address.data.ipv6[1] == 0x0000 );
        check( address.data.ipv6[2] == 0x0000 );
        check( address.data.ipv6[3] == 0x0000 );
        check( address.data.ipv6[4] == 0x0000 );
        check( address.data.ipv6[5] == 0x0000 );
        check( address.data.ipv6[6] == 0x0000 );
        check( address.data.ipv6[7] == 0x0001 );
    }

    {
        struct next_address_t address;
        check( next_address_parse( &address, "[fe80::202:b3ff:fe1e:8329]:40000" ) == NEXT_OK );
        check( address.type == NEXT_ADDRESS_IPV6 );
        check( address.port == 40000 );
        check( address.data.ipv6[0] == 0xfe80 );
        check( address.data.ipv6[1] == 0x0000 );
        check( address.data.ipv6[2] == 0x0000 );
        check( address.data.ipv6[3] == 0x0000 );
        check( address.data.ipv6[4] == 0x0202 );
        check( address.data.ipv6[5] == 0xb3ff );
        check( address.data.ipv6[6] == 0xfe1e );
        check( address.data.ipv6[7] == 0x8329 );
    }

    {
        struct next_address_t address;
        check( next_address_parse( &address, "[::]:40000" ) == NEXT_OK );
        check( address.type == NEXT_ADDRESS_IPV6 );
        check( address.port == 40000 );
        check( address.data.ipv6[0] == 0x0000 );
        check( address.data.ipv6[1] == 0x0000 );
        check( address.data.ipv6[2] == 0x0000 );
        check( address.data.ipv6[3] == 0x0000 );
        check( address.data.ipv6[4] == 0x0000 );
        check( address.data.ipv6[5] == 0x0000 );
        check( address.data.ipv6[6] == 0x0000 );
        check( address.data.ipv6[7] == 0x0000 );
    }

    {
        struct next_address_t address;
        check( next_address_parse( &address, "[::1]:40000" ) == NEXT_OK );
        check( address.type == NEXT_ADDRESS_IPV6 );
        check( address.port == 40000 );
        check( address.data.ipv6[0] == 0x0000 );
        check( address.data.ipv6[1] == 0x0000 );
        check( address.data.ipv6[2] == 0x0000 );
        check( address.data.ipv6[3] == 0x0000 );
        check( address.data.ipv6[4] == 0x0000 );
        check( address.data.ipv6[5] == 0x0000 );
        check( address.data.ipv6[6] == 0x0000 );
        check( address.data.ipv6[7] == 0x0001 );
    }
#endif
}

static void test_crypto_box()
{
    #define MESSAGE (const unsigned char *) "test"
    #define MESSAGE_LEN 4
    #define CIPHERTEXT_LEN ( crypto_box_MACBYTES + MESSAGE_LEN )

    unsigned char sender_publickey[crypto_box_PUBLICKEYBYTES];
    unsigned char sender_secretkey[crypto_box_SECRETKEYBYTES];
    crypto_box_keypair( sender_publickey, sender_secretkey );

    unsigned char receiver_publickey[crypto_box_PUBLICKEYBYTES];
    unsigned char receiver_secretkey[crypto_box_SECRETKEYBYTES];
    crypto_box_keypair( receiver_publickey, receiver_secretkey );

    unsigned char nonce[crypto_box_NONCEBYTES];
    unsigned char ciphertext[CIPHERTEXT_LEN];
    randombytes_buf( nonce, sizeof nonce );
    check( crypto_box_easy( ciphertext, MESSAGE, MESSAGE_LEN, nonce, receiver_publickey, sender_secretkey ) == 0 );

    unsigned char decrypted[MESSAGE_LEN];
    check( crypto_box_open_easy( decrypted, ciphertext, CIPHERTEXT_LEN, nonce, sender_publickey, receiver_secretkey ) == 0 );

    check( memcmp( decrypted, MESSAGE, MESSAGE_LEN ) == 0 );
}

void test_replay_protection()
{
    next_replay_protection_t replay_protection;

    int i;
    for ( i = 0; i < 2; ++i )
    {
        next_replay_protection_reset( &replay_protection );

        check( replay_protection.most_recent_sequence == 0 );

        // the first time we receive packets, they should not be already received

        #define MAX_SEQUENCE ( NEXT_REPLAY_PROTECTION_BUFFER_SIZE * 4 )

        uint64_t sequence;
        for ( sequence = 0; sequence < MAX_SEQUENCE; ++sequence )
        {
            check( next_replay_protection_packet_already_received( &replay_protection, sequence ) == 0 );
        }

        // old packets outside buffer should be considered already received

        check( next_replay_protection_packet_already_received( &replay_protection, 0 ) == 1 );

        // packets received a second time should be flagged already received

        for ( sequence = MAX_SEQUENCE - 10; sequence < MAX_SEQUENCE; ++sequence )
        {
            check( next_replay_protection_packet_already_received( &replay_protection, sequence ) == 1 );
        }

        // jumping ahead to a much higher sequence should be considered not already received

        check( next_replay_protection_packet_already_received( &replay_protection, MAX_SEQUENCE + NEXT_REPLAY_PROTECTION_BUFFER_SIZE ) == 0 );

        // old packets should be considered already received

        for ( sequence = 0; sequence < MAX_SEQUENCE; ++sequence )
        {
            check( next_replay_protection_packet_already_received( &replay_protection, sequence ) == 1 );
        }
    }
}

static void test_flow_token()
{
    uint8_t buffer[NEXT_ENCRYPTED_FLOW_TOKEN_BYTES];

    next_flow_token_t input_token;
    memset( &input_token, 0, sizeof( input_token ) );

    input_token.expire_timestamp = 1234241431241LL;
    input_token.flow_id = 1234241431241LL;
    input_token.flow_version = 5;
    input_token.flow_flags = 1;
    input_token.next_address.type = NEXT_ADDRESS_IPV4;
    input_token.next_address.data.ipv4[0] = 127;
    input_token.next_address.data.ipv4[1] = 0;
    input_token.next_address.data.ipv4[2] = 0;
    input_token.next_address.data.ipv4[3] = 1;
    input_token.next_address.port = 40000;

    next_write_flow_token( &input_token, buffer, NEXT_FLOW_TOKEN_BYTES );

    unsigned char sender_public_key[NEXT_PUBLIC_KEY_BYTES];
    unsigned char sender_private_key[NEXT_PRIVATE_KEY_BYTES];
    crypto_box_keypair( sender_public_key, sender_private_key );

    unsigned char receiver_public_key[NEXT_PUBLIC_KEY_BYTES];
    unsigned char receiver_private_key[NEXT_PRIVATE_KEY_BYTES];
    crypto_box_keypair( receiver_public_key, receiver_private_key );

    unsigned char nonce[crypto_box_NONCEBYTES];
    next_random_bytes( nonce, crypto_box_NONCEBYTES );

    check( next_encrypt_flow_token( sender_private_key, receiver_public_key, nonce, buffer, sizeof( buffer ) ) == NEXT_OK );

    check( next_decrypt_flow_token( sender_public_key, receiver_private_key, nonce, buffer ) == NEXT_OK );

    next_flow_token_t output_token;

    next_read_flow_token( &output_token, buffer );

    check( input_token.expire_timestamp == output_token.expire_timestamp );
    check( input_token.flow_id == output_token.flow_id );
    check( input_token.flow_version == output_token.flow_version );
    check( input_token.flow_flags == output_token.flow_flags );
    check( input_token.kbps_up == output_token.kbps_up );
    check( input_token.kbps_down == output_token.kbps_down );
    check( memcmp( input_token.private_key, output_token.private_key, NEXT_SYMMETRIC_KEY_BYTES ) == 0 );
    check( next_address_equal( &input_token.next_address, &output_token.next_address ) == 1 );

    uint8_t * p = buffer;

    check( next_write_encrypted_flow_token( &p, &input_token, sender_private_key, receiver_public_key ) );

    p = buffer;

    check( next_read_encrypted_flow_token( &p, &output_token, sender_public_key, receiver_private_key ) );

    check( input_token.expire_timestamp == output_token.expire_timestamp );
    check( input_token.flow_id == output_token.flow_id );
    check( input_token.flow_version == output_token.flow_version );
    check( input_token.flow_flags == output_token.flow_flags );
    check( input_token.kbps_up == output_token.kbps_up );
    check( input_token.kbps_down == output_token.kbps_down );
    check( memcmp( input_token.private_key, output_token.private_key, NEXT_SYMMETRIC_KEY_BYTES ) == 0 );
    check( next_address_equal( &input_token.next_address, &output_token.next_address ) == 1 );
}

static void test_continue_token()
{
    uint8_t buffer[NEXT_ENCRYPTED_CONTINUE_TOKEN_BYTES];

    next_continue_token_t input_token;
    memset( &input_token, 0, sizeof( input_token ) );

    input_token.expire_timestamp = 1234241431241LL;
    input_token.flow_id = 1234241431241LL;
    input_token.flow_version = 5;
    input_token.flow_flags = 1;

    next_write_continue_token( &input_token, buffer, NEXT_CONTINUE_TOKEN_BYTES );

    unsigned char sender_public_key[NEXT_PUBLIC_KEY_BYTES];
    unsigned char sender_private_key[NEXT_PRIVATE_KEY_BYTES];
    crypto_box_keypair( sender_public_key, sender_private_key );

    unsigned char receiver_public_key[NEXT_PUBLIC_KEY_BYTES];
    unsigned char receiver_private_key[NEXT_PRIVATE_KEY_BYTES];
    crypto_box_keypair( receiver_public_key, receiver_private_key );

    unsigned char nonce[crypto_box_NONCEBYTES];
    next_random_bytes( nonce, crypto_box_NONCEBYTES );

    check( next_encrypt_continue_token( sender_private_key, receiver_public_key, nonce, buffer, sizeof( buffer ) ) == NEXT_OK );

    check( next_decrypt_continue_token( sender_public_key, receiver_private_key, nonce, buffer ) == NEXT_OK );

    next_continue_token_t output_token;

    next_read_continue_token( &output_token, buffer );

    check( input_token.expire_timestamp == output_token.expire_timestamp );
    check( input_token.flow_id == output_token.flow_id );
    check( input_token.flow_version == output_token.flow_version );
    check( input_token.flow_flags == output_token.flow_flags );

    uint8_t * p = buffer;

    check( next_write_encrypted_continue_token( &p, &input_token, sender_private_key, receiver_public_key ) );

    p = buffer;

    check( next_read_encrypted_continue_token( &p, &output_token, sender_public_key, receiver_private_key ) );

    check( input_token.expire_timestamp == output_token.expire_timestamp );
    check( input_token.flow_id == output_token.flow_id );
    check( input_token.flow_flags == output_token.flow_flags );
}

static void test_server_token()
{
    uint8_t buffer[NEXT_ENCRYPTED_SERVER_TOKEN_BYTES];

    next_server_token_t input_token;
    memset( &input_token, 0, sizeof( input_token ) );

    input_token.expire_timestamp = 1234241431241LL;
    input_token.flow_id = 1234241431241LL;
    input_token.flow_version = 5;
    input_token.flow_flags = 1;

    next_write_server_token( &input_token, buffer, NEXT_SERVER_TOKEN_BYTES );

    unsigned char sender_public_key[NEXT_PUBLIC_KEY_BYTES];
    unsigned char sender_private_key[NEXT_PRIVATE_KEY_BYTES];
    crypto_box_keypair( sender_public_key, sender_private_key );

    unsigned char receiver_public_key[NEXT_PUBLIC_KEY_BYTES];
    unsigned char receiver_private_key[NEXT_PRIVATE_KEY_BYTES];
    crypto_box_keypair( receiver_public_key, receiver_private_key );

    unsigned char nonce[crypto_box_NONCEBYTES];
    next_random_bytes( nonce, crypto_box_NONCEBYTES );

    check( next_encrypt_server_token( sender_private_key, receiver_public_key, nonce, buffer, sizeof( buffer ) ) == NEXT_OK );

    check( next_decrypt_server_token( sender_public_key, receiver_private_key, nonce, buffer ) == NEXT_OK );

    next_server_token_t output_token;

    next_read_server_token( &output_token, buffer );

    check( input_token.expire_timestamp == output_token.expire_timestamp );
    check( input_token.flow_id == output_token.flow_id );
    check( input_token.flow_version == output_token.flow_version );
    check( input_token.flow_flags == output_token.flow_flags );

    uint8_t * p = buffer;

    check( next_write_encrypted_server_token( &p, &input_token, sender_private_key, receiver_public_key ) );

    p = buffer;

    check( next_read_encrypted_server_token( &p, &output_token, sender_public_key, receiver_private_key ) );

    check( input_token.expire_timestamp == output_token.expire_timestamp );
    check( input_token.flow_id == output_token.flow_id );
    check( input_token.flow_version == output_token.flow_version );
    check( input_token.flow_flags == output_token.flow_flags );
}

void test_header()
{
    uint8_t private_key[NEXT_SYMMETRIC_KEY_BYTES];
    next_random_bytes( private_key, NEXT_SYMMETRIC_KEY_BYTES );

    uint8_t buffer[NEXT_MTU];

    uint64_t sequence = 123123130131LL;
    uint64_t flow_id = 0x12313131;
    uint8_t flow_version = 0x12;
    uint8_t flow_flags = 0x1;

    check( next_write_header( NEXT_PACKET_TYPE_V2_ROUTE_RESPONSE, sequence, flow_id, flow_version, flow_flags, private_key, buffer, sizeof( buffer ) ) == NEXT_OK );

    uint8_t read_type = 0;
    uint64_t read_sequence = 0;
    uint64_t read_flow_id = 0;
    uint8_t read_flow_version = 0;
    uint8_t read_flow_flags = 0;

    check( next_peek_header( &read_type, &read_sequence, &read_flow_id, &read_flow_version, &read_flow_flags, buffer, sizeof( buffer ) ) == NEXT_OK );

    read_type = 0;
    read_sequence = 0;
    read_flow_id = 0;
    read_flow_version = 0;
    read_flow_flags = 0;

    check( next_read_header( &read_type, &read_sequence, &read_flow_id, &read_flow_version, &read_flow_flags, private_key, buffer, sizeof( buffer ) ) == NEXT_OK );

    check( read_type == NEXT_PACKET_TYPE_V2_ROUTE_RESPONSE );
    check( read_sequence == sequence );
    check( read_flow_id == flow_id );
    check( read_flow_version == flow_version );
    check( read_flow_flags == flow_flags );
}

void test_client()
{
    next_client_config_t config;
    memset( &config, 0, sizeof(next_client_config_t) );
    next_client_t * client = next_client_create( &config );
    check( client );
    next_client_destroy( client );
}

void test_server()
{
    next_server_config_t config;
    memset( &config, 0, sizeof(next_server_config_t) );
    next_server_t * server = next_server_create( &config, "0.0.0.0:50000" );
    check( server );
    for ( int i = 0; i < 100; ++i )
    {
        next_server_update( server );
    }
    next_server_destroy( server );
}

void test_direct_flow_id()
{
    struct next_address_t address;
    check( next_address_parse( &address, "107.77.207.77:16000" ) == NEXT_OK );

    uint64_t flow_id = next_direct_address_to_flow_id( &address );

    check( flow_id != 0 );

    next_address_t address_out;
    next_direct_address_from_flow_id( flow_id, &address_out );

    check( next_address_equal( &address, &address_out ) );
}

void test_session_address()
{
    int session_index = 10000;
    uint8_t session_sequence = 10;

    next_address_t session_address;
    next_session_to_address( session_index, session_sequence, &session_address );

    int session_index_out;
    uint8_t session_sequence_out;
    next_session_from_address( &session_address, &session_index_out, &session_sequence_out );

    check( session_index_out == session_index );
    check( session_sequence_out == session_sequence );
}

#define RUN_TEST( test_function )                                           \
    do                                                                      \
    {                                                                       \
        fflush( stdout );                                                   \
        printf( "    " #test_function "\n" );                               \
        test_function();                                                    \
    }                                                                       \
    while (0)

void next_test()
{
    //while ( 1 )
    {
        RUN_TEST( test_vector );
        RUN_TEST( test_address );
        RUN_TEST( test_crypto_box );
        RUN_TEST( test_replay_protection );
        RUN_TEST( test_flow_token );
        RUN_TEST( test_continue_token );
        RUN_TEST( test_server_token );
        RUN_TEST( test_header );
        RUN_TEST( test_client );
        RUN_TEST( test_server );
        RUN_TEST( test_direct_flow_id );
        RUN_TEST( test_session_address );
    }
}

#endif // #if NEXT_ENABLE_TESTS
