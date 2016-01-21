//////////////////////////////////////////////////////////////////////
//
// Copyright (c) 2006 Audiokinetic Inc. / All Rights Reserved
//
//////////////////////////////////////////////////////////////////////

// AkSyncLoader.h

/// \file 
/// Class for synchronous calls of asynchronous models

#ifndef _AK_SYNC_CALLER_H_
#define _AK_SYNC_CALLER_H_

#include <AK/Tools/Common/AkPlatformFuncs.h>

namespace AK
{
	namespace SoundEngine
	{
		/// AkSyncLoader: Init to create a sync event, call the asynchronous method, passing
		/// it the address of this object as the cookie, then call Wait. 
		class AkSyncCaller
		{
		public:
			/// Initialize.
			AKRESULT Init()
			{
				if ( AKPLATFORM::AkCreateEvent( m_hEvent ) != AK_Success )
				{
					AKASSERT( !"Could not create synchronization event" );
					return AK_Fail;
				}
				return AK_Success;
			}

			/// Wait until the async function calls its callback.
			AKRESULT Wait( AKRESULT in_eResult )
			{
				if ( in_eResult != AK_Success )
				{
					AKPLATFORM::AkDestroyEvent( m_hEvent );
					return in_eResult;
				}

				// Task queueing successful. Block until completion.
				AKPLATFORM::AkWaitForEvent( m_hEvent );
				AKPLATFORM::AkDestroyEvent( m_hEvent );

				return m_eResult;
			}

			/// Call this from callback to release blocked thread.
			inline void Done() { AKPLATFORM::AkSignalEvent( m_hEvent ); }

			AKRESULT	m_eResult;	///< Operation result

		private:
			AkEvent		m_hEvent;	///< Sync event
		};
	}
}

#endif // _AK_SYNC_CALLER_H_
