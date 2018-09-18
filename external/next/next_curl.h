/*
    Network Next: v2.14-667-g7e8ad02d
    Copyright © 2017 - 2018 Network Next, Inc. All rights reserved.
*/

typedef void CURL;
typedef void CURLM;
struct curl_slist;

struct next_http_request_t
{
    next_http_callback_t * callback;
    CURL * easy;
    void * user_data;
    curl_slist * request_headers;
    next_vector_t<char> data;
    char error[256];
};

struct next_http_t
{
    CURLM * multi;
    CURL * easy;
    next_vector_t<next_http_request_t*> active;
    char url_base[1024];
};

extern bool next_curl_init();