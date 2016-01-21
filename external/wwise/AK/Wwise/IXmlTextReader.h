//////////////////////////////////////////////////////////////////////
//
// Copyright (c) 2006 Audiokinetic Inc. / All Rights Reserved
//
//////////////////////////////////////////////////////////////////////

/// \file
/// Persistance interface for Wwise plugins.

#ifndef AK_XML_READER
#define AK_XML_READER

namespace AK
{
	/// 
	namespace XmlWhiteSpaceHandling
	{
		/// See MSDN documentation
		enum WhiteSpaceHandling
		{
			All,
			None,
			Significant
		};
	}

	/// Types of possible XML elements.  See MSDN documentation topics for XmlNodeType.
	namespace XmlNodeType
	{
		/// See MSDN documentation topics for XmlNodeType.
		/// MUST match XmlLite node types
		enum NodeType
		{
			Attribute = 2,
			CDATA = 4,
			Comment = 8,
			Document = 9,
			DocumentFragment = 11,
			DocumentType = 10,
			Element = 1,
			EndElement = 15,
			EndEntity = 16,
			Entity = 6,
			EntityReference = 5,
			None = 0,
			Notation = 12,
			ProcessingInstruction = 7,
			SignificantWhitespace = 14,
			Text = 3,
			Whitespace = 13,
			XmlDeclaration = 17
		};
	}

	/// Interface for plugin persistance.
	/// \sa AK::Wwise::IAudioPlugin::Load
	class IXmlTextReader
	{
	public:
		/// Destroys the text reader.  You should not call this function.
		virtual void Destroy() = 0;

		// Properties

		/// Gets the name of the current element
		virtual CString GetName() const = 0;

		/// Gets the current node type 
		/// \ref AK::XmlNodeType
		virtual XmlNodeType::NodeType GetNodeType() const = 0;

		/// Tests if the current element is empty
		/// \return True if the current element is empty, false otherwise
		virtual bool IsEmptyElement() const = 0;

		/// Gets the string representation of the current element's value
		/// \return The value
		virtual CString GetValue() const = 0;

		/// \return True if the end of XML document was reached
		virtual bool IsEOF() const = 0;

		/// Gets the current element's line number.  If the element is multiline, the start line is given.
		/// \return the element's line number
		virtual int GetLineNumber() const = 0;

		/// Gets the current element's horizontal position.  
		/// \return the element column
		virtual int GetLinePosition() const = 0;

		// Methods

		/// Checks whether the current node is a content (non-white space text, CDATA, Element, EndElement,
		/// EntityReference, or EndEntity) node. If the node is not a content node, the reader skips ahead
		/// to the next content node or end of file. It skips over nodes of the following type: 
		/// ProcessingInstruction, DocumentType, Comment, Whitespace, or SignificantWhitespace.
		/// \return The NodeType of the current node found by the method or XmlNodeType.None if the reader has reached the end of the input stream. 
		virtual AK::XmlNodeType::NodeType MoveToContent() = 0;

		/// Teads the next node from the stream. 
		/// \return true if the next node was read successfully; false if there are no more nodes to read. 
		virtual bool Read() = 0;

		/// This is a helper method for reading simple text-only elements. 
		/// \return The text contained in the element that was read. An empty string if the element is empty. 
		virtual CString ReadElementString( const CString& in_rcsElementNameValidation ) = 0;

		/// Reads the element and decodes the BinHex content. 
		/// \return The number of bytes written to the buffer. 
		virtual int ReadBinHex( BYTE* io_pBuffer,	/// The buffer into which to copy the resulting text. This value cannot be NULL.
								int in_offset,		/// The offset into the buffer where to start copying the result.
								int in_length		/// The maximum number of bytes to copy into the buffer. The actual number of bytes copied is returned from this method.
								) = 0;

		/// Reads all the content, including markup, as a string. 
		/// \return All the XML content, including markup, in the current node. If the current node has no children, an empty string is returned. If the current node is neither an element nor attribute, an empty string is returned. 
		virtual void ReadInnerXml( CString& out_csXml ) = 0;

		/// Reads the content, including markup, representing this node and all its children. 
		/// \return If the reader is positioned on an element or an attribute node, this method returns all the XML content, including markup, of the current node and all its children; otherwise, it returns an empty string. 
		virtual void ReadOuterXml( CString& out_csXml ) = 0;

		/// Skips the children of the current node.
		virtual void Skip() = 0;

		/// Gets the value of an attribute. 
		/// \return The attribute value
		virtual CString GetAttribute( const CString& in_rcsAttributeName /// The attribute name
			) = 0;
		
		/// Gets the value of an attribute. 
		/// \return The true if value exists
		virtual bool GetAttribute( 
			const CString& in_rcsAttributeName, /// The attribute name
			CString& out_rcsValue/// The attribute name
			) = 0;	
	};
}

#endif