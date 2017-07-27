#include "http.h"
#include "curl/curl.h"
#include "data/pin_array.h"
#include <new>

namespace VI
{

namespace Net
{

namespace Http
{


Request::~Request()
{
	if (request_headers)
		curl_slist_free_all(request_headers);
}

struct State
{
	CURLM* curl_multi;
	PinArray<Request, 1024> requests; // up to N requests active at a time
};
State state;

b8 init()
{
	if (curl_global_init(CURL_GLOBAL_SSL)) // no need for CURL_GLOBAL_WIN32 as Sock::init() initializes Win32 socket libs
		return false;

	state.curl_multi = curl_multi_init();

	if (state.curl_multi)
		return true;
	else
		return false;
}

size_t write_callback(void* data, size_t size, size_t count, void* userdata)
{
	Request* request = (Request*)userdata;
	size_t total = size * count;
	s32 offset = request->data.length;
	request->data.resize(request->data.length + s32(total));
	memcpy(&request->data[offset], data, total);
	return total;
}

void update()
{
	s32 _; // never used
	curl_multi_perform(state.curl_multi, &_);

	while (CURLMsg* msg = curl_multi_info_read(state.curl_multi, &_))
	{
		if (msg->msg == CURLMSG_DONE)
		{
			CURL* curl = msg->easy_handle;
			curl_multi_remove_handle(state.curl_multi, curl);
			Request* request = nullptr;
			for (auto i = state.requests.iterator(); !i.is_last(); i.next())
			{
				if (i.item()->curl == curl)
				{
					request = i.item();
					break;
				}
			}
			vi_assert(request);
			if (request->callback)
			{
				s32 response_code;
				curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &response_code);
				request->callback(response_code, request->data.length > 0 ? &request->data[0] : nullptr, request->user_data);
			}
			request->~Request();
			state.requests.remove(request - &state.requests[0]);
			curl_easy_cleanup(curl);
		}
	}
}

Request* get(const char* url, Callback* callback, const char* header, void* user_data)
{
	Request* request = state.requests.add();
	new (request) Request();
	request->callback = callback;
	request->user_data = user_data;

	request->curl = curl_easy_init();
	curl_easy_setopt(request->curl, CURLOPT_URL, url);
	curl_easy_setopt(request->curl, CURLOPT_NOPROGRESS, 1);
	curl_easy_setopt(request->curl, CURLOPT_WRITEFUNCTION, &write_callback);
	curl_easy_setopt(request->curl, CURLOPT_WRITEDATA, request);

	if (header)
	{
		request->request_headers = curl_slist_append(request->request_headers, header);
		curl_easy_setopt(request->curl, CURLOPT_HTTPHEADER, request->request_headers);
	}

	curl_multi_add_handle(state.curl_multi, request->curl);

	return request;
}

Request* request_for_user_data(void* user_data)
{
	for (auto i = state.requests.iterator(); !i.is_last(); i.next())
	{
		if (i.item()->user_data == user_data)
			return i.item();
	}
	return nullptr;
}

void term()
{
	for (auto i = state.requests.iterator(); !i.is_last(); i.next())
	{
		curl_multi_remove_handle(state.curl_multi, i.item()->curl);
		curl_easy_cleanup(i.item()->curl);
		i.item()->~Request();
		state.requests.remove(i.index);
	}
	curl_multi_cleanup(state.curl_multi);
	curl_global_cleanup();
}


}

}

}