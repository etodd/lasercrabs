//////////////////////////////////////////////////////////////////////
//
// Copyright (c) 2006 Audiokinetic Inc. / All Rights Reserved
//
//////////////////////////////////////////////////////////////////////

/// \file
/// Persistance interface for Wwise plugins.

#ifndef AK_XML_WRITER
#define AK_XML_WRITER

namespace AK
{
	///
	namespace XmlWriteState
	{
		/// WriteState
		enum WriteState
		{
			Attribute,	///< Attribute
			Closed,		///< Closed	
			Content,	///< Content
			Element,	///< Element
			Prolog,		///< Prolog	
			Start		///< Start	
		};
	}

	/// Possible error codes when writing XML
	namespace XmlWriteReady
	{
		/// Possible error codes when writing XML
		enum WriteReady
		{
			Ready,				///< No error
			ErrorPathTooLong,	///< Path exceeds max length
			ErrorAccessDenied,	///< Cannot open file due to access permissions
			ErrorUnknown		///< Unknown error
		};
	}

	namespace XmlElementType
	{
		// These element types have an impact when outputting in alternate formats such as JSON.
		enum ElementType
		{
			Map,		// Associative (Unique Keys)
			Array,		// Enumerative
			MultiMap	// Associative (Shared Keys)
		};
	};

	/// Interface for plugin persistance.
	/// \sa AK::Wwise::IAudioPlugin::Save
	class IXmlTextWriter
	{
	public:
		/// Destroys the text writer.  You should not call this function.
		virtual void Destroy() = 0;

		/// \return True if the writer is ready.
		virtual bool IsReady() const = 0;

		virtual XmlWriteReady::WriteReady GetReadyState() const = 0;

		virtual bool Append( IXmlTextWriter* in_pWriterToAppend ) = 0;

		// Properties
		virtual XmlWriteState::WriteState GetWriteState() const = 0;

		// Methods
		/// Writes the XML declaration. 
		virtual void WriteStartDocument() = 0;

		/// Closes one element and pops the corresponding namespace scope.
		virtual void WriteEndDocument() = 0;

		/// Writes out a start tag with the specified local name. 
		virtual void WriteStartElement( const CString& in_rcsElementName, /// The local name of the element. 
										XmlElementType::ElementType in_eType = XmlElementType::Map /// Element type.
										) = 0;

		/// Closes one element and pops the corresponding namespace scope.
		virtual void WriteEndElement() = 0;

		/// Writes an attribute with the specified value. 
		virtual void WriteAttributeString(	const CString& in_rcsAttribute, /// The local name of the attribute. 
											const CString& in_rcsValue		/// The value of the attribute. 
											) = 0;

		/// Writes the given text content. 
		virtual void WriteString( const CString& in_rcsValue	/// The text to write. 
									) = 0;

		/// Writes out a <![CDATA[...]]> block containing the specified text. 
		virtual void WriteCData( const CString& in_rcsValue		/// The text to place inside the CDATA block. 
									) = 0;

		/// Encodes the specified binary bytes as BinHex and writes out the resulting text. 
		virtual void WriteBinHex(	BYTE* in_pBuffer,	/// Byte array to encode. 
									int in_index,		/// The position in the buffer indicating the start of the bytes to write.
									int in_count		/// The number of bytes to write.
									) = 0;

		/// Writes raw markup manually. 
		virtual void WriteRaw( const CString& in_rcsValue	/// String containing the text to write
								) = 0;

		// Helpers
		/// Use this class to handle the WriteStartElement/WriteEndElement pair automatically in a C++ scope.
		class AutoStartEndElement
		{
		public:
		
			/// Calls WriteStartElement automatically
			AutoStartEndElement( const CString& in_rcsElementName, IXmlTextWriter* in_pWriter, XmlElementType::ElementType in_eType = XmlElementType::Map )
				: m_pWriter( in_pWriter )
			{
				m_pWriter->WriteStartElement( in_rcsElementName, in_eType );
			}
			
			/// Calls WriteEndElement automatically
			~AutoStartEndElement()
			{
				m_pWriter->WriteEndElement();
			}

		private:
			IXmlTextWriter* m_pWriter;
		};
	};
}

#endif