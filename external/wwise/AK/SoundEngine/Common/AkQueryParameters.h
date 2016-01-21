// Copyright (c) 2006 Audiokinetic Inc. / All Rights Reserved
// AkQueryParameters.h

/// \file 
/// The sound engine parameter query interface.


#ifndef _AK_QUERYPARAMS_H_
#define _AK_QUERYPARAMS_H_

#include <AK/SoundEngine/Common/AkSoundEngineExport.h>
#include <AK/SoundEngine/Common/AkTypes.h>
#include <AK/SoundEngine/Common/AkCommonDefs.h>
#include <AK/Tools/Common/AkArray.h>

/// Positioning information obtained from an object
struct AkPositioningInfo
{
	AkReal32			fCenterPct;			///< Center % [0..1]
	AkPannerType	    pannerType;			///< Positioning Type (2D, 3D)
	AkPositionSourceType posSourceType;		///< Positioning source (GameDef or UserDef)
	bool				bUpdateEachFrame;   ///< Update at each frame (valid only with game-defined)
	bool				bUseSpatialization; ///< Use spatialization
	bool				bUseAttenuation;	///< Use attenuation parameter set

	bool				bUseConeAttenuation; ///< Use the cone attenuation
	AkReal32			fInnerAngle;		///< Inner angle
	AkReal32			fOuterAngle;		///< Outer angle
	AkReal32			fConeMaxAttenuation; ///< Cone max attenuation
	AkLPFType			LPFCone;			///< Cone low pass filter value
	AkLPFType			HPFCone;			///< Cone low pass filter value

	AkReal32			fMaxDistance;		///< Maximum distance
	AkReal32			fVolDryAtMaxDist;	///< Volume dry at maximum distance
	AkReal32			fVolAuxGameDefAtMaxDist;	///< Volume wet at maximum distance (if any) (based on the Game defined distance attenuation)
	AkReal32			fVolAuxUserDefAtMaxDist;	///< Volume wet at maximum distance (if any) (based on the User defined distance attenuation)
	AkLPFType			LPFValueAtMaxDist;  ///< Low pass filter value at max distance (if any)
	AkLPFType			HPFValueAtMaxDist;  ///< High pass filter value at max distance (if any)
};

/// Object information structure for QueryAudioObjectsIDs
struct AkObjectInfo
{
	AkUniqueID	objID;		///< Object ID
	AkUniqueID	parentID;	///< Object ID of the parent 
	AkInt32		iDepth;		///< Depth in tree
};

// Audiokinetic namespace
namespace AK
{
	// Audiokinetic sound engine namespace
	namespace SoundEngine
	{
		/// Query namespace
		/// \remarks The functions in this namespace are thread-safe, unless stated otherwise.
		///
		/// \warning Unless noted otherwise in the function definition that it will not acquire the main audio lock, the functions in this namespace 
		/// might stall for several milliseconds before returning (as they cannot execute while the 
		/// main sound engine thread is busy). They should therefore not be called from any 
		/// game critical thread, such as the main game loop.
		///
		/// \warning There might be a significant delay between a Sound Engine call (such as PostEvent) and
		/// the information being reflected in a Query (such as GetIsGameObjectActive). 

		namespace Query
		{
			////////////////////////////////////////////////////////////////////////
			/// @name Game Objects
			//@{

			/// Get the position of a game object.
			/// \return AK_Success if succeeded, or AK_IDNotFound if the game object was not registered
			/// \sa 
			/// - \ref soundengine_3dpositions
			AK_EXTERNAPIFUNC( AKRESULT, GetPosition )( 
				AkGameObjectID in_GameObjectID,				///< Game object identifier
				AkSoundPosition& out_rPosition				///< Position to get
				);

			//@}

			////////////////////////////////////////////////////////////////////////
			/// @name Listeners
			//@{

			/// Get a game object's active listeners.
			/// \return AK_Success if succeeded, or AK_IDNotFound if the game object was not registered
			/// \sa 
			/// - \ref soundengine_listeners_multi_assignobjects
			AK_EXTERNAPIFUNC( AKRESULT, GetActiveListeners )(
				AkGameObjectID in_GameObjectID,				///< Game object identifier
				AkUInt32& out_ruListenerMask				///< Bitmask representing the active listeners (LSB = Listener 0, set to 1 means active)
				);

			/// Get a listener's position.
			/// \return AK_Success if succeeded, or AK_InvalidParameter if the index is out of range
			/// \sa 
			/// - \ref soundengine_listeners_settingpos
			AK_EXTERNAPIFUNC( AKRESULT, GetListenerPosition )( 
				AkUInt32 in_uIndex, 						///< Listener index (0: first listener, 7: 8th listener)
				AkListenerPosition& out_rPosition			///< Position set
				);

			/// Get a listener's spatialization parameters. 
			/// \return AK_Success if succeeded, or AK_InvalidParameter if the index is out of range
			/// \sa 
			/// - AK::SoundEngine::SetListenerSpatialization().
			/// - \ref soundengine_listeners_spatial
			AK_EXTERNAPIFUNC( AKRESULT, GetListenerSpatialization )(
				AkUInt32 in_uIndex,							///< Listener index (0: first listener, 7: 8th listener)
				bool& out_rbSpatialized,					///< Spatialization enabled
				AK::SpeakerVolumes::VectorPtr & out_pVolumeOffsets,	///< Per-speaker vector of volume offsets, in decibels. Use the functions of AK::SpeakerVolumes::Vector to interpret it.
				AkChannelConfig &out_channelConfig			///< Channel configuration associated with out_rpVolumeOffsets. 
				);
			
			//@}


			////////////////////////////////////////////////////////////////////////
			/// @name Game Syncs
			//@{

			/// Enum used to request a specific RTPC Value.
			/// Also used to inform the user of where the RTPC Value comes from.
			///
			/// For example, the user may request the GameObject specific value by specifying RTPCValue_GameObject
			/// and can receive the Global Value if there was no GameObject specific value, and even the 
			/// default value is there was no Global value either.
			/// \sa 
			/// - \ref GetRTPCValue
			enum RTPCValue_type
			{
				RTPCValue_Default,		///< The value is the Default RTPC.
				RTPCValue_Global,		///< The value is the Global RTPC.
				RTPCValue_GameObject,	///< The value is the game object specific RTPC.
				RTPCValue_Unavailable	///< The value is not available for the RTPC specified.
			};

			/// Get the value of a real-time parameter control (by ID).
			/// \return AK_Success if succeeded, AK_IDNotFound if the game object was not registered, or AK_Fail if the RTPC value could not be obtained
			/// \sa 
			/// - \ref soundengine_rtpc
			/// - \ref RTPCValue_type
			AK_EXTERNAPIFUNC( AKRESULT, GetRTPCValue )( 
				AkRtpcID in_rtpcID, 				///< ID of the RTPC
				AkGameObjectID in_gameObjectID,		///< Associated game object ID
				AkRtpcValue& out_rValue, 			///< Value returned
				RTPCValue_type&	io_rValueType		///< In/Out value, the user must specify the requested type. The function will return in this variable the type of the returned value.
				);

#ifdef AK_SUPPORT_WCHAR
			/// Get the value of a real-time parameter control.
			/// \return AK_Success if succeeded, AK_IDNotFound if the game object was not registered or the rtpc name could not be found, or AK_Fail if the RTPC value could not be obtained
			/// \sa 
			/// - \ref soundengine_rtpc
			/// - \ref RTPCValue_type
			AK_EXTERNAPIFUNC( AKRESULT, GetRTPCValue )( 
				const wchar_t* in_pszRtpcName,		///< String name of the RTPC
				AkGameObjectID in_gameObjectID,		///< Associated game object ID
				AkRtpcValue& out_rValue, 			///< Value returned
				RTPCValue_type&	io_rValueType		///< In/Out value, the user must specify the requested type. The function will return in this variable the type of the returned value.
				);
#endif //AK_SUPPORT_WCHAR

			/// Get the value of a real-time parameter control.
			/// \return AK_Success if succeeded, AK_IDNotFound if the game object was not registered or the rtpc name could not be found, or AK_Fail if the RTPC value could not be obtained
			/// \sa 
			/// - \ref soundengine_rtpc
			/// - \ref RTPCValue_type
			AK_EXTERNAPIFUNC( AKRESULT, GetRTPCValue )( 
				const char* in_pszRtpcName,			///< String name of the RTPC
				AkGameObjectID in_gameObjectID,		///< Associated game object ID
				AkRtpcValue& out_rValue, 			///< Value returned
				RTPCValue_type&	io_rValueType		///< In/Out value, the user must specify the requested type. The function will return in this variable the type of the returned value.
				);

			/// Get the state of a switch group (by IDs).
			/// \return AK_Success if succeeded, or AK_IDNotFound if the game object was not registered
			/// \sa 
			/// - \ref soundengine_switch
			AK_EXTERNAPIFUNC( AKRESULT, GetSwitch )( 
				AkSwitchGroupID in_switchGroup, 			///< ID of the switch group
				AkGameObjectID  in_gameObjectID,			///< Associated game object ID
				AkSwitchStateID& out_rSwitchState 			///< ID of the switch
				);

#ifdef AK_SUPPORT_WCHAR
			/// Get the state of a switch group.
			/// \return AK_Success if succeeded, or AK_IDNotFound if the game object was not registered or the switch group name can not be found
			/// \sa 
			/// - \ref soundengine_switch
			AK_EXTERNAPIFUNC( AKRESULT, GetSwitch )( 
				const wchar_t* in_pstrSwitchGroupName,			///< String name of the switch group
				AkGameObjectID in_GameObj,					///< Associated game object ID
				AkSwitchStateID& out_rSwitchState			///< ID of the switch
				);
#endif //AK_SUPPORT_WCHAR

			/// Get the state of a switch group.
			/// \return AK_Success if succeeded, or AK_IDNotFound if the game object was not registered or the switch group name can not be found
			/// \sa 
			/// - \ref soundengine_switch
			AK_EXTERNAPIFUNC( AKRESULT, GetSwitch )( 
				const char* in_pstrSwitchGroupName,			///< String name of the switch group
				AkGameObjectID in_GameObj,					///< Associated game object ID
				AkSwitchStateID& out_rSwitchState			///< ID of the switch
				);

			/// Get the state of a state group (by IDs).
			/// \return AK_Success if succeeded
			/// \sa 
			/// - \ref soundengine_states
			AK_EXTERNAPIFUNC( AKRESULT, GetState )( 
				AkStateGroupID in_stateGroup, 				///< ID of the state group
				AkStateID& out_rState 						///< ID of the state
				);

#ifdef AK_SUPPORT_WCHAR
			/// Get the state of a state group.
			/// \return AK_Success if succeeded, or AK_IDNotFound if the state group name can not be found
			/// \sa 
			/// - \ref soundengine_states
			AK_EXTERNAPIFUNC( AKRESULT, GetState )( 
				const wchar_t* in_pstrStateGroupName,			///< String name of the state group
				AkStateID& out_rState						///< ID of the state
				);
#endif //AK_SUPPORT_WCHAR

			/// Get the state of a state group.
			/// \return AK_Success if succeeded, or AK_IDNotFound if the state group name can not be found
			/// \sa 
			/// - \ref soundengine_states
			AK_EXTERNAPIFUNC( AKRESULT, GetState )( 
				const char* in_pstrStateGroupName,			    ///< String name of the state group
				AkStateID& out_rState						///< ID of the state
				);

			//@}

			////////////////////////////////////////////////////////////////////////
			/// @name Environments
			//@{

			/// Get the environmental ratios used by the specified game object.
			/// The array size cannot exceed AK_MAX_AUX_PER_OBJ.
			/// To clear the game object's environments, in_uNumEnvValues must be 0.
			/// \aknote The actual maximum number of environments in which a game object can be is AK_MAX_AUX_PER_OBJ. \endaknote
			/// \sa 
			/// - \ref soundengine_environments
			/// - \ref soundengine_environments_dynamic_aux_bus_routing
			/// - \ref soundengine_environments_id_vs_string
			/// \return AK_Success if succeeded, or AK_InvalidParameter if io_ruNumEnvValues is 0 or out_paEnvironmentValues is NULL, or AK_PartialSuccess if more environments exist than io_ruNumEnvValues
			/// AK_InvalidParameter
			AK_EXTERNAPIFUNC( AKRESULT, GetGameObjectAuxSendValues )( 
				AkGameObjectID		in_gameObjectID,		///< Associated game object ID
				AkAuxSendValue*		out_paAuxSendValues,	///< Variable-size array of AkAuxSendValue structures
																///< (it may be NULL if no aux send must be set, and its size 
																///< cannot exceed AK_MAX_AUX_PER_OBJ)
				AkUInt32&			io_ruNumSendValues		///< The number of Auxilliary busses at the pointer's address
															///< (it must be 0 if no aux bus is set, and can not exceed AK_MAX_AUX_PER_OBJ)
				);

			/// Get the environmental dry level to be used for the specified game object
			/// The control value is a number ranging from 0.0f to 1.0f.
			/// 0.0f stands for 0% dry, while 1.0f stands for 100% dry.
			/// \aknote Reducing the dry level does not mean increasing the wet level. \endaknote
			/// \sa 
			/// - \ref soundengine_environments
			/// - \ref soundengine_environments_setting_dry_environment
			/// - \ref soundengine_environments_id_vs_string
			/// \return AK_Success if succeeded, or AK_IDNotFound if the game object was not registered
			AK_EXTERNAPIFUNC( AKRESULT, GetGameObjectDryLevelValue )( 
				AkGameObjectID		in_gameObjectID,		///< Associated game object ID
				AkReal32&			out_rfControlValue		///< Dry level control value, ranging from 0.0f to 1.0f
															///< (0.0f stands for 0% dry, while 1.0f stands for 100% dry)
				);

			/// Get a game object's obstruction and occlusion levels.
			/// \sa 
			/// - \ref soundengine_obsocc
			/// - \ref soundengine_environments
			/// \return AK_Success if succeeded, AK_IDNotFound if the game object was not registered
			AK_EXTERNAPIFUNC( AKRESULT, GetObjectObstructionAndOcclusion )(  
				AkGameObjectID in_ObjectID,			///< Associated game object ID
				AkUInt32 in_uListener,				///< Listener index (0: first listener, 7: 8th listener)
				AkReal32& out_rfObstructionLevel,		///< ObstructionLevel: [0.0f..1.0f]
				AkReal32& out_rfOcclusionLevel			///< OcclusionLevel: [0.0f..1.0f]
				);

			//@}

			/// Get the list of audio object IDs associated to an event.
			/// \aknote It is possible to call QueryAudioObjectIDs with io_ruNumItems = 0 to get the total size of the
			/// structure that should be allocated for out_aObjectInfos. \endaknote
			/// \return AK_Success if succeeded, AK_IDNotFound if the eventID cannot be found, AK_InvalidParameter if out_aObjectInfos is NULL while io_ruNumItems > 0
			/// or AK_UnknownObject if the event contains an unknown audio object, 
			/// or AK_PartialSuccess if io_ruNumItems was set to 0 to query the number of available items.
			AK_EXTERNAPIFUNC( AKRESULT, QueryAudioObjectIDs )(
				AkUniqueID in_eventID,				///< Event ID
				AkUInt32& io_ruNumItems,			///< Number of items in array provided / Number of items filled in array
				AkObjectInfo* out_aObjectInfos		///< Array of AkObjectInfo items to fill
				);

#ifdef AK_SUPPORT_WCHAR
			/// Get the list of audio object IDs associated to a event name.
			/// \aknote It is possible to call QueryAudioObjectIDs with io_ruNumItems = 0 to get the total size of the
			/// structure that should be allocated for out_aObjectInfos. \endaknote
			/// \return AK_Success if succeeded, AK_IDNotFound if the event name cannot be found, AK_InvalidParameter if out_aObjectInfos is NULL while io_ruNumItems > 0
			/// or AK_UnknownObject if the event contains an unknown audio object, 
			/// or AK_PartialSuccess if io_ruNumItems was set to 0 to query the number of available items.
			AK_EXTERNAPIFUNC( AKRESULT, QueryAudioObjectIDs )(
				const wchar_t* in_pszEventName,		///< Event name
				AkUInt32& io_ruNumItems,			///< Number of items in array provided / Number of items filled in array
				AkObjectInfo* out_aObjectInfos		///< Array of AkObjectInfo items to fill
				);
#endif //AK_SUPPORT_WCHAR

			/// Get the list of audio object IDs associated to an event name.
			/// \aknote It is possible to call QueryAudioObjectIDs with io_ruNumItems = 0 to get the total size of the
			/// structure that should be allocated for out_aObjectInfos. \endaknote
			/// \return AK_Success if succeeded, AK_IDNotFound if the event name cannot be found, AK_InvalidParameter if out_aObjectInfos is NULL while io_ruNumItems > 0
			/// or AK_UnknownObject if the event contains an unknown audio object, 
			/// or AK_PartialSuccess if io_ruNumItems was set to 0 to query the number of available items.
			AK_EXTERNAPIFUNC( AKRESULT, QueryAudioObjectIDs )(
				const char* in_pszEventName,		///< Event name
				AkUInt32& io_ruNumItems,			///< Number of items in array provided / Number of items filled in array
				AkObjectInfo* out_aObjectInfos		///< Array of AkObjectInfo items to fill
				);

			/// Get positioning information associated to an audio object.
			/// \return AK_Success if succeeded, AK_IDNotFound if the object ID cannot be found, AK_NotCompatible if the audio object cannot expose positioning
			AK_EXTERNAPIFUNC( AKRESULT, GetPositioningInfo )( 
				AkUniqueID in_ObjectID,						///< Audio object ID
				AkPositioningInfo& out_rPositioningInfo		///< Positioning information structure to be filled
				);

			/// List passed to GetActiveGameObjects.
			/// After calling this function, the list will contain the list of all game objects that are currently active in the sound engine.
			/// Being active means that either a sound is playing or pending to be played using this game object.
			/// The caller is responsible for calling Term() on the list when the list is not required anymore
			/// \sa 
			/// - \ref GetActiveGameObjects
			typedef AkArray<AkGameObjectID, AkGameObjectID, ArrayPoolDefault, 32> AkGameObjectsList;

			/// Fill the provided list with all the game object IDs that are currently active in the sound engine.
			/// The function may be used to avoid updating game objects positions that are not required at the moment.
			/// After calling this function, the list will contain the list of all game objects that are currently active in the sound engine.
			/// Being active means that either a sound is playing or pending to be played using this game object.
			/// \sa 
			/// - \ref AkGameObjectsList
			AK_EXTERNAPIFUNC( AKRESULT, GetActiveGameObjects )( 
				AkGameObjectsList& io_GameObjectList	///< returned list of active game objects.
				);

			/// Query if the specified game object is currently active.
			/// Being active means that either a sound is playing or pending to be played using this game object.
			AK_EXTERNAPIFUNC( bool, GetIsGameObjectActive )( 
				AkGameObjectID in_GameObjId ///< Game object ID
				);

			/// Game object and max distance association.
			/// \sa 
			/// - \ref AkRadiusList
			struct GameObjDst
			{
				/// Default constructor
				GameObjDst()
					: m_gameObjID( AK_INVALID_GAME_OBJECT )
					, m_dst( -1.0f )
				{}

				/// Easy constructor
				GameObjDst( AkGameObjectID in_gameObjID, AkReal32 in_dst )
					: m_gameObjID( in_gameObjID )
					, m_dst( in_dst )
				{}

				AkGameObjectID	m_gameObjID;	///< Game object ID
				AkReal32		m_dst;			///< MaxDistance
			};

			/// List passed to GetMaxRadius.
			/// \sa 
			/// - \ref AK::SoundEngine::Query::GetMaxRadius
			typedef AkArray<GameObjDst, const GameObjDst&, ArrayPoolDefault, 32> AkRadiusList;

			/// Returns the maximum distance used in attenuations associated to all sounds currently playing.
			/// This may be used for example by the game to know if some processing need to be performed on the game side, that would not be required
			/// if the object is out of reach anyway.
			///
			/// Example usage:
			/// \code
			/// /*******************************************************/
			/// AkRadiusList RadLst; //creating the list( array ).
			/// // Do not reserve any size for the array, 
			/// // the system will reserve the correct size.
			///
			/// GetMaxRadius( RadLst );
			/// // Use the content of the list
			/// (...)
			///
			/// RadLst.Term();// the user is responsible to free the memory allocated
			/// /*******************************************************/
			/// \endcode
			///
			/// \aknote The returned value is NOT the distance from a listener to an object but
			/// the maximum attenuation distance of all sounds playing on this object. This is
			/// not related in any way to the curent 3D position of the object. \endaknote
			///
			/// \return 
			/// - AK_Success if succeeded
			/// - AK_InsuficientMemory if there was not enough memory
			///
			/// \aknote 
			/// The Scaling factor (if one was specified on the game object) is included in the return value.
			/// The Scaling factor is not updated once a sound starts playing since it 
			/// is computed only when the playback starts with the initial scaling factor of this game object. Scaling factor will 
			/// be re-computed for every playback instance, always using the scaling factor available at this time.
			/// \endaknote
			///
			/// \sa 
			/// - \ref AkRadiusList
			AK_EXTERNAPIFUNC( AKRESULT, GetMaxRadius )(
				AkRadiusList & io_RadiusList	///< List that will be filled with AK::SoundEngine::Query::GameObjDst objects.
				);

			/// Returns the maximum distance used in attenuations associated to sounds playing using the specified game object.
			/// This may be used for example by the game to know if some processing need to be performed on the game side, that would not be required
			/// if the object is out of reach anyway.
			/// 
			/// \aknote The returned value is NOT the distance from a listener to an object but the maximum attenuation distance of all sounds playing on this object. \endaknote
			///
			/// \return
			/// - A negative number if the game object specified is not playing.
			/// - 0, if the game object was only associated to sounds playing using no distance attenuation ( like 2D sounds ).
			/// - A positive number represents the maximum of all the distance attenuations playing on this game object.
			///
			/// \aknote 
			/// The Scaling factor (if one was specified on the game object) is included in the return value.
			/// The Scaling factor is not updated once a sound starts playing since it 
			/// is computed only when the playback starts with the initial scaling factor of this game object. Scaling factor will 
			/// be re-computed for every playback instance, always using the scaling factor available at this time.
			/// \endaknote
			///
			/// \sa 
			/// - \ref AK::SoundEngine::SetAttenuationScalingFactor
			AK_EXTERNAPIFUNC( AkReal32, GetMaxRadius )(
				AkGameObjectID in_GameObjId ///< Game object ID
				);

			/// Get the Event ID associated to the specified PlayingID.
			/// This function does not acquire the main audio lock.
			///
			/// \return AK_INVALID_UNIQUE_ID on failure.
			AK_EXTERNAPIFUNC( AkUniqueID, GetEventIDFromPlayingID )(
				AkPlayingID in_playingID ///< Associated PlayingID
				);

			/// Get the ObjectID associated to the specified PlayingID.
			/// This function does not acquire the main audio lock.
			///
			/// \return AK_INVALID_GAME_OBJECT on failure.
			AK_EXTERNAPIFUNC( AkGameObjectID, GetGameObjectFromPlayingID )(
				AkPlayingID in_playingID ///< Associated PlayingID
				);

			/// Get the list PlayingIDs associated with the given game object.
			/// This function does not acquire the main audio lock.
			///
			/// \aknote It is possible to call GetPlayingIDsFromGameObject with io_ruNumItems = 0 to get the total size of the
			/// structure that should be allocated for out_aPlayingIDs. \endaknote
			/// \return AK_Success if succeeded, AK_InvalidParameter if out_aPlayingIDs is NULL while io_ruNumItems > 0
			AK_EXTERNAPIFUNC( AKRESULT, GetPlayingIDsFromGameObject )(
				AkGameObjectID in_GameObjId,		///< Game object ID
				AkUInt32& io_ruNumIDs,				///< Number of items in array provided / Number of items filled in array
				AkPlayingID* out_aPlayingIDs		///< Array of AkPlayingID items to fill
				);

			/// Get the value of a custom property of integer or boolean type.
			AK_EXTERNAPIFUNC( AKRESULT, GetCustomPropertyValue )(
				AkUniqueID in_ObjectID,		///< Object ID
				AkUInt32 in_uPropID,			///< Property ID
				AkInt32& out_iValue				///< Property Value
				);

			/// Get the value of a custom property of real type.
			AK_EXTERNAPIFUNC( AKRESULT, GetCustomPropertyValue )(
				AkUniqueID in_ObjectID,		///< Object ID
				AkUInt32 in_uPropID,			///< Property ID
				AkReal32& out_fValue			///< Property Value
				);

		} //namespace Query
	} //namespace SoundEngine
} //namespace AK

#endif // _AK_QUERYPARAMS_H_
