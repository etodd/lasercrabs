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

	AkForceInline void operator delete(void *, AK::IAkPluginMemAlloc *, const char*, AkUInt32) throw() {}
	
#endif

	AkForceInline void * operator new(size_t size,AK::IAkPluginMemAlloc * in_pAllocator) throw()
	{
		return in_pAllocator->Malloc( size );
	}

	AkForceInline void operator delete(void *,AK::IAkPluginMemAlloc *) throw() {}

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
