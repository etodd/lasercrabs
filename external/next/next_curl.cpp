/*
    Network Next: v2.14-667-g7e8ad02d
    Copyright © 2017 - 2018 Network Next, Inc. All rights reserved.
*/

#if _WIN32
#define NOMINMAX
#endif

#include "curl/curl.h"
#include "next_internal.h"

// platform must define this function
extern void next_platform_curl_easy_init( CURL * curl );

static void * next_calloc( size_t nmemb, size_t size )
{
    size_t s = nmemb * size;
    void * p = next_alloc( s );
    memset( p, 0, s );
    return p;
}

static char * next_strdup( const char *s )
{
    char * d = (char *)( next_alloc( strlen( s ) + 1 ) );
    if ( !d )
        return NULL;
    strcpy(d, s);
    return d;
}

bool next_curl_init()
{
    CURLcode result;
    if ( ( result = curl_global_init_mem( CURL_GLOBAL_ALL, next_alloc, next_free, next_realloc, next_strdup, next_calloc ) ) != CURLE_OK )
    {
        next_printf( NEXT_LOG_LEVEL_ERROR, "failed to initialize curl: %s", curl_easy_strerror( result ) );
        return false;
    }

    return true;
}

void next_http_term()
{
    curl_global_cleanup();
}

void next_http_create( next_http_t * context, const char * url )
{
    next_assert( url );
    next_assert( context );

    memset( context, 0, sizeof( *context ) );

    strncpy( context->url_base, url, 1023 );
}

static void http_request_cleanup( next_http_request_t * request )
{
    curl_easy_cleanup( request->easy );
    request->data.clear();
    if ( request->request_headers )
    {
        curl_slist_free_all( request->request_headers );
    }
    next_free( request );
}

void next_http_cancel_all( next_http_t * context )
{
    for ( int i = 0; i < context->active.length; i++ )
    {
        next_http_request_t * request = context->active[i];
        curl_multi_remove_handle( context->multi, request->easy );
        http_request_cleanup( request );
    }
    context->active.length = 0;
}

void next_http_destroy( next_http_t * context )
{
    if ( context->easy )
    {
        curl_easy_cleanup( context->easy );
    }

    next_http_cancel_all( context );
    context->active.clear();

    if ( context->multi )
    {
        curl_multi_cleanup( context->multi );
    }
}

struct http_response_buffer
{
    char * pointer;
    size_t available;
};

static size_t next_http_response_callback( void * contents, size_t size, size_t nmemb, void * userp )
{
    http_response_buffer * buffer = (http_response_buffer *)( userp );

    if ( buffer->available > 0 )
    {
        size_t available = size_t( buffer->available - 1 );
        size *= nmemb;
        size_t to_copy = ( size > available ) ? available : size;
        memcpy( buffer->pointer, contents, to_copy );
        buffer->available -= to_copy;
        buffer->pointer += to_copy;
        buffer->pointer[0] = '\0';
        return to_copy;
    }

    return 0;
}

static size_t next_http_response_callback_null( void * contents, size_t size, size_t nmemb, void * userp )
{
    (void) contents;
    (void) userp;
    return nmemb * size;
}

static CURL * http_easy_init()
{
    CURL * curl = curl_easy_init();
    if ( curl )
    {
        curl_easy_setopt( curl, CURLOPT_VERBOSE, ( next_log_level() >= NEXT_LOG_LEVEL_DEBUG ) ? 1L : 0L );
        curl_easy_setopt( curl, CURLOPT_SSL_VERIFYPEER, 1L );
        curl_easy_setopt( curl, CURLOPT_SSL_VERIFYHOST, 2L );
        curl_easy_setopt( curl, CURLOPT_USERAGENT, "next/1.0");
        next_platform_curl_easy_init( curl );
    }
    return curl;
}

int next_http_get( next_http_t * context, const char * path, char * response, int * response_bytes, int timeout_ms )
{
    next_assert( path );
    next_assert( response );
    next_assert( response_bytes );
    next_assert( *response_bytes > 0 );

    if ( !context->easy )
    {
        context->easy = http_easy_init();
        if ( !context->easy )
            return -1;
    }

    curl_easy_setopt( context->easy, CURLOPT_HTTPGET, 1L ); 

    char url[1024];
    snprintf( url, sizeof(url), "%s%s", context->url_base, path );

    curl_easy_setopt( context->easy, CURLOPT_URL, url );

    curl_easy_setopt( context->easy, CURLOPT_HTTPHEADER, NULL );

    curl_easy_setopt( context->easy, CURLOPT_TIMEOUT_MS, long( timeout_ms ) );

    http_response_buffer buffer;
    buffer.pointer = response;
    buffer.available = *response_bytes;

    curl_easy_setopt( context->easy, CURLOPT_WRITEDATA, (void *)( &buffer ) );
    curl_easy_setopt( context->easy, CURLOPT_WRITEFUNCTION, next_http_response_callback );

    int response_code;

    CURLcode result;
    if ( ( result = curl_easy_perform( context->easy ) ) == CURLE_OK )
    {
        long code;
        curl_easy_getinfo( context->easy, CURLINFO_RESPONSE_CODE, &code );
        response_code = int( code );
        *response_bytes = *response_bytes - int( buffer.available );
    }
    else
    {
        next_printf( NEXT_LOG_LEVEL_WARN, "http request failed: %s", curl_easy_strerror( result ) );
        response[0] = '\0';
        *response_bytes = 0;
        response_code = -1;
    }

    return response_code;
}

int next_http_post_json( next_http_t * context, const char * path, const char * body, char * response, int * response_bytes, int timeout_ms )
{
    next_assert( path );
    next_assert( response );
    next_assert( response_bytes );
    next_assert( *response_bytes > 0 );

    if ( !context->easy )
    {
        context->easy = http_easy_init();
        if ( !context->easy )
            return -1;
    }

    curl_easy_setopt( context->easy, CURLOPT_POST, 1L );
    curl_easy_setopt( context->easy, CURLOPT_POSTFIELDS, body );

    char url[1024];
    snprintf( url, 1024, "%s%s", context->url_base, path );
    curl_easy_setopt( context->easy, CURLOPT_URL, url );

    struct curl_slist * headers = NULL;

    headers = curl_slist_append( headers, "Content-Type: application/json" );
    headers = curl_slist_append( headers, "Expect:" );

    curl_easy_setopt( context->easy, CURLOPT_HTTPHEADER, headers );

    curl_easy_setopt( context->easy, CURLOPT_TIMEOUT_MS, long( timeout_ms ) );

    http_response_buffer buffer;
    if ( response )
    {
        buffer.pointer = response;
        buffer.available = *response_bytes;

        curl_easy_setopt( context->easy, CURLOPT_WRITEDATA, (void *)( &buffer ) );
        curl_easy_setopt( context->easy, CURLOPT_WRITEFUNCTION, next_http_response_callback );
    }
    else
    {
        curl_easy_setopt( context->easy, CURLOPT_WRITEDATA, NULL );
        curl_easy_setopt( context->easy, CURLOPT_WRITEFUNCTION, next_http_response_callback_null );
        memset( &buffer, 0, sizeof( buffer ) );
    }

    int response_code;

    CURLcode result;
    if ( ( result = curl_easy_perform( context->easy ) ) == CURLE_OK )
    {
        long code;
        curl_easy_getinfo( context->easy, CURLINFO_RESPONSE_CODE, &code );
        response_code = int( code );
    }
    else
    {
        response_code = -1;
    }

    curl_easy_setopt( context->easy, CURLOPT_HTTPHEADER, NULL );

    curl_slist_free_all( headers );

    if ( response_bytes )
        *response_bytes = *response_bytes - int( buffer.available );

    return response_code;
}

static size_t next_http_nonblock_response_callback( void * contents, size_t size, size_t nmemb, void * userp )
{
    next_http_request_t * request = (next_http_request_t *)( userp );
    size_t total = size * nmemb;
    int offset = request->data.length == 0 ? 0 : request->data.length - 1;
    request->data.resize( offset + int( total ) + 1 );
    memcpy( &request->data[offset], contents, total );
    request->data[request->data.length - 1] = '\0';
    return total;
}

void next_http_nonblock_get( next_http_t * context, const char * path, next_http_callback_t * callback, void * user_data, int timeout_ms )
{
    next_assert( path );

    if ( !context->multi )
    {
        context->multi = curl_multi_init();
        next_assert( context->multi );
    }

    next_http_request_t * request = (next_http_request_t *)( next_alloc( sizeof( next_http_request_t ) ) );
    next_assert( request );

    request->easy = http_easy_init();
    next_assert( request->easy );

    request->callback = callback;
    request->user_data = user_data;
    memset( &request->data, 0, sizeof( request->data ) );
    request->error[0] = '\0';
    request->request_headers = NULL;

    context->active.add( request );

    char url[1024];
    snprintf( url, sizeof(url), "%s%s", context->url_base, path );
    curl_easy_setopt( request->easy, CURLOPT_URL, url );

    curl_easy_setopt( request->easy, CURLOPT_HTTPGET, 1L ); 
    curl_easy_setopt( request->easy, CURLOPT_TIMEOUT_MS, long( timeout_ms ) );
    curl_easy_setopt( request->easy, CURLOPT_WRITEDATA, (void *)( request ) );

    if ( callback )
    {
        curl_easy_setopt( request->easy, CURLOPT_WRITEFUNCTION, next_http_nonblock_response_callback );
    }
    else
    {
        curl_easy_setopt( request->easy, CURLOPT_WRITEFUNCTION, next_http_response_callback_null );
    }

    curl_multi_add_handle( context->multi, request->easy );
}

void next_http_nonblock_post_json( next_http_t * context, const char * path, const char * body, next_http_callback_t * callback, void * user_data, int timeout_ms )
{
    next_assert( path );

    if ( !context->multi )
    {
        context->multi = curl_multi_init();
        next_assert( context->multi );
    }

    next_http_request_t * request = (next_http_request_t *)( next_alloc( sizeof( next_http_request_t ) ) );
    next_assert( request );

    request->easy = http_easy_init();
    next_assert( request->easy );

    request->callback = callback;
    request->user_data = user_data;
    memset( &request->data, 0, sizeof( request->data ) );
    request->error[0] = '\0';
    request->request_headers = NULL;
    request->request_headers = curl_slist_append( request->request_headers, "Content-Type: application/json" );
    request->request_headers = curl_slist_append( request->request_headers, "Expect:" );

    context->active.add( request );

    char url[1024];
    snprintf( url, sizeof(url), "%s%s", context->url_base, path );
    curl_easy_setopt( request->easy, CURLOPT_URL, url );

    curl_easy_setopt( request->easy, CURLOPT_POST, 1L );
    curl_easy_setopt( request->easy, CURLOPT_COPYPOSTFIELDS, body );
    curl_easy_setopt( request->easy, CURLOPT_HTTPHEADER, request->request_headers );
    curl_easy_setopt( request->easy, CURLOPT_TIMEOUT_MS, long( timeout_ms ) );
    curl_easy_setopt( request->easy, CURLOPT_WRITEDATA, (void *)( request ) );
    if ( callback )
    {
        curl_easy_setopt( request->easy, CURLOPT_WRITEFUNCTION, next_http_nonblock_response_callback );
    }
    else
    {
        curl_easy_setopt( request->easy, CURLOPT_WRITEFUNCTION, next_http_response_callback_null );
    }

    curl_multi_add_handle( context->multi, request->easy );
}

void next_http_nonblock_update( next_http_t * context )
{
    if ( !context->multi )
        return;

    int _; // never used
    curl_multi_perform(context->multi, &_);

    while ( CURLMsg * msg = curl_multi_info_read(context->multi, &_) )
    {
        if ( msg->msg == CURLMSG_DONE )
        {
            CURL * easy = msg->easy_handle;
            curl_multi_remove_handle( context->multi, easy );
            next_http_request_t * request = NULL;
            for (int i = 0; i < context->active.length; i++ )
            {
                next_http_request_t * r = context->active[i];
                if (r->easy == easy)
                {
                    request = r;
                    context->active.remove( i );
                    break;
                }
            }
            next_assert( request );

            long response_code;
            curl_easy_getinfo(request->easy, CURLINFO_RESPONSE_CODE, &response_code);
            const char* response = request->data.length > 0 ? &request->data[0] : NULL;
#if NEXT_DEBUG_HTTP
            {
                char * url = NULL;
                curl_easy_getinfo(request->easy, CURLINFO_EFFECTIVE_URL, &url);
                next_printf( NEXT_LOG_LEVEL_DEBUG, "HTTP response - %s - code %d: %s", url, response_code, response );
                if ( request->error[0] )
                    next_printf( NEXT_LOG_LEVEL_DEBUG, "HTTP error: %s", request->error );
            }
#endif // #if NEXT_DEBUG_HTTP
            if (request->callback)
            {
                if ( !request->callback(response_code, response, request->user_data) )
                {
                    // if the callback returns false, that means it deleted the HTTP context. exit immediately.
                    break;
                }
            }
            http_request_cleanup( request );
        }
    }
}
