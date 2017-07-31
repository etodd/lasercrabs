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
/// Wwise source control containers interface that is used to pass data containers (list and map) in parameters.

#ifndef _AK_WWISE_SOURCECONTROLCONTAINERS_H
#define _AK_WWISE_SOURCECONTROLCONTAINERS_H

// Audiokinetic namespace
namespace AK
{
	// Audiokinetic Wwise namespace
	namespace Wwise
	{
		namespace SourceControlContainers
		{
			/// Container position
			struct __AkPos{};
			/// Pointer to a container position
			typedef __AkPos* AkPos;

			// IAkList
			/// Template parameters:
			/// - Type: Class of object stored in the list.
			/// - Arg_Type: Type used to reference objects stored in the list. Can be a reference.  By default, this is a reference to the type.
			///
			/// \warning The functions in this interface are not thread-safe, unless stated otherwise.
			///
			/// \aknote The class implementing this interface is a wrapper around the MFC \b CList class.  Documentation can be found on MSDN.
			/// \endaknote
			template <class Type, class Arg_Type = const Type&>
			class IAkList
			{
			public:
				virtual unsigned int GetCount() const = 0;
				virtual unsigned int GetSize() const = 0;
				virtual bool IsEmpty() const = 0;

				virtual AkPos AddHead( Arg_Type in_newElement ) = 0;
				virtual AkPos AddTail( Arg_Type in_newElement ) = 0;

				virtual void RemoveHead() = 0;
				virtual void RemoveTail() = 0;
				virtual void RemoveAt( AkPos in_position ) = 0;
				virtual void RemoveAll() = 0;

				virtual Type& GetHead() = 0;
				virtual const Type& GetHead() const = 0;
				virtual Type& GetTail() = 0;
				virtual const Type& GetTail() const = 0;
				virtual AkPos GetHeadPosition() const = 0;
				virtual AkPos GetTailPosition() const = 0;
				virtual Type& GetNext( AkPos& in_rPosition ) = 0;
				virtual const Type& GetNext( AkPos& in_rPosition ) const = 0;
				virtual Type& GetPrev( AkPos& in_rPosition ) = 0;
				virtual const Type& GetPrev( AkPos& in_rPosition ) const = 0;
				virtual Type& GetAt( AkPos in_position ) = 0;
				virtual const Type& GetAt( AkPos in_position ) const = 0;

				virtual void SetAt( AkPos in_pos, Arg_Type in_newElement ) = 0;
				virtual AkPos InsertBefore( AkPos in_position, Arg_Type in_newElement ) = 0;
				virtual AkPos InsertAfter( AkPos in_position, Arg_Type in_newElement ) = 0;
			};

			// IAkMap
			/// Template parameters:
			/// - Key: Class of the object used as the map key.
			/// - Arg_Key: Data type used for Key arguments.
			/// - Value: Class of the object stored in the map.
			/// - Arg_Value: Data type used for Value arguments; usually a reference to Value.
			///
			/// \warning The functions in this interface are not thread-safe, unless stated otherwise.
			///
			/// \aknote The class implementing this interface is a wrapper around the MFC \b CMap class.  Documentation can be found on MSDN.
			/// \endaknote
			template <class Key, class Arg_Key, class Value, class Arg_Value>
			class IAkMap
			{
			public:
				virtual unsigned int GetCount() const = 0;
				virtual unsigned int GetSize() const = 0;
				virtual bool IsEmpty() const = 0;

				virtual bool Lookup( Arg_Key in_key, Value& in_rValue ) const = 0;

				virtual Value& operator[]( Arg_Key in_key ) = 0;
				virtual void SetAt( Arg_Key in_key, Arg_Value in_newValue ) = 0;

				virtual bool RemoveKey( Arg_Key in_key ) = 0;
				virtual void RemoveAll() = 0;

				virtual AkPos GetStartPosition() const = 0;
				virtual void GetNextAssoc( AkPos& in_rNextPosition, Key& in_rKey, Value& in_rValue ) const = 0;
			};		
		};
	}
}

#endif // _AK_WWISE_SOURCECONTROLCONTAINERS_H