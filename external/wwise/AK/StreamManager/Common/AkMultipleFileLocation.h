//////////////////////////////////////////////////////////////////////
//
// AkMultipleFileLocation.h
//
// File location resolving: Supports multiple base paths for file access, searched in reverse order.
// For more details on resolving file location, refer to section "File Location" inside
// "Going Further > Overriding Managers > Streaming / Stream Manager > Low-Level I/O"
// of the SDK documentation. 
//
// Copyright (c) 2014 Audiokinetic Inc. / All Rights Reserved
//
//////////////////////////////////////////////////////////////////////

#ifndef _AK_MULTI_FILE_LOCATION_H_
#define _AK_MULTI_FILE_LOCATION_H_

struct AkFileSystemFlags;

#include <AK/SoundEngine/Common/IAkStreamMgr.h>
#include <AK/SoundEngine/Common/AkStreamMgrModule.h>
#include <AK/Tools/Common/AkListBareLight.h>


// This file location class supports multiple base paths for Wwise file access.
// Each path will be searched the reverse order of the addition order until the file is found.
template<class OPEN_POLICY>
class CAkMultipleFileLocation
{
protected:

	// Internal user paths.
	struct FilePath
	{
		FilePath *pNextLightItem;
		AkOSChar szPath[1];	//Variable length
	};
public:
	CAkMultipleFileLocation();
	void Term();

	//
	// Global path functions.
	// ------------------------------------------------------

	// Base path is prepended to all file names.
	// Audio source path is appended to base path whenever uCompanyID is AK and uCodecID specifies an audio source.
	// Bank path is appended to base path whenever uCompanyID is AK and uCodecID specifies a sound bank.
	// Language specific dir name is appended to path whenever "bIsLanguageSpecific" is true.
	AKRESULT SetBasePath(const AkOSChar*   in_pszBasePath)
	{
		return AddBasePath(in_pszBasePath);
	}

	AKRESULT AddBasePath(const AkOSChar*   in_pszBasePath);

	AKRESULT Open( 
		const AkOSChar* in_pszFileName,     // File name.
		AkOpenMode      in_eOpenMode,       // Open mode.
		AkFileSystemFlags * in_pFlags,      // Special flags. Can pass NULL.
		bool			in_bOverlapped,		// Overlapped IO open
		AkFileDesc &    out_fileDesc        // Returned file descriptor.
		);

	
	AKRESULT Open( 
		AkFileID        in_fileID,          // File ID.
		AkOpenMode      in_eOpenMode,       // Open mode.
		AkFileSystemFlags * in_pFlags,      // Special flags. Can pass NULL.
		bool			in_bOverlapped,		// Overlapped IO open
		AkFileDesc &    out_fileDesc        // Returned file descriptor.
		);
	
	//
	// Path resolving services.
	// ------------------------------------------------------

	// String overload.
	// Returns AK_Success if input flags are supported and the resulting path is not too long.
	// Returns AK_Fail otherwise.
	AKRESULT GetFullFilePath(
		const AkOSChar *	in_pszFileName,		// File name.
		AkFileSystemFlags * in_pFlags,			// Special flags. Can be NULL.
		AkOpenMode			in_eOpenMode,		// File open mode (read, write, ...).
		AkOSChar *			out_pszFullFilePath, // Full file path.
		FilePath*			in_pBasePath = NULL	// Base path to use.  If null, the first suitable location will be given.		
		);  

protected:

	AkListBareLight<FilePath> m_Locations;
};

#include "AkMultipleFileLocation.inl"

#endif //_AK_MULTI_FILE_LOCATION_H_
