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
		pthread_mutexattr_t	mutex_attr;
		AKVERIFY(!pthread_mutexattr_init( &mutex_attr ));
		AKVERIFY(!pthread_mutexattr_settype( &mutex_attr, PTHREAD_MUTEX_RECURSIVE ));
		AKVERIFY(!pthread_mutex_init( &m_mutex, &mutex_attr));
		AKVERIFY(!pthread_mutexattr_destroy( &mutex_attr ));
    }

	/// Destructor
	~CAkLock()
    {
		AKVERIFY(!pthread_mutex_destroy( &m_mutex ));
    }

    /// Lock 
    inline AKRESULT Lock( void )
	{
		if( !pthread_mutex_lock(&m_mutex) )
		{
			return AK_Success;
		}
		return AK_Fail;
	}

	/// Unlock
    inline AKRESULT Unlock( void )
	{
		if( !pthread_mutex_unlock(&m_mutex) )
		{
			return AK_Success;
		}
		return AK_Fail;
	}

private:
    pthread_mutex_t m_mutex;
};
