//////////////////////////////////////////////////////////////////////
//
// Copyright (c) 2006 Audiokinetic Inc. / All Rights Reserved
//
//////////////////////////////////////////////////////////////////////

// AkCommonDefs.h

/// \file 
/// AudioLib common defines, enums, and structs.


#ifndef _AK_COMMON_DEFS_H_
#define _AK_COMMON_DEFS_H_

#include <AK/SoundEngine/Common/AkSpeakerConfig.h>
#include <AK/SoundEngine/Common/AkSpeakerVolumes.h>

//-----------------------------------------------------------------------------
// AUDIO DATA FORMAT
//-----------------------------------------------------------------------------

// Per-platform standard/largest setup definitions.
#if defined(AK_71AUDIO)
#define AK_SPEAKER_SETUP_DEFAULT_PLANE			(AK_SPEAKER_SETUP_7POINT1)	///< All speakers on the plane, supported on this platform.
#define AK_SUPPORTED_STANDARD_CHANNEL_MASK		(AK_SPEAKER_SETUP_ALL_SPEAKERS)	///< Platform supports all standard channels.
#elif defined(AK_LFECENTER) && defined(AK_REARCHANNELS)
#define AK_SPEAKER_SETUP_DEFAULT_PLANE			(AK_SPEAKER_SETUP_5POINT1)	///< All speakers on the plane, supported on this platform.
#define AK_SUPPORTED_STANDARD_CHANNEL_MASK		(AK_SPEAKER_SETUP_DEFAULT_PLANE)	///< Platform supports 5.1
#elif defined(AK_REARCHANNELS)
	#ifdef AK_WII
		#define AK_SPEAKER_SETUP_DEFAULT_PLANE	(AK_SPEAKER_SETUP_DPL2 | AK_SPEAKER_FRONT_CENTER)	///< All speakers on the plane, supported on this platform.
	#else
		#define AK_SPEAKER_SETUP_DEFAULT_PLANE	(AK_SPEAKER_SETUP_4 | AK_SPEAKER_FRONT_CENTER)		///< All speakers on the plane, supported on this platform.
	#endif
	#define AK_SUPPORTED_STANDARD_CHANNEL_MASK	(AK_SPEAKER_SETUP_DEFAULT_PLANE)	///< Most complete speaker configuration supported on this platform.
#else 
#define AK_SPEAKER_SETUP_DEFAULT_PLANE			(AK_SPEAKER_SETUP_STEREO | AK_SPEAKER_FRONT_CENTER)	///< All speakers on the plane, supported on this platform.
#define AK_SUPPORTED_STANDARD_CHANNEL_MASK		(AK_SPEAKER_SETUP_STEREO)	///< Most complete speaker configuration supported on this platform.
#endif

// Channel mask helpers.
namespace AK
{
	/// Returns true when the LFE channel is present in a given channel configuration.
	/// \return True if the LFE channel is present.
	AkForceInline bool HasLFE( AkChannelMask in_uChannelMask )
	{ 
		return ( in_uChannelMask & AK_SPEAKER_LOW_FREQUENCY ) > 0; 
	}

	/// Returns true when the center channel is present in a given channel configuration.
	/// Note that mono configurations have one channel which is arbitrary set to AK_SPEAKER_FRONT_CENTER,
	/// so HasCenter() returns true for mono signals.
	/// \return True if the center channel is present.
	AkForceInline bool HasCenter( AkChannelMask in_uChannelMask )
	{ 
		// All supported non-mono configurations have an AK_SPEAKER_FRONT_LEFT.
		return ( in_uChannelMask & AK_SPEAKER_FRONT_CENTER ) > 0; 
	}

	/// Returns the number of angle values required to represent the given channel configuration.
	/// Use this function with supported 2D standard channel configurations only.
	/// \sa AK::SoundEngine::SetSpeakerAngles().
	AkForceInline AkUInt32 GetNumberOfAnglesForConfig( AkChannelMask in_uChannelMask )
	{
		AKASSERT( ( in_uChannelMask & ~AK_SPEAKER_SETUP_DEFAULT_PLANE ) == 0 );

		// LFE is irrelevant.
		in_uChannelMask &= ~AK_SPEAKER_LOW_FREQUENCY;
		// Center speaker is always in the center and thus does not require an angle.
		in_uChannelMask &= ~AK_SPEAKER_FRONT_CENTER;
		// We should have complete pairs at this point, unless there is a speaker at 180 degrees, 
		// in which case we need one more angle to specify it.
		AKASSERT( ( in_uChannelMask & AK_SPEAKER_BACK_CENTER ) || ( ( AK::GetNumNonZeroBits( in_uChannelMask ) % 2 ) == 0 ) );
		return AK::GetNumNonZeroBits( in_uChannelMask ) >> 1;
	}
}

// Audio data format.
// ------------------------------------------------

const AkDataTypeID		AK_INT				= 0;		///< Integer data type (uchar, short, and so on)
const AkDataTypeID		AK_FLOAT			= 1;		///< Float data type

const AkDataInterleaveID AK_INTERLEAVED		= 0;		///< Interleaved data
const AkDataInterleaveID AK_NONINTERLEAVED	= 1;		///< Non-interleaved data

// Native format currently the same on all supported platforms, may become platform specific in the future
const AkUInt32 AK_LE_NATIVE_BITSPERSAMPLE  = 32;					///< Native number of bits per sample.
const AkUInt32 AK_LE_NATIVE_SAMPLETYPE = AK_FLOAT;					///< Native data type.
const AkUInt32 AK_LE_NATIVE_INTERLEAVE = AK_NONINTERLEAVED;			///< Native interleaved setting.

/// Channel configuration type. 
enum AkChannelConfigType
{
	AK_ChannelConfigType_Anonymous			= 0x0,	// Channel mask == 0 and channels are anonymous.
	AK_ChannelConfigType_Standard			= 0x1,	// Channels must be identified with standard defines in AkSpeakerConfigs.	
	AK_ChannelConfigType_Ambisonic			= 0x2	// Ambisonic. Channel mask == 0 and channels follow standard ambisonic order.
};

/// Defines a channel configuration.
struct AkChannelConfig
{
	// Channel config: 
	// - uChannelMask is a bit field, whose channel identifiers depend on AkChannelConfigType (up to 20). Channel bits are defined in AkSpeakerConfig.h.
	// - eConfigType is a code that completes the identification of channels by uChannelMask.
	// - uNumChannels is the number of channels, identified (deduced from channel mask) or anonymous (set directly). 
	AkUInt32	uNumChannels	:8;	///< Number of channels.
	AkUInt32	eConfigType		:4;	///< Channel config type (AkChannelConfigType).
	AkUInt32	uChannelMask	:20;///< Channel mask (configuration). 

	/// Constructor. Clears / sets the channel config in "invalid" state (IsValid() returns false).
	AkForceInline AkChannelConfig()
		: uNumChannels( 0 )
		, eConfigType( 0 )
		, uChannelMask( 0 )
	{
	}

	/// Copy constructor.
	AkForceInline AkChannelConfig( AkChannelMask in_uChannelMask )
	{
		SetStandard( in_uChannelMask );
	}
	
	/// Operator != with a 32-bit word.
	AkForceInline bool operator!=( AkUInt32 in_uBitField )
	{
		return ( *((AkUInt32*)this) != in_uBitField );
	}

	/// Clear the channel config. Becomes "invalid" (IsValid() returns false).
	AkForceInline void Clear()
	{
		uNumChannels	= 0;
		eConfigType		= 0;
		uChannelMask	= 0;
	}

	/// Set channel config as a standard configuration specified with given channel mask.
	AkForceInline void SetStandard( AkUInt32 in_uChannelMask )
	{
		uNumChannels	= AK::GetNumNonZeroBits( in_uChannelMask );
		eConfigType		= AK_ChannelConfigType_Standard;
		uChannelMask	= in_uChannelMask;
	}
	
	/// Set channel config as either a standard or an anonymous configuration, specified with both a given channel mask (0 if anonymous) and a number of channels (which must match the channel mask if standard).
	AkForceInline void SetStandardOrAnonymous( AkUInt32 in_uNumChannels, AkUInt32 in_uChannelMask )
	{
		AKASSERT( in_uChannelMask == 0 || in_uNumChannels == AK::GetNumNonZeroBits( in_uChannelMask ) );
		uNumChannels	= in_uNumChannels;
		eConfigType		= ( in_uChannelMask ) ? AK_ChannelConfigType_Standard : AK_ChannelConfigType_Anonymous;
		uChannelMask	= in_uChannelMask;
	}

	/// Set channel config as an anonymous configuration specified with given number of channels.
	AkForceInline void SetAnonymous( AkUInt32 in_uNumChannels )
	{
		uNumChannels	= in_uNumChannels;
		eConfigType		= AK_ChannelConfigType_Anonymous;
		uChannelMask	= 0;
	}

	/// Set channel config as an ambisonic configuration specified with given number of channels.
	AkForceInline void SetAmbisonic( AkUInt32 in_uNumChannels )
	{
		uNumChannels	= in_uNumChannels;
		eConfigType		= AK_ChannelConfigType_Ambisonic;
		uChannelMask	= 0;
	}

	/// Returns true if valid, false otherwise (as when it is constructed, or invalidated using Clear()).
	AkForceInline bool IsValid()
	{
		return uNumChannels != 0;
	}

	/// Serialize channel config into a 32-bit word.
	AkForceInline void Serialize( AkUInt32 & out_uChannelConfig ) const
	{
		out_uChannelConfig = uNumChannels | ( eConfigType << 8 ) | ( uChannelMask << 12 );
	}
	
	/// Deserialize channel config from a 32-bit word.
	AkForceInline void Deserialize( AkUInt32 in_uChannelConfig )
	{
		uNumChannels = in_uChannelConfig & 0x000000ff;
		eConfigType = ( in_uChannelConfig >> 8 ) & 0x0000000f;
		uChannelMask = ( in_uChannelConfig >> 12 ) & 0x000fffff;
	}

	/// Returns a new config based on 'this' with no LFE.
	AkForceInline AkChannelConfig RemoveLFE() const
	{
		AkChannelConfig newConfig = *this;
#ifdef AK_LFECENTER
		AkUInt32 uNewChannelMask = newConfig.uChannelMask & ~AK_SPEAKER_LOW_FREQUENCY;
		AkUInt32 uNumLFEChannel = ( newConfig.uChannelMask - uNewChannelMask ) >> 3; // 0 or 1
		AKASSERT( uNumLFEChannel == 0 || uNumLFEChannel == 1 );
		newConfig.uNumChannels -= uNumLFEChannel;
		newConfig.uChannelMask = uNewChannelMask;
#endif
		return newConfig;
	}

	/// Returns a new config based on 'this' with no Front Center channel.
	AkForceInline AkChannelConfig RemoveCenter() const
	{
		AkChannelConfig newConfig = *this;
#ifdef AK_LFECENTER
		AkUInt32 uNewChannelMask = newConfig.uChannelMask & ~AK_SPEAKER_FRONT_CENTER;
		AkUInt32 uNumCenterChannel = ( newConfig.uChannelMask - uNewChannelMask ) >> 2;	// 0 or 1.
		AKASSERT( uNumCenterChannel == 0 || uNumCenterChannel == 1 );
		newConfig.uNumChannels -= uNumCenterChannel;
		newConfig.uChannelMask = uNewChannelMask;
#endif
		return newConfig;
	}

	/// Operator ==
	AkForceInline bool operator==( const AkChannelConfig & in_other ) const
	{
		return uNumChannels	== in_other.uNumChannels
			&& eConfigType == in_other.eConfigType
			&& uChannelMask	== in_other.uChannelMask;
	}

	/// Operator !=
	AkForceInline bool operator!=( const AkChannelConfig & in_other ) const
	{
		return uNumChannels	!= in_other.uNumChannels
			|| eConfigType != in_other.eConfigType
			|| uChannelMask	!= in_other.uChannelMask;
	}

	/// Checks if the channel configuration is supported by the source pipeline.
	/// \return The interleaved type
	AkForceInline bool IsChannelConfigSupported() const
	{
		if ( eConfigType == AK_ChannelConfigType_Standard )
		{
			bool bIsSupported = true;
			switch ( uChannelMask )
			{
			case AK_SPEAKER_SETUP_MONO:
			case AK_SPEAKER_SETUP_STEREO:
#ifdef AK_LFECENTER
			case AK_SPEAKER_SETUP_0POINT1:
			case AK_SPEAKER_SETUP_1POINT1:
			case AK_SPEAKER_SETUP_2POINT1:
			case AK_SPEAKER_SETUP_3STEREO:
			case AK_SPEAKER_SETUP_3POINT1:
#ifdef AK_REARCHANNELS
			case AK_SPEAKER_SETUP_4:
			case AK_SPEAKER_SETUP_4POINT1:
			case AK_SPEAKER_SETUP_5:
			case AK_SPEAKER_SETUP_5POINT1:
#endif
#endif
#ifdef AK_71AUDIO
			case AK_SPEAKER_SETUP_7:
			case AK_SPEAKER_SETUP_7POINT1:
#endif // AK_71AUDIO
				break;
			default:
				bIsSupported = false;
			}
			return bIsSupported;
		}
		else if ( eConfigType == AK_ChannelConfigType_Anonymous )
		{
			return true;
		}
		else
		{
			// TODO
			AKASSERT( eConfigType == AK_ChannelConfigType_Ambisonic );
			return false;
		}
	}

	/// Query if LFE channel is present.
	/// \return True when LFE channel is present
	AkForceInline bool HasLFE() const
	{
#ifdef AK_LFECENTER
		return AK::HasLFE( uChannelMask ); 
#else
		return false;
#endif
	}

	/// Query if center channel is present.
	/// Note that mono configurations have one channel which is arbitrary set to AK_SPEAKER_FRONT_CENTER,
	/// so HasCenter() returns true for mono signals.
	/// \return True when center channel is present and configuration has more than 2 channels.
	AkForceInline bool HasCenter() const
	{ 
#ifdef AK_LFECENTER
		return AK::HasCenter( uChannelMask ); 
#else
		return false;
#endif
	}
};

/// Defines the parameters of an audio buffer format.
struct AkAudioFormat
{
	AkUInt32	uSampleRate;		///< Number of samples per second

	AkChannelConfig channelConfig;	///< Channel configuration.

	AkUInt32	uBitsPerSample	:6; ///< Number of bits per sample.
	AkUInt32	uBlockAlign		:10;///< Number of bytes per sample frame. 
	AkUInt32	uTypeID			:2; ///< Data type ID (AkDataTypeID). 
	AkUInt32	uInterleaveID	:1; ///< Interleave ID (AkDataInterleaveID). 
	
	/// Get the number of channels.
	/// \return The number of channels
	AkForceInline AkUInt32 GetNumChannels() const
	{
		return channelConfig.uNumChannels;
	}

	/// Query if LFE channel is present.
	/// \return True when LFE channel is present
	AkForceInline bool HasLFE() const
	{ 
		return channelConfig.HasLFE(); 
	}

	/// Query if center channel is present.
	/// Note that mono configurations have one channel which is arbitrary set to AK_SPEAKER_FRONT_CENTER,
	/// so HasCenter() returns true for mono signals.
	/// \return True when center channel is present and configuration has more than 2 channels.
	AkForceInline bool HasCenter() const
	{ 
		return channelConfig.HasCenter(); 
	}

	/// Get the number of bits per sample.
	/// \return The number of bits per sample
	AkForceInline AkUInt32 GetBitsPerSample()	const						
	{ 
		return uBitsPerSample;
	}

	/// Get the block alignment.
	/// \return The block alignment
	AkForceInline AkUInt32 GetBlockAlign() const
	{
		return uBlockAlign;
	}

	/// Get the data sample format (Float or Integer).
	/// \return The data sample format
	AkForceInline AkUInt32 GetTypeID() const
	{
		return uTypeID;
	}

	/// Get the interleaved type.
	/// \return The interleaved type
	AkForceInline AkUInt32 GetInterleaveID() const
	{
		return uInterleaveID;
	}

	/// Set all parameters of the audio format structure.
	/// Channels are specified by channel mask (standard configs).
	void SetAll(
		AkUInt32    in_uSampleRate,		///< Number of samples per second
		AkChannelConfig in_channelConfig,	///< Channel configuration
		AkUInt32    in_uBitsPerSample,	///< Number of bits per sample
		AkUInt32    in_uBlockAlign,		///< Block alignment
		AkUInt32    in_uTypeID,			///< Data sample format (Float or Integer)
		AkUInt32    in_uInterleaveID	///< Interleaved type
		)
	{
		uSampleRate		= in_uSampleRate;
		channelConfig	= in_channelConfig;
		uBitsPerSample	= in_uBitsPerSample;
		uBlockAlign		= in_uBlockAlign;
		uTypeID			= in_uTypeID;
		uInterleaveID	= in_uInterleaveID;
	}

	/// Checks if the channel configuration is supported by the source pipeline.
	/// \return The interleaved type
	AkForceInline bool IsChannelConfigSupported() const
	{
		return channelConfig.IsChannelConfigSupported();
	}

};

// Build a 32 bit class identifier based on the Plug-in type,
// CompanyID and PluginID.
//
// Parameters:
//   - in_pluginType: A value from enum AkPluginType (4 bits)
//   - in_companyID: CompanyID as defined in the Plug-in's XML file (12 bits)
//			* 0-63: Reserved for Audiokinetic
//			* 64-255: Reserved for clients' in-house Plug-ins
//			* 256-4095: Assigned by Audiokinetic to third-party plug-in developers
//   - in_pluginID: PluginID as defined in the Plug-in's XML file (16 bits)
//			* 0-65535: Set freely by the Plug-in developer
#define AKMAKECLASSID( in_pluginType, in_companyID, in_pluginID ) \
	( (in_pluginType) + ( (in_companyID) << 4 ) + ( (in_pluginID) << ( 4 + 12 ) ) )

#define AKGETPLUGINTYPEFROMCLASSID( in_classID ) ( (in_classID) & AkPluginTypeMask )
#define AKGETCOMPANYIDFROMCLASSID( in_classID ) ( ( (in_classID) & 0x0000FFF0 ) >> 4 )
#define AKGETPLUGINIDFROMCLASSID( in_classID ) ( ( (in_classID) & 0xFFFF0000 ) >> ( 4 + 12 ) )

#define CODECID_FROM_PLUGINID AKGETPLUGINIDFROMCLASSID


namespace AK
{
	/// Interface to retrieve metering information about a buffer.
	class IAkMetering
	{
	protected:
		/// Virtual destructor on interface to avoid warnings.
		virtual ~IAkMetering(){}

	public:

		/// Get peak of each channel in this frame.
		/// Depending on when this function is called, you may get metering data computed in the previous frame only. In order to force recomputing of
		/// meter values, pass in_bForceCompute=true.
		/// \return Vector of linear peak levels, corresponding to each channel. NULL if AK_EnableBusMeter_Peak is not set (see IAkMixerPluginContext::SetMeteringFlags() or AK::SoundEngine::RegisterBusMeteringCallback()).
		virtual AK::SpeakerVolumes::ConstVectorPtr GetPeak() = 0;

		/// Get true peak of each channel (as defined by ITU-R BS.1770) in this frame.
		/// Depending on when this function is called, you may get metering data computed in the previous frame only. 
		/// \return Vector of linear true peak levels, corresponding to each channel. NULL if AK_EnableBusMeter_TruePeak is not set (see IAkMixerPluginContext::SetMeteringFlags() or AK::SoundEngine::RegisterBusMeteringCallback()).
		virtual AK::SpeakerVolumes::ConstVectorPtr GetTruePeak() = 0;

		/// Get the RMS value of each channel in this frame.
		/// Depending on when this function is called, you may get metering data computed in the previous frame only. In order to force recomputing of
		/// meter values, pass in_bForceCompute=true.
		/// \return Vector of linear rms levels, corresponding to each channel. NULL if AK_EnableBusMeter_RMS is not set (see IAkMixerPluginContext::SetMeteringFlags() or AK::SoundEngine::RegisterBusMeteringCallback()).
		virtual AK::SpeakerVolumes::ConstVectorPtr GetRMS() = 0;
		
		/// Get the mean k-weighted power value in this frame, used to compute loudness (as defined by ITU-R BS.1770).
		/// Depending on when this function is called, you may get metering data computed in the previous frame only.
		/// \return Total linear k-weighted power of all channels. 0 if AK_EnableBusMeter_KPower is not set (see IAkMixerPluginContext::SetMeteringFlags() or AK::SoundEngine::RegisterBusMeteringCallback()).
		virtual AkReal32 GetKWeightedPower() = 0;
	};
}

// Audio buffer.
// ------------------------------------------------

/// Native sample type. 
/// \remarks Sample values produced by insert effects must use this type.
/// \remarks Source plug-ins can produce samples of other types (specified through 
/// according fields of AkAudioFormat, at initial handshaking), but these will be 
/// format converted internally into the native format.
/// \sa
/// - \ref iaksourceeffect_init
/// - \ref iakmonadiceffect_init
#if defined AK_WII_FAMILY_HW || defined(AK_3DS)
typedef AkInt16 AkSampleType;	///< Audio sample data type (Wii-specific: 16 bit signed integer)
#else
typedef AkReal32 AkSampleType;	///< Audio sample data type (32 bit floating point)
#endif

/// Audio buffer structure including the address of an audio buffer, the number of valid frames inside, 
/// and the maximum number of frames the audio buffer can hold.
/// \sa
/// - \ref fx_audiobuffer_struct
class AkAudioBuffer
{
public:

	/// Constructor.
	AkAudioBuffer() 
	{ 
		Clear(); 
	}

	/// Clear data pointer.
	AkForceInline void ClearData()
	{
#if !defined(AK_WII_FAMILY_HW) && !defined(AK_3DS)
		pData = NULL;
#else
		arData[0] = arData[1] = NULL;		
#endif
	}

	/// Clear members.
	AkForceInline void Clear()
	{
		ClearData();
		uValidFrames		= 0;
		uMaxFrames			= 0;
		eState				= AK_DataNeeded;
	}
	
	/// \name Channel queries.
	//@{
	/// Get the number of channels.
	AkForceInline AkUInt32 NumChannels()
	{
		return channelConfig.uNumChannels;
	}

	/// Returns true if there is an LFE channel present.
	AkForceInline bool HasLFE()
	{ 
		return channelConfig.HasLFE(); 
	}

	AkForceInline AkChannelConfig GetChannelConfig() const { return channelConfig; }

	//@}

	/// \name Interleaved interface
	//@{
	/// Get address of data: to be used with interleaved buffers only.
	/// \remarks Only source plugins can output interleaved data. This is determined at 
	/// initial handshaking.
	/// \sa 
	/// - \ref fx_audiobuffer_struct
#if !defined(AK_3DS) && !defined(AK_WII_FAMILY_HW)
	AkForceInline void * GetInterleavedData()
	{ 
		return pData; 
	}

	/// Attach interleaved data. Allocation is performed outside.
	inline void AttachInterleavedData( void * in_pData, AkUInt16 in_uMaxFrames, AkUInt16 in_uValidFrames, AkChannelConfig in_channelConfig )
	{ 
		pData = in_pData; 
		uMaxFrames = in_uMaxFrames; 
		uValidFrames = in_uValidFrames; 
		channelConfig = in_channelConfig; 
	}
#endif
	//@}

	/// \name Deinterleaved interface
	//@{

	/// Check if buffer has samples attached to it.
	AkForceInline bool HasData() 
	{
#if !defined(AK_WII_FAMILY_HW) && !defined(AK_3DS)
		return ( NULL != pData ); 
#else
		return ( NULL != arData[0] );
#endif
	}

	/// Convert a channel, identified by a single channel bit, to a buffer index used in GetChannel() below, for a given channel config.
	/// Standard indexing follows channel bit order (see AkSpeakerConfig.h). Pipeline/buffer indexing is the same but the LFE is moved to the end.
	static inline AkUInt32 StandardToPipelineIndex( 
		AkChannelConfig 
#ifdef AK_LFECENTER
						in_channelConfig		///< Channel configuration.
#endif
		, AkUInt32		in_uChannelIdx			///< Channel index in standard ordering to be converted to pipeline ordering.
		)
	{
#ifdef AK_LFECENTER
		if ( in_channelConfig.HasLFE() )
		{
			AKASSERT( in_channelConfig.eConfigType == AK_ChannelConfigType_Standard );	// in_channelConfig.HasLFE() would not have returned true otherwise.
			AKASSERT( AK::GetNumNonZeroBits( in_channelConfig.uChannelMask ) );
			AkUInt32 uIdxLFE = AK::GetNumNonZeroBits( ( AK_SPEAKER_LOW_FREQUENCY - 1 ) & in_channelConfig.uChannelMask );
			if ( in_uChannelIdx == uIdxLFE )
				return in_channelConfig.uNumChannels - 1;
			else if ( in_uChannelIdx > uIdxLFE )
				return in_uChannelIdx - 1;
		}
#endif
		return in_uChannelIdx;
	}

	/// Get the buffer of the ith channel. 
	/// Access to channel data is most optimal through this method. Use whenever the
	/// speaker configuration is known, or when an operation must be made independently
	/// for each channel.
	/// \remarks When using a standard configuration, use ChannelMaskToBufferIndex() to convert channel bits to buffer indices.
	/// \return Address of the buffer of the ith channel.
	/// \sa
	/// - \ref fx_audiobuffer_struct
	/// - \ref fx_audiobuffer_struct_channels
	inline AkSampleType * GetChannel(
		AkUInt32 in_uIndex		///< Channel index [0,NumChannels()-1]
		)
	{
		AKASSERT( in_uIndex < NumChannels() );
#if defined (AK_WII_FAMILY_HW) || defined(AK_3DS)
		return (AkSampleType*)arData[in_uIndex];
#else
		return (AkSampleType*)((AkUInt8*)(pData) + ( in_uIndex * sizeof(AkSampleType) * MaxFrames() ));
#endif
	}

	/// Get the buffer of the LFE.
	/// \return Address of the buffer of the LFE. Null if there is no LFE channel.
	/// \sa
	/// - \ref fx_audiobuffer_struct_channels
	inline AkSampleType * GetLFE()
	{
		if ( channelConfig.uChannelMask & AK_SPEAKER_LOW_FREQUENCY )
			return GetChannel( NumChannels()-1 );
		
		return (AkSampleType*)0;
	}

	/// Can be used to transform an incomplete into a complete buffer with valid data.
	/// The invalid frames are made valid (zeroed out) for all channels and the validFrames count will be made equal to uMaxFrames.
	void ZeroPadToMaxFrames()
	{
		// Zero out all channels.
		const AkUInt32 uNumChannels = NumChannels();
		const AkUInt32 uNumZeroFrames = MaxFrames()-uValidFrames;
		if ( uNumZeroFrames )
		{
			for ( AkUInt32 i = 0; i < uNumChannels; ++i )
			{
				AKPLATFORM::AkMemSet( GetChannel(i) + uValidFrames, 0, uNumZeroFrames * sizeof(AkSampleType) );
			}
			uValidFrames = MaxFrames();
		}
	}

#if !defined(AK_3DS) && !defined(AK_WII_FAMILY_HW)
	/// Attach deinterleaved data where channels are contiguous in memory. Allocation is performed outside.
	AkForceInline void AttachContiguousDeinterleavedData( void * in_pData, AkUInt16 in_uMaxFrames, AkUInt16 in_uValidFrames, AkChannelConfig in_channelConfig )
	{ 
		AttachInterleavedData( in_pData, in_uMaxFrames, in_uValidFrames, in_channelConfig );
	}
	/// Detach deinterleaved data where channels are contiguous in memory. The address of the buffer is returned and fields are cleared.
	AkForceInline void * DetachContiguousDeinterleavedData()
	{
		uMaxFrames = 0; 
		uValidFrames = 0; 
		channelConfig.Clear();
		void * pDataOld = pData;
		pData = NULL;
		return pDataOld;
	}
#endif

#if defined(_DEBUG) && !defined(AK_WII_FAMILY_HW)
	bool CheckValidSamples()
	{
		// Zero out all channels.
		const AkUInt32 uNumChannels = NumChannels();
		for ( AkUInt32 i = 0; i < uNumChannels; ++i )
		{
			AkSampleType * AK_RESTRICT pfChan = GetChannel(i);
			if ( pfChan )
			{
				for ( AkUInt32 j = 0; j < uValidFrames; j++ )
				{
					AkSampleType fSample = *pfChan++;
					if ( fSample > 4.f )
						return false;
					else if ( fSample < -4.f )
						return false;
				}
			}
		}

		return true;
	}
#endif

#ifdef AK_PS3
	/// Access to contiguous channels for DMA transfers on SPUs (PS3 specific).
	/// \remarks On the PS3, deinterleaved channels are guaranteed to be contiguous
	/// in memory to allow one-shot DMA transfers.
	AkForceInline void * GetDataStartDMA()
	{
		return pData;
	}
#endif

#ifdef __SPU__
	/// Construct AkAudioBuffer on SPU from data obtained through explicit DMAs.
	/// \remarks Address provided should point to a contiguous memory space for all deinterleaved channels.
	AkForceInline void CreateFromDMA( void * in_pData, AkUInt16 in_uMaxFrames, AkUInt16 in_uValidFrames, AkChannelConfig in_channelConfig, AKRESULT in_eState )
	{ 
		pData = in_pData; 
		uMaxFrames = in_uMaxFrames; 
		uValidFrames = in_uValidFrames; 
		channelConfig = in_channelConfig; 
		eState = in_eState;
	}
#endif
	//@}

#if !defined(AK_3DS) && !defined(AK_WII_FAMILY_HW)
	void RelocateMedia( AkUInt8* in_pNewMedia,  AkUInt8* in_pOldMedia )
	{
		AkUIntPtr uMemoryOffset = (AkUIntPtr)in_pNewMedia - (AkUIntPtr)in_pOldMedia;
		pData = (void*) (((AkUIntPtr)pData) + uMemoryOffset);
	}
#endif

protected:
#if defined (AK_WII_FAMILY_HW) || defined(AK_3DS)
	void *			arData[AK_VOICE_MAX_NUM_CHANNELS];	///< Array of audio buffers for each channel (Wii-specific implementation).
#else
	void *			pData;				///< Start of the audio buffer.
#endif
	AkChannelConfig	channelConfig;		///< Channel config.
public:	
	AKRESULT		eState;				///< Execution status	
protected:	
	AkUInt16		uMaxFrames;			///< Number of sample frames the buffer can hold. Access through AkAudioBuffer::MaxFrames().

public:
	/// Access to the number of sample frames the buffer can hold.
	/// \return Number of sample frames the buffer can hold.
	AkForceInline AkUInt16 MaxFrames() { return uMaxFrames; }
	
	AkUInt16		uValidFrames;		///< Number of valid sample frames in the audio buffer
} AK_ALIGN_DMA;

#endif // _AK_COMMON_DEFS_H_

