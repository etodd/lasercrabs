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

extern char ca_path[MAX_PATH_LENGTH + 1];

struct Request
{
	Callback* callback;
	CURL* curl;
	curl_slist* request_headers;
	void* user_data;
	Array<char> data;
	char error[256];

	~Request();
};

b8 init();
Request* get(const char*, Callback* = nullptr, const char* = nullptr, void* = nullptr);
Request* request_for_user_data(void*);
void update();
void term();


}

}

}
