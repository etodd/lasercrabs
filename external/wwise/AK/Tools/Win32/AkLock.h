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
#ifdef AK_USE_UWP_API
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
