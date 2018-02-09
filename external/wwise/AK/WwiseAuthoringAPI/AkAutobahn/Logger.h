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

namespace AK
{
	namespace WwiseAuthoringAPI
	{
		class Logger
		{
		public:

			typedef void(*LoggerFunction)(const char* logger);

			Logger()
				: m_callback(nullptr)
			{
			}

			void SetLoggerFunction(LoggerFunction callback)
			{
				m_callback = callback;
			}

			void LogMessage(const char* origin, const char* message)
			{
				if (m_callback != nullptr)
				{
					std::string result = origin;
					result += ": ";
					result += message;

					m_callback(result.c_str());
				}
			}

			static Logger* Get()
			{
				return &m_instance;
			}

		private:

			static Logger m_instance;
			LoggerFunction m_callback;
		};
	}
}
