//////////////////////////////////////////////////////////////////////
//
// Copyright (c) 2006 Audiokinetic Inc. / All Rights Reserved
//
//////////////////////////////////////////////////////////////////////

/// \file 
/// Objects used in the SoundFrame interfaces. These are used to send Wwise object information
/// to client applications using the SoundFrame.

#ifndef _AK_SOUNDFRAME_OBJECT_SF_H
#define _AK_SOUNDFRAME_OBJECT_SF_H

#include <wtypes.h>

#include <AK/SoundEngine/Common/AkTypes.h>

namespace AK
{
	class IReadBytes;
	class IWriteBytes;

	namespace SoundFrame
	{
		class IActionList;
		class IStateList;
		class ISwitchList;
		class IArgumentList;
		class IArgumentValueList;

		/// Base interface for all reference-counted objects of the Sound Frame.
		class ISFRefCount
		{
		public:
			/// Increment the reference count. 
			/// \return The reference count
			virtual long AddRef() = 0;

			/// Decrement the reference count.
			/// \return The reference count
			virtual long Release() = 0;
		};

		/// Base interface for all Sound Frame objects.
		class ISFObject : public ISFRefCount
		{
		public:
			/// Get the name of the object.
			/// \return	A pointer to a string containing the object's name
			/// \aknote
			/// - The object's name could change.
			/// - The pointer will become invalid when the object is destroyed.
			/// \endaknote
			virtual const WCHAR * GetName() const = 0;

			/// Get the 32-bit identifier for this object.
			/// \return 32-bit ID. Note: this identifier might change if the object is renamed.
			virtual AkUniqueID GetID() const = 0;

			/// Get the globally unique identifier for this object.
			/// \return Globally unique, permanent identifier which remains unchanged for the life time of the object. GUID_NULL if not applicable.
			virtual GUID GetGUID() const = 0;
		};

		/// Base interface for all Sound Frame object lists.
		class IObjectList : public ISFRefCount
		{
		public:
			/// Get the object count.
			/// \return	The number of objects in the list
			virtual long GetCount() const = 0;

			/// Reset the current position in list.
			virtual void Reset() = 0;
		};

		/// Event parameter interface.
		class IEvent : public ISFObject
		{
		public:
			/// Get the list of IActions triggered by this event.
			/// \return	A pointer (not AddRef'd) to a list of IActions
			virtual IActionList * GetActionList() = 0;

			/// Load an object that implements IEvent from \e in_pBytes.
			/// \return	AddRef'd IEvent interface if successful, NULL otherwise
			static IEvent * From( 
				IReadBytes * in_pBytes		///< IReadBytes interface from which data will be read
				);

			/// Save this IEvent to \e io_pBytes.
			/// \return	True if the operation was successful, False otherwise
			virtual bool To( 
				IWriteBytes * io_pBytes		///< IWriteBytes interface to which data will be written
				) = 0;
		};

		/// Event action parameter interface.
		class IAction : public ISFRefCount
		{
		public:
			/// Get the name of the IAction.
			/// \return	A pointer to a string containing the IAction's name
			/// \aknote
			/// The pointer will become invalid when the IAction is destroyed.
			/// \endaknote
			virtual const WCHAR * GetName() const = 0;

			/// Get the unique ID of the target sound object.
			/// \return The unique ID of the sound object, or AK_INVALID_UNIQUE_ID if no sound object is associated with the IAction
			virtual AkUniqueID GetSoundObjectID() const = 0;
		};

		/// Dialogue event parameter interface.
		class IDialogueEvent : public ISFObject
		{
		public:
			/// Get the list of IArguments referenced by this dialogue event.
			/// \return	A pointer (not AddRef'd) to a list of IArguments
			virtual IArgumentList * GetArgumentList() = 0;

			/// Load an object that implements IDialogueEvent from \e in_pBytes.
			/// \return	AddRef'd IEvent interface if successful, NULL otherwise
			static IDialogueEvent * From( 
				IReadBytes * in_pBytes		///< IReadBytes interface from which data will be read
				);

			/// Save this IDialogueEvent to \e io_pBytes.
			/// \return	True if the operation was successful, False otherwise
			virtual bool To( 
				IWriteBytes * io_pBytes		///< IWriteBytes interface to which data will be written
				) = 0;
		};

		/// Argument parameter interface.
		class IArgument : public ISFObject
		{
		public:
			/// Get the list of IArgumentValues of this argument.
			/// \return	A pointer (not AddRef'd) to a list of IArgumentValues
			virtual IArgumentValueList * GetArgumentValueList() = 0;

			/// Load an object that implements IArgument from \e in_pBytes.
			/// \return	AddRef'd IEvent interface if successful, NULL otherwise
			static IArgument * From( 
				IReadBytes * in_pBytes		///< IReadBytes interface from which data will be read
				);

			/// Save this IArgument to \e io_pBytes.
			/// \return	True if the operation was successful, False otherwise
			virtual bool To( 
				IWriteBytes * io_pBytes		///< IWriteBytes interface to which data will be written
				) = 0;
		};

		/// Argument value parameter interface
		class IArgumentValue : public ISFObject 
		{};

		/// Sound object parameter interface.
		class ISoundObject : public ISFObject
		{
		public:
			/// Query if a sound object has an attenuation.
			/// \return	True if the sound object has attenuation, False otherwise
			virtual bool HasAttenuation() const = 0;

			/// Get the maximum attenuation for a sound object.
			/// \return	The sound object's maximum attenuation
			virtual double AttenuationMaxDistance() const = 0;

			/// Load an object that implements ISoundObject from \e in_pBytes.
			/// \return	AddRef'd ISoundObject interface if successful, NULL otherwise
			static ISoundObject * From( 
				IReadBytes * in_pBytes		///< IReadBytes interface from which data will be read
				);	

			/// Save this ISoundObject to \e io_pBytes.
			/// \return	True if the operation was successful, False otherwise
			virtual bool To( 
				IWriteBytes * io_pBytes		///< IWriteBytes interface to which data will be written
				) = 0;
		};

		/// State group parameter interface.
		class IStateGroup : public ISFObject
		{
		public:
			/// Get the list of IStates in this state group.
			/// \return	A pointer (not AddRef'd) to a list of IStates
			virtual IStateList * GetStateList() = 0;

			/// Load an object that implements IStateGroup from \e in_pBytes.
			/// \return	AddRef'd IStateGroup interface if successful, NULL otherwise
			static IStateGroup * From( 
				IReadBytes * in_pBytes		///< IReadBytes interface from which data will be read
				);

			/// Save this IStateGroup to \e io_pBytes.
			/// \return	True if the operation was successful, False otherwise
			virtual bool To( 
				IWriteBytes * io_pBytes		///< IWriteBytes interface to which data will be written
				) = 0;
		};

		/// State parameter interface
		class IState : public ISFObject 
		{};

		/// Switch Group parameter interface.
		class ISwitchGroup : public ISFObject
		{
		public:
			/// Get the list of ISwitches in this switch group
			/// \return	A pointer (not AddRef'd) to a list of ISwitches
			virtual ISwitchList * GetSwitchList() = 0;

			/// Load an object that implements ISwitchGroup from \e in_pBytes.
			/// \return	AddRef'd ISwitchGroup interface if successful, NULL otherwise
			static ISwitchGroup * From( 
				IReadBytes * in_pBytes		///< IReadBytes interface from which data will be read
				);

			/// Save this ISwitchGroup to \e io_pBytes.
			/// \return	True if the operation was successful, False otherwise
			virtual bool To( 
				IWriteBytes * io_pBytes		///< IWriteBytes interface to which data will be written
				) = 0;
		};

		/// Switch parameter interface
		class ISwitch : public ISFObject 
		{};

		/// Game parameter interface
		/// \aknote
		/// Game parameters and RTPCs are different concepts. For details on their relationship, please 
		/// refer to the documentation for the Wwise authoring tool.
		/// \endaknote
		class IGameParameter : public ISFObject
		{
		public:

			/// Get the minimum value of this game parameter's range.
			/// \return The lowest value defined by this game parameter's range
			virtual double RangeMin() const = 0;

			/// Get the maximum value of this game parameter's range.
			/// \return The highest value defined by this game parameter's range
			virtual double RangeMax() const = 0;

			/// Get the default value of this game parameter.
			/// \return The default value of this game parameter
			virtual double Default() const = 0;

			/// Load an object that implements IGameParameter from \e in_pBytes.
			/// \return	AddRef'd IGameParameter interface if successful, NULL otherwise
			static IGameParameter * From( 
				IReadBytes * in_pBytes		///< IReadBytes interface from which data will be read
				);	

			/// Save this IGameParameter to \e io_pBytes.
			/// \return	True if the operation was successful, False otherwise
			virtual bool To( 
				IWriteBytes * io_pBytes		///< IWriteBytes interface to which data will be written
				) = 0;
		};

		/// Conversion settings interface
		class IConversionSettings : public ISFObject
		{
		public:
			/// Load an object that implements IConversionSettings from \e in_pBytes.
			/// \return	AddRef'd IConversionSettings interface if successful, NULL otherwise
			static IConversionSettings * From( 
				IReadBytes * in_pBytes		///< IReadBytes interface from which data will be read
				);

			/// Saves this IConversionSettings to \e io_pBytes.
			/// \return	True if the operation was successful, False otherwise
			virtual bool To( 
				IWriteBytes * io_pBytes		///< IWriteBytes interface to which data will be written
				) = 0;
		};

		/// Trigger interface
		class ITrigger : public ISFObject 
		{
		public:
			/// Load an object that implements ITrigger from \e in_pBytes.
			/// \return	AddRef'd ITrigger interface if successful, NULL otherwise
			static ITrigger * From( 
				IReadBytes * in_pBytes		///< IReadBytes interface from which data will be read
				);	

			/// Save this ITrigger to \e io_pBytes.
			/// \return	True if the operation was successful, False otherwise
			virtual bool To( 
				IWriteBytes * io_pBytes		///< IWriteBytes interface to which data will be written
				) = 0;
		};

		/// Auxiliary Bus interface
		class IAuxBus : public ISFObject 
		{
		public:
			/// Load an object that implements IAuxBus from \e in_pBytes.
			/// \return	AddRef'd IAuxBus interface if successful, NULL otherwise
			static IAuxBus * From( 
				IReadBytes * in_pBytes		///< IReadBytes interface from which data will be read
				);	

			/// Save this IAuxBus to \e io_pBytes.
			/// \return	True if the operation was successful, False otherwise
			virtual bool To( 
				IWriteBytes * io_pBytes		///< IWriteBytes interface to which data will be written
				) = 0;
		};

		/// SoundBank interface
		class ISoundBank : public ISFObject
		{
		public:
			/// Load an object that implements ISoundBank from \e in_pBytes.
			/// \return	AddRef'd IConversionSettings interface if successful, NULL otherwise
			static ISoundBank * From( 
				IReadBytes * in_pBytes		///< IReadBytes interface from which data will be read
				);

			/// Saves this ISoundBank to \e io_pBytes.
			/// \return	True if the operation was successful, False otherwise
			virtual bool To( 
				IWriteBytes * io_pBytes		///< IWriteBytes interface to which data will be written
				) = 0;
		};

		/// Game object interface
		class IGameObject : public ISFRefCount
		{
		public:
			/// Invalid Game Object ID used to set global information in the sound engine.
			/// \sa AK_INVALID_GAME_OBJECT
			static const AkGameObjectID s_InvalidGameObject = (unsigned int)(~0);

			/// Wwise Game Object ID used to access the default Wwise game object
			static const AkGameObjectID s_WwiseGameObject = 0;

			/// Get the name of the object.
			/// \return	A pointer to a string containing the object's name
			/// \aknote
			/// - The object's name could change.
			/// - The pointer will become invalid when the object is destroyed.
			/// \endaknote
			virtual const WCHAR * GetName() const = 0;

			/// Get the unique ID of the object.
			/// \return The unique ID of the object
			virtual AkGameObjectID GetID() const = 0;

			/// Load an object that implements IGameObject from \e in_pBytes.
			/// \return	AddRef'd IGameObject interface if successful, NULL otherwise
			static IGameObject * From( 
				IReadBytes * in_pBytes		///< IReadBytes interface from which data will be read
				);

			/// Saves this IGameObject to \e io_pBytes.
			/// \return	True if the operation was successful, False otherwise
			virtual bool To( 
				IWriteBytes * io_pBytes		///< IWriteBytes interface to which data will be written
				) = 0;
		};

		/// Original file interface
		class IOriginalFile : public ISFObject
		{
		public:
			/// Load an object that implements IOriginalFile from \e in_pBytes.
			/// \return	AddRef'd IOriginalFile interface if successful, NULL otherwise
			static IOriginalFile * From( 
				IReadBytes * in_pBytes		///< IReadBytes interface from which data will be read
				);

			/// Saves this IOriginalFile to \e io_pBytes.
			/// \return	True if the operation was successful, False otherwise
			virtual bool To( 
				IWriteBytes * io_pBytes		///< IWriteBytes interface to which data will be written
				) = 0;
		};

		/// Event list interface
		class IEventList : public IObjectList
		{
		public:
			/// Get the next IEvent in the list.
			/// \return	A pointer (not AddRef'd) to the next IEvent, or NULL if the end is reached
			virtual IEvent * Next() = 0;		
		};

		/// Event action list interface
		class IActionList : public IObjectList
		{
		public:
			/// Get the next IAction in the list.
			/// \return	A pointer (not AddRef'd) to the next IAction, or NULL if the end is reached
			virtual IAction * Next() = 0;
		};

		/// Dialogue event list interface
		class IDialogueEventList : public IObjectList
		{
		public:
			/// Get the next IDialogueEvent in the list.
			/// \return	A pointer (not AddRef'd) to the next IDialogueEvent, or NULL if the end is reached
			virtual IDialogueEvent * Next() = 0;		
		};

		/// List of sound objects
		class ISoundObjectList : public IObjectList
		{
		public:
			/// Get the next ISoundObject in the list.
			/// \return	A pointer (not AddRef'd) to the next ISoundObject, or NULL if the end is reached
			virtual ISoundObject * Next() = 0;		
		};

		/// List of state groups
		class IStateGroupList : public IObjectList
		{
		public:
			/// Get the next IStateGroup in the list.
			/// \return	A pointer (not AddRef'd) to the next IStateGroup, or NULL if the end is reached
			virtual IStateGroup * Next() = 0;
		};

		/// List of states
		class IStateList : public IObjectList
		{
		public:
			/// Get the next IState in the list.
			/// \return	A pointer (not AddRef'd) to the next IState, or NULL if the end is reached
			virtual IState * Next() = 0;
		};

		/// List of switch groups
		class ISwitchGroupList : public IObjectList
		{
		public:
			/// Get the next ISwitchGroup in the list.
			/// \return	A pointer (not AddRef'd) to the next ISwitchGroup, or NULL if the end is reached
			virtual ISwitchGroup * Next() = 0;
		};

		/// List of switches
		class ISwitchList : public IObjectList
		{
		public:
			/// Get the next ISwitch in the list.
			/// \return	A pointer (not AddRef'd) to the next ISwitch, or NULL if the end is reached
			virtual ISwitch * Next() = 0;
		};

		/// List of game parameters
		/// \aknote
		/// Game parameters and RTPCs are different concepts. For details on their relationship, please 
		/// refer to the documentation for the Wwise authoring tool.
		/// \endaknote
		class IGameParameterList : public IObjectList
		{
		public:
			/// Get the next IGameParameter in the list.
			/// \return	A pointer (not AddRef'd) to the next IGameParameter, or NULL if the end is reached
			virtual IGameParameter * Next() = 0;
		};

		/// List of conversion settings
		class IConversionSettingsList : public IObjectList
		{
		public:
			/// Get the next IConversionSettings in the list.
			/// \return	A pointer (not AddRef'd) to the next IConversionSettings, or NULL if the end is reached
			virtual IConversionSettings * Next() = 0;
		};

		/// List of triggers
		class ITriggerList : public IObjectList
		{
		public:
			/// Get the next ITrigger in the list.
			/// \return	A pointer (not AddRef'd) to the next ITrigger, or NULL if the end is reached
			virtual ITrigger * Next() = 0;		
		};

		/// List of arguments
		class IArgumentList : public IObjectList
		{
		public:
			/// Get the next IArgument in the list.
			/// \return	A pointer (not AddRef'd) to the next IArgument, or NULL if the end is reached
			virtual IArgument * Next() = 0;		
		};

		/// List of argument values
		class IArgumentValueList : public IObjectList
		{
		public:
			/// Get the next IArgumentValue in the list.
			/// \return	A pointer (not AddRef'd) to the next IArgumentValue, or NULL if the end is reached
			virtual IArgumentValue * Next() = 0;		
		};

		/// List of Auxiliary Busses
		class IAuxBusList : public IObjectList
		{
		public:
			/// Get the next IAuxBus in the list.
			/// \return	A pointer (not AddRef'd) to the next IAuxBus, or NULL if the end is reached
			virtual IAuxBus * Next() = 0;		
		};

		/// List of sound banks
		class ISoundBankList : public IObjectList
		{
		public:
			/// Get the next ISoundBank in the list.
			/// \return	A pointer (not AddRef'd) to the next ISoundBank, or NULL if the end is reached
			virtual ISoundBank * Next() = 0;		
		};

		/// List of game objects
		class IGameObjectList : public IObjectList
		{
		public:
			/// Get the next IGameObject in the list.
			/// \return	A pointer (not AddRef'd) to the next IGameObject, or NULL if the end is reached
			virtual IGameObject * Next() = 0;		
		};

		/// List of original files
		class IOriginalFileList : public IObjectList
		{
		public:
			/// Get the next IOriginalFile in the list.
			/// \return	A pointer (not AddRef'd) to the next IOriginalFile, or NULL if the end is reached
			virtual IOriginalFile * Next() = 0;		
		};
	}
}

#endif // _AK_SOUNDFRAME_OBJECTS_SF_H
