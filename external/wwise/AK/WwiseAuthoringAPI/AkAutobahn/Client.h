/*******************************************************************************
The content of this file includes portions of the AUDIOKINETIC Wwise Technology
released in source code form as part of the SDK installer package.

Commercial License Usage

Licensees holding valid commercial licenses to the AUDIOKINETIC Wwise Technology
may use this file in accordance with the end user license agreement provided
with the software or, alternatively, in accordance with the terms contained in a
written agreement between you and Audiokinetic Inc.

Version: v2017.2.1  Build: 6524
Copyright (c) 2006-2018 Audiokinetic Inc.
*******************************************************************************/

#pragma once

#include "AK/WwiseAuthoringAPI/AkAutobahn/autobahn.h"
#include "AK/WwiseAuthoringAPI/AkAutobahn/AkJson.h"

#include "AK/WwiseAuthoringAPI/AkAutobahn/FutureUtils.h"

#include <map>
#include <cstdint>
#include <functional>
#include <mutex>

namespace AK
{
	namespace WwiseAuthoringAPI
	{
		class session;

		class Client
		{
		private:

			// Any timeout value lower than this constant will be adjusted. We must allow a reasonnable amount of time
			// for the server to respond. A timeout of -1 is considered infinite (call will block until server has responded).
			const int MIN_TIMEOUT_MS = 100;

		public:

			typedef std::function<void(uint64_t in_subscriptionId, const JsonProvider& in_jsonProvider)> WampEventCallback;

		public:

			Client();
			virtual ~Client();
			
			bool Connect(const char* in_uri, unsigned int in_port);
			bool IsConnected() const;
			void Disconnect();

			bool Subscribe(const char* in_uri, const char* in_options, WampEventCallback in_callback, uint64_t& out_subscriptionId, std::string& out_result, int in_timeoutMs = -1);
			bool Subscribe(const char* in_uri, const AkJson& in_options, WampEventCallback in_callback, uint64_t& out_subscriptionId, AkJson& out_result, int in_timeoutMs = -1);
			
			bool Unsubscribe(const uint64_t& in_subscriptionId, std::string& out_result, int in_timeoutMs = -1);
			bool Unsubscribe(const uint64_t& in_subscriptionId, AkJson& out_result, int in_timeoutMs = -1);

			bool Call(const char* in_uri, const char* in_args, const char* in_options, std::string& out_result, int in_timeoutMs = -1);
			bool Call(const char* in_uri, const AkJson& in_args, const AkJson& in_options, AkJson& out_result, int in_timeoutMs = -1);

		protected:
			
			void Log(const char* log);

		private:
			
			bool SubscribeImpl(const char* in_uri, const AkJson& in_options, handler_t in_callback, int in_timeoutMs, uint64_t& out_subscriptionId);
			bool UnsubscribeImpl(const uint64_t& in_subscriptionId, int in_timeoutMs);
			
			template<typename T> bool GetFuture(std::future<T>& in_value, int in_timeoutMs, T& out_result)
			{
				if (in_timeoutMs > -1)
				{
					if (in_timeoutMs < MIN_TIMEOUT_MS)
					{
						in_timeoutMs = MIN_TIMEOUT_MS;
					}

					return GetFutureWithTimeout(in_timeoutMs, in_value, out_result);
				}
				else
				{
					return GetFutureBlocking(in_value, out_result);
				}
			}
			
			void ErrorToAkJson(const std::exception& in_exception, AkJson& out_result);

		private:
			
			session* m_ws;
		};
	}
}
