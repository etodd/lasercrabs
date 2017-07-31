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
/// Spatial Audio interface.

#pragma once

#include <AK/SpatialAudio/Common/AkSpatialAudioTypes.h>

/// Initialization settings of the spatial audio module.
struct AkSpatialAudioInitSettings
{
	AkSpatialAudioInitSettings() : uPoolID(AK_INVALID_POOL_ID)
		, uPoolSize(4 * 1024 * 1024)
	{}

	AkMemPoolId uPoolID;		///< User-provided pool ID (see AK::MemoryMgr::CreatePool).
	AkUInt32 uPoolSize;			///< Desired memory pool size.
};

struct AkReflectImageSource;

/// Settings for a sound emitter.
struct AkEmitterSettings
{
	/// Constructor
	AkEmitterSettings() : reflectAuxBusID(AK_INVALID_UNIQUE_ID)
						, reflectionMaxPathLength(kDefaultMaxPathLength)
						, reflectionsAuxBusGain(1.0f)
						, reflectionsOrder(1)
						, reflectorFilterMask(0xFFFFFFFF)
	{
		useImageSources = true;
	}

	/// Operator =
	AkEmitterSettings& operator =(const AkEmitterSettings & src)
	{
		name = src.name;
		reflectAuxBusID = src.reflectAuxBusID;
		reflectionMaxPathLength = src.reflectionMaxPathLength;
		reflectionsAuxBusGain = src.reflectionsAuxBusGain;
		reflectionsOrder = src.reflectionsOrder;
		reflectorFilterMask = src.reflectorFilterMask;
		useImageSources = src.useImageSources;
		return *this;
	}

	/// Name given to this sound emitter.
	AK::SpatialAudio::OsString name;

	/// Aux bus with the AkReflect plug-in that is used by the geometric reflections API. Set to AK_INVALID_UNIQUE_ID to disable geometric reflections.
	/// \aknote For proper operation with AkReflect and the SpatialAudio API, any aux bus using AkReflect should have 'Enable Positioning' checked and the positioning type should be set to 2D in the Wwise authoring tool. See the Wwise Reflect documentation for more details. \endaknote
	AkUniqueID reflectAuxBusID;

	/// Maximum indirect path length for geometric reflections. Should be no longer (and possibly shorter for less CPU usage) than the maximum attenuation of
	/// the sound emitter.
	AkReal32 reflectionMaxPathLength;

	/// Send gain (0.f-1.f) that is applied when sending to the bus that has the AkReflect plug-in. (reflectAuxBusID)
	AkReal32 reflectionsAuxBusGain;

	/// Maximum number of reflections that will be processed when computing indirect paths via the geometric reflections API. Reflection processing grows
	/// exponentially with the order of reflections, so this number should be kept low.  Valid range: 1-4.
	AkUInt32 reflectionsOrder;

	/// Bit field that allows for filtering of reflector surfaces (triangles) for this sound emitter. Setting/or clearing bits that correspond to the same bits set in 
	/// the \c reflectorChannelMask of each \c AkTriangle can be used to filter out specific geometry for specific emitters. An example usage is to disable reflections 
	/// off the floor for sounds positioned at the camera. When processing the reflections, this bit mask is ANDed with each potential triangle's reflectorChannelMask to determine if the reflector/emitter pair can produce a valid hit.  
	AkUInt32 reflectorFilterMask;

	/// Enable reflections from image sources that have been added via the \c AK::SpatialAudio::AddImageSource() API. (Does not apply to geometric reflections.)
	AkUInt8 useImageSources : 1;
};

/// Structure for describing information about individual triangles 
struct AkTriangle
{
	/// Constructor
	AkTriangle(): textureID(AK_INVALID_UNIQUE_ID)
				, reflectorChannelMask((AkUInt32)-1)
				, strName(NULL) 
	{}

	/// Vertices 
	AkVector point0;
	AkVector point1;
	AkVector point2;

	/// Acoustic texture ShareSet ID
	AkUInt32 textureID;

	/// Bitfield of channels that this triangle belongs to. When processing the reflections, this bit mask is ANDed with each of the emitter's <tt>reflectorFilterMask</tt>s to determine if the reflector/emitter pair can produce a valid hit.  
	AkUInt32 reflectorChannelMask;

	/// Name to describe this triangle. When passing into \c AddGeometrySet, this string is copied internally and may be freed as soon as the function returns.
	const char* strName;
};

/// Structure for retrieving information about the indirect paths of a sound that have been calculated via the geometric reflections API.
struct AkSoundPathInfo
{
	/// Apparent source of the reflected sound that follows this path.
	AkVector imageSource;
	
	/// Vertices of the indirect path.
	AkVector reflectionPoint[AK_MAX_REFLECT_ORDER];

	/// The triangles that were hit in the path. The vertices in \c reflectionPoint[] correspond to points on these triangles.
	AkTriangle triangles[AK_MAX_REFLECT_ORDER];

	/// Number of reflections and valid elements in the \c reflectionPoint[] and \c triangles[] arrays.
	AkUInt32 numReflections;

	/// The point that was hit to cause the path to be occluded. Note that the spatial audio library must be recompiled with \c #define AK_DEBUG_OCCLUSION to enable generation of occluded paths.
	AkVector occlusionPoint;

	/// True if the sound path was occluded. Note that the spatial audio library must be recompiled with \c #define AK_DEBUG_OCCLUSION to enable generation of occluded paths.
	bool isOccluded;
};

/// Parameters passed to \c AddPortal
struct AkPortalParams
{
	/// Constructor
	AkPortalParams() :
		fGain(1.f),
		bEnabled(false)
	{}

	/// Portal opening position
	AkVector						Center;
	
	/// Portal orientation front vector
	AkVector						Front;
	
	/// Portal orientation up vector
	AkVector						Up;

	/// Gain applied to indirect send value when the portal is active.
	AkReal32						fGain;

	/// Whether or not the portal is active/enabled. For example, this parameter may be used to simulate open/closed doors.
	bool							bEnabled;

	/// Name used to identify portal (optional).
	AK::SpatialAudio::OsString		strName;
};

/// Parameters passed to \c AddRoom
struct AkRoomParams
{
	AkRoomParams() : pConnectedPortals(NULL), uNumPortals(0)
	{}

	/// Room Orientation
	AkVector						Up;
	AkVector						Front;

	/// Pointer to an array of ID's of connected portals. If the portal has not yet been added with \c AK::SpatialAudio::AddPortal, then a default, disabled portal will be created,
	/// so that the correct internal associations can be made. The user must then call \c AK::SpatialAudio::AddPortal to update the portal with the correct parameters.
	AkPortalID* pConnectedPortals;

	/// Number of connected portals in the pConnectedPortals array
	AkUInt32	uNumPortals;

	/// Name used to identify room (optional)
	AK::SpatialAudio::OsString		strName;
};

/// Audiokinetic namespace
namespace AK
{
	/// Audiokinetic spatial audio namespace
	namespace SpatialAudio
	{
		/// Access the internal pool ID passed to Init.
		AK_EXTERNAPIFUNC(AkMemPoolId, GetPoolID)();
	
		/// Initialize the SpatialAudio API.  
		AK_EXTERNAPIFUNC(AKRESULT, Init)(const AkSpatialAudioInitSettings& in_initSettings);
		
		/// Terminate the SpatialAudio API.  
		AK_EXTERNAPIFUNC(void, Term)();

		/// Register a game object as a sound emitter in the SpatialAudio API or update settings on a previously registered sound emitter. The game object must have already been 
		/// registered in the sound engine via \c AK::SoundEngine::RegisterGameObj().  
		/// \sa 
		/// - \ref AkEmitterSettings
		AK_EXTERNAPIFUNC(AKRESULT, RegisterEmitter)(
			AkGameObjectID in_gameObjectID,				///< Game object ID
			const AkEmitterSettings& in_roomSettings	///< Settings for the spatial audio emitter.
		);

		/// Unregister a game object as a sound emitter in the SpatialAudio API.  
		/// \sa 
		/// - \ref AK::SpatialAudio::RegisterEmitter
		AK_EXTERNAPIFUNC(AKRESULT, UnregisterEmitter)(
			AkGameObjectID in_gameObjectID				///< Game object ID
		);

		/// Add or update an individual image source for processing via the AkReflect plug-in.  Use this API for detailed placement of
		/// reflection image sources, whose positions have been determined by the client, such as from the results of a ray cast, computation or by manual placement.  One possible
		/// use case is generating reflections that originate far enough away that they can be modeled as a static point source, for example, off of a distant mountain.
		/// The SpatialAudio API manages image sources added via AddImageSource() and sends them to the AkReflect plug-in that is on the aux bus with ID \c in_AuxBusID. 
		/// The image source will apply to all registered spatial audio emitters, if \c AK_INVALID_GAME_OBJECT is passed for \c in_gameObjectID, 
		/// or to one particular game object, if \c in_gameObjectID contains the ID of a valid registered spatial audio emitter. 
		/// AddImageSource takes a room ID to indicate which room the reflection is logically part of, even though the position of the image source may be outside of the extents of the room.  
		/// This ID is used as a filter, so that it is not possible to hear reflections for rooms that the emitter is not inside.  To use this feature, the emitter and listeners rooms must be
		/// specified using SetGameObjectInRoom.  If you are not using the rooms and portals API, or the image source is not associated with a room, pass a default-constructed room ID.
		/// \aknote The \c AkReflectImageSource struct passed in \c in_info must contain a unique image source ID to be able to identify this image source across frames and when updating and/or removing it later.  
		/// Each instance of AkReflect has its own set of data, so you may reuse ID, if desired, a long as \c in_gameObjectID and \c in_AuxBusID are different.
		/// If you are using the geometric reflections API on the same aux bus (as set in \c AkEmitterSettings) and game object(s), there is a small chance of ID conflict, because IDs for 
		///	geometric reflections' image sources are generated internally using a hash function. If a conflict does occur, you will only hear one of the two reflections. \endaknote
		/// \aknote For proper operation with AkReflect and the SpatialAudio API, any aux bus using AkReflect should have 'Enable Positioning' checked and the positioning type should be set to 2D in the Wwise authoring tool. See the Wwise Reflect documentation for more details. \endaknote
		/// \sa 
		/// - \ref AK::SpatialAudio::RegisterEmitter
		/// - \ref AK::SpatialAudio::RemoveImageSource
		/// - \ref AK::SpatialAudio::SetGameObjectInRoom
		AK_EXTERNAPIFUNC(AKRESULT, AddImageSource)(
			const AkReflectImageSource& in_info,					///< Image source information.
			AkUniqueID in_AuxBusID,									///< Aux bus that has the AkReflect plug in for early reflection DSP.
			AkRoomID in_roomID,										///< The ID of the room that the image source is logically a part of; pass a default-constructed ID if not in a room, or not using rooms.
			AkGameObjectID in_gameObjectID = AK_INVALID_GAME_OBJECT,///< GameObjectID, or AK_INVALID_GAME_OBJECT to apply to all game objects.
			const String& in_name = ""								///< Name used to identify the image source.
		);

		/// Remove an individual reflection image source that was previously added via \c AddImageSource.
		/// \sa 
		///	- \ref AK::SpatialAudio::AddImageSource
		AK_EXTERNAPIFUNC(AKRESULT, RemoveImageSource)(
			AkImageSourceID in_srcID,									///< The ID of the image source passed to AddImageSource, within the \c AkReflectImageSource struct.
			AkUniqueID in_AuxBusID,										///< Aux bus that was passed to AddImageSource.
			AkGameObjectID in_gameObjectID = AK_INVALID_GAME_OBJECT		///< Game object ID that was passed to AddImageSource.
		);

		/// Set the aux send values for an emitter game object that has been registered with the SpatialAudio API.  
		/// This function should be called instead of \c AK::SoundEngine::SetGameObjectAuxSendValues() because the spatial audio API adds additional sends to busses for reflection processing.
		/// If the game object is registered with the sound engine, but not with the SpatialAudio API, then the call will be passed on to \c AK::SoundEngine::SetGameObjectAuxSendValues().
		/// \sa 
		/// - \ref AK::SoundEngine::SetGameObjectAuxSendValues
		AK_EXTERNAPIFUNC(AKRESULT, SetEmitterAuxSendValues)(
			AkGameObjectID in_gameObjectID, ///< Game object ID 
			AkAuxSendValue* in_pAuxSends,	///< Aux send values
			AkUInt32 in_uNumAux				///< Number of elements in in_pAuxSends
		);
		
		/// Set the room that the game object is currently located in - usually the result of a containment test performed by the client. The room must have been registered with \c AddRoom.
		/// \sa 
		///	- \ref AK::SpatialAudio::AddRoom
		///	- \ref AK::SpatialAudio::RemoveRoom
		AK_EXTERNAPIFUNC(AKRESULT, SetGameObjectInRoom)(
			AkGameObjectID in_gameObjectID, ///< Game object ID 
			AkRoomID in_CurrentRoomID		///< RoomID that was passed to \c AK::SpatialAudio::AddRoom
			);

		/// Set the position of an emitter game object that has been registered with the SpatialAudio API.  
		/// This function should be called instead of \c AK::SoundEngine::SetPosition(). The argument \c in_sourcePosition should represent the real position of the emitter. It is this position that is used 
		/// for all spatial audio services, such as Rooms and GeometrySets (see SpatialAudio::AddRoom and SpatialAudio::AddGeometrySet). If the user wishes to have apparent or "virtual" positions sent to the sound engine
		/// (for example if the object is occluded and sounds as if it is coming from a different place), then \c in_virtualPositions can be used: 
		/// This array of virtual positions is passed internally to Wwise for computing 3D positioning between the emitter and its listener(s). 
		/// If the array has more than one position, they are interpreted as multiple positions in \c MultiPositionType_MultiDirections mode. See AK::SoundEngine::SetMultiplePositions for more details on multi-position modes.
		/// If you pass 0 virtual positions (default), then \c in_sourcePosition is passed to the Wwise sound engine and further used for spatialization.
		/// If the game object is registered with the sound engine, but not with the SpatialAudio API, then the call will be passed on to AK::SoundEngine::SetPosition().
		AK_EXTERNAPIFUNC(AKRESULT, SetEmitterPosition)(
			AkGameObjectID in_gameObjectID,			///< Game object ID of the sound emitter.
			const AkTransform& in_sourcePosition,	///< Physical position of the emitter in the simulation.
			const AkTransform* in_virtualPositions = NULL, ///< Virtual/apparent positions of the sound, in case of occlusion. Ignored if \c in_uNumVirtualPositions is 0.
			AkUInt16 in_uNumVirtualPositions = 0	///< Number of \c AkTransform structures in \c in_virtualPositions. If 0, \c in_sourcePosition is used for spatialization.
		);

		/// Add or update a set of geometry from the \c SpatialAudio module for geometric reflection processing. A geometry set is a logical set of triangles, formed by any criteria that suits the client,
		/// which will be referenced by the same ID. The ID (\c in_GeomSetID) must be unique and is also chosen by the client in a manner similar to \c AkGameObjectID's. 
		/// The data pointed to by \c in_pTriangles will be copied internally and may be released after this function returns.
		/// \sa 
		///	- \ref AkTriangle
		///	- \ref AK::SpatialAudio::RemoveGeometrySet
		AK_EXTERNAPIFUNC(AKRESULT, AddGeometrySet)(
			AkGeometrySetID in_GeomSetID,	///< Unique geometry set ID, choosen by client.
			AkTriangle* in_pTriangles,		///< Pointer to an array of AkTriangle structures.
			AkUInt32 in_uNumTriangles		///< Number of triangles in in_pTriangles.
		);
		
		/// Remove a set of geometry to the SpatialAudio API.
		/// \sa 
		///	- \ref AK::SpatialAudio::AddGeometrySet
		AK_EXTERNAPIFUNC(AKRESULT, RemoveGeometrySet)(
			AkGeometrySetID in_SetID		///< ID of geometry set to be removed.
		);

		/// Add or update a room. Rooms are used to connect portals and define an orientation for oriented reverbs. This function may be called multiple times with the same ID to update the parameters of the room.
		/// The ID (\c in_RoomID) must be chosen in the same manner as \c AkGameObjectID's, as they are in the same ID-space. The spatial audio lib manages the 
		/// registration/unregistration of internal game objects for rooms that use these IDs and, therefore, must not collide.
		/// \sa
		/// - \ref AkRoomID
		/// - \ref AkRoomParams
		/// - \ref AK::SpatialAudio::RemoveRoom
		AK_EXTERNAPIFUNC(AKRESULT, AddRoom)(
			AkRoomID in_RoomID,				///< Unique room ID, chosen by the client.
			const AkRoomParams& in_Params	///< Parameter for the room.
			);

		/// Remove a room.
		/// \sa
		/// - \ref AkRoomID
		/// - \ref AK::SpatialAudio::AddRoom
		AK_EXTERNAPIFUNC(AKRESULT, RemoveRoom)(
			AkRoomID in_RoomID	///< Room ID that was passed to \c AddRoom.
			);

		/// Add or update an acoustic portal. A portal is an opening that connects two or more rooms to simulate the transmission of reverberated (indirect) sound between the rooms. 
		/// This function may be called multiple times with the same ID to update the parameters of the portal. The ID (\c in_PortalID) must be chosen in the same manner as \c AkGameObjectID's, 
		/// as they are in the same ID-space. The spatial audio lib manages the registration/unregistration of internal game objects for portals that use these IDs, and therefore must not collide.
		/// \sa
		/// - \ref AkPortalID
		/// - \ref AkPortalParams
		/// - \ref AK::SpatialAudio::RemovePortal
		AK_EXTERNAPIFUNC(AKRESULT, AddPortal)(
			AkPortalID in_PortalID,		///< Unique portal ID, chosen by the client.
			const AkPortalParams& in_Params	///< Parameter for the portal.
			);

		/// Remove a portal.
		/// \sa
		/// - \ref AkPortalID
		/// - \ref AK::SpatialAudio::AddPortal
		AK_EXTERNAPIFUNC(AKRESULT, RemovePortal)(
			AkPortalID in_PortalID		///< ID of portal to be removed, which was originally passed to AddPortal.
			);

		/// Query information about the indirect paths that have been calculated via geometric reflection processing in the SpatialAudio API. This function can be used for debugging purposes.
		/// This function must acquire the global sound engine lock and, therefore, may block waiting for the lock.
		/// \sa
		/// - \ref AkSoundPathInfo
		AK_EXTERNAPIFUNC(AKRESULT, QueryIndirectPaths)(
			AkGameObjectID in_gameObjectID, ///< The ID of the game object that the client wishes to query.
			AkVector& out_listenerPos,		///< The position of the listener game object that is associated with the game object \c in_gameObjectID.
			AkVector& out_emitterPos,		///< The position of the emitter game object \c in_gameObjectID.
			AkSoundPathInfo* out_aPaths,	///< Pointer to an array of \c AkSoundPathInfo's which will be filled after returning.
			AkUInt32& io_uArraySize			///< The number of slots in \c out_aPaths, after returning the number of valid elements written.
		);

		/// Helper function.  
		/// If the sound is occluded (the listener and emitter are in different rooms with no line of sight), then this function may be used to calculate virtual positions of the sound
		/// emitter. The virtual positions represent the 'apparent' source position, because we assume that very little energy is transmitted through the solid wall, 
		/// the sound appears to come from the portal. The virtual positions may then be passed to the sound engine via \c AK::SpatialAudio::SetEmitterPosition.
		/// \sa
		/// - \ref AK::SpatialAudio::SetEmitterPosition
		AK_EXTERNAPIFUNC(bool, CalcOcclusionAndVirtualPositions)(
			const AkVector&		in_EmitterPos,			///< Position of sound emitter.
			AkRoomID			in_EmitterRoomID,		///< ID of the room that the sound emitter is within.
			const AkVector&		in_ListenerPos,			///< Position of listener.
			AkRoomID			in_listenerRoomID,		///< ID of the room that the listener is within.
			AkReal32&			out_fOcclusionFactor,	///< After returning, contains the occlusion value for the emitter, to be passed to Wwise.
			AkReal32&			out_fObstructionFactor, ///< After returning, contains the obstruction value for the emitter, to be passed to Wwise.
			AkTransform*		io_aVirtualPositions,	///< An array of size \c io_uArraySize.  After returning true, contains the virtual positions of the sound emitter.
			AkUInt32&			io_uArraySize			///< Number of elements in \c io_aVirtualPositions.  After returning, contains the number of valid virtual positions.
		);
	}
};
