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

#ifndef _AK_OBJECT_H_
#define _AK_OBJECT_H_

#include <AK/SoundEngine/Common/AkMemoryMgr.h>

extern AKSOUNDENGINE_API AkMemPoolId g_DefaultPoolId;
extern AKSOUNDENGINE_API AkMemPoolId g_LEngineDefaultPoolId;

//-----------------------------------------------------------------------------
// Placement New definition. Use like this:
// AkPlacementNew( memorybuffer ) T(); // where T is your type constructor
//-----------------------------------------------------------------------------

/// Unique structure identifier for AkPlacementNew. 
struct AkPlacementNewKey 
{ 
	/// ctor 
	AkForceInline AkPlacementNewKey(){} 
};

AkForceInline void * operator new( size_t /*size*/, void * memory, const AkPlacementNewKey & /*key*/ ) throw()
{
      return memory;
}

#define AkPlacementNew(_memory) ::new( _memory, AkPlacementNewKey() )

// Matching operator delete for AK placement new. This needs to be defined to avoid compiler warnings
// with projects built with exceptions enabled.
AkForceInline void operator delete( void *, void *, const AkPlacementNewKey & ) throw() {}

//-----------------------------------------------------------------------------
// Macros
//-----------------------------------------------------------------------------

/// Unique structure identifier for AkNew. 
struct AkPoolNewKey 
{ 
	/// ctor 
	AkForceInline AkPoolNewKey(){} 
};

// Important: Use these macros with appropriate delete.
#if defined (AK_MEMDEBUG)
	#define AkNew(_pool,_what)				new((_pool),AkPoolNewKey(),__FILE__,__LINE__) _what
	#define AkAlloc(_pool,_size)			(AK::MemoryMgr::dMalloc((_pool),_size,__FILE__,__LINE__))
	#define AkNew2(_ptr,_pool,_type,_what)	{ _ptr = (_type *) AK::MemoryMgr::dMalloc((_pool),sizeof(_type),__FILE__,__LINE__); if ( _ptr ) AkPlacementNew( _ptr ) _what; }
	#define AkMalign(_pool,_size,_align)	(AK::MemoryMgr::dMalign((_pool),_size,_align, __FILE__,__LINE__))
	#define AkNewAligned(_pool,_what,_align)	new((_pool),AkPoolNewKey(),(_align),__FILE__,__LINE__) _what
#else
	#define AkNew(_pool,_what)				new((_pool),AkPoolNewKey()) _what
	#define AkAlloc(_pool,_size)			(AK::MemoryMgr::Malloc((_pool),_size))
	#define AkNew2(_ptr,_pool,_type,_what)	{ _ptr = (_type *) AK::MemoryMgr::Malloc((_pool),sizeof(_type)); if ( _ptr ) AkPlacementNew( _ptr ) _what; }
	#define AkMalign(_pool,_size,_align)	(AK::MemoryMgr::Malign((_pool),_size,_align))
	#define AkNewAligned(_pool,_what,_align)	new((_pool),AkPoolNewKey(),(_align)) _what
#endif

#define AkFree(_pool,_pvmem)				(AK::MemoryMgr::Free((_pool),(_pvmem)))
#define AkFalign(_pool,_pvmem)				(AK::MemoryMgr::Falign((_pool),(_pvmem)))

#if defined (AK_MEMDEBUG)

	AkForceInline void * operator new(size_t size,AkMemPoolId in_PoolId,const AkPoolNewKey &,const char* szFile,AkUInt32 ulLine) throw()
	{
		return AK::MemoryMgr::dMalloc( in_PoolId, size, szFile, ulLine );
	}

	AkForceInline void * operator new(size_t size,AkMemPoolId in_PoolId,const AkPoolNewKey &,AkUInt32 in_align,const char* szFile,AkUInt32 ulLine) throw()
	{
		return AK::MemoryMgr::dMalign( in_PoolId, size, in_align, szFile, ulLine );
	}
	
	AkForceInline void operator delete(void *,AkMemPoolId,const AkPoolNewKey &,const char*,AkUInt32) throw() {}
	AkForceInline void operator delete(void *,AkMemPoolId,const AkPoolNewKey &,AkUInt32,const char*,AkUInt32) throw() {}
	
#else

	AkForceInline void * operator new(size_t size,AkMemPoolId in_PoolId,const AkPoolNewKey &) throw()
	{
		return AK::MemoryMgr::Malloc( in_PoolId, size );
	}

	AkForceInline void * operator new(size_t size,AkMemPoolId in_PoolId,const AkPoolNewKey &,AkUInt32 in_align) throw()
	{
		return AK::MemoryMgr::Malign( in_PoolId, size, in_align );
	}
	
	AkForceInline void operator delete(void *,AkMemPoolId,const AkPoolNewKey &) throw() {}
	AkForceInline void operator delete(void *,AkMemPoolId,const AkPoolNewKey &,AkUInt32) throw() {}

#endif

template <class T>
AkForceInline void AkDelete( AkMemPoolId in_PoolId, T * in_pObject )
{
	if ( in_pObject )
	{
		in_pObject->~T();
		AK::MemoryMgr::Free( in_PoolId, in_pObject );
	}
}

template <class T>
AkForceInline void AkDeleteAligned( AkMemPoolId in_PoolId, T * in_pObject )
{
	if ( in_pObject )
	{
		in_pObject->~T();
		AK::MemoryMgr::Falign( in_PoolId, in_pObject );
	}
}

#endif // _AK_OBJECT_H_
