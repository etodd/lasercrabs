//////////////////////////////////////////////////////////////////////
//
// AkAutoLock.h
//
// AudioKinetic Lock base class.
//
// Copyright (c) 2006 Audiokinetic Inc. / All Rights Reserved
//
//////////////////////////////////////////////////////////////////////
#ifndef _AUTO_LOCK_H_
#define _AUTO_LOCK_H_

#include <AK/SoundEngine/Common/AkTypes.h>

template< class TLock >
class AkAutoLock
{
public:
	/// Constructor
	AkForceInline AkAutoLock( TLock& in_rLock )
		: m_rLock( in_rLock )
	{
		m_rLock.Lock();
	}

	/// Destructor
	AkForceInline ~AkAutoLock()
	{
		m_rLock.Unlock();
	}

private:
	AkAutoLock& operator=(AkAutoLock&);
	TLock& m_rLock;
};

#endif //_AUTO_LOCK_H_
