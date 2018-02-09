/*******************************************************************************
The content of this file includes portions of the AUDIOKINETIC Wwise Technology
released in source code form as part of the SDK installer package.

Commercial License Usage

Licensees holding valid commercial licenses to the AUDIOKINETIC Wwise Technology
may use this file in accordance with the end user license agreement provided 
with the software or, alternatively, in accordance with the terms contained in a
written agreement between you and Audiokinetic Inc.

  Version: v2017.2.1  Build: 6524
  Copyright (c) 2006-2018 Audiokinetic Inc.
*******************************************************************************/
//////////////////////////////////////////////////////////////////////
//
// AkFileLocationBase.cpp
//
// Basic file location resolving: Uses simple path concatenation logic.
// Exposes basic path functions for convenience.
// For more details on resolving file location, refer to section "File Location" inside
// "Going Further > Overriding Managers > Streaming / Stream Manager > Low-Level I/O"
// of the SDK documentation. 
//
//////////////////////////////////////////////////////////////////////

#include <AK/SoundEngine/Common/AkTypes.h>
#include <AK/Tools/Common/AkAssert.h>

#include <AK/SoundEngine/Common/AkStreamMgrModule.h>
#include <AK/Tools/Common/AkPlatformFuncs.h>
#ifdef AK_SUPPORT_WCHAR
#include <wchar.h>
#endif //AK_SUPPORT_WCHAR
#include <stdio.h>
#include <AK/Tools/Common/AkAssert.h>
#include <AK/Tools/Common/AkObject.h>

#include "AkFileHelpers.h"

#define MAX_NUMBER_STRING_SIZE      (10)    // 4G
#define ID_TO_STRING_FORMAT_BANK    AKTEXT("%u.bnk")
#define ID_TO_STRING_FORMAT_WEM     AKTEXT("%u.wem")
#define MAX_EXTENSION_SIZE          (4)     // .xxx
#define MAX_FILETITLE_SIZE          (MAX_NUMBER_STRING_SIZE+MAX_EXTENSION_SIZE+1)   // null-terminated

template<class OPEN_POLICY>
CAkMultipleFileLocation<OPEN_POLICY>::CAkMultipleFileLocation()
{
}

template<class OPEN_POLICY>
void CAkMultipleFileLocation<OPEN_POLICY>::Term()
{
	if (!m_Locations.IsEmpty())
	{
		FilePath *p = (*m_Locations.Begin());
		while (p)
		{
			FilePath *next = p->pNextLightItem;
			AkDelete(AK::StreamMgr::GetPoolID(), p);
			p = next;
		}
	}
	m_Locations.Term();
}

template<class OPEN_POLICY>
AKRESULT CAkMultipleFileLocation<OPEN_POLICY>::Open( 
										const AkOSChar* in_pszFileName,     // File name.
										AkOpenMode      in_eOpenMode,       // Open mode.
										AkFileSystemFlags * in_pFlags,      // Special flags. Can pass NULL.
										bool			in_bOverlapped,		// Overlapped IO open
										AkFileDesc &    out_fileDesc        // Returned file descriptor.
										)
{	
	for(typename AkListBareLight<FilePath>::Iterator it = m_Locations.Begin(); it != m_Locations.End();++it)
	{
		// Get the full file path, using path concatenation logic.
		AkOSChar szFullFilePath[AK_MAX_PATH];
		if ( GetFullFilePath( in_pszFileName, in_pFlags, in_eOpenMode, szFullFilePath, (*it) ) == AK_Success )
		{
			AKRESULT res = OPEN_POLICY::Open(szFullFilePath, in_eOpenMode, in_bOverlapped, out_fileDesc);		
			if (res == AK_Success)
			{
				//These must be set by the OpenPolicy
				AKASSERT(out_fileDesc.hFile != NULL );
				AKASSERT(out_fileDesc.iFileSize != 0 && (in_eOpenMode == AK_OpenModeRead || in_eOpenMode == AK_OpenModeReadWrite) || !(in_eOpenMode == AK_OpenModeRead || in_eOpenMode == AK_OpenModeReadWrite));
				return AK_Success;
			}			
		}
	}
	return AK_Fail;    
}

template<class OPEN_POLICY>
AKRESULT CAkMultipleFileLocation<OPEN_POLICY>::Open( 
										AkFileID        in_fileID,          // File ID.
										AkOpenMode      in_eOpenMode,       // Open mode.
										AkFileSystemFlags * in_pFlags,      // Special flags. Can pass NULL.
										bool			in_bOverlapped,		// Overlapped IO open
										AkFileDesc &    out_fileDesc        // Returned file descriptor.
										)
{
	if ( !in_pFlags ||
		!(in_pFlags->uCompanyID == AKCOMPANYID_AUDIOKINETIC || in_pFlags->uCompanyID == AKCOMPANYID_AUDIOKINETIC_EXTERNAL))
	{
		AKASSERT( !"Unhandled file type" );
		return AK_Fail;
	}

	AkOSChar pszTitle[MAX_FILETITLE_SIZE+1];
	if ( in_pFlags->uCodecID == AKCODECID_BANK )
		AK_OSPRINTF( pszTitle, MAX_FILETITLE_SIZE, ID_TO_STRING_FORMAT_BANK, (unsigned int)in_fileID );
	else
		AK_OSPRINTF( pszTitle, MAX_FILETITLE_SIZE, ID_TO_STRING_FORMAT_WEM, (unsigned int)in_fileID );

	return Open(pszTitle, in_eOpenMode, in_pFlags, in_bOverlapped, out_fileDesc);
}

// String overload.
// Returns AK_Success if input flags are supported and the resulting path is not too long.
// Returns AK_Fail otherwise.
template<class OPEN_POLICY>
AKRESULT CAkMultipleFileLocation<OPEN_POLICY>::GetFullFilePath(
	const AkOSChar*		in_pszFileName,		// File name.
	AkFileSystemFlags * in_pFlags,			// Special flags. Can be NULL.
	AkOpenMode			in_eOpenMode,		// File open mode (read, write, ...).
	AkOSChar*			out_pszFullFilePath, // Full file path.
	FilePath*			in_pBasePath		// Base path to use, might be null	
	)
{
    if ( !in_pszFileName )
    {
        AKASSERT( !"Invalid file name" );
        return AK_InvalidParameter;
    }

	// Prepend string path (basic file system logic).

    // Compute file name with file system paths.
	size_t uiPathSize = AKPLATFORM::OsStrLen( in_pszFileName );

    if ( uiPathSize >= AK_MAX_PATH )
	{
		AKASSERT( !"Input string too large" );
		return AK_InvalidParameter;
	}

	if (in_pBasePath == NULL)
		in_pBasePath = (*m_Locations.Begin());

	AKPLATFORM::SafeStrCpy( out_pszFullFilePath, in_pBasePath->szPath, AK_MAX_PATH );
    if ( in_pFlags && in_eOpenMode == AK_OpenModeRead )
    {        
		// Add language directory name if needed.
		if ( in_pFlags->bIsLanguageSpecific )
		{
			size_t uLanguageStrLen = AKPLATFORM::OsStrLen( AK::StreamMgr::GetCurrentLanguage() );
			if ( uLanguageStrLen > 0 )
			{
				uiPathSize += ( uLanguageStrLen + 1 );
				if ( uiPathSize >= AK_MAX_PATH )
				{
					AKASSERT( !"Path is too large" );
					return AK_Fail;
				}
				AKPLATFORM::SafeStrCat( out_pszFullFilePath, AK::StreamMgr::GetCurrentLanguage(), AK_MAX_PATH );
				AKPLATFORM::SafeStrCat( out_pszFullFilePath, AK_PATH_SEPARATOR, AK_MAX_PATH );
			}
		}
	}
        
    // Append file title.
	uiPathSize += AKPLATFORM::OsStrLen( out_pszFullFilePath );
	if ( uiPathSize >= AK_MAX_PATH )
	{
		AKASSERT( !"File name string too large" );
		return AK_Fail;
	}
	AKPLATFORM::SafeStrCat( out_pszFullFilePath, in_pszFileName, AK_MAX_PATH );
	return AK_Success;
}

template<class OPEN_POLICY>
AKRESULT CAkMultipleFileLocation<OPEN_POLICY>::AddBasePath(const AkOSChar* in_pszBasePath)
{
	AkUInt32 origLen = (AkUInt32)AKPLATFORM::OsStrLen(in_pszBasePath);
    AkUInt32 newByteSize = origLen + 1;	// One for the trailing \0
    if(in_pszBasePath[origLen - 1] != AK_PATH_SEPARATOR[0])
    {
        newByteSize++; // Add space for a trailing slash
    }

    // Make sure the new base path is not too long in case language gets appended
    // Format of the test is: basePath/Language/.
	if ( origLen + 1 + AKPLATFORM::OsStrLen( AK::StreamMgr::GetCurrentLanguage() + 1 ) >= AK_MAX_PATH )
		return AK_InvalidParameter;

	FilePath * pPath = (FilePath*)AkAlloc(AK::StreamMgr::GetPoolID(), sizeof(FilePath) + sizeof(AkOSChar)*(newByteSize - 1));
	if (pPath == NULL)
		return AK_InsufficientMemory;

	// Copy the base path EVEN if the directory does not exist.
	AKPLATFORM::SafeStrCpy( pPath->szPath, in_pszBasePath, origLen + 1);
    
    // Add the trailing slash, if necessary
	if (in_pszBasePath[origLen - 1] != AK_PATH_SEPARATOR[0])
	{
		pPath->szPath[origLen] = AK_PATH_SEPARATOR[0];
		pPath->szPath[origLen + 1] = 0;
	}
	pPath->pNextLightItem = NULL;
	m_Locations.AddFirst(pPath);

	AKRESULT eDirectoryResult = CAkFileHelpers::CheckDirectoryExists( in_pszBasePath );
	if( eDirectoryResult == AK_Fail ) // AK_NotImplemented could be returned and should be ignored.
	{
		return AK_PathNotFound;
	}

	return AK_Success;
}
