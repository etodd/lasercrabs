//////////////////////////////////////////////////////////////////////
//
// Copyright (c) 2006 Audiokinetic Inc. / All Rights Reserved
//
//////////////////////////////////////////////////////////////////////

/// \file 
/// Software source plug-in and effect plug-in interfaces.

#ifndef _IAK_PLUGIN_H_
#define _IAK_PLUGIN_H_

#include <AK/SoundEngine/Common/AkCommonDefs.h>
#include <AK/SoundEngine/Common/IAkRTPCSubscriber.h>
#include <AK/SoundEngine/Common/IAkPluginMemAlloc.h>
#include <AK/SoundEngine/Common/AkFPUtilities.h>
#include <AK/Tools/Common/AkLock.h>
#include <AK/Tools/Common/AkPlatformFuncs.h>
#include <AK/Tools/Common/AkMonitorError.h>
#include <AK/SoundEngine/Common/AkSoundEngineExport.h>
#include <AK/SoundEngine/Common/IAkProcessorFeatures.h>
#include <AK/SoundEngine/Common/AkMidiTypes.h>

#ifndef AK_WII
// math.h may cause conflicts on the Wii due to inclusion order.
#include <math.h>
#endif

#if defined AK_CPU_X86  || defined AK_CPU_X86_64
#include <xmmintrin.h>
#endif

#ifdef AK_XBOX360
#include "ppcintrinsics.h"
#endif

#ifdef __PPU__
#include <altivec.h>
#include <ppu_intrinsics.h>
#include <stdlib.h>
#include <string.h>
#endif

#ifdef AK_PS3
#include <AK/Plugin/PluginServices/PS3/MultiCoreServices.h>
#endif

#ifdef AK_WII
#include <revolution/ax.h>
#endif

#ifdef AK_WIIU
#include <cafe/ax.h>
#endif

#ifdef AK_3DS
#include <nn/snd.h>
#endif

#ifdef AK_VITA_HW
#include <ngs.h>
#endif

/*#ifdef AK_PS4
#include <audioout.h>
#endif*/


/// Plug-in type.
/// \sa 
/// - AkPluginInfo
enum AkPluginType
{
	AkPluginTypeNone            = 0,	///< Unknown/invalid plug-in type.
	AkPluginTypeCodec           = 1,	///< Compressor/decompressor plug-in (allows support for custom audio file types).
	AkPluginTypeSource          = 2,	///< Source plug-in: creates sound by synthesis method (no input, just output).
	AkPluginTypeEffect          = 3,	///< Effect plug-in: applies processing to audio data.
	AkPluginTypeMotionDevice	= 4,	///< Motion Device plug-in: feeds movement data to devices.
	AkPluginTypeMotionSource	= 5,	///< Motion Device source plug-in: feeds movement data to device busses.
	AkPluginTypeMixer			= 6,	///< Mixer plug-in: mix voices at the bus level.
	AkPluginTypeSink			= 7,	///< Sink plug-in: implement custom sound engine end point.
	AkPluginTypeMask            = 0xf 	///< Plug-in type mask is 4 bits.
};

/// Plug-in information structure.
/// \remarks The bIsInPlace field is only relevant for effect plug-ins.
/// \remarks Currently asynchronous effects are only supported on PS3 effect plug-ins (not source plug-ins), otherwise effects should be synchronous.
/// \sa
/// - \ref iakeffect_geteffectinfo
struct AkPluginInfo
{
	AkPluginType eType;            ///< Plug-in type
	bool         bIsInPlace; 	   ///< Buffer usage (in-place or not)
	bool         bIsAsynchronous;  ///< Asynchronous plug-in flag
};

namespace AK
{
	class IAkStreamMgr;

	/// Game object information available to plugins.
	class IAkGameObjectPluginInfo
	{
	protected:
		/// Virtual destructor on interface to avoid warnings.
		virtual ~IAkGameObjectPluginInfo(){}

	public:

		/// Get the ID of the game object.
		virtual AkGameObjectID GetGameObjectID() const = 0;

		/// Retrieve the number of emitter-listener pairs (rays) of the game object. 
		/// A game object may have more than one position, and be listened to more than one listener.
		/// The returned value is the product of these two numbers. Use the returned value as a higher 
		/// bound for the index of GetEmitterListenerPair().
		/// Note that rays whose listener is irrelevant to the current context are ignored. For example,
		/// if the calling plugin exists on a bus, only listeners that are routed to the end point's 
		/// device are considered.
		/// \sa
		/// - AK::SoundEngine::SetPosition()
		/// - AK::SoundEngine::SetMultiplePositions()
		/// - AK::SoundEngine::SetListenerPosition()
		/// - AK::SoundEngine::SetActiveListeners()
		/// - AK::IAkGameObjectPluginInfo::GetEmitterListenerPair()
		/// - AK::SoundEngine::AddSecondaryOutput()
		virtual AkUInt32 GetNumEmitterListenerPairs() const = 0;

		/// Retrieve the emitter-listener pair (ray) of the game object at index in_uIndex.
		/// Call GetNumEmitterListenerPairs() prior to this function to get the total number of 
		/// emitter-listener pairs of the game object.
		/// The emitter-listener pair is expressed as the game object's position relative to the 
		/// listener, in spherical coordinates. 
		/// \note
		/// - The distance takes game object and listener scaling factors into account.
		/// - Returned distance and angles are those of the game object, and do not necessarily correspond
		/// to any sound's positioning data. 
		/// \return AK_Fail if the index is invalid, AK_Success otherwise.
		/// \sa 
		/// - AK::SoundEngine::SetAttenuationScalingFactor()
		/// - AK::SoundEngine::SetListenerScalingFactor()
		/// - AK::IAkGameObjectPluginInfo::GetNumEmitterListenerPairs()
		virtual AKRESULT GetEmitterListenerPair(
			AkUInt32 in_uIndex,				///< Index of the pair, [0, GetNumEmitterListenerPairs()[
			AkEmitterListenerPair & out_emitterListenerPair	///< Returned relative source position in spherical coordinates.
			) const = 0;

		/// Get the number of positions of the game object. Use this value to determine the indices to be 
		/// passed to GetGameObjectPosition().
		/// \sa
		/// - AK::SoundEngine::SetPosition()
		/// - AK::SoundEngine::SetMultiplePositions()
		/// - AK::IAkGameObjectPluginInfo::GetGameObjectPosition();
		virtual AkUInt32 GetNumGameObjectPositions() const = 0;
		
		/// Get the raw position of the game object at index in_uIndex. 
		/// Use GetNumGameObjectPositions() prior to this function to get the total number of positions 
		/// of that game object.
		/// \return AK_Fail if the index is out of bounds, AK_Success otherwise.
		/// \sa
		/// - AK::SoundEngine::SetPosition()
		/// - AK::SoundEngine::SetMultiplePositions()
		/// - AK::IAkGameObjectPluginInfo::GetNumGameObjectPositions()
		virtual AKRESULT GetGameObjectPosition(
			AkUInt32 in_uIndex,				///< Index of the position, [0, GetNumGameObjectPositions()[
			AkSoundPosition & out_position	///< Returned raw position info.
			) const = 0;

		/// Get the multi-position type assigned to the game object.
		/// \return MultiPositionType_MultiSources when the effect is instantiated on a bus.
		/// \sa
		/// - AK::SoundEngine::SetPosition()
		/// - AK::SoundEngine::SetMultiplePositions()
		virtual SoundEngine::MultiPositionType GetGameObjectMultiPositionType() const = 0;

		/// Get the distance scaling factor of the associated game object.
		/// \sa
		/// - AK::SoundEngine::SetAttenuationScalingFactor()
		virtual AkReal32 GetGameObjectScaling() const = 0;

		/// Get the listeners that are listening ("active") to the game object.
		/// Each set bit represents a listener.
		/// Note that only listeners relevant to the current context are considered. For example,
		/// if the calling plugin exists on a bus, only listeners that are routed to the end point's 
		/// device are added to the returned mask.
		/// \return The listener mask: 0x01 is listener 0, 0x02 is listener 1, 0x04 is listener 2, and so on.
		/// \sa
		/// - AK::SoundEngine::SetActiveListeners()
		virtual AkUInt32 GetListenerMask() const = 0;

		/// Get information about a listener. Use GetListenerMask() prior to this function
		/// in order to know which listeners are listening to the associated game object. You should
		/// only get information about one listener at a time, so there should be only one bit set
		/// in in_uListenerMask.
		/// \return AK_Fail if the listener mask is invalid. AK_Success otherwise.
		/// \sa
		/// - AK::SoundEngine::SetActiveListeners()
		/// - AK::IAkGameObjectPluginInfo::GetListenerMask()
		virtual AKRESULT GetListenerData(
			AkUInt32 in_uListenerMask,		///< Bit field identifying the listener for which you desire information.
			AkListener & out_listener		///< Returned listener info.
			) const = 0;
	};

	/// Voice-specific information available to plugins. The associated game object's information is 
	/// available through the methods of the base class IAkGameObjectPluginInfo.
	class IAkVoicePluginInfo : public IAkGameObjectPluginInfo
	{
	protected:
		/// Virtual destructor on interface to avoid warnings.
		virtual ~IAkVoicePluginInfo(){}

	public:

		/// Retrieve the Playing ID of the event corresponding to this voice (if applicable).
		virtual AkPlayingID GetPlayingID() const = 0;

		/// Get priority value associated to this voice. When priority voice is modified by distance, the minimum distance among emitter-listener pairs is used.
		/// \return The priority between AK_MIN_PRIORITY and AK_MAX_PRIORITY inclusively.
		virtual AkPriority GetPriority() const = 0;

		/// Get priority value associated to this voice, for a specified distance, which may differ from the minimum distance that is used by default.
		/// \return The priority between AK_MIN_PRIORITY and AK_MAX_PRIORITY inclusively.
		virtual AkPriority ComputePriorityWithDistance(
			AkReal32 in_fDistance				///< Distance.
			) const = 0;
	};

	/// Interface to retrieve contextual information available to all types of plugins.
	class IAkPluginContextBase
	{
	protected:
		/// Virtual destructor on interface to avoid warnings.
		virtual ~IAkPluginContextBase(){}

	public:

		/// Retrieve the streaming manager access interface.
		virtual IAkStreamMgr * GetStreamMgr() const = 0;

		/// Retrieve the maximum number of frames that Execute() will be called with for this effect. 
		/// Can be used by the effect to make memory allocation at initialization based on this worst case scenario.
		/// \return Maximum number of frames.
		virtual AkUInt16 GetMaxBufferLength() const = 0;

		/// Identify the mixing graph where the plug-in is instantiated by getting the end-point's
		/// output ID and device type that were given to AddSecondaryOutput(). Primary output graph
		/// returns out_uOutputID = 0 and out_uDeviceType = (AkUInt32)AkOutput_Main.
		/// Applicable to plug-ins instantiated in the Master-Mixer hierarchy only (bus insert, mixer, sink). 
		/// Plug-ins instantiated in the Actor-Mixer hierarchy (i.e. on voices) return AK_NotCompatible.
		/// \sa integrating_secondary_outputs
		/// \return The device type and unique identifier. AK_Success if successful, AK_NotCompatible otherwise.
		virtual AKRESULT GetOutputID(
			AkUInt32 &			out_uOutputID,		///< Device identifier, when multiple devices of the same type are possible.
			AkUInt32 &			out_uDeviceType		///< Device type, must be one of the currently supported devices types.  Can be casted to platforms-specific AkAudioOutputType.
			) const = 0;

		/// Return the pointer and size of the plug-in media corresponding to the specified index.
		/// The pointer returned will be NULL if the plug-in media is either not loaded or inexistant.
		/// When this function is called and returns a valid data pointer, the data can only be used by this 
		/// instance of the plugin and is guaranteed to be valid only during the plug-in lifespan.
		virtual void GetPluginMedia( 
			AkUInt32 in_dataIndex,		///< Index of the plug-in media to be returned.
			AkUInt8* &out_rpData,		///< Pointer to the data
			AkUInt32 &out_rDataSize		///< size of the data returned in bytes.
			) = 0;

		/// Post a monitoring message or error string. This will be displayed in the Wwise capture
		/// log.
		/// \return AK_Success if successful, AK_Fail if there was a problem posting the message.
		///			In optimized mode, this function returns AK_NotCompatible.
		/// \remark This function is provided as a tracking tool only. It does nothing if it is 
		///			called in the optimized/release configuration and return AK_NotCompatible.
		virtual AKRESULT PostMonitorMessage(
			const char* in_pszError,				///< Message or error string to be displayed
			AK::Monitor::ErrorLevel in_eErrorLevel	///< Specifies whether it should be displayed as a message or an error
			) = 0;
		
		/// Post a custom blob of data to the UI counterpart of this effect plug-in.
		/// Data is sent asynchronously through the profiling system.
		/// Notes:
		/// - It is only possible to post data when the instance of the plug-in is on a bus, 
		///		because there is a one-to-one relationship with its effect settings view.
		///		You may call CanPostMonitorData() to determine if your plug-in can send data to the UI.
		/// - Data is copied into the communication buffer within this method,
		///		so you may discard it afterwards.
		/// - You need to handle byte swapping on one side or the other when sending
		/// 	data from a big-endian platform.
		/// - Sending data to the UI is only possible in Debug and Profile. Thus, you should 
		///		enclose your calls to package and send that data within !AK_OPTIMIZED preprocessor flag.
		/// \return AK_Success if the plug-in exists on a bus, AK_Fail otherwise.
		virtual AKRESULT PostMonitorData(
			void *		in_pData,		///< Blob of data.
			AkUInt32	in_uDataSize	///< Size of data.
			) = 0;

		/// Query the context to know if it is possible to send data to the UI counterpart of this effect plug-in.
		/// It is only possible to post data when the instance of the plug-in is on a bus, because there is a 
		/// one-to-one relationship with its effect settings view. 
		/// \return True if the instance of the plug-in is on a bus, and the authoring tool is connected and 
		///		monitoring the game, false otherwise.
		/// \sa PostMonitorData()
		virtual bool	 CanPostMonitorData() = 0;

#if (defined AK_CPU_X86 || defined AK_CPU_X86_64) && !(defined AK_IOS)
		/// Return an interface to query processor specific features.
		virtual IAkProcessorFeatures * GetProcessorFeatures() = 0;
#endif
	};

	/// Interface to retrieve contextual information for an effect plug-in.
	/// \sa
	/// - \ref iakmonadiceffect_init
	class IAkEffectPluginContext : public IAkPluginContextBase
	{
	protected:
		/// Virtual destructor on interface to avoid warnings.
		virtual ~IAkEffectPluginContext(){}

	public:

		/// Determine whether the effect is to be used in Send Mode or not.
		/// Effects used in auxiliary busses are always used in Send Mode.
		/// \return True if the effect is in Send Mode, False otherwise
		virtual bool IsSendModeEffect() const = 0;

		/// Obtain the interface to access the voice in which the plugin is inserted.
		/// \return The interface to voice info. NULL if the plugin is instantiated on a bus.
		virtual IAkVoicePluginInfo * GetVoiceInfo() = 0;
	};

	/// Interface to retrieve contextual information for a source plug-in.
	/// \sa
	/// - \ref iaksourceeffect_init
	class IAkSourcePluginContext : public IAkPluginContextBase
	{
	protected:
		/// Virtual destructor on interface to avoid warnings.
		virtual ~IAkSourcePluginContext(){}

	public:

		/// Retrieve the number of loops the source should produce.
		/// \return The number of loop iterations the source should produce (0 if infinite looping)
		virtual AkUInt16 GetNumLoops() const = 0;

		/// Obtain the interface to access the voice in which the plugin is inserted.
		/// \return The interface to voice info.
		virtual IAkVoicePluginInfo * GetVoiceInfo() = 0;

		/// Obtain the MIDI event info associated to the source.
		/// \return The MIDI event info.
		/// 
		virtual AkMidiEvent GetMidiEvent() const = 0;
	};

	/// Interface to retrieve contextual information for a mixer.
	class IAkMixerPluginContext : public IAkPluginContextBase
	{
	protected:
		/// Virtual destructor on interface to avoid warnings.
		virtual ~IAkMixerPluginContext(){}

	public:

		/// Get ID of bus on which the plugin is inserted. It can be matched with the bus name converted to a unique ID using AK::SoundEngine::GetIDFromString().
		/// \return ID of bus.
		/// \sa AK::SoundEngine::GetIDFromString()
		virtual AkUniqueID GetBusID() = 0;

		/// Get the type of the bus on which the mixer plugin is instantiated. 
		/// AkBusType is a bit field, indicating whether the bus is the master (top-level) bus or not,
		/// and whether it is in the primary or secondary mixing graph.
		/// \return The bus type.
		virtual AkBusType GetBusType() = 0;

		/// Get the cumulative gain of all mixing stages, from this bus down to the device end point.
		/// \return The cumulative downstream gain.
		virtual AkReal32 GetDownstreamGain() = 0;

		/// Get speaker angles of the specified device.
		/// The speaker angles are expressed as an array of loudspeaker pairs, in degrees, relative to azimuth ]0,180].
		/// Supported loudspeaker setups are always symmetric; the center speaker is always in the middle and thus not specified by angles.
		/// Angles must be set in ascending order. 
		/// You may call this function with io_pfSpeakerAngles set to NULL to get the expected number of angle values in io_uNumAngles, 
		/// in order to allocate your array correctly. You may also obtain this number by calling
		/// AK::GetNumberOfAnglesForConfig( AK_SPEAKER_SETUP_DEFAULT_PLANE ).
		/// If io_pfSpeakerAngles is not NULL, the array is filled with up to io_uNumAngles.
		/// Typical usage:
		/// - AkUInt32 uNumAngles;
		/// - GetSpeakerAngles( NULL, uNumAngles );
		/// - AkReal32 * pfSpeakerAngles = AkAlloca( uNumAngles * sizeof(AkReal32) );
		/// - GetSpeakerAngles( pfSpeakerAngles, uNumAngles );
		/// \warning Call this function only after the sound engine has been properly initialized.
		/// \return AK_Success if the end point device is properly initialized, AK_Fail otherwise.
		/// \sa AK::SoundEngine::GetSpeakerAngles()
		virtual AKRESULT GetSpeakerAngles(
			AkReal32 *			io_pfSpeakerAngles,			///< Returned array of loudspeaker pair angles, in degrees relative to azimuth [0,180]. Pass NULL to get the required size of the array.
			AkUInt32 &			io_uNumAngles,				///< Returned number of angles in io_pfSpeakerAngles, which is the minimum between the value that you pass in, and the number of angles corresponding to the output configuration, or just the latter if io_pfSpeakerAngles is NULL.
			AkReal32 &			out_fHeightAngle			///< Elevation of the height layer, in degrees relative to the plane.
			) = 0;

		/// \name Services.
		//@{

		/// Compute a direct speaker assignment volume matrix with proper downmixing rules between two channel configurations.
		virtual void ComputeSpeakerVolumesDirect(
			AkChannelConfig		in_inputConfig,				///< Channel configuration of the input.
			AkChannelConfig		in_outputConfig,			///< Channel configuration of the mixer output.
			AkReal32			in_fCenterPerc,				///< Center percentage. Only applies to mono inputs to outputs that have no center.
			AK::SpeakerVolumes::MatrixPtr out_mxVolumes		///< Returned volumes matrix. Must be preallocated using AK::SpeakerVolumes::Matrix::GetRequiredSize() (see AK::SpeakerVolumes::Matrix services).
			) = 0;

		/// Compute a volume matrix given the position of the panner (Wwise 2D panner).
		virtual void ComputeSpeakerVolumesPanner(
			const AkVector &	in_position,				///< x,y,z panner position [-1,1]. Note that z has no effect at the moment.
			AkReal32			in_fCenterPct,				///< Center percentage.
			AkChannelConfig		in_inputConfig,				///< Channel configuration of the input.
			AkChannelConfig		in_outputConfig,			///< Channel configuration of the mixer output.
			AK::SpeakerVolumes::MatrixPtr out_mxVolumes		///< Returned volumes matrix. Must be preallocated using AK::SpeakerVolumes::Matrix::GetRequiredSize() (see AK::SpeakerVolumes::Matrix services).
			) = 0;

		/// Compute panning gains on the plane given an incidence angle and channel configuration.
		virtual AKRESULT ComputePlanarVBAPGains( 
			AkReal32			in_fAngle,					///< Incident angle, in radians [-pi,pi], where 0 is the azimuth (positive values are clockwise)
			AkChannelConfig		in_outputConfig,			///< Desired output configuration. 
			AkReal32			in_fCenterPerc,				///< Center percentage. Only applies to mono inputs to outputs that have no center.
			AK::SpeakerVolumes::VectorPtr out_vVolumes		///< Returned volumes (see AK::SpeakerVolumes::Vector services). Must be allocated prior to calling this function with the size returned by AK::SpeakerVolumes::Vector::GetRequiredSize() for the desired configuration.
			) = 0;

		/// Initialize spherical VBAP 
		virtual AKRESULT InitSphericalVBAP( 
			AK::IAkPluginMemAlloc* in_pAllocator,			///< Memory allocator
			const AkSphericalCoord* in_SphericalPositions, 	///< Array of points in spherical coordinate, representign the virtual position of each channels.
			const AkUInt32 in_NbPoints,						///< Number of points in the position array
			void *& out_pPannerData							///< Contains data relevant to the 3D panner that shoud be re-used accross plugin instances.
			) = 0;

		/// Compute panning gains on the plane given an incidence angle and channel configuration.
		virtual AKRESULT ComputeSphericalVBAPGains( 
			void*				in_pPannerData,				///< Contains data relevant to the 3D panner that shoud be re-used accross plugin instances.
			AkReal32			in_fAzimuth,				///< Incident angle, in radians [-pi,pi], where 0 is the azimuth (positive values are clockwise)
			AkReal32			in_fElevation,				///< Incident angle, in radians [0,pi], where 0 is the elevation (positive values are clockwise)
			AkUInt32			in_uNumChannels,			///< Number of output channels. 
			AK::SpeakerVolumes::VectorPtr out_vVolumes		///< Returned volumes (see AK::SpeakerVolumes::Vector services). Must be allocated prior to calling this function with the size returned by AK::SpeakerVolumes::Vector::GetRequiredSize() for the desired configuration.
			) = 0;

		/// Clear 
		virtual AKRESULT TermSphericalVBAP( 
			AK::IAkPluginMemAlloc*	in_pAllocator,			///< Memory allocator
			void*					in_pPannerData			///< Contains data relevant to the 3D panner that shoud be re-used accross plugin instances.
			) = 0;
	
		/// Compute standard 3D positioning.
		virtual AKRESULT Compute3DPositioning( 
			AkReal32			in_fAngle,					///< Incident angle, in radians [-pi,pi], where 0 is the azimuth (positive values are clockwise)
			AkReal32			in_fSpread,					///< Spread ([0,100]).
			AkReal32			in_fFocus,					///< Focus ([0,100]).
			AkChannelConfig		in_inputConfig,				///< Channel configuration of the input.
			AkChannelConfig		in_outputConfig,			///< Desired output configuration. 
			AkReal32			in_fCenterPerc,				///< Center percentage. Only applies to mono inputs to outputs that have no center.
			AK::SpeakerVolumes::MatrixPtr out_mxVolumes		///< Returned volumes matrix. Must be preallocated using AK::SpeakerVolumes::Matrix::GetRequiredSize() (see AK::SpeakerVolumes::Matrix services).
			) = 0;

		/// N to N channels mix
		virtual void MixNinNChannels(
			AkAudioBuffer *	in_pInputBuffer,				///< Input multichannel buffer.
			AkAudioBuffer *	in_pMixBuffer,					///< Multichannel buffer with which the input buffer is mixed.
			AkReal32		in_fPrevGain,					///< Gain, corresponding to the beginning of the buffer, to apply uniformly to each mixed channel.
			AkReal32		in_fNextGain,					///< Gain, corresponding to the end of the buffer, to apply uniformly to each mixed channel.
			AK::SpeakerVolumes::MatrixPtr in_mxPrevVolumes,	///< In/out channel volume distribution corresponding to the beginning of the buffer (see AK::SpeakerVolumes::Matrix services).
			AK::SpeakerVolumes::MatrixPtr in_mxNextVolumes	///< In/out channel volume distribution corresponding to the end of the buffer (see AK::SpeakerVolumes::Matrix services).
			) = 0;
			
		/// 1 to N channels mix
		virtual void Mix1inNChannels(
			AkReal32 * AK_RESTRICT in_pInChannel,			///< Input channel buffer.
			AkAudioBuffer *	in_pMixBuffer,					///< Multichannel buffer with which the input buffer is mixed.
			AkReal32		in_fPrevGain,					///< Gain, corresponding to the beginning of the input channel.
			AkReal32		in_fNextGain,					///< Gain, corresponding to the end of the input channel.
			AK::SpeakerVolumes::VectorPtr in_vPrevVolumes,	///< Output channel volume distribution corresponding to the beginning of the buffer (see AK::SpeakerVolumes::Vector services).
			AK::SpeakerVolumes::VectorPtr in_vNextVolumes	///< Output channel volume distribution corresponding to the end of the buffer (see AK::SpeakerVolumes::Vector services).
			) = 0;

		/// Single channel mix
		virtual void MixChannel(
			AkReal32 * AK_RESTRICT in_pInBuffer,			///< Input channel buffer.
			AkReal32 * AK_RESTRICT in_pOutBuffer,			///< Output channel buffer.
			AkReal32		in_fPrevGain,					///< Gain, corresponding to the beginning of the input channel.
			AkReal32		in_fNextGain,					///< Gain, corresponding to the end of the input channel.
			AkUInt16		in_uNumFrames					///< Number of frames to mix.
			) = 0;

		//@}

		/// \name Metering.
		//@{
		
		/// Set flags for controlling computation of metering values on the mix buffer. 
		/// Pass AK_NoMetering to disable metering.
		/// \sa 
		/// - AK::IAkMetering
		virtual void EnableMetering( AkMeteringFlags in_eFlags ) = 0;
	};

	/// Parameter node interface, managing access to an enclosed parameter structure.
	/// \aknote The implementer of this interface should also expose a static creation function
	/// that will return a new parameter node instance when required (see \ref se_plugins_overview). \endaknote
	/// \sa
	/// - \ref shared_parameter_interface
	class IAkPluginParam : public IAkRTPCSubscriber
	{
	protected:
		/// Virtual destructor on interface to avoid warnings.
		virtual ~IAkPluginParam(){}

	public:
		/// Create a duplicate of the parameter node instance in its current state.
		/// \aknote The allocation of the new parameter node should be done through the AK_PLUGIN_NEW() macro. \endaknote
		/// \return Pointer to a duplicated plug-in parameter node interface
		/// \sa
		/// - \ref iakeffectparam_clone
		virtual IAkPluginParam * Clone( 
			IAkPluginMemAlloc * in_pAllocator 	///< Interface to memory allocator to be used
			) = 0;

		/// Initialize the plug-in parameter node interface.
		/// Initializes the internal parameter structure to default values or with the provided parameter 
		/// block if it is valid. \endaknote
		/// \aknote If the provided parameter block is valid, use SetParamsBlock() to set all parameters at once. \endaknote
		/// \return Possible return values are: AK_Success, AK_Fail, AK_InvalidParameter
		/// \sa
		/// - \ref iakeffectparam_init
		virtual AKRESULT Init( 
			IAkPluginMemAlloc *	in_pAllocator,		///< Interface to the memory allocator to be used					   
			const void *		in_pParamsBlock,	///< Pointer to a parameter structure block
			AkUInt32        	in_uBlockSize		///< Size of the parameter structure block
			) = 0;

		/// Called by the sound engine when a parameter node is terminated.
		/// \aknote The self-destruction of the parameter node must be done using the AK_PLUGIN_DELETE() macro. \endaknote
		/// \return AK_Success if successful, AK_Fail otherwise
		/// \sa
		/// - \ref iakeffectparam_term
		virtual AKRESULT Term( 
			IAkPluginMemAlloc * in_pAllocator		///< Interface to memory allocator to be used
			) = 0;	

		/// Set all plug-in parameters at once using a parameter block.
		/// \return AK_Success if successful, AK_InvalidParameter otherwise
		/// \sa
		/// - \ref iakeffectparam_setparamsblock
		virtual AKRESULT SetParamsBlock( 
			const void *in_pParamsBlock, 	///< Pointer to a parameter structure block
			AkUInt32	in_uBlockSize		///< Size of the parameter structure block
			) = 0;

		/// Update a single parameter at a time and perform the necessary actions on the parameter changes.
		/// \aknote The parameter ID corresponds to the AudioEnginePropertyID in the plug-in XML description file. \endaknote
		/// \return AK_Success if successful, AK_InvalidParameter otherwise
		/// \sa
		/// - \ref iakeffectparam_setparam
		virtual AKRESULT SetParam( 
			AkPluginParamID	in_paramID,		///< ID number of the parameter to set
			const void *	in_pValue, 		///< Pointer to the value of the parameter to set
			AkUInt32    	in_uParamSize	///< Size of the value of the parameter to set
			) = 0;

		/// Use this constant with AK::Wwise::IPluginPropertySet::NotifyInternalDataChanged, 
		/// AK::Wwise::IAudioPlugin::GetPluginData and IAkPluginParam::SetParam. This tells
		/// that the whole plugin data needs to be saved/transferred.
		///\sa
		/// - AK::Wwise::IPluginPropertySet::NotifyInternalDataChanged
		/// - AK::Wwise::IAudioPlugin::GetPluginData
		/// - AK::IAkPluginParam::SetParam
		static const AkPluginParamID ALL_PLUGIN_DATA_ID = 0x7FFF;
	};

	/// Wwise sound engine plug-in interface. Shared functionality across different plug-in types.
	/// \aknote The implementer of this interface should also expose a static creation function
	/// that will return a new plug-in instance when required (see \ref soundengine_plugins). \endaknote
	class IAkPlugin
	{
	protected:
		/// Virtual destructor on interface to avoid warnings.
		virtual ~IAkPlugin(){}

	public:
		/// Release the resources upon termination of the plug-in.
		/// \return AK_Success if successful, AK_Fail otherwise
		/// \aknote The self-destruction of the plug-in must be done using AK_PLUGIN_DELETE() macro. \endaknote
		/// \sa
		/// - \ref iakeffect_term
		virtual AKRESULT Term( 
			IAkPluginMemAlloc * in_pAllocator 	///< Interface to memory allocator to be used by the plug-in
			) = 0;

		/// The reset action should perform any actions required to reinitialize the state of the plug-in 
		/// to its original state (e.g. after Init() or on effect bypass).
		/// \return AK_Success if successful, AK_Fail otherwise.
		/// \sa
		/// - \ref iakeffect_reset
		virtual AKRESULT Reset() = 0;

		/// Plug-in information query mechanism used when the sound engine requires information 
		/// about the plug-in to determine its behavior
		/// \return AK_Success if successful.
		/// \sa
		/// - \ref iakeffect_geteffectinfo
		virtual AKRESULT GetPluginInfo( 
			AkPluginInfo & out_rPluginInfo	///< Reference to the plug-in information structure to be retrieved
			) = 0;

		/// Some plug-ins are accessing Media from the Wwise sound bank system.
		/// If the IAkPlugin object is not using media, this function will not be used and should simply return false.
		/// If the IAkPlugin object is using media, the RelocateMedia feature can be optionally implemented.
		/// To implement correctly the feature, the plugin must be able to "Hot swap" from a memory location to another one in a single blocking call (AK::IAkPlugin::RelocateMedia)
		///
		/// \sa
		/// - AK::IAkPlugin::RelocateMedia
		virtual bool SupportMediaRelocation() const
		{
			return false;
		}

		/// Some plug-ins are accessing Media from the Wwise sound bank system.
		/// If the IAkPlugin object is not using media, this function will not be used.
		/// If the IAkPlugin object is using media, the RelocateMedia feature can be optionally implemented.
		/// When this function is being called, the IAkPlugin object must make the required changes to remove all 
		/// referenced from the old memory pointer (previously obtained by GetPluginMedia() (and offsets to) to not access anymore the content of the old memory data and start using the newly provided pointer instead.
		/// The change must be done within the function RelocateMedia(). 
		/// After this call, the memory space in in_pOldInMemoryData will be invalidated and cannot be used safely anymore.
		///
		/// This function will not be called if SupportMediaRelocation returned false.
		///
		/// \sa
		/// - AK::IAkPlugin::SupportMediaRelocation
		virtual AKRESULT RelocateMedia( 
			AkUInt8* /*in_pNewMedia*/,  
			AkUInt8* /*in_pOldMedia*/ 
			)
		{
			return AK_NotImplemented;
		}

	};

	/// Software effect plug-in interface (see \ref soundengine_plugins_effects).
	class IAkEffectPlugin : public IAkPlugin
	{
	protected:
		/// Virtual destructor on interface to avoid warnings.
		virtual ~IAkEffectPlugin(){}

	public:
		/// Software effect plug-in initialization. Prepares the effect for data processing, allocates memory and sets up the initial conditions. 
		/// \aknote Memory allocation should be done through appropriate macros (see \ref fx_memory_alloc). \endaknote
		/// \sa
		/// - \ref iakmonadiceffect_init
		virtual AKRESULT Init( 
			IAkPluginMemAlloc *			in_pAllocator,				///< Interface to memory allocator to be used by the effect
			IAkEffectPluginContext *	in_pEffectPluginContext,	///< Interface to effect plug-in's context		    
			IAkPluginParam *			in_pParams,					///< Interface to plug-in parameters
			AkAudioFormat &				io_rFormat					///< Audio data format of the input/output signal. Only an out-of-place plugin is allowed to change the channel configuration.
			) = 0;

#if defined AK_WII_FAMILY_HW
		/// Wii effects must provide the callback function that will be called on effect execution.
		virtual AXAuxCallback GetFXCallback() = 0;

		/// Wii effects must provide a pointer to the params that will be used by the effect callback.
		virtual void* GetFXParams() = 0;

		/// Wii effects must provide an estimated time after what the effect tail will be considered finished.
		/// This information will be used by the sound engine to stop processing the environmentals effects
		/// that are not in use so that the Aux_A, Aux_B and Aux_C can be attributed to other effects.
		/// This function will be called only after the FX initialization was completed.
		/// \return The estimated time in milliseconds.
		virtual AkUInt32 GetTailTime() = 0;
#endif
#ifdef AK_3DS
		/// 3DS effects can handle live parameter changes through this method, called at every audio frame.
		virtual void Update() = 0;

		/// 3DS effects must provide the callback function that will be called on effect execution.
		virtual nn::snd::CTR::AuxCallback GetFXCallback() = 0;

		/// 3DS effects must provide an estimated time after what the effect tail will be considered finished.
		/// This information will be used by the sound engine to stop processing the environmentals effects
		/// that are not in use so that the Aux_A and Aux_B can be attributed to other effects.
		/// This function will be called only after the FX initialization was completed.
		/// \return The estimated time in milliseconds.
		virtual AkUInt32 GetTailTime() = 0;
#endif
#ifdef AK_VITA_HW
		virtual const SceNgsVoiceDefinition * GetVoiceDefinition(){ AKASSERT( false && "Non hardware plugin called on Vita HW" ); return NULL; }
		virtual AKRESULT AttachVoice( SceNgsHVoice in_hVoice){ AKASSERT( false && "Non hardware plugin called on Vita HW" ); return AK_Fail; }
		virtual AkReal32 GetTailTime() const { AKASSERT( false && "Non hardware plugin called on Vita HW" ); return 0; }
		virtual AKRESULT SetBypass( SceUInt32 in_uBypassFlag ) { AKASSERT( false && "Non hardware plugin called on Vita HW" ); return AK_Fail; }
#endif
	};

	/// Software effect plug-in interface for in-place processing (see \ref soundengine_plugins_effects).
	class IAkInPlaceEffectPlugin : public IAkEffectPlugin
	{
	public:
		/// Software effect plug-in DSP execution for in-place processing.
		/// \aknote The effect should process all the input data (uValidFrames) as long as AK_DataReady is passed in the eState field. 
		/// When the input is finished (AK_NoMoreData), the effect can output more sample than uValidFrames up to MaxFrames() if desired. 
		/// All sample frames beyond uValidFrames are not initialized and it is the responsibility of the effect to do so when outputting an effect tail.
		/// The effect must notify the pipeline by updating uValidFrames if more frames are produced during the effect tail.
		/// \aknote The effect will stop being called by the pipeline when AK_NoMoreData is returned in the the eState field of the AkAudioBuffer structure.
		/// See \ref iakmonadiceffect_execute_general.
		virtual void Execute( 
				AkAudioBuffer *							io_pBuffer		///< In/Out audio buffer data structure (in-place processing)
#ifdef AK_PS3
				, AK::MultiCoreServices::DspProcess*&	out_pDspProcess	///< Asynchronous DSP process utilities on PS3
#endif
				) = 0;		

		/// Skips execution of some frames, when the voice is virtual playing from elapsed time.  
		/// This can be used to simulate processing that would have taken place (e.g. update internal state).
		/// Return AK_DataReady or AK_NoMoreData, depending if there would be audio output or not at that point.
		virtual AKRESULT TimeSkip( 
			AkUInt32 in_uFrames	///< Number of frames the audio processing should advance.
			) = 0;
	};
		

	/// Software effect plug-in interface for out-of-place processing (see \ref soundengine_plugins_effects).
	class IAkOutOfPlaceEffectPlugin : public IAkEffectPlugin
	{
	public:
		/// Software effect plug-in for out-of-place processing.
		/// \aknote An input buffer is provided and will be passed back to Execute() (with an advancing offset based on uValidFrames consumption by the plug-in).
		/// The output buffer should be filled entirely by the effect (at which point it can report AK_DataReady) except on last execution where AK_NoMoreData should be used.
		/// AK_DataNeeded should be used when more input data is necessary to continue processing. 
		/// \aknote Only the output buffer eState field is looked at by the pipeline to determine the effect state.
		/// See \ref iakmonadiceffect_execute_outofplace.
		virtual void Execute( 
				AkAudioBuffer *							in_pBuffer,		///< Input audio buffer data structure
				AkUInt32								in_uInOffset,	///< Offset position into input buffer data
				AkAudioBuffer *							out_pBuffer		///< Output audio buffer data structure
#ifdef AK_PS3
				, AK::MultiCoreServices::DspProcess*&	out_pDspProcess	///< Asynchronous DSP process utilities on PS3
#endif
				) = 0;

		/// Skips execution of some frames, when the voice is virtual playing from elapsed time.  
		/// This can be used to simulate processing that would have taken place (e.g. update internal state).
		/// Return AK_DataReady or AK_NoMoreData, depending if there would be audio output or not at that point.
		virtual AKRESULT TimeSkip(
			AkUInt32 &io_uFrames	///< Number of frames the audio processing should advance.  The output value should be the number of frames that would be consumed to output the number of frames this parameter has at the input of the function.
			) = 0;
	};

	/// Interface to retrieve information about an input of a mixer.
	class IAkMixerInputContext
	{
	protected:
		/// Virtual destructor on interface to avoid warnings.
		virtual ~IAkMixerInputContext(){}

	public:

		/// Obtain the parameter blob for the mixer plugin that were attached to this input.
		/// \return The parameter blob, which can be safely cast into the plugin's implementation. 
		/// If all parameters are default value, NULL is returned. It is up to the plugin's implementation to know
		/// what the default values are.
		virtual IAkPluginParam * GetInputParam() = 0;

		/// Obtain the interface to access the voice info of this input.
		/// \return The interface to voice info. NULL when the input is not a voice but the output of another bus instead.
		virtual IAkVoicePluginInfo * GetVoiceInfo() = 0;

		/// Query the nature of the connection between this input and the mixer.
		/// \return The connection type (direct/dry, user-defined auxiliary send, game-defined auxiliary send). Bus inputs are always "direct".
		virtual AkConnectionType GetConnectionType() = 0;

		/// Get internal ID of sound associated to this input.
		/// In the case of a voice, the ID is internal but corresponds to what you would get from the duration 
		/// callback (see AkDurationCallbackInfo::audioNodeID). In the case of a bus, it can be matched with the bus name converted 
		/// to a unique ID using AK::SoundEngine::GetIDFromString().
		/// \return ID of input.
		/// \sa 
		///	- AkDurationCallbackInfo
		/// - AK::SoundEngine::PostEvent()
		/// - AK::SoundEngine::GetIDFromString()
		virtual AkUniqueID GetAudioNodeID() = 0;

		/// Use this method to retrieve user data to this context. It is always initialized to NULL until you decide to set it otherwise.
		/// \return Attached user data.
		/// \sa SetUserData()
		virtual void * GetUserData() = 0;

		/// Use this method to attach user data to this context. It is always initialized to NULL until you decide to set it otherwise.
		/// \return Attached user data.
		/// \sa GetUserData()
		virtual void SetUserData( void * in_pUserData ) = 0;

		/// \name Default positioning information.
		/// \warning The methods of this group are deprecated.
		//@{

		/// Query whether the object corresponding to this input is spatialized or is instead assigned 
		/// directly to output channels.
		/// \return True if positioning is enabled, false in case of direct speaker assignment.
		virtual bool IsPositioningEnabled() = 0;

		/// Retrieve center percentage of this input.
		/// Note that the returned value is always 1 unless positioning is enabled.
		/// \return Center percentage, between 0 and 1.
		/// \sa 
		/// - IsPositioningEnabled()
		virtual AkReal32 GetCenterPerc() = 0;
		
		/// Retrieve the panner type of this input ("2D" versus "3D").
		/// Note that the returned value is only relevant if positioning is enabled.
		/// \sa 
		/// - IsPositioningEnabled()
		virtual AkPannerType GetPannerType() = 0;

		/// Get whether positioning is driven by the game (when positioning type is 3D game defined) or by the
		/// user through the use of an automatable panner (such as 2D or 3D user defined).
		/// \sa
		/// - GetPannerType()
		virtual AkPositionSourceType GetPositionSourceType() = 0;

		/// Default 2D positioning: 
		/// Retrieve the 2D panner position (each vector component is between -1 and 1) of this input.
		/// Note that the returned value is only relevant if positioning is enabled, and panner type is "2D".
		/// \sa 
		/// - IsPositioningEnabled()
		/// - GetPannerType()
		virtual void GetPannerPosition(
			AkVector & out_position			///< Returned sound position.
			) = 0;

		/// Default 3D positioning:
		/// Retrieve the number of emitter-listener pairs (rays) of this input. 
		/// Note that the returned value is always 0 unless the input is in 3D panner mode.
		virtual AkUInt32 GetNum3DPositions() = 0;

		/// Default 3D positioning:
		/// Retrieve the spherical coordinates of the desired emitter-listener pair (ray) corresponding to this 
		/// input, as automated by the engine. When in 3D game-defined positioning mode, the returned position 
		/// corresponds to the game object position (also obtainable via the AK::IAkGameObjectPluginInfo interface).
		/// In 3D user-defined positioning mode, returned rays are those that result from engine automation.
		/// \return AK_Success if the pair index is valid, AK_Fail otherwise.
		/// \sa
		/// - GetNum3DPositions()
		virtual AKRESULT Get3DPosition(
			AkUInt32 in_uIndex,							///< Index of the pair, [0, GetNum3DPositions()[
			AkEmitterListenerPair & out_soundPosition	///< Returned sound position, in spherical coordinates.
			) = 0;

		/// Default 3D positioning:
		/// Evaluate spread value at the distance of the desired emitter-listener pair for this input.
		/// \return The spread value, between 0 and 100. 0 if the pair index is invalid.
		/// \sa 
		/// - GetNum3DPositions()
		/// - Get3DPosition()
		virtual AkReal32 GetSpread(
			AkUInt32 in_uIndex				///< Index of the pair, [0, GetNum3DPositions()[
			) = 0;

		/// Default 3D positioning:
		/// Evaluate focus value at the distance of the desired emitter-listener pair for this input.
		/// \return The focus value, between 0 and 100. 0 if the pair index is invalid.
		/// \sa 
		/// - GetNum3DPositions()
		/// - Get3DPosition()
		virtual AkReal32 GetFocus(
			AkUInt32 in_uIndex				///< Index of the pair, [0, GetNum3DPositions()[
			) = 0;

		/// Get the max distance as defined in the attenuation editor. 
		/// \return True if this input has attenuation, false otherwise.
		virtual bool GetMaxAttenuationDistance(
			AkReal32 & out_fMaxAttenuationDistance	///< Returned max distance.
			) = 0;
		
		/// Get next volumes as computed by the sound engine for this input.
		virtual void GetSpatializedVolumes(
			AK::SpeakerVolumes::MatrixPtr out_mxPrevVolumes,	///< Returned in/out channel volume distribution corresponding to the beginning of the buffer. Must be preallocated (see AK::SpeakerVolumes::Matrix services).
			AK::SpeakerVolumes::MatrixPtr out_mxNextVolumes		///< Returned in/out channel volume distribution corresponding to the end of the buffer. Must be preallocated (see AK::SpeakerVolumes::Matrix services).
			) = 0;
		
		//@}
	};

	/// Interface to retrieve contextual information for a sink plugin.
	/// \sa
	/// - AK::IAkSinkPlugin
	class IAkSinkPluginContext : public IAkPluginContextBase
	{
	protected:
		/// Virtual destructor on interface to avoid warnings.
		virtual ~IAkSinkPluginContext(){}

	public:
		
		/// Query if the sink plugin is instantiated on the main output device (primary tree).
		/// \return True if the sink plugin is instantiated on the main output device (primary tree), false otherwise.
		/// \sa
		/// - AK::IAkSinkPlugin::IsDataNeeded()
		/// - AK::IAkSinkPlugin::Consume()
		virtual bool IsPrimary() = 0;
		
		/// Sink plugins may need to call this function to notify the audio thread that it should wake up
		/// in order to potentially process an audio frame. Note that the audio thread may wake up for other 
		/// reasons, for example following calls to AK::SoundEngine::RenderAudio().
		/// Once the audio thread is awaken, it will ask the sink plugin how many audio frames need to be 
		/// processed and presented to the plugin. This is done through AK::IAkSinkPlugin::IsDataNeeded()
		/// and AK::IAkSinkPlugin::Consume() respectively.
		/// Note that only the sink plugin that is instantiated on the main output device (primary tree) may control 
		/// the audio thread synchronization. 
		/// \return AK_Success if the calling plugin is instantiated on the main output device (primary tree),
		/// AK_Fail otherwise.
		/// \sa
		/// - AK::IAkSinkPluginContext::IsPrimary()
		/// - AK::IAkSinkPlugin::IsDataNeeded()
		/// - AK::IAkSinkPlugin::Consume()
		virtual AKRESULT SignalAudioThread() = 0;

		/// Query engine's user-defined sink queue depth (AkPlatformInitSettings::uNumRefillsInVoice).
		/// \return The engine's AkPlatformInitSettings::uNumRefillsInVoice value on platforms for which it exists, 0 otherwise.
		virtual AkUInt16 GetNumRefillsInVoice() = 0;
	};
	
	/// Software effect plug-in interface for sink (audio end point) plugins.
	class IAkSinkPlugin : public IAkPlugin
	{
	public:

		/// Initialization of the sink plugin.
		/// \return AK_Success if successful.
		virtual AKRESULT Init(
			IAkPluginMemAlloc *		in_pAllocator,			///< Interface to memory allocator to be used by the effect.
			IAkSinkPluginContext *	in_pSinkPluginContext,	///< Interface to sink plug-in's context.
			AkAudioFormat &			io_rFormat				///< Audio data format of the input signal. Note that the channel configuration is suggestive and may even be specified as !AkChannelConfig::IsValid(). The plugin is free to determine the true channel configuration.
			) = 0;

		/// Obtain the number of audio frames that should be processed by the sound engine and presented
		/// to this plugin via AK::IAkSinkPlugin::Consume(). The size of a frame is determined by the sound engine and 
		/// obtainable via AK::IAkPluginContextBase::GetMaxBufferLength().
		/// \return AK_Success if successful, AK_Fail if there was a critical error.
		/// \sa
		/// - AK::IAkSinkPlugin::Consume()
		/// - AK::IAkSinkPluginContext::SignalAudioThread()
		virtual AKRESULT IsDataNeeded( 
			AkUInt32 &				out_uNumFramesNeeded	///< Returned number of audio frames needed.
			) = 0;

		/// Present an audio buffer to the sink. The audio buffer is in the native format of the sound engine
		/// (typically float, deinterleaved), as specified by io_rFormat passed to Init(). It is up to the 
		/// plugin to transform it into a format that is compatible with its output.
		/// Note that Consume() is not called if the output for this frame consists of silence. Plugins should
		/// detect this in OnFrameEnd().
		/// \sa
		/// - AK::IAkSinkPlugin::IsDataNeeded()
		/// - AK::IAkSinkPlugin::OnFrameEnd()
		virtual void Consume(
			AkAudioBuffer *			in_pInputBuffer,		///< Input audio buffer data structure. Plugins should avoid processing data in-place.
			AkRamp					in_gain					///< Volume gain to apply to this input (prev corresponds to the beginning, next corresponds to the end of the buffer).
			) = 0;

		/// Called at the end of the audio frame. Plugins do whatever bookkeeping needed.
		/// \sa
		/// - AK::IAkSinkPlugin::Consume()
		virtual void OnFrameEnd() = 0;

		/// Ask the plugin whether starvation occurred.
		/// \return True if starvation occurred, false otherwise.
		virtual bool IsStarved() = 0;

		/// Reset the "starvation" flag after IsStarved() returned true.
		virtual void ResetStarved() = 0;
	};

	/// Wwise sound engine source plug-in interface (see \ref soundengine_plugins_source).
	class IAkSourcePlugin : public IAkPlugin
	{
	protected:
		/// Virtual destructor on interface to avoid warnings.
		virtual ~IAkSourcePlugin(){}

	public:
		/// Source plug-in initialization. Gets the plug-in ready for data processing, allocates memory and sets up the initial conditions. 
		/// \aknote Memory allocation should be done through the appropriate macros (see \ref fx_memory_alloc). \endaknote
		/// \sa
		/// - \ref iaksourceeffect_init
		virtual AKRESULT Init( 
			IAkPluginMemAlloc *			in_pAllocator,					///< Interface to the memory allocator to be used by the plug-in
			IAkSourcePluginContext *	in_pSourcePluginContext,		///< Interface to the source plug-in's context
			IAkPluginParam *			in_pParams,						///< Interface to the plug-in parameters
			AkAudioFormat &				io_rFormat						///< Audio format of the output data to be produced by the plug-in (mono native by default)
			) = 0;

		/// This method is called to determine the approximate duration of the source.
		/// \return The duration of the source, in milliseconds.
		/// \sa
		/// - \ref iaksourceeffect_getduration
		virtual AkReal32 GetDuration() const = 0;

		/// This method is called to determine the estimated envelope of the source.
		/// \return The estimated envelope of the data that will be generated in the next call to 
		/// Execute(). The envelope value should be normalized to the highest peak of the entire
		/// duration of the source. Expected range is [0,1]. If envelope and peak value cannot be 
		/// predicted, the source should return 1 (no envelope).
		/// \sa
		/// - \ref iaksourceeffect_getenvelope
		virtual AkReal32 GetEnvelope() const 
		{ 
			return 1.f;
		}

		/// This method is called to tell the source to stop looping.
		/// This will typically be called when an action of type "break" will be triggered on the playing source.
		/// Break (or StopLooping) means: terminate gracefully... if possible. In most situations it finishes the current loop and plays the sound release if there is one.
		/// 
		/// \return 
		/// - AK_Success if the source ignores the break command and plays normally till the end or if the source support to stop looping and terminates gracefully.
		/// - AK_Fail if the source cannot simply stop looping, in this situation, the break command will end up stopping this source.
		/// \sa
		/// - \ref iaksourceeffect_stoplooping
		virtual AKRESULT StopLooping(){ return AK_Success; }

		/// This method is called to tell the source to seek to an arbitrary sample.
		/// This will typically be called when the game calls AK::SoundEngine::SeekOnEvent() where the event plays 
		/// a sound that wraps this source plug-in.
		/// If the plug-in does not handle seeks, it should return AK_Success. If it returns AK_Fail, it will
		/// be terminated by the sound engine.
		/// 
		/// \return 
		/// - AK_Success if the source handles or ignores seek command.
		/// - AK_Fail if the source considers that seeking requests should provoke termination, for example, if
		///		the desired position is greater than the prescribed source duration.
		/// \sa
		/// - AK::SoundEngine::SeekOnEvent()
		virtual AKRESULT Seek( 
			AkUInt32 /* in_uPosition */	///< Position to seek to, in samples, at the rate specified in AkAudioFormat (see AK::IAkSourcePlugin::Init()).
			) { return AK_Success; }

		/// Skips execution when the voice is virtual playing from elapsed time to simulate processing that would have taken place (e.g. update internal state) while
		/// avoiding most of the CPU hit of plug-in execution.
		/// Given the number of frames requested adjust the number of frames that would have been produced by a call to Execute() in the io_uFrames parameter and return and
		/// return AK_DataReady or AK_NoMoreData, depending if there would be audio output or not at that point.
		/// Returning AK_NotImplemented will trigger a normal execution of the voice (as if it was not virtual) thus not enabling the CPU savings of a proper from elapsed time behavior.
		/// Note that returning AK_NotImplemeted for a source plug-ins that support asynchronous processing (PS3 platform only) will produce a 'resume' virtual voice behavior instead.
		virtual AKRESULT TimeSkip(
			AkUInt32 & /*io_uFrames	*/ ///< (Input) Number of frames that the audio buffer processing can advance (equivalent to MaxFrames()). The output value should be the number of frames that would be produced this execution.
			) { return AK_NotImplemented; }

		/// Software effect plug-in DSP execution.
		/// \aknote The effect can output as much as wanted up to MaxFrames(). All sample frames passed uValidFrames at input time are 
		/// not initialized and it is the responsibility of the effect to do so. When modifying the number of valid frames within execution
		/// (e.g. to flush delay lines) the effect should notify the pipeline by updating uValidFrames accordingly.
		/// \aknote The effect will stop being called by the pipeline when AK_NoMoreData is returned in the the eState field of the AkAudioBuffer structure.
		virtual void Execute( 
				AkAudioBuffer *							io_pBuffer		///< In/Out audio buffer data structure (in-place processing)
#ifdef AK_PS3
				, AK::MultiCoreServices::DspProcess*&	out_pDspProcess	///< Asynchronous DSP process utilities on PS3
#endif
				) = 0;
	};

#if defined AK_WII
	/// This function can be useful to convert from normalized floating point audio samples to Wii-pipeline format samples. 
	static AkForceInline AkInt16 AK_FLOAT_TO_SAMPLETYPE(AkReal32 in_fIn)
	{
		AkInt32 i32 = (AkInt32)(in_fIn * 0x7FFF);
		i32 -= ((i32<=32767)-1)&(i32-32767);
		i32 -= ((i32>=-32768)-1)&(i32+32768);
		AkInt16 i16 = (AkInt16)i32;
		return i16;
	}

	/// This function can be useful to convert from normalized floating point audio samples to Wii-pipeline format samples when the input is not not to exceed (-1,1) range.
	static AkForceInline AkInt16 AK_FLOAT_TO_SAMPLETYPE_NOCLIP(AkReal32 in_fIn)
	{
		return (AkInt16)(in_fIn * 0x7FFF);
	}

	/// This function can be useful to convert from Wii-pipeline format samples to normalized floating point audio samples. 
	static AkForceInline AkReal32 AK_SAMPLETYPE_TO_FLOAT(AkInt16 in_iIn)
	{
		return (((AkReal32)in_iIn) * (1.f/0x7FFF));
	}
#else
	/// This function can be useful to convert from normalized floating point audio samples to Wii-pipeline format samples. 
	#define AK_FLOAT_TO_SAMPLETYPE( __in__ ) (__in__)
	/// This function can be useful to convert from normalized floating point audio samples to Wii-pipeline format samples when the input is not not to exceed (-1,1) range.
	#define AK_FLOAT_TO_SAMPLETYPE_NOCLIP( __in__ ) (__in__)
	/// This function can be useful to convert from Wii-pipeline format samples to normalized floating point audio samples. 
	#define AK_SAMPLETYPE_TO_FLOAT( __in__ ) (__in__)
#endif

	#define AK_DBTOLIN( __db__ ) (powf(10.f,(__db__) * 0.05f))
}

/// Registered plugin creation function prototype.
AK_CALLBACK( AK::IAkPlugin*, AkCreatePluginCallback )( AK::IAkPluginMemAlloc * in_pAllocator );
/// Registered plugin parameter node creation function prototype.
AK_CALLBACK( AK::IAkPluginParam*, AkCreateParamCallback )( AK::IAkPluginMemAlloc * in_pAllocator );

#endif // _IAK_PLUGIN_H_
