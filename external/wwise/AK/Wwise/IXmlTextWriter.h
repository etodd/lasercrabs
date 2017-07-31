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