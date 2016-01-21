//////////////////////////////////////////////////////////////////////
//
// Copyright (c) 2006 Audiokinetic Inc. / All Rights Reserved
//
//////////////////////////////////////////////////////////////////////

/// \file 
/// Defines the API of Audiokinetic's I/O streaming solution.

#ifndef _IAK_STREAM_MGR_H_
#define _IAK_STREAM_MGR_H_

#include <AK/SoundEngine/Common/AkMemoryMgr.h>

//-----------------------------------------------------------------------------
// Defines. 
//-----------------------------------------------------------------------------

/// \name Profiling string lengths.
//@{
#define AK_MONITOR_STREAMNAME_MAXLENGTH    (64)
#define AK_MONITOR_DEVICENAME_MAXLENGTH    (16)
//@}
 
//-----------------------------------------------------------------------------
// Enums.
//-----------------------------------------------------------------------------

/// Stream status.
enum AkStmStatus
{
    AK_StmStatusIdle           	= 0,    	///< The stream is idle
    AK_StmStatusCompleted      	= 1,    	///< Operation completed / Automatic stream reached end
    AK_StmStatusPending        	= 2,    	///< Operation pending / The stream is waiting for I/O
    AK_StmStatusCancelled      	= 3,		///< Operation cancelled
    AK_StmStatusError          	= 4     	///< The low-level I/O reported an error
};

/// Move method for position change. 
/// \sa
/// - AK::IAkStdStream::SetPosition()
/// - AK::IAkAutoStream::SetPosition()
enum AkMoveMethod
{
    AK_MoveBegin               	= 0,    	///< Move offset from the start of the stream
    AK_MoveCurrent             	= 1,    	///< Move offset from the current stream position
    AK_MoveEnd                 	= 2     	///< Move offset from the end of the stream
};

/// File open mode.
enum AkOpenMode
{
    AK_OpenModeRead				= 0,		///< Read-only access
    AK_OpenModeWrite		    = 1,		///< Write-only access (opens the file if it already exists)
    AK_OpenModeWriteOvrwr	    = 2,		///< Write-only access (deletes the file if it already exists)
    AK_OpenModeReadWrite	    = 3	    	///< Read and write access
};

/// File system flags for file descriptors mapping.
struct AkFileSystemFlags
{
	AkFileSystemFlags()
		: uCacheID( AK_INVALID_FILE_ID ) {}

	AkFileSystemFlags( AkUInt32 in_uCompanyID, AkUInt32 in_uCodecID, AkUInt32 in_uCustomParamSize, void * in_pCustomParam, bool in_bIsLanguageSpecific, bool in_bIsFromRSX, AkFileID in_uCacheID )
		: uCompanyID( in_uCompanyID )
		, uCodecID( in_uCodecID )
		, uCustomParamSize( in_uCustomParamSize )
		, pCustomParam( in_pCustomParam )
		, bIsLanguageSpecific( in_bIsLanguageSpecific )
		, bIsFromRSX( in_bIsFromRSX )
		, uCacheID( in_uCacheID ) 
		, uNumBytesPrefetch( 0 ) {}

    AkUInt32            uCompanyID;         ///< Company ID (Wwise uses AKCOMPANYID_AUDIOKINETIC, defined in AkTypes.h, for soundbanks and standard streaming files, and AKCOMPANYID_AUDIOKINETIC_EXTERNAL for streaming external sources).
    AkUInt32            uCodecID;           ///< File/codec type ID (defined in AkTypes.h)
    AkUInt32            uCustomParamSize;   ///< Size of the custom parameter
    void *              pCustomParam;       ///< Custom parameter
    bool                bIsLanguageSpecific;///< True when the file location depends on language
	bool				bIsFromRSX;			///< True if the "RSX" option is checked in the sound's streaming properties
	bool                bIsAutomaticStream;	///< True when the file is opened to be used as an automatic stream. Note that you don't need to set it. 
											///< If you pass an AkFileSystemFlags to IAkStreamMgr CreateStd|Auto(), it will be set internally to the correct value.
	AkFileID			uCacheID;			///< Cache ID for caching system used by automatic streams. The user is responsible for guaranteeing unicity of IDs. 
											///< When set, it supersedes the file ID passed to AK::IAkStreamMgr::CreateAuto() (ID version). Caching is optional and depends on the implementation.
	AkUInt32			uNumBytesPrefetch;	///< For prefetch streams, indicates number of bytes to lock in cache.
};

/// Stream information.
/// \sa
/// - AK::IAkStdStream::GetInfo()
/// - AK::IAkAutoStream::GetInfo()
struct AkStreamInfo
{
    AkDeviceID          deviceID;           ///< Device ID
    const AkOSChar *	pszName;            ///< User-defined stream name (specified through AK::IAkStdStream::SetStreamName() or AK::IAkAutoStream::SetStreamName())
    AkUInt64            uSize;              ///< Total stream/file size in bytes
	bool				bIsOpen;			///< True when the file is open (implementations may defer file opening)
};

/// Automatic streams heuristics.
struct AkAutoStmHeuristics
{
    AkReal32            fThroughput;        ///< Average throughput in bytes/ms
    AkUInt32            uLoopStart;         ///< Set to the start of loop (byte offset from the beginning of the stream) for streams that loop, 0 otherwise
    AkUInt32            uLoopEnd;           ///< Set to the end of loop (byte offset from the beginning of the stream) for streams that loop, 0 otherwise
    AkUInt8				uMinNumBuffers;     ///< Minimum number of buffers if you plan to own more than one buffer at a time, 0 or 1 otherwise
                                            ///< \remarks You should always release buffers as fast as possible, therefore this heuristic should be used only when 
                                            ///< dealing with special contraints, like drivers or hardware that require more than one buffer at a time.\n
                                            ///< Also, this is only a heuristic: it does not guarantee that data will be ready when calling AK::IAkAutoStream::GetBuffer().
    AkPriority          priority;           ///< The stream priority. it should be between AK_MIN_PRIORITY and AK_MAX_PRIORITY (included).
};

/// Automatic streams buffer settings/constraints.
struct AkAutoStmBufSettings
{
    AkUInt32			uBufferSize;		///< Hard user constraint: When non-zero, forces the I/O buffer to be of size uBufferSize
											///< (overriding the device's granularity).
											///< Otherwise, the size is determined by the device's granularity.
    AkUInt32            uMinBufferSize;     ///< Soft user constraint: When non-zero, specifies a minimum buffer size
                                            ///< \remarks Ignored if uBufferSize is specified.
	AkUInt32            uBlockSize;  		///< Hard user constraint: When non-zero, buffer size will be a multiple of that number, and returned addresses will always be aligned on multiples of this value.
};

/// \name Profiling structures.
//@{

/// Device descriptor.
struct AkDeviceDesc
{
    AkDeviceID          deviceID;           ///< Device ID
    bool                bCanWrite;          ///< Specifies whether or not the device is writable
    bool                bCanRead;           ///< Specifies whether or not the device is readable
    AkUtf16				szDeviceName[AK_MONITOR_DEVICENAME_MAXLENGTH];      ///< Device name
    AkUInt32            uStringSize;        ///< Device name string's size (number of characters)
};

/// Device descriptor.
struct AkDeviceData
{
	AkDeviceID          deviceID;           ///< Device ID
	AkUInt32			uMemSize;			///< IO memory pool size
	AkUInt32			uMemUsed;			///< IO memory pool used	
	AkUInt32			uAllocs;			///< Cumulative number of allocations
	AkUInt32			uFrees;				///< Cumulative number of deallocations
	AkUInt32			uPeakRefdMemUsed;			///< Memory peak since monitoring started
	AkUInt32			uUnreferencedCachedBytes;	///< IO memory that is cached but is not currently used for active streams.
	AkUInt32			uGranularity;		///< IO memory pool block size
	AkUInt32			uNumActiveStreams;	///< Number of streams that have been active in the previous frame
	AkUInt32			uTotalBytesTransferred;			///< Number of bytes transferred, including cached transfers
	AkUInt32			uLowLevelBytesTransferred;		///< Number of bytes transferred exclusively via low-level
	AkReal32			fAvgCacheEfficiency;			///< Total bytes from cache as a percentage of total bytes. 
	AkUInt32			uNumLowLevelRequestsCompleted;	///< Number of low-level transfers that have completed in the previous monitoring frame
	AkUInt32			uNumLowLevelRequestsCancelled;	///< Number of low-level transfers that were cancelled in the previous monitoring frame
	AkUInt32			uNumLowLevelRequestsPending;	///< Number of low-level transfers that are currently pending
	AkUInt32			uCustomParam;		///< Custom number queried from low-level IO.
	AkUInt32			uCachePinnedBytes;  ///< Number of bytes that can be pinned into cache.
};

/// Stream general information.
struct AkStreamRecord
{
    AkUInt32            uStreamID;          ///< Unique stream identifier
    AkDeviceID          deviceID;           ///< Device ID
    AkUtf16				szStreamName[AK_MONITOR_STREAMNAME_MAXLENGTH];       ///< Stream name
    AkUInt32            uStringSize;        ///< Stream name string's size (number of characters)
    AkUInt64            uFileSize;          ///< File size
	AkUInt32			uCustomParamSize;	///< File descriptor's uCustomParamSize
    AkUInt32			uCustomParam;		///< File descriptor's pCustomParam (on 32 bits)
    bool                bIsAutoStream;      ///< True for auto streams
	bool				bIsCachingStream;	///< True for caching streams
};

/// Stream statistics.
struct AkStreamData
{
    AkUInt32            uStreamID;          ///< Unique stream identifier
    // Status (replace)
    AkUInt32            uPriority;          ///< Stream priority
	AkUInt64            uFilePosition;      ///< Current position
    AkUInt32            uTargetBufferingSize;		///< Total stream buffer size (specific to IAkAutoStream)
    AkUInt32            uVirtualBufferingSize;		///< Size of available data including requested data (specific to IAkAutoStream)
	AkUInt32            uBufferedSize;				///< Size of available data (specific to IAkAutoStream)
	AkUInt32            uNumBytesTransfered;		///< Transfered amount since last query (Accumulate/Reset)
	AkUInt32            uNumBytesTransferedLowLevel;///< Transfered amount (from low-level IO only) since last query (Accumulate/Reset)
	AkUInt32            uMemoryReferenced;			///< Amount of streaming memory referenced by this stream
	AkReal32			fEstimatedThroughput;		///< Estimated throughput heuristic
	bool				bActive;			///< True if this stream has been active (that is, was ready for I/O or had at least one pending I/O transfer, uncached or not) in the previous frame
};
//@}

namespace AK
{
    /// \name Profiling interfaces.
    //@{
    
    /// Profiling interface of streams.
	/// \warning The functions in this interface are not thread-safe, unless stated otherwise.
    class IAkStreamProfile
    {
    protected:
        /// Virtual destructor on interface to avoid warnings.
		virtual ~IAkStreamProfile(){}

	public:
		/// Returns the stream's record (once).
		/// \sa
		/// - \ref streamingdevicemanager
        virtual void    GetStreamRecord( 
            AkStreamRecord & out_streamRecord   ///< Returned stream record interface
            ) = 0;

        /// Returns the stream's statistics (every profile frame).
		/// \sa
		/// - \ref streamingdevicemanager
        virtual void    GetStreamData(
            AkStreamData &   out_streamData     ///< Returned periodic stream data interface
            ) = 0;

        /// Query the stream's "new" flag.
        /// \return True, until AK::IAkStreamProfile::ClearNew() is called.
		/// \sa
		/// - \ref streamingdevicemanager
        virtual bool    IsNew() = 0;

        /// Resets the stream's "new" flag.
		/// \sa
		/// - \ref streamingdevicemanager
        virtual void    ClearNew() = 0;
    };


    /// Profiling interface of high-level I/O devices.
	/// \warning The functions in this interface are not thread-safe, unless stated otherwise.
    class IAkDeviceProfile
    {
    protected:
    	/// Virtual destructor on interface to avoid warnings.
		virtual ~IAkDeviceProfile(){}

	public:

		/// Notify device when monitor sampling starts.
		virtual void	OnProfileStart() = 0;

		/// Notify device when monitor sampling ends.
		virtual void	OnProfileEnd() = 0;

		/// Query the device's description (once).
		/// \sa
		/// - \ref streamingdevicemanager
        virtual void    GetDesc(
            AkDeviceDesc &  out_deviceDesc      ///< Device descriptor.
            ) = 0;

		/// Query the device's statistics (at every profiling frame).
		/// \sa
		/// - \ref streamingdevicemanager
        virtual void    GetData(
            AkDeviceData &  out_deviceData		///< Device data.
            ) = 0;

        /// Query the device's "new" flag.
        /// \return True, until ClearNew() is called.
		/// \sa
		/// - \ref streamingdevicemanager
        virtual bool    IsNew() = 0;

        /// Resets the device's "new" flag.
		/// \sa
		/// - \ref streamingdevicemanager
        virtual void    ClearNew() = 0;

        /// Get the number of streams currently associated with that device.
        /// \return The number of streams
		/// \sa
		/// - \ref streamingdevicemanager
        virtual AkUInt32 GetNumStreams() = 0;

        /// Get a stream profile, for a specified stream index.
        /// \remarks GetStreamProfile() refers to streams by index, which must honor the call to AK::IAkDeviceProfile::GetNumStreams().
		/// \sa
		/// - \ref streamingdevicemanager
        virtual IAkStreamProfile * GetStreamProfile( 
			AkUInt32    in_uStreamIndex     ///< Stream index: [0,numStreams[
            ) = 0;
    };

    /// Profiling interface of the Stream Manager.
	/// \warning The functions in this interface are not thread-safe, unless stated otherwise.
    class IAkStreamMgrProfile
    {
    protected:
    	/// Virtual destructor on interface to avoid warnings.
		virtual ~IAkStreamMgrProfile(){}
		
	public:
        /// Start profile monitoring.
        /// \return AK_Success if successful, AK_Fail otherwise.
		/// \sa
		/// - \ref streamingdevicemanager
        virtual AKRESULT StartMonitoring() = 0;

        /// Stop profile monitoring.
		/// \sa
		/// - \ref streamingdevicemanager
	    virtual void     StopMonitoring() = 0;
        
        /// Device enumeration.
        /// \return The number of devices.
		/// \sa
		/// - \ref streamingdevicemanager
        virtual AkUInt32 GetNumDevices() = 0;

        /// Get a device profile for a specified device index.
        /// \remarks GetDeviceProfile() refers to devices by index, which must honor the call to AK::IAkStreamMgrProfile::GetNumDevices().
        /// \remarks The device index is not the same as the device ID (AkDeviceID).
		/// \sa
		/// - \ref streamingdevicemanager
        virtual IAkDeviceProfile * GetDeviceProfile( 
			AkUInt32    in_uDeviceIndex     ///< Device index: [0,numDevices[
            ) = 0;
    };
    //@}

    /// \name High-level streams API.
    //@{

    /// Interface of standard streams. Used as a handle to a standard stream. Has methods for 
    /// stream control. Obtained through the Stream Manager's AK::IAkStreamMgr::CreateStd() method.
	/// \warning The functions in this interface are not thread-safe, unless stated otherwise.
    class IAkStdStream
    {
    protected:
       	/// Virtual destructor on interface to avoid warnings.
		virtual ~IAkStdStream(){}

	public:
        /// \name Stream management and settings.
        //@{
        /// Close the stream. The object is destroyed and the interface becomes invalid.
		/// \sa
		/// - \ref streamingdevicemanager
        virtual void      Destroy() = 0;

        /// Get information about a stream.
		/// \sa
		/// - \ref streamingdevicemanager
        virtual void      GetInfo(
            AkStreamInfo &	out_info			///< Returned stream info
            ) = 0;

		/// Returns a unique cookie for a given stream.
		/// The default implementation of the Stream Manager returns its file descriptor (see AkStreamMgrModule.h).
		virtual void * GetFileDescriptor() = 0;

        /// Give the stream a name (appears in the Wwise Profiler).
		/// \sa
		/// - \ref streamingdevicemanager
        virtual AKRESULT  SetStreamName(
            const AkOSChar *	in_pszStreamName    ///< Stream name
            ) = 0;

        /// Get the I/O block size.
		/// \remark Queries the low-level I/O, by calling AK::StreamMgr::IAkLowLevelIOHook::GetBlockSize() with the
        /// stream's file descriptor.
        /// \return The block size, in bytes.
		/// \sa
		/// - \ref streamingdevicemanager
        virtual AkUInt32  GetBlockSize() = 0;   
        //@}

        /// \name I/O operations.
        //@{
        
        /// Schedule a read request.
        /// \warning Use only with a multiple of the block size, queried via AK::IAkStdStream::GetBlockSize().
        /// \remarks If the call is asynchronous (in_bWait = false), wait until AK::IAkStdStream::GetStatus() stops returning AK_StmStatusPending.
        /// \return AK_Success if the operation was successfully scheduled (but not necessarily completed)
		/// \sa
		/// - \ref streamingdevicemanager
        virtual AKRESULT Read(
            void *          in_pBuffer,         ///< User buffer address 
            AkUInt32        in_uReqSize,        ///< Requested read size
            bool            in_bWait,           ///< Block until the operation is complete
            AkPriority      in_priority,        ///< Heuristic: operation priority
            AkReal32        in_fDeadline,       ///< Heuristic: operation deadline (ms)
            AkUInt32 &      out_uSize           ///< The size that was actually read
            ) = 0;

        /// Schedule a write request.
        /// \warning Use only with a multiple of the block size, queried via AK::IAkStdStream::GetBlockSize().
        /// \remarks If the call is asynchronous (in_bWait = false), wait until GetStatus() stops returning AK_StmStatusPending.
        /// \return AK_Success if the operation was successfully scheduled
		/// \sa
		/// - \ref streamingdevicemanager
        virtual AKRESULT Write(
            void *          in_pBuffer,         ///< User buffer address
            AkUInt32        in_uReqSize,        ///< Requested write size
            bool            in_bWait,           ///< Block until the operation is complete
            AkPriority      in_priority,        ///< Heuristic: operation priority
            AkReal32        in_fDeadline,       ///< Heuristic: operation deadline (ms)
            AkUInt32 &      out_uSize           ///< The size that was actually written
            ) = 0;

        /// Get the current stream position.
        /// \remarks If an operation is pending, there is no guarantee that the position was queried before (or after) the operation was completed.
        /// \return The current stream position
		/// \sa
		/// - \ref streamingdevicemanager
        virtual AkUInt64 GetPosition( 
            bool *          out_pbEndOfStream   ///< Returned end-of-stream flag, only for streams opened with AK_OpenModeRead (can pass NULL)
            ) = 0;

        /// Set the stream position. Modifies the position for the next read/write operation.
        /// \warning No operation should be pending.
        /// \remarks The new position will snap to the lowest block boundary.
        /// \return AK_Success if the stream position was changed
		/// \sa
		/// - \ref streamingdevicemanager
        virtual AKRESULT SetPosition(
            AkInt64         in_iMoveOffset,     ///< Seek offset
            AkMoveMethod    in_eMoveMethod,     ///< Seek method, from the beginning, end, or current file position
            AkInt64 *       out_piRealOffset    ///< The actual seek offset may differ from the expected value when the block size is bigger than 1.
                                                ///< In that case, the seek offset floors to the sector boundary. Can pass NULL.
            ) = 0;

        /// Cancel the current operation.
        /// \remarks When it returns, the caller is guaranteed that no operation is pending.
        /// \remarks This method can block the caller for the whole duration of the I/O operation, if the request was already posted.
		/// \sa
		/// - \ref streamingdevicemanager
        virtual void Cancel() = 0;

        //@}

        /// \name Access to data and status.
        //@{
        /// Get user data (and accessed size).
        /// \return The address of data provided by user
		/// \sa
		/// - \ref streamingdevicemanager
        virtual void *   GetData( 
            AkUInt32 &      out_uSize           ///< Size actually read or written
            ) = 0;

        /// Get the stream's status.
        /// \return The stream status.
		/// \sa
		/// - \ref streamingdevicemanager
        virtual AkStmStatus GetStatus() = 0;  

        //@}
    };


    /// Interface of automatic streams. It is used as a handle to a stream, 
    /// I/O operations are triggered from here. 
    /// Obtained through the Stream Manager's AK::IAkStreamMgr::CreateAuto() method.
	/// \warning The functions in this interface are not thread-safe, unless stated otherwise.
	/// \sa
	/// - \ref streamingdevicemanager
    class IAkAutoStream 
    {
    protected:
    	/// Virtual destructor on interface to avoid warnings.
		virtual ~IAkAutoStream(){}

	public:
        /// \name Stream management, settings access, and run-time change.
        //@{
        /// Close the stream. The object is destroyed and the interface becomes invalid.
		/// \sa
		/// - \ref streamingdevicemanager
        virtual void      Destroy() = 0;

        /// Get information about the stream.
		/// \sa
		/// - \ref streamingdevicemanager
        virtual void      GetInfo(
            AkStreamInfo &      out_info        ///< Returned stream info
            ) = 0;

		/// Returns a unique cookie for a given stream.
		/// The default implementation of the Stream Manager returns its file descriptor (see AkStreamMgrModule.h).
		virtual void * GetFileDescriptor() = 0;

        /// Get the stream's heuristics.
		/// \sa
		/// - \ref streamingdevicemanager
        virtual void      GetHeuristics(
            AkAutoStmHeuristics & out_heuristics///< Returned stream heuristics
            ) = 0;

        /// Run-time change of the stream's heuristics.
		/// \sa
		/// - \ref streamingdevicemanager
        virtual AKRESULT  SetHeuristics(
            const AkAutoStmHeuristics & in_heuristics   ///< New stream heuristics
            ) = 0;

		/// Run-time change of the stream's minimum buffer size that can be handed out to client
		/// in GetBuffer() (except the last buffer at the end of file).
		/// Corresponds to the uMinBufferSize field of the AkAutoStmBufSettings passed to CreateAuto().
		/// \sa
		/// - AkAutoStmBufSettings
		/// - \ref streamingdevicemanager
		virtual AKRESULT  SetMinimalBufferSize(
			AkUInt32		in_uMinBufferSize	///< Minimum buffer size that can be handed out to client.
			) = 0;

        /// Give the stream a name (appears in the Wwise profiler).
		/// \sa
		/// - \ref streamingdevicemanager
        virtual AKRESULT  SetStreamName(
            const AkOSChar *	in_pszStreamName    ///< Stream name
            ) = 0;

        /// Get the I/O block size.
		/// \remark Queries the actual low-level I/O device, by calling AK::StreamMgr::IAkLowLevelIOHook::GetBlockSize() with the
        /// stream's file descriptor.
        /// \return The block size (in bytes)
		/// \sa
		/// - \ref streamingdevicemanager
        virtual AkUInt32  GetBlockSize() = 0;

		/// Get the amount of buffering that the stream has. 
		/// The buffering is defined as the number of bytes that the stream has buffered, excluding the number
		/// of bytes that is currently granted to the user (with GetBuffer()).
		/// \remark The returned value corresponds to the buffering status at that moment, and does not even 
		/// guarantee that it will not shrink.
		/// \return 
		/// - AK_DataReady: Some data has been buffered (out_uNumBytesAvailable is greater than 0).
		/// - AK_NoDataReady: No data is available, and the end of file has not been reached.
		/// - AK_NoMoreData: Some or no data is available, but the end of file has been reached. The stream will not buffer any more data.
		/// - AK_Fail: The stream is invalid due to an I/O error.
		virtual AKRESULT QueryBufferingStatus( 
			AkUInt32 & out_uNumBytesAvailable	///< Number of bytes available in the stream's buffer(s).
			) = 0;

		/// Returns the target buffering size based on the throughput heuristic.
		/// \return
		/// Target buffering length expressed in bytes. 
		virtual AkUInt32 GetNominalBuffering() = 0;

        //@}

        /// \name Stream operations.
        //@{
        
        /// Start the automatic scheduling.
        /// \return AK_Success if the automatic scheduling was started successfully
		/// \sa
		/// - \ref streamingdevicemanager
        virtual AKRESULT Start() = 0;

        /// Stop the automatic scheduling.
        /// \return AK_Success if the automatic scheduling was stopped successfully.
		/// \sa
		/// - \ref streamingdevicemanager
        virtual AKRESULT Stop() = 0;

        /// Get the stream's position.
        /// \remarks The stream position is the position seen by the user, not the position of the file
        /// already streamed into the Stream Manager's memory. The stream position is updated when buffers 
        /// are released, using AK::IAkAutoStream::ReleaseBuffer().
        /// \return The file position in bytes of the beginning of the first buffer owned by the user. 
		/// If the user does not own a buffer, it returns the position of the beginning of the buffer that 
		/// would be returned from a call to AK::IAkAutoStream::GetBuffer().
		/// \sa
		/// - \ref streamingdevicemanager
        virtual AkUInt64 GetPosition( 
            bool *          out_pbEndOfStream   ///< Returned end-of-stream flag (can pass NULL)
            ) = 0;  

        /// Set the stream's position. 
        /// The next call to AK::IAkAutoStream::GetBuffer() will grant data that corresponds to the position specified here.
        /// \remarks Data already streamed into the Stream Manager's memory might be flushed.
        /// \remarks The new position will round down to the low-level I/O block size.
        /// \return AK_Success if the resulting position is valid
		/// \sa
		/// - \ref streamingdevicemanager
        virtual AKRESULT SetPosition(
            AkInt64         in_iMoveOffset,     ///< Seek offset
            AkMoveMethod    in_eMoveMethod,     ///< Seek method, from the beginning, end or current file position
            AkInt64 *       out_piRealOffset    ///< The actual seek offset may differ from the expected value when the low-level's block size is greater than 1.
                                                ///< In that case, the real absolute position rounds down to the block boundary. Can pass NULL.
            ) = 0;

        //@}


        /// \name Data/status access.
        //@{

        /// Get data from the Stream Manager buffers.
        /// \remarks Grants a buffer if data is available. Each successful call to this method returns a new 
        /// buffer of data, at the current stream position.
        /// Buffers should be released as soon as they are not needed, using AK::IAkAutoStream::ReleaseBuffer().
        /// \aknote AK::IAkAutoStream::ReleaseBuffer() does not take any argument, because it releases buffers in order. \endaknote
        /// \return
        ///     - AK_DataReady     : the buffer was granted
		///     - AK_NoDataReady   : the buffer is not granted yet (never happens when called with in_bWait flag)
        ///     - AK_NoMoreData    : the buffer was granted but reached end of file (next call will return with size 0)
        ///     - AK_Fail          : there was an I/O error
		/// \sa
		/// - \ref streamingdevicemanager
        virtual AKRESULT GetBuffer(
            void *&         out_pBuffer,        ///< Returned address of granted data space
            AkUInt32 &      out_uSize,          ///< Returned size of granted data space
            bool            in_bWait            ///< Block until data is ready
            ) = 0;

        /// Release buffer granted through GetBuffer(). Buffers are released in order.
        /// \return AK_Success if a valid buffer was released, AK_Fail if the user did not own any buffer.
		/// \note To implementers: Clients like the sound engine may release buffers until this function returns AK_Fail.
		/// Failing to release a buffer should not be considered as a fatal error.
		/// \sa
		/// - \ref streamingdevicemanager
        virtual AKRESULT ReleaseBuffer() = 0;
        //@}
    };

    //@}


    /// Interface of the Stream Manager.
	/// \warning The functions in this interface are not thread-safe, unless stated otherwise.
    class IAkStreamMgr
    {
    protected:
    	/// Virtual destructor on interface to avoid warnings.
		virtual ~IAkStreamMgr(){}
		
	public:
        /// Global access to singleton.
        /// \return The interface of the global Stream Manager
		/// \sa
		/// - \ref streamingdevicemanager
        inline static IAkStreamMgr * Get()
        {
            return m_pStreamMgr;
        }

        /// Destroy the Stream Manager.
		/// \sa
		/// - \ref streamingdevicemanager
        virtual void     Destroy() = 0;


        /// \name Profiling.
        //@{
        /// Get the profiling interface.
        /// \return The interface of the Stream Manager profiler
        virtual IAkStreamMgrProfile * GetStreamMgrProfile() = 0;
        //@}


        /// \name Stream creation interface.
        //@{
        
        // Standard stream creation methods.

        /// Create a standard stream (string overload).
        /// \return AK_Success if the stream was created successfully
        /// \remarks The string overload of AK::StreamMgr::IAkFileLocationResolver::Open() will be called.
		/// \sa
		/// - \ref streamingdevicemanager
        virtual AKRESULT CreateStd(
            const AkOSChar*     in_pszFileName,     ///< Application-defined string (title only, or full path, or code...)
            AkFileSystemFlags * in_pFSFlags,        ///< Special file system flags. Can pass NULL
            AkOpenMode          in_eOpenMode,       ///< Open mode (read, write, ...)
            IAkStdStream *&     out_pStream,		///< Returned interface to a standard stream. If the function does not return AK_Success, this pointer is left untouched.
			bool				in_bSyncOpen		///< If true, force the Stream Manager to open file synchronously. Otherwise, it is left to its discretion.
            ) = 0;

        /// Create a standard stream (ID overload).
        /// \return AK_Success if the stream was created successfully
        /// \remarks The ID overload of AK::StreamMgr::IAkFileLocationResolver::Open() will be called.
		/// \sa
		/// - \ref streamingdevicemanager
        virtual AKRESULT CreateStd(
            AkFileID            in_fileID,          ///< Application-defined ID
            AkFileSystemFlags * in_pFSFlags,        ///< Special file system flags (can pass NULL)
            AkOpenMode          in_eOpenMode,       ///< Open mode (read, write, ...)
            IAkStdStream *&     out_pStream,		///< Returned interface to a standard stream. If the function does not return AK_Success, this pointer is left untouched.
			bool				in_bSyncOpen		///< If true, force the Stream Manager to open file synchronously. Otherwise, it is left to its discretion.
            ) = 0;

        
        // Automatic stream create methods.

        /// Create an automatic stream (string overload).
        /// \return AK_Success if the stream was created successfully
        /// \remarks The stream needs to be started explicitly with AK::IAkAutoStream::Start().
		/// \remarks The string overload of AK::StreamMgr::IAkFileLocationResolver::Open() will be called.
		/// \sa
		/// - \ref streamingdevicemanager
        virtual AKRESULT CreateAuto(
            const AkOSChar*             in_pszFileName,     ///< Application-defined string (title only, or full path, or code...)
            AkFileSystemFlags *         in_pFSFlags,        ///< Special file system flags (can pass NULL)
            const AkAutoStmHeuristics & in_heuristics,      ///< Streaming heuristics
            AkAutoStmBufSettings *      in_pBufferSettings, ///< Stream buffer settings (it is recommended to pass NULL in order to use the default settings)
            IAkAutoStream *&            out_pStream,		///< Returned interface to an automatic stream. If the function does not return AK_Success, this pointer is left untouched.
			bool						in_bSyncOpen		///< If true, force the Stream Manager to open file synchronously. Otherwise, it is left to its discretion.
            ) = 0;

        /// Create an automatic stream (ID overload).
        /// \return AK_Success if the stream was created successfully
        /// \remarks The stream needs to be started explicitly with IAkAutoStream::Start().
        /// \remarks The ID overload of AK::StreamMgr::IAkFileLocationResolver::Open() will be called.
		/// \sa
		/// - \ref streamingdevicemanager
        virtual AKRESULT CreateAuto(
            AkFileID                    in_fileID,          ///< Application-defined ID
            AkFileSystemFlags *         in_pFSFlags,        ///< Special file system flags (can pass NULL)
            const AkAutoStmHeuristics & in_heuristics,      ///< Streaming heuristics
            AkAutoStmBufSettings *      in_pBufferSettings, ///< Stream buffer settings (it is recommended to pass NULL to use the default settings)
			IAkAutoStream *&            out_pStream,		///< Returned interface to an automatic stream. If the function does not return AK_Success, this pointer is left untouched.
			bool						in_bSyncOpen		///< If true, force the Stream Manager to open file synchronously. Otherwise, it is left to its discretion.
            ) = 0;

		/// Start streaming the first "in_pFSFlags->uNumBytesPrefetch" bytes of the file with id "in_fileID" into cache.  The stream will be scheduled only after
		/// all regular streams (not file caching streams) are serviced.  The file will stay cached until either the UnpinFileInCache is called,
		/// or the limit as set by uMaxCachePinnedBytes is reached and another higher priority file (in_uPriority) needs the space.  
		/// \sa
		/// - \ref streamingdevicemanager
		virtual AKRESULT PinFileInCache(
			AkFileID                    in_fileID,          ///< Application-defined ID
			AkFileSystemFlags *         in_pFSFlags,        ///< Special file system flags (can NOT pass NULL)
			AkPriority					in_uPriority		///< Stream caching priority.  Note: Caching requests only get serviced after all regular streaming requests.
			) = 0;

		/// Un-pin a file that has been previouly pinned into cache.  This function must be called once for every call to PinFileInCache() with the same file id.
		/// The file may still remain in stream cache after this is called, until the memory is reused by the streaming memory manager in accordance with to its 
		/// cache management algorithm.
		/// \sa
		/// - \ref streamingdevicemanager
		virtual AKRESULT UnpinFileInCache(
			AkFileID                    in_fileID,          	///< Application-defined ID
			AkPriority					in_uPriority 		///< Priority of stream that you are unpinning
			) = 0;
		
		/// Update the priority of the caching stream.  Higher priority streams will be serviced before lower priority caching streams, and will be more likely to stay in 
		/// memory if the cache pin limit as set by "uMaxCachePinnedBytes" is reached.
		/// \sa
		/// - \ref streamingdevicemanager
		virtual AKRESULT UpdateCachingPriority(
			AkFileID                    in_fileID,			///< Application-defined ID
			AkPriority					in_uPriority, 		///< Priority
			AkPriority					in_uOldPriority 		///< Old priority
			) = 0;
		
		/// Return information about a file that has been pinned into cache.
		/// Retrieves the percentage of the requested buffer size that has been streamed in and stored into stream cache, and whether 
		/// the cache pinned memory limit is preventing this buffer from filling.
		virtual AKRESULT GetBufferStatusForPinnedFile( 
			AkFileID in_fileID,								///< Application-defined ID
			AkReal32& out_fPercentBuffered,					///< Percentage of buffer full (out of 100)
			bool& out_bCacheFull							///< Set to true if the rest of the buffer can not fit into the cache-pinned memory limit.
			) = 0;

		//@}

    protected:
        /// Definition of the global pointer to the interface of the Stream Manager singleton.
		/// \sa
		/// - \ref streamingdevicemanager
        static AKSOUNDENGINE_API IAkStreamMgr * m_pStreamMgr;
    };

}
#endif //_IAK_STREAM_MGR_H_
