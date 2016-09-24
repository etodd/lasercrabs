//////////////////////////////////////////////////////////////////////
//
// AkObject.h
//
// Base class for object that use dynamic allocation.
// Overloads new and delete to call those of the memory manager.
//
// Copyright (c) 2006 Audiokinetic Inc. / All Rights Reserved
//
//////////////////////////////////////////////////////////////////////

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
#ifndef AK_3DS
AkForceInline void operator delete( void *, void *, const AkPlacementNewKey & ) throw() {}
#endif

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
	
	#ifndef AK_3DS
	AkForceInline void operator delete(void *,AkMemPoolId,const AkPoolNewKey &,const char*,AkUInt32) throw() {}
	AkForceInline void operator delete(void *,AkMemPoolId,const AkPoolNewKey &,AkUInt32,const char*,AkUInt32) throw() {}
	#endif
	
#else

	AkForceInline void * operator new(size_t size,AkMemPoolId in_PoolId,const AkPoolNewKey &) throw()
	{
		return AK::MemoryMgr::Malloc( in_PoolId, size );
	}

	AkForceInline void * operator new(size_t size,AkMemPoolId in_PoolId,const AkPoolNewKey &,AkUInt32 in_align) throw()
	{
		return AK::MemoryMgr::Malign( in_PoolId, size, in_align );
	}
	
	#ifndef AK_3DS
	AkForceInline void operator delete(void *,AkMemPoolId,const AkPoolNewKey &) throw() {}
	AkForceInline void operator delete(void *,AkMemPoolId,const AkPoolNewKey &,AkUInt32) throw() {}
	#endif

#endif

//-----------------------------------------------------------------------------
// Name: Class CAkObject
// Desc: Base allocator object: DEPRECATED.
//-----------------------------------------------------------------------------

class CAkObject
{
public:
	/// Destructor
    virtual ~CAkObject( ) { }
};

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
