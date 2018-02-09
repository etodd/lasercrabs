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

#include <string>
#include <cassert>

extern "C" {
    struct mg_connection;
}

namespace AK
{
	namespace WwiseAuthoringAPI
	{
		class IWebSocketClientHandler;

		class WebSocketClient final
		{
		public:

			WebSocketClient(const WebSocketClient&) = delete;
			WebSocketClient(WebSocketClient&&) = delete;

			WebSocketClient(IWebSocketClientHandler* handler)
				: m_handler(handler)
			{
				assert(handler != nullptr);
			}

			~WebSocketClient()
			{
				// Must wait for the connection close to be handled before being able to delete the object, otherwise
				// we could have a dangling callback in the lib that would call us from another thread
				// on an already deleted object.
			}

			void Connect(const char* host, const int port);
			void Close();

			void SendUTF8(const std::string& message);

			bool IsConnected()
			{
				return (m_connection != nullptr);
			}

		private:

			static bool m_isNetworkInitialized;

			static inline void EnsureNetworkInit()
			{
				if (!m_isNetworkInitialized)
				{
					InitializeNetwork();
					m_isNetworkInitialized = true;
				}
			}

			static void InitializeNetwork();

			static int OnMessage(
				mg_connection *conn,
				int flags,
				char* data,
				size_t data_len,
				void* user_data);

			static void OnClose(
				const mg_connection *conn,
				void *user_data);

			mg_connection* m_connection = nullptr;
			IWebSocketClientHandler* m_handler;
		};
	}
}
