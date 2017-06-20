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

  Version: v2016.2.4  Build: 6097
  Copyright (c) 2006-2017 Audiokinetic Inc.
*******************************************************************************/

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
