/*******************************************************************************
The content of this file includes portions of the AUDIOKINETIC Wwise Technology
released in source code form as part of the SDK installer package.

Commercial License Usage

Licensees holding valid commercial licenses to the AUDIOKINETIC Wwise Technology
may use this file in accordance with the end user license agreement provided 
with the software or, alternatively, in accordance with the terms contained in a
written agreement between you and Audiokinetic Inc.

Apache License Usage

Alternatively, this file may be used under the Apache License, Version 2.0 (the 
"Apache License"); you may not use this file except in compliance with the 
Apache License. You may obtain a copy of the Apache License at 
http://www.apache.org/licenses/LICENSE-2.0.

Unless required by applicable law or agreed to in writing, software distributed
under the Apache License is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES
OR CONDITIONS OF ANY KIND, either express or implied. See the Apache License for
the specific language governing permissions and limitations under the License.

  Version: v2017.1.0  Build: 6302
  Copyright (c) 2006-2017 Audiokinetic Inc.
*******************************************************************************/

// IAkRTPCSubscriber.h

/// \file 
/// RTPC Subscriber interface.

#ifndef _IAK_RTPC_SUBSCRIBER_H_
#define _IAK_RTPC_SUBSCRIBER_H_

#include <AK/SoundEngine/Common/AkTypes.h>

namespace AK
{   
	/// Real-Time Parameter Control Subscriber interface.
	/// This interface must be implemented by every AK::IAkPluginParam implementation, allowing
	/// real-time editing with Wwise and in-game RTPC control.
	/// \warning The functions in this interface are not thread-safe, unless stated otherwise.
	/// \sa
	/// - AK::IAkPluginParam
	class IAkRTPCSubscriber
	{
	protected:
		/// Virtual destructor on interface to avoid warnings.
		virtual ~IAkRTPCSubscriber(){}

	public:
		/// This function will be called to notify the subscriber every time a selected value is entered or modified
		/// \return AK_Success if successful, AK_Fail otherwise
		virtual AKRESULT SetParam(
			AkPluginParamID in_paramID,         ///< Plug-in parameter ID
			const void *	in_pParam,          ///< Parameter value pointer
			AkUInt32		in_uParamSize		///< Parameter size
			) = 0;
	};
}

#endif //_IAK_RTPC_SUBSCRIBER_H_

