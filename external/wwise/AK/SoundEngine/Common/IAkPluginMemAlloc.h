//////////////////////////////////////////////////////////////////////
//
// Copyright (c) 2006 Audiokinetic Inc. / All Rights Reserved
//
//////////////////////////////////////////////////////////////////////

/// \file 
/// Memory allocation macros for Wwise sound engine plug-ins. 

#ifndef _IAKPLUGINMEMALLOC_H_
#define _IAKPLUGINMEMALLOC_H_

#include <AK/SoundEngine/Common/AkTypes.h>
#include <AK/SoundEngine/Common/AkMemoryMgr.h> // For AK_MEMDEBUG

namespace AK
{
	/// Interface to memory allocation
	/// \warning The functions in this interface are not thread-safe, unless stated otherwise.
	///
	/// \akcaution SDK users should never call these function directly, but use memory allocation macros instead. \endakcaution
	/// \sa 
	/// - \ref fx_memory_alloc
	class IAkPluginMemAlloc
	{
	protected:
		/// Virtual destructor on interface to avoid warnings.
		virtual ~IAkPluginMemAlloc(){}

	public:
		
	    /// Allocate memory. 
		/// \return A pointer to the newly-allocated memory. 
		/// \sa 
		/// - \ref fx_memory_alloc
	    virtual void * Malloc( 
            size_t in_uSize		///< Allocation size in bytes
            ) = 0;

		/// Free allocated memory.
		/// \sa 
		/// - \ref fx_memory_alloc
        virtual void Free(
            void * in_pMemAddress	///< Allocated memory start address
            ) = 0;

#if defined (AK_MEMDEBUG)
	    /// Debug malloc.
		/// \sa 
		/// - \ref fx_memory_alloc
	    virtual void * dMalloc( 
            size_t	 in_uSize,		///< Allocation size
            const char*  in_pszFile,///< Allocation file name (for tracking purposes)
		    AkUInt32 in_uLine		///< Allocation line number (for tracking purposes)
		    ) = 0;
#endif
	};
}

// Memory allocation macros to be used by sound engine plug-ins.
#if defined (AK_MEMDEBUG)

	AkForceInline void * operator new(size_t size,AK::IAkPluginMemAlloc * in_pAllocator,const char* szFile,AkUInt32 ulLine) throw()
	{
		return in_pAllocator->dMalloc( size, szFile, ulLine );
	}

#ifndef AK_3DS
		AkForceInline void operator delete(void *, AK::IAkPluginMemAlloc *, const char*, AkUInt32) throw() {}
#endif
	
#endif

	AkForceInline void * operator new(size_t size,AK::IAkPluginMemAlloc * in_pAllocator) throw()
	{
		return in_pAllocator->Malloc( size );
	}
	
	#ifdef AK_PS3
	AkForceInline void * operator new(size_t size, unsigned int align, AK::IAkPluginMemAlloc * in_pAllocator) throw()
	{
		return in_pAllocator->Malloc( size );
	}
	#endif

	#ifndef AK_3DS
	AkForceInline void operator delete(void *,AK::IAkPluginMemAlloc *) throw() {}
	#endif

#if defined (AK_MEMDEBUG)
	#define AK_PLUGIN_NEW(_allocator,_what)	            new((_allocator),__FILE__,__LINE__) _what
	#define AK_PLUGIN_ALLOC(_allocator,_size)           (_allocator)->dMalloc((_size),__FILE__,__LINE__)
#else
	/// Macro used to allocate objects.
	/// \param _allocator Memory allocator interface.
	/// \param _what Desired object type. 
	/// \return A pointer to the newly-allocated object.
	/// \aknote Use AK_PLUGIN_DELETE() for memory allocated with this macro. \endaknote
	/// \sa
	/// - \ref fx_memory_alloc
	/// - AK_PLUGIN_DELETE()
	#define AK_PLUGIN_NEW(_allocator,_what)	            new(_allocator) _what
	/// Macro used to allocate arrays of built-in types.
	/// \param _allocator Memory allocator interface.
	/// \param _size Requested size in bytes.
	/// \return A void pointer to the the allocated memory.
	/// \aknote Use AK_PLUGIN_FREE() for memory allocated with this macro. \endaknote
	/// \sa
	/// - \ref fx_memory_alloc
	/// - AK_PLUGIN_FREE()
	#define AK_PLUGIN_ALLOC(_allocator,_size)           (_allocator)->Malloc((_size))
#endif

/// Macro used to deallocate objects.
/// \param in_pAllocator Memory allocator interface.
/// \param in_pObject A pointer to the allocated object.
/// \sa
/// - \ref fx_memory_alloc
/// - AK_PLUGIN_NEW()
template <class T>
AkForceInline void AK_PLUGIN_DELETE( AK::IAkPluginMemAlloc * in_pAllocator, T * in_pObject )      
{
	if ( in_pObject )
	{
		in_pObject->~T();
		in_pAllocator->Free( in_pObject );
	}
}

/// Macro used to deallocate arrays of built-in types.
/// \param _allocator Memory allocator interface.
/// \param _pvmem A void pointer to the allocated memory.
/// \sa
/// - \ref fx_memory_alloc
/// - AK_PLUGIN_ALLOC()
#define AK_PLUGIN_FREE(_allocator,_pvmem)       (_allocator)->Free((_pvmem))

#endif // _IAKPLUGINMEMALLOC_H_
