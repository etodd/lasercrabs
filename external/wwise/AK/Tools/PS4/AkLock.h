//////////////////////////////////////////////////////////////////////
//
// AkLock.h
//
// AudioKinetic Lock class
//
// Copyright (c) 2011 Audiokinetic Inc. / All Rights Reserved
//
//////////////////////////////////////////////////////////////////////

#pragma once

#include <AK/SoundEngine/Common/AkTypes.h>
#include <AK/Tools/Common/AkAssert.h>

//-----------------------------------------------------------------------------
// CAkLock class
//-----------------------------------------------------------------------------
class CAkLock
{
public:
    /// Constructor
	CAkLock()
    {
		/// Todo: use sceKernelCreateLwMutex when sdk is available for it
		
		ScePthreadMutexattr	mutex_attr;
		AKVERIFY(!scePthreadMutexattrInit( &mutex_attr ));	
		AKVERIFY(!scePthreadMutexattrSettype( &mutex_attr, SCE_PTHREAD_MUTEX_RECURSIVE ));
		AKVERIFY(!scePthreadMutexattrSetprotocol(&mutex_attr, SCE_PTHREAD_PRIO_INHERIT));
		AKVERIFY(!scePthreadMutexInit( &m_mutex, &mutex_attr, NULL));
		AKVERIFY(!scePthreadMutexattrDestroy( &mutex_attr ));
    }

	/// Destructor
	~CAkLock()
    {
		AKVERIFY(!scePthreadMutexDestroy( &m_mutex ));
    }

    /// Lock 
    inline AKRESULT Lock( void )
	{
		if( scePthreadMutexLock(&m_mutex) == SCE_OK )
		{
			return AK_Success;
		}
		return AK_Fail;
	}

	/// Unlock
    inline AKRESULT Unlock( void )
	{
		if( scePthreadMutexUnlock(&m_mutex) == SCE_OK )
		{
			return AK_Success;
		}
		return AK_Fail;
	}

private:
    ScePthreadMutex m_mutex;
};
