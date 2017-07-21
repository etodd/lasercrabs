#pragma once
#include "types.h"
#include "data/array.h"

typedef void CURL;
struct curl_slist;

namespace VI
{

namespace Net
{

namespace Http
{

typedef void Callback(s32, const char*, void*);

struct Request
{
	Callback* callback;
	CURL* curl;
	curl_slist* request_headers;
	void* user_data;
	Array<char> data;

	~Request();
};

b8 init();
void get(const char*, Callback* = nullptr, const char* = nullptr, void* = nullptr);
void update();
void term();


}

}

}