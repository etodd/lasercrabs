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

#include <cstdint>
#include <future>

namespace AK
{
	namespace WwiseAuthoringAPI
	{
		template<typename T> bool GetFutureWithTimeout(int timeoutMs, std::future<T>& value, T& out_result)
		{
			if (value.wait_for(std::chrono::milliseconds(timeoutMs)) != std::future_status::ready)
			{
				return false;
			}

			out_result = value.get();
			return true;
		}

		template<typename T> bool GetFutureBlocking(std::future<T>& value, T& out_result)
		{
			out_result = value.get();
			return true;
		}
	}
}
