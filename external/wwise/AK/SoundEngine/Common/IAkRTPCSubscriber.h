//////////////////////////////////////////////////////////////////////
//
// Copyright (c) 2006 Audiokinetic Inc. / All Rights Reserved
//
//////////////////////////////////////////////////////////////////////

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

