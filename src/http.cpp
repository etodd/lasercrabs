#include "http.h"
#include "curl/curl.h"
#include "data/pin_array.h"
#include <new>
#include "settings.h"

namespace VI
{

namespace Net
{

namespace Http
{


#define DEBUG_HTTP 0

char ca_path[MAX_PATH_LENGTH + 1];

Request::~Request()
{
	if (request_headers)
		curl_slist_free_all(request_headers);
}

struct SmtpRequest
{
	CURL* curl;
	curl_slist* recipients;
	Array<char> data;
	s32 data_index;

	~SmtpRequest()
	{
		if (recipients)
			curl_slist_free_all(recipients);
	}
};

struct State
{
	CURLM* curl_multi;
	PinArray<Request, 1024> requests; // up to N requests active at a time
	Array<SmtpRequest> smtp_requests;
};
State state;

void init()
{
	if (curl_global_init(CURL_GLOBAL_SSL)) // no need for CURL_GLOBAL_WIN32 as Sock::init() initializes Win32 socket libs
		vi_assert(false);

	state.curl_multi = curl_multi_init();

	if (!state.curl_multi)
		vi_assert(false);
}

size_t write_callback(void* data, size_t size, size_t count, void* user_data)
{
	Request* request = (Request*)(user_data);
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

			if (request) // it's an HTTP request
			{
				s32 response_code;
				curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &response_code);
				const char* response = request->data.length > 0 ? &request->data[0] : nullptr;
#if DEBUG_HTTP
				{
					char *url = NULL;
					curl_easy_getinfo(curl, CURLINFO_EFFECTIVE_URL, &url);
					vi_debug("HTTP response - %s - code %d: %s", url, response_code, response);
					if (request->error[0])
						vi_debug("HTTP error: %s", request->error);
				}
#endif
				if (request->callback)
					request->callback(response_code, response, request->user_data);
				request->~Request();
				state.requests.remove(s32(request - &state.requests[0]));
			}
			else
			{
				// it's an SMTP request
				SmtpRequest* request = nullptr;
				for (s32 i = 0; i < state.smtp_requests.length; i++)
				{
					SmtpRequest* r = &state.smtp_requests[i];
					if (curl == r->curl)
					{
						request = r;
						break;
					}
				}
				vi_assert(request);

				request->~SmtpRequest();
				state.smtp_requests.remove(s32(request - &state.smtp_requests[0]));
			}
			curl_easy_cleanup(curl);
		}
	}
}

Request* get_headers(const char* url, Callback* callback, struct curl_slist* headers, u64 user_data)
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
	curl_easy_setopt(request->curl, CURLOPT_ERRORBUFFER, request->error);
	if (ca_path[0])
		curl_easy_setopt(request->curl, CURLOPT_CAPATH, ca_path);

	if (headers)
	{
		request->request_headers = headers;
		curl_easy_setopt(request->curl, CURLOPT_HTTPHEADER, headers);
	}

#if DEBUG_HTTP
	curl_easy_setopt(request->curl, CURLOPT_VERBOSE, 1L);
	{
		struct curl_slist* header = headers;
		vi_debug("HTTP GET: %s", url);
		while (header)
		{
			vi_debug("\t%s", header->data);
			header = header->next;
		}
	}
#endif

	curl_multi_add_handle(state.curl_multi, request->curl);

	return request;
}

Request* add(CURL* curl, Callback* callback, u64 user_data)
{
	Request* request = state.requests.add();
	new (request) Request();
	request->callback = callback;
	request->user_data = user_data;

	request->curl = curl;
	curl_easy_setopt(request->curl, CURLOPT_NOPROGRESS, 1);
	curl_easy_setopt(request->curl, CURLOPT_ERRORBUFFER, request->error);
	if (ca_path[0])
		curl_easy_setopt(request->curl, CURLOPT_CAPATH, ca_path);

#if DEBUG_HTTP
	curl_easy_setopt(request->curl, CURLOPT_VERBOSE, 1L);
#endif

	curl_multi_add_handle(state.curl_multi, request->curl);

	return request;
}

Request* get(const char* url, Callback* callback, const char* header, u64 user_data)
{
	struct curl_slist* headers = nullptr;
	if (header)
		headers = curl_slist_append(nullptr, header);

	return get_headers(url, callback, headers, user_data);
}

static size_t smtp_read(void* buffer, size_t element_size, size_t element_count, void* userdata)
{
	SmtpRequest* request = (SmtpRequest*)userdata;

	size_t buffer_size = element_size * element_count;
	if (buffer_size < 1)
		return 0;

	size_t data_size = vi_min(buffer_size, size_t(request->data.length - request->data_index));
	if (data_size > 0)
		memcpy(buffer, &request->data[request->data_index], data_size);
	request->data_index += s32(data_size);

	return data_size;
}

void smtp(const char* from, const char* to, const char* subject, const char* body)
{
	SmtpRequest* request = state.smtp_requests.add();
	new (request) SmtpRequest();
	request->curl = curl_easy_init();
	curl_easy_setopt(request->curl, CURLOPT_URL, "smtp://localhost");
	curl_easy_setopt(request->curl, CURLOPT_MAIL_FROM, from);
	request->recipients = curl_slist_append(request->recipients, to);
	curl_easy_setopt(request->curl, CURLOPT_MAIL_RCPT, request->recipients);
	curl_easy_setopt(request->curl, CURLOPT_READFUNCTION, smtp_read);
	curl_easy_setopt(request->curl, CURLOPT_READDATA, request);
	curl_easy_setopt(request->curl, CURLOPT_UPLOAD, 1L);

	static const char* format =
		"To: %s\r\n"
		"From: %s\r\n"
		"Subject: %s\r\n"
		"\r\n" // empty line to divide headers from body, see RFC5322
		"%s";

#if DEBUG_HTTP
	curl_easy_setopt(request->curl, CURLOPT_VERBOSE, 1L);
#endif
	
	request->data.resize(s32(strlen(format) + strlen(to) + strlen(from) + strlen(subject) + strlen(body) + 1));
	sprintf(request->data.data, format, to, from, subject, body);

	curl_multi_add_handle(state.curl_multi, request->curl);
}

Request* request_for_user_data(u64 user_data)
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
	for (s32 i = 0; i < state.smtp_requests.length; i++)
	{
		SmtpRequest* r = &state.smtp_requests[i];
		curl_multi_remove_handle(state.curl_multi, r->curl);
		curl_easy_cleanup(r->curl);
		r->~SmtpRequest();
	}
	state.smtp_requests.length = 0;
	curl_multi_cleanup(state.curl_multi);
	curl_global_cleanup();
}


}

}

}
