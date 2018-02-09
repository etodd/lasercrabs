///////////////////////////////////////////////////////////////////////////////
//
//  Copyright (C) 2014 Tavendo GmbH
//
//  Licensed under the Apache License, Version 2.0 (the "License");
//  you may not use this file except in compliance with the License.
//  You may obtain a copy of the License at
//
//      http://www.apache.org/licenses/LICENSE-2.0
//
//  Unless required by applicable law or agreed to in writing, software
//  distributed under the License is distributed on an "AS IS" BASIS,
//  WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
//  See the License for the specific language governing permissions and
//  limitations under the License.
//
///////////////////////////////////////////////////////////////////////////////

///////////////////////////////////////////////////////////////////////////////
// This file was modified by Audiokinetic inc.
///////////////////////////////////////////////////////////////////////////////

#ifndef AUTOBAHN_H
#define AUTOBAHN_H

#include <atomic>
#include <condition_variable>
#include <cstdint>
#include <functional>
#include <future>
#include <istream>
#include <map>
#include <mutex>
#include <ostream>
#include <queue>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

#include "AK/WwiseAuthoringAPI/AkAutobahn/AkVariant.h"
#include "AK/WwiseAuthoringAPI/AkAutobahn/AkJson.h"
#include "AK/WwiseAuthoringAPI/AkAutobahn/JsonProvider.h"

#include "AK/WwiseAuthoringAPI/AkAutobahn/IWebSocketClientHandler.h"
#include "AK/WwiseAuthoringAPI/AkAutobahn/WebSocketClient.h"

// thank you microsoft
#ifdef ERROR
#undef ERROR
#endif

namespace AK
{
	namespace WwiseAuthoringAPI
	{
		typedef AkVariant any;

		/// A map holding any values and string keys.
		typedef AkJson anymap;

		/// A vector holding any values.
		typedef std::vector<any> anyvec;

		/// A pair of ::anyvec and ::anymap.
		typedef std::pair<anyvec, anymap> anyvecmap;


		/// Handler type for use with session::subscribe(const std::string&, handler_t)
		typedef std::function<void(uint64_t, const JsonProvider&)> handler_t;

		/// Represents a procedure registration.
		struct registration
		{
			registration() : id(0){};
			registration(uint64_t id) : id(id){};
			uint64_t id;
		};

		/// Represents a topic subscription.
		struct subscription
		{
			subscription() : id(0){};
			subscription(uint64_t id) : id(id){};
			uint64_t id;
		};

		/// Represents an event publication (for acknowledged publications).
		struct publication
		{
			publication() : id(0){};
			publication(uint64_t id) : id(id){};
			uint64_t id;
		};

		/// Represents the authentication information sent on welcome
		struct authinfo
		{
			std::string authmethod;
			std::string authprovider;
			std::string authid;
			std::string authrole;
		};

		struct publish_options
		{
			bool acknowledge = false;
			bool exclude_me = false;
			std::vector<int> exclude;
			std::vector<int> eligible;
			bool disclose_me = false;

			AkJson toDict() const
			{
				AkJson obj(AkJson::Type::Map);

				if (acknowledge)
				{
					obj.GetMap()["acknowledge"] = AkVariant(acknowledge);
				}

				if (exclude_me)
				{
					obj.GetMap()["exclude_me"] = AkVariant(exclude_me);
				}

				if (!exclude.empty())
				{
					AkJson jsonExclude(AkJson::Type::Array);

					for (auto value : exclude)
					{
						jsonExclude.GetArray().push_back(AkJson(AkVariant(value)));
					}

					obj.GetMap()["exclude"] = jsonExclude;
				}

				if (!eligible.empty())
				{
					AkJson jsonEligible(AkJson::Type::Array);

					for (auto value : exclude)
					{
						jsonEligible.GetArray().push_back(AkJson(AkVariant(value)));
					}

					obj.GetMap()["eligible"] = jsonEligible;
				}

				if (disclose_me)
				{
					obj.GetMap()["disclose_me"] = AkVariant(disclose_me);
				}

				return obj;
			}
		};

		/*!
		* A WAMP session.
		*/
		class session : public IWebSocketClientHandler
		{

		public:
			session();
			~session();

			/*!
			* Start listening on the IStream provided to the constructor
			* of this session.
			*/
			void start(const char* in_uri, unsigned int in_port);

			/*!
			* Closes the IStream and the OStream provided to the constructor
			* of this session.
			*/
			void stop(std::exception_ptr abortExc);

			bool isConnected() const;

			uint64_t getSessionId() const { return m_session_id; }

			/*!
			* Join a realm with this session.
			*
			* \param realm The realm to join on the WAMP router connected to.
			* \param method The method used for login. Empty string will cause no login.
			* \param authid The authid to login with.
			* \param signature The signature to use when logging in. For method "ticket" the ticket, for method "wampcra" the
			* passphrase.
			* \return A future that resolves with the session ID when the realm was joined.
			*/
			std::future<uint64_t> join(const std::string& realm, const std::string& method = "", const std::string& authid = "",
				const std::string& signature = "");

			/*!
			* Leave the realm.
			*
			* \param reason An optional WAMP URI providing a reason for leaving.
			* \return A future that resolves with the reason sent by the peer.
			*/
			std::future<std::string> leave(const std::string& reason = std::string("wamp.error.close_realm"));


			authinfo getAuthInfo() const;


			/*!
			* Publish an event with both positional and keyword payload to a topic.
			*
			* \param topic The URI of the topic to publish to.
			* \param args The positional payload for the event.
			* \param kwargs The keyword payload for the event.
			*/
			void publish(const std::string& topic, const anyvec& args = {}, const anymap& kwargs = AkJson(AkJson::Type::Map), const publish_options& options = publish_options());


			/*!
			* Subscribe a handler to a topic to receive events.
			*
			* \param topic The URI of the topic to subscribe to.
			* \param handler The handler that will receive events under the subscription.
			* \param options WAMP options for the subscription request.
			* \return A future that resolves to a autobahn::subscription
			*/
			std::future<subscription> subscribe(const std::string& topic, handler_t handler, const anymap& options = AkJson(AkJson::Type::Map));


			std::future<anymap> unsubscribe(uint64_t subscription_id);

			/*!
			* Calls a remote procedure with no arguments.
			*
			* \param procedure The URI of the remote procedure to call.
			* \return A future that resolves to the result of the remote procedure call.
			*/
			std::future<anymap> call(const std::string& procedure);

			std::future<anymap> call_options(const std::string& procedure, const anymap& options);

			/*!
			* Calls a remote procedure with positional arguments.
			*
			* \param procedure The URI of the remote procedure to call.
			* \param args The positional arguments for the call.
			* \return A future that resolves to the result of the remote procedure call.
			*/
			std::future<anymap> call(const std::string& procedure, const anyvec& args);

			std::future<anymap> call_options(const std::string& procedure, const anyvec& args, const anymap& options);

			/*!
			* Calls a remote procedure with positional and keyword arguments.
			*
			* \param procedure The URI of the remote procedure to call.
			* \param args The positional arguments for the call.
			* \param kwargs The keyword arguments for the call.
			* \return A future that resolves to the result of the remote procedure call.
			*/
			std::future<anymap> call(const std::string& procedure, const anyvec& args, const anymap& kwargs);

			std::future<anymap> call_options(const std::string& procedure, const anyvec& args, const anymap& kwargs,
				const anymap& options);

		private:

			//////////////////////////////////////////////////////////////////////////////////////
			/// Caller

			/// An outstanding WAMP call.
			struct call_t
			{
				call_t() {}
				call_t(call_t&& c) : m_res(std::move(c.m_res)) {}
				std::promise<anymap> m_res;
			};

			/// Map of outstanding WAMP calls (request ID -> call).
			typedef std::map<uint64_t, call_t> calls_t;

			/// Map of WAMP call ID -> call
			calls_t m_calls;

			std::mutex m_callsMutex;


			//////////////////////////////////////////////////////////////////////////////////////
			/// Subscriber

			/// An outstanding WAMP subscribe request.
			struct subscribe_request_t
			{
				subscribe_request_t(){};
				subscribe_request_t(subscribe_request_t&& s) : m_handler(std::move(s.m_handler)), m_res(std::move(s.m_res)) {}
				subscribe_request_t(handler_t handler) : m_handler(handler){};
				handler_t m_handler;
				std::promise<subscription> m_res;
			};

			/// Map of outstanding WAMP subscribe requests (request ID -> subscribe request).
			typedef std::map<uint64_t, subscribe_request_t> subscribe_requests_t;

			/// Map of WAMP subscribe request ID -> subscribe request
			subscribe_requests_t m_subscribe_requests;

			std::mutex m_subreqMutex;

			/// Map of subscribed handlers (subscription ID -> handler)
			typedef std::map<uint64_t, handler_t> handlers_t;

			/// Map of WAMP subscription ID -> handler
			std::mutex m_handlersMutex;
			handlers_t m_handlers;

			// No mutex required.

			//////////////////////////////////////////////////////////////////////////////////////
			/// Unsubscriber

			/// An outstanding WAMP unsubscribe request.
			struct unsubscribe_request_t
			{
				unsubscribe_request_t(){};
				unsubscribe_request_t(unsubscribe_request_t&& s) : m_res(std::move(s.m_res)) {}
				std::promise<anymap> m_res;
			};

			/// Map of outstanding WAMP subscribe requests (request ID -> subscribe request).
			typedef std::map<uint64_t, unsubscribe_request_t> unsubscribe_requests_t;

			/// Map of WAMP subscribe request ID -> subscribe request
			unsubscribe_requests_t m_unsubscribe_requests;

			std::mutex m_unsubreqMutex;


			//////////////////////////////////////////////////////////////////////////////////////
			/// Callee

			/// An outstanding WAMP register request.
			struct register_request_t
			{
				register_request_t(){};
				register_request_t(register_request_t&& r) : m_endpoint(std::move(r.m_endpoint)), m_res(std::move(r.m_res)) {}
				register_request_t(any endpoint) : m_endpoint(endpoint){};
				any m_endpoint;
				std::promise<registration> m_res;
			};

			/// Map of outstanding WAMP register requests (request ID -> register request).
			typedef std::map<uint64_t, register_request_t> register_requests_t;

			/// Map of WAMP register request ID -> register request
			register_requests_t m_register_requests;

			std::mutex m_regreqMutex;

			/// Map of registered endpoints (registration ID -> endpoint)
			typedef std::map<uint64_t, any> endpoints_t;

			/// Map of WAMP registration ID -> endpoint
			endpoints_t m_endpoints;


			/// An unserialized, raw WAMP message.
			typedef AkJson wamp_msg_t;


			/// Process a WAMP WELCOME message.
			void process_welcome(const wamp_msg_t& msg);

			/// Process a WAMP ABORT message.
			void process_abort(const wamp_msg_t& msg);

			/// Process a WAMP CHALLENGE message.
			void process_challenge(const wamp_msg_t& msg);

			/// Process a WAMP ERROR message.
			void process_error(const wamp_msg_t& msg);

			/// Process a WAMP RESULT message.
			void process_call_result(const wamp_msg_t& msg);

			/// Process a WAMP SUBSCRIBED message.
			void process_subscribed(const wamp_msg_t& msg);

			/// Process a WAMP UNSUBSCRIBED message.
			void process_unsubscribed(const wamp_msg_t& msg);

			/// Process a WAMP EVENT message.
			void process_event(const wamp_msg_t& msg);

			/// Process a WAMP REGISTERED message.
			void process_registered(const wamp_msg_t& msg);

			/// Process a WAMP INVOCATION message.
			void process_invocation(const wamp_msg_t& msg);

			/// Process a WAMP GOODBYE message.
			void process_goodbye(const wamp_msg_t& msg);


			/// Send wamp message. Asynchronous.
			void send(const AkJson& jsonPayload);
			void send(std::string ssout);

			/// Process incoming message.
			void got_msg(const std::string& jsonPayload);

			// IWebSocketClientHandler
			void OnMessage(std::string&& message);
			void OnConnectionLost();

			void sendThread();

			void logMessage(const char* logContent);

#ifdef VALIDATE_WAMP
			void WampAssert(bool value, const char* message);
#endif

			std::atomic<bool> m_running;

			std::shared_ptr<WebSocketClient> m_websocket;
			std::mutex m_websocketMutex;

			std::thread m_sendThread;

			std::mutex m_sendQueueMutex;
			std::condition_variable m_sendEvent;
			std::queue<std::shared_ptr<std::vector<char>>> m_sendQueue;

			//Poco::JSON::Parser m_parser;

			/// WAMP session ID (if the session is joined to a realm).
			uint64_t m_session_id = 0;

			/// Future to be fired when session was joined.
			std::promise<uint64_t> m_session_join;

			std::mutex m_joinMutex;

			/// Last request ID of outgoing WAMP requests.
			uint64_t m_request_id = 0;

			/// Signature to be used to authenticate
			std::string m_signature;

			/// Authentication information sent on welcome
			authinfo m_authinfo;


			bool m_goodbye_sent = false;

			std::promise<std::string> m_session_leave;

			/// WAMP message type codes.
			enum class msg_code : int
			{
				HELLO = 1,
				WELCOME = 2,
				ABORT = 3,
				CHALLENGE = 4,
				AUTHENTICATE = 5,
				GOODBYE = 6,
				HEARTBEAT = 7,
				ERROR = 8,
				PUBLISH = 16,
				PUBLISHED = 17,
				SUBSCRIBE = 32,
				SUBSCRIBED = 33,
				UNSUBSCRIBE = 34,
				UNSUBSCRIBED = 35,
				EVENT = 36,
				CALL = 48,
				CANCEL = 49,
				RESULT = 50,
				REGISTER = 64,
				REGISTERED = 65,
				UNREGISTER = 66,
				UNREGISTERED = 67,
				INVOCATION = 68,
				INTERRUPT = 69,
				YIELD = 70
			};
		};


		class protocol_error : public std::runtime_error
		{
		public:
			protocol_error(const std::string& msg) : std::runtime_error(msg){};
		};

		class no_session_error : public std::runtime_error
		{
		public:
			no_session_error() : std::runtime_error("session not joined"){};
		};

		class server_error : public std::runtime_error
		{
		public:
			server_error(const std::string& msg) : std::runtime_error(msg){};
		};

		class connection_error : public std::runtime_error
		{
		public:
			connection_error(const std::string& msg) : std::runtime_error(msg){};
		};
	}
}

#endif // AUTOBAHN_H
