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
/// AK::IReadBytes, AK::IWriteBytes simple serialization interfaces.

#ifndef _AK_IBYTES_H
#define _AK_IBYTES_H

#include <wchar.h>
#include <AK/Tools/Common/AkPlatformFuncs.h> 

namespace AK
{
	/// Generic binary input interface.
	/// \warning The functions in this interface are not thread-safe, unless stated otherwise.
	class IReadBytes
	{
	public:
		////////////////////////////////////////////////////////////////////////
		/// @name Interface
		//@{

		/// Reads some bytes into a buffer.
		/// \return	True if the operation was successful, False otherwise
		virtual bool ReadBytes( 
			void * in_pData,		///< Pointer to a buffer
			AkInt32 in_cBytes,			///< Size of the buffer (in bytes)
			AkInt32 & out_cRead 		///< Returned number of read bytes
			) = 0;

		//@}

		////////////////////////////////////////////////////////////////////////
		/// @name Helpers
		//@{

		/// Reads a simple type or structure.
		/// \warning Not for object serialization.
		/// \return	True if the operation was successful, False otherwise.
		template<class T>
		bool Read( 
			T & out_data )	///< Data to be read
		{
			AkInt32 cRead;
			return ReadBytes( &out_data, sizeof( T ), cRead );
		}

		/// Reads a simple type or structure.
		/// \warning This method does not allow for error checking. Use other methods when error cases need to be handled.
		/// \warning Not for object serialization.
		/// \return	Read data
		template<class T>
		T Read()
		{
			T value;

			AkInt32 cRead;
			ReadBytes( &value, sizeof( T ), cRead );

			return value;
		}

		/// Reads a unicode string into a fixed-size buffer. 
		/// \return	True if the operation was successful, False otherwise. An insufficient buffer size does not cause failure.
		bool ReadString( 
			wchar_t * out_pszString,	///< Pointer to a fixed-size buffer
			AkInt32 in_nMax )			///< Maximum number of characters to be read in out_pszString, including the terminating NULL character
		{
			AkInt32 cChars;
			if ( !Read<AkInt32>( cChars ) ) 
				return false;

			bool bRet = true;

			if ( cChars > 0 )
			{
				AkInt32 cRead;

				if ( cChars < in_nMax )
				{
					ReadBytes( out_pszString, cChars * sizeof( wchar_t ), cRead );
					out_pszString[ cChars ] = 0;

					bRet = cRead == (AkInt32)( cChars * sizeof( wchar_t ) );
				}
				else
				{
					ReadBytes( out_pszString, in_nMax * sizeof( wchar_t ), cRead );
					out_pszString[ in_nMax - 1 ] = 0;

					bRet = cRead == (AkInt32)( in_nMax * sizeof(wchar_t));

					if ( bRet )
					{
						// Read extra characters in temp buffer.
						AkInt32 cRemaining = cChars - in_nMax;

						wchar_t * pTemp = new wchar_t[ cRemaining ];

						ReadBytes( pTemp, cRemaining * sizeof( wchar_t ), cRead );

						bRet = cRead == (AkInt32)(cRemaining * sizeof(wchar_t));

						delete [] pTemp;
					}
				}
			}
			else
			{
				out_pszString[ 0 ] = 0;
			}

			return bRet;
		}
		//@}
	};

	/// Generic binary output interface.
	/// \warning The functions in this interface are not thread-safe, unless stated otherwise.
	class IWriteBytes
	{
	public:
		////////////////////////////////////////////////////////////////////////
		/// @name Interface
		//@{

		/// Writes some bytes from a buffer.
		/// \return	True if the operation was successful, False otherwise
		virtual bool WriteBytes( 
			const void * in_pData,	///< Pointer to a buffer
			AkInt32 in_cBytes, 		///< Size of the buffer (in bytes)
			AkInt32 & out_cWritten		///< Returned number of written bytes
			) = 0;

		//@}

		////////////////////////////////////////////////////////////////////////
		/// @name Helpers
		//@{

		/// Writes a simple type or struct.
		/// \warning Not for object serialization.
		/// \return	True if the operation was successful, False otherwise
		template<class T>
		bool Write( 
			const T & in_data )		///< Data to be written
		{
			AkInt32 cWritten;
			return WriteBytes( &in_data, sizeof( T ), cWritten );
		}

		/// Writes a unicode string. 
		/// \return	True if the operation was successful, False otherwise
		bool WriteString( 
			const wchar_t * in_pszString )	///< String to be written
		{
			if ( in_pszString != NULL )
			{
				size_t cChars = wcslen( in_pszString );
				if ( !Write<AkUInt32>( (AkUInt32) cChars ) )
					return false;

				AkInt32 cWritten = 0;
				AkInt32 cToWrite = (AkInt32)( cChars * sizeof( wchar_t ) );

				if ( cChars > 0 )
				{
					WriteBytes( in_pszString, cToWrite, cWritten );
				}

				return cWritten == cToWrite;
			}
			return Write<AkUInt32>( 0 );
		}

		/// Writes an ansi string. 
		/// \return	True if the operation was successful, False otherwise
		bool WriteString( 
			const char * in_pszString )	///< String to be written
		{
			if ( in_pszString != NULL )
			{
				size_t cChars = strlen( in_pszString );
				if ( !Write<AkUInt32>( (AkUInt32) cChars ) )
					return false;

				AkInt32 cWritten = 0;

				if ( cChars > 0 )
				{
					WriteBytes( in_pszString, (AkInt32) cChars, cWritten );
				}

				return cWritten == (AkInt32) cChars;
			}
			return Write<AkUInt32>( 0 );
		}
		//@}
	};

	/// Generic memory buffer interface.
	/// \warning The functions in this interface are not thread-safe, unless stated otherwise.
	class IWriteBuffer : public IWriteBytes
	{
	public:
		////////////////////////////////////////////////////////////////////////
		/// @name Interface
		//@{

		/// Get the number of bytes written to the buffer.
		/// \return	number of bytes written.
		virtual AkInt32 Count() const = 0;

		/// Get pointer to buffer.
		/// \return pointer to buffer.
		virtual AkUInt8 * Bytes() const = 0;

		/// Set number of bytes written.
		virtual void SetCount( AkInt32 in_cBytes ) = 0;

		/// Allocate memory.
		/// \return true if allocation was successful.
		virtual bool Reserve( AkInt32 in_cBytes ) = 0;

		/// Clear the buffer contents.
		virtual void Clear() = 0;

		/// Return pointer to buffer and clear internal pointer.
		virtual AkUInt8 * Detach() = 0;

		//@}
	};
}

#endif // _AK_IBYTES_H
