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

// AkSmartPtr.h

/// \file 
/// Helper file.

#ifndef _AK_SMARTPTR_H
#define _AK_SMARTPTR_H

#include <AK/SoundEngine/Common/AkTypes.h>

template <class T> class CAkSmartPtr
{
public:
	/// Smart pointer constructor
	AkForceInline CAkSmartPtr()
		: m_pT( NULL )
	{
	}

	/// Smart pointer constructor
    AkForceInline CAkSmartPtr( T* in_pT )
    {
        m_pT = in_pT;
        if (m_pT)
            m_pT->AddRef();
    }

	/// Smart pointer constructor
    AkForceInline CAkSmartPtr( const CAkSmartPtr<T>& in_rPtr )
    {
        m_pT = in_rPtr.m_pT;
        if (m_pT)
            m_pT->AddRef();
    }

	/// Smart pointer destructor
    ~CAkSmartPtr()
    {
        Release();
    }

	/// Release
    AkForceInline void Release()
    {
        if( m_pT )
		{
			m_pT->Release();
			m_pT = NULL;
		}
    }

    /// Assign with no Addref.
    AkForceInline void Attach( T* in_pObj )
    {
        _Assign( in_pObj, false );   
    }

    /// Give the pointer without releasing it.
    AkForceInline T* Detach()
    {
        T* pObj = m_pT;
        m_pT = NULL;

        return pObj;
    }

	/// Assignation operator
	const CAkSmartPtr<T>& operator=( const CAkSmartPtr<T>& in_pObj )
	{
        _Assign( in_pObj.m_pT );
        return *this;
	}

	/// Assignation operator
	const CAkSmartPtr<T>& operator=( T* in_pObj )
	{
        _Assign( in_pObj );
        return *this;
	}

	/// Operator *
    T& operator*() { return *m_pT; }

	/// Operator ->
    T* operator->() const { return m_pT; }

	/// Operator
	operator T*() const { return m_pT; }

	/// Operators to pass to functions like QueryInterface and other functions returning an addref'd pointer.
	T** operator &() { return &m_pT; }

	/// Operator *
    const T& operator*() const { return *m_pT; }

	/// Cast
	T* Cast() { return m_pT; }

	/// Cast
	const T* Cast() const { return m_pT; }

protected:

	/// internal use only
    void _Assign( T* in_pObj, bool in_bAddRef = true )
    {
	    if (in_pObj != NULL && in_bAddRef)
		    in_pObj->AddRef();

		// Must use a local pointer since we cannot call Release(); without having set the m_pT to its final value.
		T* l_Ptr = m_pT;
		m_pT = in_pObj;
		if (l_Ptr)
            l_Ptr->Release();
    }

	/// internal use only
    bool _Compare( const T* in_pObj ) const
    {
        return m_pT == in_pObj;
    }

	/// internal use only
    T* m_pT; 
};

#endif // _AK_SMARTPTR_H

