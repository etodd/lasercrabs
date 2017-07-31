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

#ifndef _AK_WWISE_UNDO_H
#define _AK_WWISE_UNDO_H

#include <AK/Wwise/Utilities.h>

#ifdef _DEBUG
#define UNDO_EVENT_DEBUG_INFO
#endif // _DEBUG

namespace AK
{
	namespace Wwise
	{
		class IUndoEvent
			: public IPluginBase
		{
		public:
			// Un-execute the action
			virtual bool Undo()	= 0;

			// Re-execute the action
			virtual bool Redo()	= 0;

			// Get the name of the action
			virtual bool GetName( CString& out_csName ) = 0;

			// Check if this undo event is relevant all by itself. For example, 
			// a selection change is not necessary, but is nice to have around when
			// surrounded by other events in a complex undo.
			virtual bool IsNecessary() = 0;

			// Return the associated object GUID this undo is modifying
			virtual GUID GetObjectID() const = 0;

#ifdef UNDO_EVENT_DEBUG_INFO
			// Get a string representing data for this
			// undo event. It will be used to display info in the
			// debug window. The object should prepend in_szPrefix
			// to the string (for formatting complex undo info)
			virtual bool GetDebugString( LPCTSTR in_szPrefix, CString& out_csString ) = 0;
#endif // UNDO_EVENT_DEBUG_INFO
		};

		class IComplexUndo
			: public IUndoEvent
		{
		public:
			// Add an event to this complex undo event
			virtual bool AddEvent( IUndoEvent* in_pEvent ) = 0;

			// Check if this complex undo is empty ( i.e. contains no sub events ).
			virtual bool IsEmpty() = 0;

			// If this complex undo contains only one sub event, remove it and return it 
			virtual IUndoEvent* ExtractSingleSubEvent() = 0;
		};

		class IUndoManager
		{
		public:
			// Add an undo event
			virtual bool AddEvent( IUndoEvent* in_pEvent ) = 0;

			// Open a complex undo event that will contain all subsequent undo events
			virtual bool OpenComplex( IComplexUndo * in_pComplex = NULL ) = 0;

			// Close the current complex undo
			virtual bool CloseComplex( LPCTSTR in_szName, bool in_bKeepEvenIfContainsSingleEvent = false ) = 0;

			// Cancel the current complex undo
			virtual bool CancelComplex() = 0;

			// Check if we are currently in a state where we can add undo events.
			virtual bool CanAddEvent() = 0;

			// Check if we are busy (undoing or redoing).
			virtual bool IsBusy() = 0;
		};
	}
}

#endif // _AK_WWISE_UNDO_H
