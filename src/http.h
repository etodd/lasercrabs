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

typedef void Callback(s32, const char*, u64);

extern char ca_path[MAX_PATH_LENGTH + 1];

struct Request
{
	Callback* callback;
	CURL* curl;
	curl_slist* request_headers;
	u64 user_data;
	Array<char> data;
	char error[256];

	~Request();
};

void init();
Request* get(const char*, Callback* = nullptr, const char* = nullptr, u64 = 0);
Request* request_for_user_data(u64);
void smtp(const char*, const char*, const char*, const char*);
void update();
void term();


}

}

}
