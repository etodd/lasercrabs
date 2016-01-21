//////////////////////////////////////////////////////////////////////
//
// Copyright (c) 2006 Audiokinetic Inc. / All Rights Reserved
//
//////////////////////////////////////////////////////////////////////

/// \file 
/// Memory Manager namespace.

#ifndef _AKMEMORYMGR_H_
#define _AKMEMORYMGR_H_

#include <AK/SoundEngine/Common/AkTypes.h>
#include <AK/SoundEngine/Common/AkSoundEngineExport.h>

#ifndef AK_OPTIMIZED

#define AK_MAX_MEM_POOL_NAME_SIZE 64

/// Set the debug name for a pool. This is the name shown in the Memory tab of the Advanced Profiler.
#define AK_SETPOOLNAME( _poolid, _name )				\
	if( AK_INVALID_POOL_ID != _poolid )					\
	{													\
		AK::MemoryMgr::SetPoolName( _poolid, _name );	\
	}

//#define AK_MEMDEBUG

#else
#define AK_SETPOOLNAME(_poolid,_name)
#endif


namespace AK
{   
	/// Memory Manager namespace.
	/// \remarks The functions in this namespace are thread-safe, unless stated otherwise.
	/// \sa
	/// - \ref memorymanager
	namespace MemoryMgr
	{
		/// Memory pool statistics. 
		/// \remarks These statistics are not collected in the Release configuration of 
		/// the default memory manager implementation.
		/// \sa 
		/// - AK::MemoryMgr::GetPoolStats()
		/// - \ref memorymanager
		struct PoolStats
		{
			// Current state
			AkUInt32 uReserved;		///< Reserved memory (in bytes)
			AkUInt32 uUsed;			///< Used memory (in bytes)
			AkUInt32 uMaxFreeBlock;	///< Size of biggest free block (in bytes)

			// Statistics
			AkUInt32 uAllocs;		///< Number of Alloc calls since initialization
			AkUInt32 uFrees;		///< Number of Free calls since initialization
			AkUInt32 uPeakUsed;		///< Peak used memory (in bytes)
		};

		/// Memory pool current state. 
		/// \sa 
		/// - AK::MemoryMgr::GetPoolMemoryUsed()
		/// - \ref memorymanager
		struct PoolMemInfo
		{
			// Current state
			AkUInt32 uReserved;		///< Reserved memory (in bytes)
			AkUInt32 uUsed;			///< Used memory (in bytes)
		};

		/// Query whether the Memory Manager has been sucessfully initialized.
		/// \warning This function is not thread-safe. It should not be called at the same time as MemoryMgr::Init or MemoryMgr::Term.
		/// \return True if the Memory Manager is initialized, False otherwise
		/// \sa 
		/// - AK::MemoryMgr::Init()
		/// - \ref memorymanager
		AK_EXTERNAPIFUNC( bool, IsInitialized )();

		/// Terminate the Memory Manager.
		/// \warning This function is not thread-safe. 
		/// \sa
		/// - \ref memorymanager
	    AK_EXTERNAPIFUNC( void, Term )();

		////////////////////////////////////////////////////////////////////////
		/// @name Memory Pools
		//@{

		/// Create a new memory pool.
		/// \return The ID of the created memory pool, or AK_INVALID_POOL_ID if creation failed
		/// \aktip
		/// Refer to \ref memorymanager_pools for information about pool resource overhead.
		/// \endaktip
		/// \sa
		/// - \ref memorymanager
		AK_EXTERNAPIFUNC( AkMemPoolId, CreatePool )(
			void *			in_pMemAddress,		///< Memory address of the pool, or NULL if it should be allocated
			AkUInt32		in_uMemSize,		///< Size of the pool (in bytes)
			AkUInt32		in_uBlockSize,		///< Size of a block (in bytes)
			AkUInt32		in_eAttributes,		///< Memory pool attributes: use values of \ref AkMemPoolAttributes
			AkUInt32        in_uBlockAlign = 0	///< Alignment of memory blocks
			);

#ifdef AK_SUPPORT_WCHAR
		/// Set the name of a memory pool.
		/// \return AK_Success if successful
		/// \sa
		/// - \ref memorymanager
		AK_EXTERNAPIFUNC( AKRESULT, SetPoolName )( 
			AkMemPoolId     in_poolId,			///< ID of memory pool
			const wchar_t*	in_pszPoolName		///< Pointer to unicode name string
			);
#endif //AK_SUPPORT_WCHAR

		/// Set the name of a memory pool.
		/// \return AK_Success if successful
		/// \sa
		/// - \ref memorymanager
		AK_EXTERNAPIFUNC( AKRESULT, SetPoolName )( 
			AkMemPoolId     in_poolId,			///< ID of memory pool
			const char*		in_pszPoolName		///< Pointer to name string
			);

		/// Get the name of a memory pool.
		/// \return A pointer to the name of the memory pool (NULL if the operation failed)
		/// \sa
		/// - \ref memorymanager
		AK_EXTERNAPIFUNC( AkOSChar*, GetPoolName )( 
			AkMemPoolId     in_poolId			///< ID of memory pool
			);
		
        /// Enables or disables error notifications posted by a memory pool.
        /// The notifications are enabled by default when creating a pool.
        /// They are always disabled in the Release build.
        /// \return AK_Success if the pool exists
		/// \sa
		/// - \ref memorymanager
		AK_EXTERNAPIFUNC( AKRESULT, SetMonitoring )(
            AkMemPoolId     in_poolId,			///< ID of memory pool
            bool            in_bDoMonitor       ///< Enables error monitoring (has no effect in Release build)
            );

	    /// Destroy a memory pool.
		/// \return AK_Success if successful
		/// \sa
		/// - \ref memorymanager
	    AK_EXTERNAPIFUNC( AKRESULT, DestroyPool )(
			AkMemPoolId     in_poolId			///< ID of memory pool
		    );

        /// Get a memory pool's statistics.
		/// \sa
		/// - AK::MemoryMgr::PoolStats
		/// - \ref memorymanager
	    AK_EXTERNAPIFUNC( AKRESULT, GetPoolStats )(
			AkMemPoolId     in_poolId,			///< ID of memory pool
			PoolStats&      out_stats			///< Returned statistics structure
		    );

		/// Get a memory pool current used size.
		/// Mostly used by the memory threshold features.
		/// If this function cannot be implemented if your memory manager, at least set the member uUsed to 0, that
		/// will disable the memory threshold feature.
		/// \sa
		/// - AK::MemoryMgr::PoolMemInfo
		/// - \ref memorymanager
	    AK_EXTERNAPIFUNC( void, GetPoolMemoryUsed )(
			AkMemPoolId     in_poolId,			///< ID of memory pool
			PoolMemInfo&    out_memInfo			///< Returned statistics structure
		    );

		/// Get the current number of memory pools.
		/// \return The current number of memory pools
		/// \sa
		/// - \ref memorymanager
		AK_EXTERNAPIFUNC( AkInt32, GetNumPools )();

		/// Get the maximum number of memory pools.
		/// \return The maximum number of memory pools
		/// \sa
		/// - \ref memorymanager
		AK_EXTERNAPIFUNC( AkInt32, GetMaxPools )();

		/// Test the validity of a pool ID.
		/// This is used to verify the validity of a memory pool ID.
		/// \return AK_Success if the pool exists, AK_InvalidID otherwise
		/// \sa
		/// - \ref memorymanager
		AK_EXTERNAPIFUNC( AKRESULT, CheckPoolId )(
			AkMemPoolId		in_poolId			///< ID of memory pool to test
			);

		/// Get pool attributes.
		/// \return The memory pool's attributes.
		/// \sa
		/// - \ref memorymanager
		AK_EXTERNAPIFUNC( AkMemPoolAttributes, GetPoolAttributes )(
			AkMemPoolId		in_poolId			///< ID of memory pool
			);

		//@}

		////////////////////////////////////////////////////////////////////////
		/// @name Memory Allocation
		//@{

#if defined (AK_MEMDEBUG)
		/// Allocate memory from a pool: debug version.
		/// \return A pointer to the start of the allocated memory (NULL if the system is out of memory)
		/// \sa
		/// - \ref memorymanager
	    AK_EXTERNAPIFUNC( void *, dMalloc )(
			AkMemPoolId in_poolId,				///< ID of the memory pool
		    size_t		in_uSize,				///< Number of bytes to allocate
		    const char *in_pszFile,				///< Debug file name
		    AkUInt32	in_uLine				///< Debug line number
			);
#endif
		/// Allocate memory from a pool.
		/// \return A pointer to the start of the allocated memory (NULL if the system is out of memory)
		/// \sa
		/// - \ref memorymanager
	    AK_EXTERNAPIFUNC( void *, Malloc )(
			AkMemPoolId in_poolId,				///< ID of the memory pool
		    size_t		in_uSize 				///< Number of bytes to allocate
		    );

		/// Free memory from a pool.
		/// \return AK_Success if successful
		/// \sa
		/// - \ref memorymanager
	    AK_EXTERNAPIFUNC( AKRESULT, Free )(
			AkMemPoolId in_poolId,				///< ID of the memory pool
			void *		in_pMemAddress			///< Pointer to the start of memory allocated with Malloc
		    );

#if defined (AK_MEMDEBUG)
		/// Allocate memory from a pool, overriding the pool's default memory alignment. Needs to be used
		/// in conjunction with AK::MemoryMgr::Falign. debug version.
		/// \return A pointer to the start of the allocated memory (NULL if the system is out of memory)
		/// \sa
		/// - \ref memorymanager
	    AK_EXTERNAPIFUNC( void *, dMalign )(
			AkMemPoolId in_poolId,				///< ID of the memory pool
		    size_t		in_uSize,				///< Number of bytes to allocate
		    AkUInt32	in_uAlignment, 			///< Alignment (in bytes)
		    const char*	 in_pszFile,			///< Debug file name
		    AkUInt32	in_uLine				///< Debug line number
			);
#endif

		/// Allocate memory from a pool, overriding the pool's default memory alignment. Needs to be used
		/// in conjunction with AK::MemoryMgr::Falign.
		/// \return A pointer to the start of the allocated memory (NULL if the system is out of memory)
		/// \sa
		/// - \ref memorymanager
	    AK_EXTERNAPIFUNC( void *, Malign )(
			AkMemPoolId in_poolId,				///< ID of the memory pool
		    size_t		in_uSize, 				///< Number of bytes to allocate
		    AkUInt32	in_uAlignment 			///< Alignment (in bytes)
		    );

		/// Free memory from a pool, overriding the pool's default memory alignment. Needs to be used in
		/// conjunction with AK::MemoryMgr::Malign.
		/// 
		/// \return AK_Success if successful
		/// \sa
		/// - \ref memorymanager
	    AK_EXTERNAPIFUNC( AKRESULT, Falign )(
			AkMemPoolId in_poolId,				///< ID of the memory pool
			void *		in_pMemAddress			///< Pointer to the start of memory allocated with Malloc
		    );

		//@}

		////////////////////////////////////////////////////////////////////////
		/// @name Fixed-Size Blocks Memory Allocation Mode
		//@{

		/// Get a block from a Fixed-Size Block type pool. To be used with pools created with AkFixedSizeBlocksMode
		/// block management type, along with any of the block allocation types.
		/// \return A pointer to the start of the allocated memory (NULL if the system is out of memory)
		///			The size of the memory block is always in_uBlockSize, specified in AK::MemoryMgr::CreatePool.
		/// \warning This method is not thread-safe. Fixed-Size Block pool access must be protected.
		/// \sa 
		/// - AK::MemoryMgr::CreatePool
		/// - AkMemPoolAttributes
		/// - \ref memorymanager
	    AK_EXTERNAPIFUNC( void *, GetBlock )(
			AkMemPoolId in_poolId				///< ID of the memory pool
		    );

		/// Free memory from a Fixed-Size Block type pool.
		/// \return AK_Success if successful
		/// \warning This method is not thread-safe. Fixed-Size Block pool access must be protected.
		/// \sa
		/// - \ref memorymanager
	    AK_EXTERNAPIFUNC( AKRESULT, ReleaseBlock )(
			AkMemPoolId in_poolId,				///< ID of the memory pool
			void *		in_pMemAddress			///< Pointer to the start of memory allocated with Malloc
		    );

		/// Get block size of blocks obtained with GetBlock() for a given memory pool.
		/// The block size is fixed and set when creating a pool with AkFixedSizeBlocksMode.
		/// \return Block size
		/// \sa
		/// - AK::MemoryMgr::CreatePool
		/// - AK::MemoryMgr::GetBlock
		/// - \ref memorymanager
	    AK_EXTERNAPIFUNC( AkUInt32, GetBlockSize )(
			AkMemPoolId in_poolId				///< ID of the memory pool
			);

		/// Called to start profiling memory usage for one thread (the calling thread).
		/// \note Not implementing this will result in the Soundbank tab of the Wwise Profiler to show 0 bytes for memory usage.
		AK_EXTERNAPIFUNC( void, StartProfileThreadUsage) (
			AkMemPoolId in_PoolId	///< Pool to profile
			);

		/// Called to stop profiling memory usage for the current thread.
		/// \return The amount of memory allocated by this thread since \ref StartProfileThreadUsage was called.
		/// \note Not implementing this will result in the Soundbank tab of the Wwise Profiler to show 0 bytes for memory usage.
		AK_EXTERNAPIFUNC( AkUInt32, StopProfileThreadUsage ) (
			AkMemPoolId in_PoolId	///< Pool to profile
			);

		//@}
    }
}

#endif // _AKMEMORYMGR_H_
