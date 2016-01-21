//////////////////////////////////////////////////////////////////////
//
// Copyright (c) 2006 Audiokinetic Inc. / All Rights Reserved
//
//////////////////////////////////////////////////////////////////////

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
			long in_cBytes,			///< Size of the buffer (in bytes)
			long & out_cRead 		///< Returned number of read bytes
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
			long cRead;
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

			long cRead;
			ReadBytes( &value, sizeof( T ), cRead );

			return value;
		}

		/// Reads a unicode string into a fixed-size buffer. 
		/// \return	True if the operation was successful, False otherwise. An insufficient buffer size does not cause failure.
		bool ReadString( 
			wchar_t * out_pszString,	///< Pointer to a fixed-size buffer
			long in_nMax )			///< Maximum number of characters to be read in out_pszString, including the terminating NULL character
		{
			long cChars;
			if ( !Read<long>( cChars ) ) 
				return false;

			bool bRet = true;

			if ( cChars > 0 )
			{
				long cRead;

				if ( cChars < in_nMax )
				{
					ReadBytes( out_pszString, cChars * sizeof( wchar_t ), cRead );
					out_pszString[ cChars ] = 0;

					bRet = cRead == (long)( cChars * sizeof( wchar_t ) );
				}
				else
				{
					ReadBytes( out_pszString, in_nMax * sizeof( wchar_t ), cRead );
					out_pszString[ in_nMax - 1 ] = 0;

					bRet = cRead == (long)( cChars * sizeof( wchar_t ) );

					if ( bRet )
					{
						// Read extra characters in temp buffer.
						long cRemaining = cChars - in_nMax;

						wchar_t * pTemp = new wchar_t[ cRemaining ];

						ReadBytes( pTemp, cRemaining * sizeof( wchar_t ), cRead );

						bRet = cRemaining == (long)( cChars * sizeof( wchar_t ) );

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
