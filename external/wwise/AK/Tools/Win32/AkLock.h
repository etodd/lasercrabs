//////////////////////////////////////////////////////////////////////
//
// AkLock.h
//
// AudioKinetic Lock class
//
// Copyright (c) 2006 Audiokinetic Inc. / All Rights Reserved
//
//////////////////////////////////////////////////////////////////////

#ifndef _AKLOCK_H_
#define _AKLOCK_H_

#include <AK/SoundEngine/Common/AkTypes.h>
#include <windows.h>	//For CRITICAL_SECTION

//-----------------------------------------------------------------------------
// CAkLock class
//-----------------------------------------------------------------------------
class CAkLock
{
public:
    /// Constructor
	CAkLock() 
    {
#ifdef AK_USE_METRO_API
        ::InitializeCriticalSectionEx( &m_csLock, 0, 0 );
#else
        ::InitializeCriticalSection( &m_csLock );
#endif
    }

	/// Destructor
	~CAkLock()
    {
        ::DeleteCriticalSection( &m_csLock );
    }

    /// Lock 
    inline AKRESULT Lock( void )
	{
	    ::EnterCriticalSection( &m_csLock );
		return AK_Success;
	}

	/// Unlock
    inline AKRESULT Unlock( void )
	{
	    ::LeaveCriticalSection( &m_csLock );
		return AK_Success;
	}

/*  // Returns AK_Success if lock aquired, AK_Fail otherwise.
	inline AKRESULT Trylock( void )
    {
        if ( ::TryEnterCriticalSection( &m_csLock ) )
            return AK_Success;
        return AK_Fail;
    } */

private:
    CRITICAL_SECTION  m_csLock; ///< Platform specific lock
};

#endif // _AKLOCK_H_
