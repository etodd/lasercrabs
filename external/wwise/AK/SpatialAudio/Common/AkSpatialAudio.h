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

Version: v2017.2.1  Build: 6524
Copyright (c) 2006-2018 Audiokinetic Inc.
*******************************************************************************/

/// \file 
/// Spatial Audio interface.

#pragma once

#include <AK/SpatialAudio/Common/AkSpatialAudioTypes.h>
#include <AK/Plugin/AkReflectGameData.h>

/// AkDiffractionFlags determine if diffraction values for sound passing through portals will be calculated, and how to apply those calculations to Wwise parameters.
enum AkDiffractionFlags
{
	DiffractionFlags_UseBuiltInParam = 1 << 0,				///< Set the Wwise built-in game parameter value for diffraction angle. This is a flexible approach that can be mapped to any Wwise parameter.  Can also be used simply to monitor diffraction using the game object profiler in Wwise.
	DiffractionFlags_UseObstruction = 1 << 1,				///< Use Wwise obstruction for dry-path diffraction. Dry-path diffraction is related to the angle off of a straight line of sight to a sound emitter.
	DiffractionFlags_CalcEmitterVirtualPosition = 1 << 3,	///< An emitter that is not in the same room as the listener will have its apparent or virtual position calculated by Wwise spatial audio and passed on to the sound engine.

	DefaultDiffractionFlags = DiffractionFlags_UseBuiltInParam | DiffractionFlags_UseObstruction | DiffractionFlags_CalcEmitterVirtualPosition
};

/// Initialization settings of the spatial audio module.
struct AkSpatialAudioInitSettings
{
	AkSpatialAudioInitSettings() : uPoolID(AK_INVALID_POOL_ID)
		, uPoolSize(4 * 1024 * 1024)
		, uMaxSoundPropagationDepth(AK_MAX_SOUND_PROPAGATION_DEPTH)
		, uDiffractionFlags((AkUInt32)DefaultDiffractionFlags)
		, fDiffractionShadowAttenFactor(AK_DEFAULT_DIFFR_SHADOW_ATTEN)
		, fDiffractionShadowDegrees(AK_DEFAULT_DIFFR_SHADOW_DEGREES)
	{}

	AkMemPoolId uPoolID;					///< User-provided pool ID (see AK::MemoryMgr::CreatePool).
	AkUInt32 uPoolSize;						///< Desired memory pool size if a new pool should be created. A pool will be created if uPoolID is not set (AK_INVALID_POOL_ID).
	AkUInt32 uMaxSoundPropagationDepth;		///< Maximum number of rooms that sound can propagate through; must be less than or equal to AK_MAX_SOUND_PROPAGATION_DEPTH.
	AkUInt32 uDiffractionFlags;				///< Enable or disable specific diffraction features. See AkDiffractionFlags.
	AkReal32 fDiffractionShadowAttenFactor;	///< Multiplier that is applied to the distance attenuation of diffracted sounds (sounds that are in the 'shadow region') to simulate the phenomenon where by diffracted sound waves decay faster than incident sound waves. 
	AkReal32 fDiffractionShadowDegrees;		///< Interpolation angle, in degrees, over which the fDiffractionShadowAttenFactor is applied.  
											///< At a diffraction of 0, a multiplier of 1 (ie. none) is applied, and at an angle of fDiffractionShadowDegrees or greater, fDiffractionShadowAttenFactor is applied.
											///< A linear interpolation between 1 and fDiffractionShadowAttenFactor is applied when the angle is between 0 and fDiffractionShadowDegrees.
};

// Settings for individual image sources.
struct AkImageSourceSettings
{
	AkImageSourceSettings() {}

	AkImageSourceSettings(AkVector in_sourcePosition, AkReal32 in_fDistanceScalingFactor, AkReal32 in_fLevel)
		: params(in_sourcePosition, in_fDistanceScalingFactor, in_fLevel)
		, texture()
		, name()
	{
	}

	~AkImageSourceSettings()
	{
		name.Term();
	}

	/// Operator =
	AkImageSourceSettings& operator =(const AkImageSourceSettings & src)
	{
		params = src.params;
		texture = src.texture;
		name = src.name;
		name.AllocCopy();
		return *this;
	}

	void SetOneTexture(AkUniqueID in_texture)
	{
		texture.uNumTexture = 1;
		texture.arTextureID[0] = in_texture;
	}

	void SetName(const char* in_pName)
	{
		name = in_pName;
		name.AllocCopy();
	}

	/// Image source parameters.
	AkImageSourceParams params;

	/// Acoustic texture that goes with this image source.
	AkImageSourceTexture texture;

	/// Name given to image source, can be used to identify the image source in the AK Reflect plugin UI.
	AK::SpatialAudio::String name;
};

/// Settings for a sound emitter.
struct AkEmitterSettings
{
	/// Constructor
	AkEmitterSettings() : reflectAuxBusID(AK_INVALID_UNIQUE_ID)
						, reflectionMaxPathLength(kDefaultMaxPathLength)
						, reflectionsAuxBusGain(1.0f)
						, reflectionsOrder(1)
						, reflectorFilterMask(0xFFFFFFFF)
						, roomReverbAuxBusGain(1.0f)
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
		roomReverbAuxBusGain = src.roomReverbAuxBusGain;
		useImageSources = src.useImageSources;
		return *this;
	}

	/// Name given to this sound emitter.
	AK::SpatialAudio::OsString name;

	/// Aux bus with the AkReflect plug-in that is used by the geometric reflections API. Set to AK_INVALID_UNIQUE_ID to disable geometric reflections.
	/// \aknote For proper operation with AkReflect and the SpatialAudio API, any aux bus using AkReflect should have 'Enable Positioning' checked and the positioning type should be set to 2D in the Wwise authoring tool. See the Wwise Reflect documentation for more details. \endaknote
	AkUniqueID reflectAuxBusID;

	/// A heuristic to stop the computation of reflections. Should be no longer (and possibly shorter for less CPU usage) than the maximum attenuation of
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

	/// Send gain (0.f-1.f) that is applied when sending to the bus that is associated with the room that the emitter is in (assigned via ReverbAuxBus field of AkRoomParams).
	/// This value is multiplied with the room-specific gain (ReverbLevel of AkRoomParams).  Note that this value is not applied to aux sends that are set via SetEmitterAuxSendValues(),
	/// and therefore may be used to scale and/or disable room reverb, either temporarily or permanently, for a specific emitter.
	/// \sa
	/// - \ref AkRoomParams
	/// - \ref AK::SpatialAudio::SetEmitterAuxSendValues
	AkReal32 roomReverbAuxBusGain;

	/// Enable reflections from image sources that have been added via the \c AK::SpatialAudio::SetImageSource() API. (Does not apply to geometric reflections.)
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

	/// Name to describe this triangle. When passing into \c SetGeometry, this string is copied internally and may be freed as soon as the function returns.
	const char* strName;
};

/// Structure for retrieving information about the indirect paths of a sound that have been calculated via the geometric reflections API. Useful for debug draw applications.
struct AkSoundPathInfo
{
	/// Apparent source of the reflected sound that follows this path.
	AkVector imageSource;
	
	/// Vertices of the indirect path.
	/// reflectionPoint[0] is closest to the emitter, reflectionPoint[numReflections-1] is closest to the listener.
	AkVector reflectionPoint[AK_MAX_REFLECT_ORDER];

	/// The triangles that were hit in the path. The vertices in \c reflectionPoint[] correspond to points on these triangles.
	/// triangles[0] is closest to the emitter, triangles[numReflections-1] is closest to the listener.
	AkTriangle triangles[AK_MAX_REFLECT_ORDER];

	/// Number of reflections and valid elements in the \c reflectionPoint[] and \c triangles[] arrays.
	AkUInt32 numReflections;

	/// The point that was hit to cause the path to be occluded. Note that the spatial audio library must be recompiled with \c #define AK_DEBUG_OCCLUSION to enable generation of occluded paths.
	AkVector occlusionPoint;

	/// True if the sound path was occluded. Note that the spatial audio library must be recompiled with \c #define AK_DEBUG_OCCLUSION to enable generation of occluded paths.
	bool isOccluded;
};

/// Structure for retrieving information about the sound propagation paths that have been calculated via the rooms and portals API.  Useful for debug draw applications.
struct AkPropagationPathInfo
{
	/// Defines the maximum number of nodes that a user can retrieve information about.  Longer paths will be truncated. 
	static const AkUInt32 kMaxNodes = AK_MAX_SOUND_PROPAGATION_DEPTH;

	///  Nodes in the path; they repreent vertices within the bounds of a portal's opening.  
	AkVector nodePoint[kMaxNodes];

	/// ID of the portal's that the path passes through.  portal[0] is the portal closest to the listener; portal[numNodes-1] is the portal closest to the emitter.
	AkPortalID portals[kMaxNodes];

	/// ID's of the rooms that the path passes through. There is always one more room than portal. room[0] is the listener's room; the emitters room is room[numNodes].
	AkRoomID rooms[kMaxNodes+1];

	/// Represents the number of valid nodes in nodePoint[] and the number of valid portals in portals[].  The number of rooms in rooms[] is numNodes+1. There is always one more room than portal.
	AkUInt32 numNodes;

	/// The sum of all straight line path segments, including the segment from the listener to nodePoint[0], between each subsequent nodePoint, and from nodePoint[numNodes-1] to the emitter.
	AkReal32 length;
	
	/// The cumulative gain (linear) of all portals traversed.  
	AkReal32 gain;

	/// The dry diffraction angle is measured in radians, and calculated from the maximum deviation from a straight line, of all angles at each nodePoint.  Can be thought of as how
	//	far into the 'shadow region' the sound has to 'bend' to reach the listener.  Valid range: 0 - PI radians.
	/// Depending on the spatial audio initialization settings, this value that is applied internally, by spatial audio, to the obstruction or built-in parameter of the emitter game object.
	/// \sa
	/// - \ref AkDiffractionFlags
	/// - \ref AkSpatialAudioInitSettings
	AkReal32 dryDiffractionAngle;

	/// The wet diffraction for the portal closest to the listener.  
	/// The wet diffraction angle is measured in radians and calculated from how far into the 'shadow region' the listener is from the closest portal.  Unlike dry diffraction, the 
	/// wet diffraction does not depend on the incident angle, but only the normal of the portal.  Valid range: 0 - PI radians.
	/// Depending on the spatial audio initialization settings, this value that is applied internally, by spatial audio, to the obstruction/built-in parameter of the room game object that is
	/// on the other side of the portal closest to the listener.
	/// \sa
	/// - \ref AkDiffractionFlags
	/// - \ref AkSpatialAudioInitSettings
	AkReal32 wetDiffractionAngle;
};

/// Parameters passed to \c SetPortal
struct AkPortalParams
{
	/// Constructor
	AkPortalParams() :
		bEnabled(false)
	{}

	/// Portal's position and orientation in the 3D world. 
	/// Position vector is the center of the opening.
	/// OrientationFront vector must be unit-length and point along the normal of the portal, and must be orthogonal to Up. It defines the local positive-Z dimension (depth/transition axis) of the portal, used by Extent. 
	/// OrientationTop vector must be unit-length and point along the top of the portal (tangent to the wall), must be orthogonal to Front. It defines the local positive-Y direction (height) of the portal, used by Extent.
	AkTransform						Transform;

	/// Portal extent. Defines the dimensions of the portal relative to its center; all components must be positive numbers. The local X and Y dimensions (side and top) are used in diffraction calculations, 
	/// whereas the Z dimension (front) defines a depth value which is used to implement smooth transitions between rooms. It is recommended that users experiment with different portal depths to find a value 
	/// that results in appropriately smooth transitions between rooms.
	AkVector						Extent;

	/// Whether or not the portal is active/enabled. For example, this parameter may be used to simulate open/closed doors.
	/// Portal diffraction is simulated when at least one portal exists and is active between an emitter and the listener. Otherwise, transmission is simulated. 
	/// Diffraction uses obstruction or the diffraction built-in game parameter, while transmission uses occlusion.
	bool							bEnabled;

	/// Name used to identify portal (optional).
	AK::SpatialAudio::OsString		strName;

	/// ID of the room to which the portal connects, in the direction of the Front vector.  If a room with this ID has not been added via AK::SpatialAudio::SetRoom,
	/// a room will be created with this ID and with default AkRoomParams.  If you would later like to update the AkRoomParams, simply call AK::SpatialAudio::SetRoom again with this same ID.
	///	- \ref AK::SpatialAudio::SetRoom
	///	- \ref AK::SpatialAudio::RemoveRoom
	/// - \ref AkRoomParams
	AkRoomID FrontRoom;

	/// ID of the room to which the portal connects, in the direction opposite to the Front vector. If a room with this ID has not been added via AK::SpatialAudio::SetRoom,
	/// a room will be created with this ID and with default AkRoomParams.  If you would later like to update the AkRoomParams, simply call AK::SpatialAudio::SetRoom again with this same ID.
	///	- \ref AK::SpatialAudio::SetRoom
	///	- \ref AK::SpatialAudio::RemoveRoom
	/// - \ref AkRoomParams
	AkRoomID BackRoom;
};

/// Parameters passed to \c SetRoom
struct AkRoomParams
{
	/// Constructor
	AkRoomParams() :  ReverbAuxBus(AK_INVALID_AUX_ID)
					, ReverbLevel(1.f)
					, WallOcclusion(1.f)
					, RoomGameObj_AuxSendLevelToSelf(0.f)
					, RoomGameObj_KeepRegistered(false)
					
	{
		Up.X = 0.f;
		Up.Y = 1.f;
		Up.Z = 0.f;
		Front.X = 0.f;
		Front.Y = 0.f;
		Front.Z = 1.f;
	}

	/// Room Orientation. Up and Front must be orthonormal.
	/// Room orientation has an effect when the associated aux bus (see ReverbAuxBus) is set with 3D positioning in Wwise, as 3D positioning implements relative rotation of the emitter (room) and listener.
	AkVector						Up;
	AkVector						Front;

	/// The reverb aux bus that is associated with this room.  
	/// When Spatial Audio is told that a game object is in a particular room via SetGameObjectInRoom, a send to this aux bus will be created to model the reverb of the room.
	/// Using a combination of Rooms and Portals, Spatial Audio manages which game object the aux bus is spawned on, and what control gain is sent to the bus.  
	/// When a game object is inside a connected portal, as defined by the portal's orientation and extent vectors, both this aux send and the aux send of the adjacent room are active.
	/// Spatial audio modulates the control value for each send based on the game object's position, in relation to the portal's z-azis and extent, to crossfade the reverb between the two rooms.
	/// If more advanced control of reverb is desired, SetEmitterAuxSendValues can be used to add additional sends on to a game object.
	/// - \ref AK::SpatialAudio::SetGameObjectInRoom
	/// - \ref AK::SpatialAudio::SetEmitterAuxSendValues
	AkAuxBusID						ReverbAuxBus;

	/// The reverb control value for the send to ReverbAuxBus. Valid range: (0.f-1.f)
	/// Can be used to implement multiple rooms that share the same aux bus, but have different reverb levels.
	AkReal32						ReverbLevel;

	/// Occlusion level to set when modeling transmission through walls.  Transmission is modeled only when no paths to the listener are found, meaning that there are no active portals between the listener's
	/// room and the emitter's room. Valid range: (0.f-1.f)
	AkReal32						WallOcclusion;

	/// Unused
	AkInt32							Priority;

	/// Name used to identify room (optional)
	AK::SpatialAudio::OsString		strName;

	/// Send level for sounds that are posted on the room game object; adds reverb to ambience and room tones. Valid range: (0.f-1.f).  Set to a value greater than 0 to have spatial audio create a send on the room game object, 
	/// where the room game object itself is specified as the listener and ReverbAuxBus is specified as the aux bus. A value of 0 disables the aux send. This should not be confused with ReverbLevel, which is the send level 
	/// for spatial audio emitters sending to the room game object.
	/// \aknote The room game object can be accessed though the ID that is passed to \c SetRoom() and the \c AkRoomID::AsGameObjectID() method.  Posting an event on the room game object leverages automatic room game object placement 
	///	by spatial audio so that when the listener is inside the room, the sound comes from all around the listener, and when the listener is outside the room, the sound comes from the portal(s). Typically, this would be used for
	/// surround ambiance beds or room tones. Point source sounds should use separate game objects that are registered as spatial audio emitters.
	/// \sa
	/// - \ref AkRoomParams::RoomGameObj_KeepRegistered
	/// - \ref AkRoomID
	AkReal32						RoomGameObj_AuxSendLevelToSelf;

	/// If set to true, the room game object will be registered on calling \c SetRoom(), and not released untill the room is deleted or removed with \c RemoveRoom(). If set to false, spatial audio will register
	/// the room object only when it is needed by the sound propagation system for the purposes of reverb, and will unregister the game object when all reverb tails have finished.
	/// If the game intends to post events on the room game object for the purpose of ambiance or room tones, RoomGameObj_KeepRegistered should be set to true.
	/// \aknote The room game object can be accessed though the ID that is passed to \c SetRoom() and the \c AkRoomID::AsGameObjectID() method.  Posting an event on the room game object leverages automatic room game object placement 
	///	by spatial audio so that when the listener is inside the room, the sound comes from all around the listener, and when the listener is outside the room, the sound comes from the portal(s). Typically, this would be used for
	/// surround ambiance beds or room tones. Point source sounds should use separate game objects that are registered as spatial audio emitters.
	/// \sa
	/// - \ref AkRoomParams::RoomGameObj_AuxSendLevelToSelf
	/// - \ref AkRoomID
	bool							RoomGameObj_KeepRegistered;


};

/// Audiokinetic namespace
namespace AK
{
	/// Audiokinetic spatial audio namespace
	namespace SpatialAudio
	{
		////////////////////////////////////////////////////////////////////////
		/// @name Basic functions. 
		/// In order to use SpatialAudio, you need to initalize it using Init, and register all emitters and listeners that you plan on using with any of the services offered by SpatialAudio, using 
		/// RegisterEmitter and RegisterListener respectively, _after_ having registered their corresponding game object to the sound engine. The position of these objects and game-defined sends should be updated with 
		/// SetPosition and SetEmitterAuxSendValues instead of their AK::SoundEngine counterparts.
		///\akwarning At the moment, there can be only one Spatial Audio listener registered at any given time.
		//@{

		/// Access the internal pool ID passed to Init.
		AK_EXTERNAPIFUNC(AkMemPoolId, GetPoolID)();
	
		/// Initialize the SpatialAudio API.  
		AK_EXTERNAPIFUNC(AKRESULT, Init)(const AkSpatialAudioInitSettings& in_initSettings);
		
		/// Register a game object as a sound emitter in the SpatialAudio API or update settings on a previously registered sound emitter. The game object must have already been 
		/// registered in the sound engine via \c AK::SoundEngine::RegisterGameObj().  
		/// \sa 
		/// - \ref AkEmitterSettings
		AK_EXTERNAPIFUNC(AKRESULT, RegisterEmitter)(
			AkGameObjectID in_gameObjectID,				///< Game object ID
			const AkEmitterSettings& in_settings		///< Settings for the spatial audio emitter.
		);

		/// Unregister a game object as a sound emitter in the SpatialAudio API; clean up Spatial Audio listener data associated with in_gameObjectID.  
		/// This function is optional - emitters are automatically unregistered when their game object is deleted in the sound engine.
		/// \sa 
		/// - \ref AK::SpatialAudio::RegisterEmitter
		AK_EXTERNAPIFUNC(AKRESULT, UnregisterEmitter)(
			AkGameObjectID in_gameObjectID				///< Game object ID
		);

		/// Assign a game object as the Spatial Audio listener.  There can be only one Spatial Audio listener registered at any given time; in_gameObjectID will replace any previously set Spatial Audio listener.
		/// The game object passed in must be registered by the client, at some point, for sound to be heard.  It is not necessary to be registered at the time of calling this function.
		/// If not listener is explicitly registered to spatial audio, then a default listener (set via \c AK::SoundEngine::SetDefaultListeners()) is selected.  If the are no default listeners, or there are more than one
		/// default listeners, then it is necessary to call RegisterListener() to specify which listener to use with Spatial Audio.
		AK_EXTERNAPIFUNC(AKRESULT, RegisterListener)(
			AkGameObjectID in_gameObjectID				///< Game object ID
			);

		/// Unregister a game object as a listener in the SpatialAudio API; clean up Spatial Audio listener data associated with in_gameObjectID.  
		/// If in_gameObjectID is the current registered listener, calling this function will clear the Spatial Audio listener and
		/// Spatial Audio features will be disabled until another listener is registered.
		/// This function is optional - listener are automatically unregistered when their game object is deleted in the sound engine.
		/// \sa 
		/// - \ref AK::SpatialAudio::RegisterListener
		AK_EXTERNAPIFUNC(AKRESULT, UnregisterListener)(
			AkGameObjectID in_gameObjectID				///< Game object ID
			);

		/// Set the position of an emitter game object that has been registered with the SpatialAudio API.  
		/// This function should be called instead of \c AK::SoundEngine::SetPosition(). The argument \c in_sourcePosition should represent the real position of the emitter. It is this position that is used 
		/// for all spatial audio services, such as Rooms and GeometrySets (see SpatialAudio::SetRoom and SpatialAudio::SetGeometry).
		/// When an emitter is in a different room from the listener, its virtual positions are automatically calculated based on the relative positions of the listener and nearby portals, and it is these virtual positions
		/// that are passed to the sound engine and used to render audio.
		/// If the game object is registered with the sound engine, but not with the SpatialAudio API, then the call will be passed on to AK::SoundEngine::SetPosition().
		AK_EXTERNAPIFUNC(AKRESULT, SetPosition)(
			AkGameObjectID in_gameObjectID,			///< Game object ID of the sound emitter.
			const AkTransform& in_sourcePosition	///< Physical position of the emitter in the simulation.
			);

		/// Set the aux send values for an emitter game object that has been registered with the SpatialAudio API.  
		/// This function should be called instead of \c AK::SoundEngine::SetGameObjectAuxSendValues() because the spatial audio API adds additional sends for room's late reverb and
		//	for early reflection processing.
		/// If the game object is registered with the sound engine, but not with the SpatialAudio API, then the call will be passed on to \c AK::SoundEngine::SetGameObjectAuxSendValues().
		/// \sa 
		/// - \ref AK::SoundEngine::SetGameObjectAuxSendValues
		AK_EXTERNAPIFUNC(AKRESULT, SetEmitterAuxSendValues)(
			AkGameObjectID in_gameObjectID, ///< Game object ID of the sound emitter.
			AkAuxSendValue* in_pAuxSends,	///< Aux send values
			AkUInt32 in_uNumAux				///< Number of elements in in_pAuxSends
			);

		//@}

		////////////////////////////////////////////////////////////////////////
		/// @name Helper functions for passing game data to the Wwise Reflect plug-in. 
		/// Use this API for detailed placement of reflection image sources.
		/// \aknote These functions are low-level and useful when your game engine already implements a geometrical approach to sound propagation such as an image-source or a ray tracing algorithm.
		/// Functions of Geometry are preferred and easier to use with the Wwise Reflect plug-in. \endaknote
		//@{

		/// Add or update an individual image source for processing via the AkReflect plug-in.  Use this API for detailed placement of
		/// reflection image sources, whose positions have been determined by the client, such as from the results of a ray cast, computation or by manual placement.  One possible
		/// use case is generating reflections that originate far enough away that they can be modeled as a static point source, for example, off of a distant mountain.
		/// The SpatialAudio API manages image sources added via SetImageSource() and sends them to the AkReflect plug-in that is on the aux bus with ID \c in_AuxBusID. 
		/// The image source will apply to all registered spatial audio emitters, if \c AK_INVALID_GAME_OBJECT is passed for \c in_gameObjectID, 
		/// or to one particular game object, if \c in_gameObjectID contains the ID of a valid registered spatial audio emitter. 
		/// SetImageSource takes a room ID to indicate which room the reflection is logically part of, even though the position of the image source may be outside of the extents of the room.  
		/// This ID is used as a filter, so that it is not possible to hear reflections for rooms that the emitter is not inside.  To use this feature, the emitter and listeners rooms must be
		/// specified using SetGameObjectInRoom.  If you are not using the rooms and portals API, or the image source is not associated with a room, pass a default-constructed room ID.
		/// \aknote The \c AkImageSourceSettings struct passed in \c in_info must contain a unique image source ID to be able to identify this image source across frames and when updating and/or removing it later.  
		/// Each instance of AkReflect has its own set of data, so you may reuse ID, if desired, as long as \c in_gameObjectID and \c in_AuxBusID are different.
		/// If you are using the geometric reflections API on the same aux bus (as set in \c AkEmitterSettings) and game object(s), there is a small chance of ID conflict, because IDs for 
		///	geometric reflections' image sources are generated internally using a hash function. If a conflict does occur, you will only hear one of the two reflections. \endaknote
		/// \aknote For proper operation with AkReflect and the SpatialAudio API, any aux bus using AkReflect should have 'Enable Positioning' checked and the positioning type should be set to 2D in the Wwise authoring tool. See the Wwise Reflect documentation for more details. \endaknote
		/// \sa 
		/// - \ref AK::SpatialAudio::RegisterEmitter
		/// - \ref AK::SpatialAudio::RemoveImageSource
		/// - \ref AK::SpatialAudio::SetGameObjectInRoom
		AK_EXTERNAPIFUNC(AKRESULT, SetImageSource)(
			AkImageSourceID in_srcID,								///< The ID of the image source being added.
			const AkImageSourceSettings& in_info,					///< Image source information.
			AkUniqueID in_AuxBusID,									///< Aux bus that has the AkReflect plug in for early reflection DSP.
			AkRoomID in_roomID,										///< The ID of the room that the image source is logically a part of; pass a default-constructed ID if not in, or not using, a room.
			AkGameObjectID in_gameObjectID = AK_INVALID_GAME_OBJECT ///< The ID of the emitter game object to which the image source applies. Pass AK_INVALID_GAME_OBJECT to apply to all emitter game objects.
		);

		/// Remove an individual reflection image source that was previously added via \c SetImageSource.
		/// \sa 
		///	- \ref AK::SpatialAudio::SetImageSource
		AK_EXTERNAPIFUNC(AKRESULT, RemoveImageSource)(
			AkImageSourceID in_srcID,									///< The ID of the image source to remove.
			AkUniqueID in_AuxBusID,										///< Aux bus that was passed to SetImageSource.
			AkGameObjectID in_gameObjectID = AK_INVALID_GAME_OBJECT		///< Game object ID that was passed to SetImageSource.
		);

		//@}

		////////////////////////////////////////////////////////////////////////
		/// @name Geometry 
		/// Geometry API for early reflection processing using Wwise Reflect.
		//@{

		/// Add or update a set of geometry from the \c SpatialAudio module for geometric reflection processing. A geometry set is a logical set of triangles, formed by any criteria that suits the client,
		/// which will be referenced by the same ID. The ID (\c in_GeomSetID) must be unique and is also chosen by the client in a manner similar to \c AkGameObjectID's. 
		/// The data pointed to by \c in_pTriangles will be copied internally and may be released after this function returns.
		/// \sa 
		///	- \ref AkTriangle
		///	- \ref AK::SpatialAudio::RemoveGeometry
		AK_EXTERNAPIFUNC(AKRESULT, SetGeometry)(
			AkGeometrySetID in_GeomSetID,	///< Unique geometry set ID, chosen by client.
			AkTriangle* in_pTriangles,		///< Pointer to an array of AkTriangle structures.
			AkUInt32 in_uNumTriangles		///< Number of triangles in in_pTriangles.
		);
		
		/// Remove a set of geometry to the SpatialAudio API.
		/// \sa 
		///	- \ref AK::SpatialAudio::SetGeometry
		AK_EXTERNAPIFUNC(AKRESULT, RemoveGeometry)(
			AkGeometrySetID in_SetID		///< ID of geometry set to be removed.
		);

		/// Query information about the indirect paths that have been calculated via geometric reflection processing in the SpatialAudio API. This function can be used for debugging purposes.
		/// This function must acquire the global sound engine lock and therefore, may block waiting for the lock.
		/// \sa
		/// - \ref AkSoundPathInfo
		AK_EXTERNAPIFUNC(AKRESULT, QueryIndirectPaths)(
			AkGameObjectID in_gameObjectID, ///< The ID of the game object that the client wishes to query.
			AkVector& out_listenerPos,		///< Returns the position of the listener game object that is associated with the game object \c in_gameObjectID.
			AkVector& out_emitterPos,		///< Returns the position of the emitter game object \c in_gameObjectID.
			AkSoundPathInfo* out_aPaths,	///< Pointer to an array of \c AkSoundPathInfo's which will be filled after returning.
			AkUInt32& io_uArraySize			///< The number of slots in \c out_aPaths, after returning the number of valid elements written.
			);

		//@}

		////////////////////////////////////////////////////////////////////////
		/// @name Rooms and Portals
		/// Sound Propagation API using rooms and portals.
		//@{

		/// Add or update a room. Rooms are used to connect portals and define an orientation for oriented reverbs. This function may be called multiple times with the same ID to update the parameters of the room.
		/// The ID (\c in_RoomID) must be chosen in the same manner as \c AkGameObjectID's, as they are in the same ID-space. The spatial audio lib manages the 
		/// registration/unregistration of internal game objects for rooms that use these IDs and, therefore, must not collide.
		/// \sa
		/// - \ref AkRoomID
		/// - \ref AkRoomParams
		/// - \ref AK::SpatialAudio::RemoveRoom
		AK_EXTERNAPIFUNC(AKRESULT, SetRoom)(
			AkRoomID in_RoomID,				///< Unique room ID, chosen by the client.
			const AkRoomParams& in_Params	///< Parameter for the room.
			);

		/// Remove a room.
		/// \sa
		/// - \ref AkRoomID
		/// - \ref AK::SpatialAudio::SetRoom
		AK_EXTERNAPIFUNC(AKRESULT, RemoveRoom)(
			AkRoomID in_RoomID	///< Room ID that was passed to \c SetRoom.
			);

		/// Add or update an acoustic portal. A portal is an opening that connects two or more rooms to simulate the transmission of reverberated (indirect) sound between the rooms. 
		/// This function may be called multiple times with the same ID to update the parameters of the portal. The ID (\c in_PortalID) must be chosen in the same manner as \c AkGameObjectID's, 
		/// as they are in the same ID-space. The spatial audio lib manages the registration/unregistration of internal game objects for portals that use these IDs, and therefore must not collide.
		/// \sa
		/// - \ref AkPortalID
		/// - \ref AkPortalParams
		/// - \ref AK::SpatialAudio::RemovePortal
		AK_EXTERNAPIFUNC(AKRESULT, SetPortal)(
			AkPortalID in_PortalID,		///< Unique portal ID, chosen by the client.
			const AkPortalParams& in_Params	///< Parameter for the portal.
			);

		/// Remove a portal.
		/// \sa
		/// - \ref AkPortalID
		/// - \ref AK::SpatialAudio::SetPortal
		AK_EXTERNAPIFUNC(AKRESULT, RemovePortal)(
			AkPortalID in_PortalID		///< ID of portal to be removed, which was originally passed to SetPortal.
			);

		/// Set the room that the game object is currently located in - usually the result of a containment test performed by the client. The room must have been registered with \c SetRoom.
		///	Setting the room for a game object provides the basis for the sound propagation service, and also sets which room's reverb aux bus to send to.  The sound propagation service traces the path
		/// of the sound from the emitter to the listener, and calculates the diffraction as the sound passes through each portal.  The portals are used to define the spatial location of the diffracted and reverberated
		/// audio.
		/// \sa 
		///	- \ref AK::SpatialAudio::SetRoom
		///	- \ref AK::SpatialAudio::RemoveRoom
		AK_EXTERNAPIFUNC(AKRESULT, SetGameObjectInRoom)(
			AkGameObjectID in_gameObjectID, ///< Game object ID 
			AkRoomID in_CurrentRoomID		///< RoomID that was passed to \c AK::SpatialAudio::SetRoom
			);

		/// Set the obstruction value of an emitter game object that has been registered with the SpatialAudio API.  
		/// This function should be called instead of \c AK::SoundEngine::SetObjectObstructionAndOcclusion(), and can be used for setting the obstruction value on a Spatial Audio emitter.
		/// SetEmitterObstruction() should be used to simulate obstructing objects between the emitter and the listener, if they are in the same room, or between the emitter and a portal, if the emitter
		///	and listener are in different room.  
		/// If the listener and the emitter are in different rooms, the maximum of the value passed into SetEmitterObstruction() and the the obstruction value calculated from the diffraction angle is passed to the sound engine.
		/// If the game object is not registered as an emitter with Spatial Audio, then an error will be reported, and the call will have no effect.
		/// \aknote The game is responsible to differentiate between obstruction between the emitter and the portal (where \c AK::SoundEngine::SetEmitterObstruction() should be used), and occlusion from room boundaries, 
		/// which is better handled by the spatial audio diffraction system.  For example, games that use ray-testing for obstruction may report 100 % obstruction when an object is very close to the the opening of a portal, 
		/// because the ray between the emitter and the listener hits a nearby wall. If the game then sends 100 %, this will erroneously override the diffraction angle calculation which is probably much less than 180 degrees, 
		/// and better matches the expected audibility of the sound. To prevent this scenario, games can ray-test for obstruction objects between the emitter and the portal, passing the results with \c AK::SoundEngine::SetEmitterObstruction(), 
		/// and then test for obstructing objects between the portal and the listener, passing the results with \c AK::SoundEngine::SetPortalObstruction().
		/// \sa 
		///	- \ref AK::SpatialAudio::SetPortalObstruction
		AK_EXTERNAPIFUNC(AKRESULT, SetEmitterObstruction)(
			AkGameObjectID in_gameObjectID,			///< Game object ID of the sound emitter.
			AkReal32 in_fObstruction				///< Obstruction value.  Valid range 0.f-1.f
			);

		/// Set the obstruction value for a portal that has been registered with Spatial Audio.
		/// Portal obstruction is used to simulate objects between the portal and the listener that are obstructing the sound coming from the portal.  The obstruction value should relate to how much of the opening
		/// is obstructed, and must be calculated by the client.  Only portals connecting directly to the listeners current room need to have their obstruction value updated. If the listener moves to a new room, 
		/// the new set of portals must be updated.
		AK_EXTERNAPIFUNC(AKRESULT, SetPortalObstruction)(
			AkPortalID in_PortalID,					///< Portal ID.
			AkReal32 in_fObstruction				///< Obstruction value.  Valid range 0.f-1.f
			);

		/// Query information about the sound propagation state for a particular listener and emitter, which has been calculated using the data provided via the rooms and portals API. This function can be used for debugging purposes.
		/// This function must acquire the global sound engine lock and therefore, may block waiting for the lock.
		/// \sa
		/// - \ref AkPropagationPathInfo
		AK_EXTERNAPIFUNC(AKRESULT, QuerySoundPropagationPaths)(
			AkGameObjectID in_gameObjectID,		///< The ID of the game object that the client wishes to query.
			AkVector& out_listenerPos,			///< Returns the position of the listener game object that is associated with the game object \c in_gameObjectID.
			AkVector& out_emitterPos,			///< Returns the position of the emitter game object \c in_gameObjectID.
			AkPropagationPathInfo* out_aPaths,	///< Pointer to an array of \c AkPropagationPathInfo's which will be filled after returning.
			AkUInt32& io_uArraySize				///< The number of slots in \c out_aPaths, after returning the number of valid elements written.
		);

		//@}
	}
};
