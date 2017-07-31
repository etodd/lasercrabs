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

#ifndef _AK_SPEAKERCONFIG_H_
#define _AK_SPEAKERCONFIG_H_

#include <AK/SoundEngine/Common/AkTypes.h>

/// Standard speakers (channel mask):
#define AK_SPEAKER_FRONT_LEFT				0x1		///< Front left speaker bit mask
#define AK_SPEAKER_FRONT_RIGHT				0x2		///< Front right speaker bit mask
#define AK_SPEAKER_FRONT_CENTER				0x4		///< Front center speaker bit mask
#define AK_SPEAKER_LOW_FREQUENCY			0x8		///< Low-frequency speaker bit mask
#define AK_SPEAKER_BACK_LEFT				0x10	///< Rear left speaker bit mask
#define AK_SPEAKER_BACK_RIGHT				0x20	///< Rear right speaker bit mask
#define AK_SPEAKER_BACK_CENTER				0x100	///< Rear center speaker ("surround speaker") bit mask
#define AK_SPEAKER_SIDE_LEFT				0x200	///< Side left speaker bit mask
#define AK_SPEAKER_SIDE_RIGHT				0x400	///< Side right speaker bit mask

/// "Height" speakers.
#define AK_SPEAKER_TOP						0x800 	///< Top speaker bit mask
#define AK_SPEAKER_HEIGHT_FRONT_LEFT		0x1000	///< Front left speaker bit mask
#define AK_SPEAKER_HEIGHT_FRONT_CENTER		0x2000	///< Front center speaker bit mask
#define AK_SPEAKER_HEIGHT_FRONT_RIGHT		0x4000	///< Front right speaker bit mask
#define AK_SPEAKER_HEIGHT_BACK_LEFT			0x8000	///< Rear left speaker bit mask
#define AK_SPEAKER_HEIGHT_BACK_CENTER		0x10000	///< Rear center speaker bit mask
#define AK_SPEAKER_HEIGHT_BACK_RIGHT		0x20000	///< Rear right speaker bit mask

//
// Supported speaker setups. Those are the ones that can be used in the Wwise Sound Engine audio pipeline.
//

#define AK_SPEAKER_SETUP_MONO			AK_SPEAKER_FRONT_CENTER		///< 1.0 setup channel mask
#define AK_SPEAKER_SETUP_0POINT1		AK_SPEAKER_LOW_FREQUENCY	///< 0.1 setup channel mask
#define AK_SPEAKER_SETUP_1POINT1		(AK_SPEAKER_FRONT_CENTER	| AK_SPEAKER_LOW_FREQUENCY)	///< 1.1 setup channel mask
#define AK_SPEAKER_SETUP_STEREO			(AK_SPEAKER_FRONT_LEFT		| AK_SPEAKER_FRONT_RIGHT)	///< 2.0 setup channel mask
#define AK_SPEAKER_SETUP_2POINT1		(AK_SPEAKER_SETUP_STEREO	| AK_SPEAKER_LOW_FREQUENCY)	///< 2.1 setup channel mask
#define AK_SPEAKER_SETUP_3STEREO		(AK_SPEAKER_SETUP_STEREO	| AK_SPEAKER_FRONT_CENTER)	///< 3.0 setup channel mask
#define AK_SPEAKER_SETUP_3POINT1		(AK_SPEAKER_SETUP_3STEREO	| AK_SPEAKER_LOW_FREQUENCY)	///< 3.1 setup channel mask
#define AK_SPEAKER_SETUP_4				(AK_SPEAKER_SETUP_STEREO	| AK_SPEAKER_SIDE_LEFT | AK_SPEAKER_SIDE_RIGHT)	///< 4.0 setup channel mask
#define AK_SPEAKER_SETUP_4POINT1		(AK_SPEAKER_SETUP_4			| AK_SPEAKER_LOW_FREQUENCY)	///< 4.1 setup channel mask
#define AK_SPEAKER_SETUP_5				(AK_SPEAKER_SETUP_4			| AK_SPEAKER_FRONT_CENTER)	///< 5.0 setup channel mask
#define AK_SPEAKER_SETUP_5POINT1		(AK_SPEAKER_SETUP_5			| AK_SPEAKER_LOW_FREQUENCY)	///< 5.1 setup channel mask
#define AK_SPEAKER_SETUP_6				(AK_SPEAKER_SETUP_4			| AK_SPEAKER_BACK_LEFT | AK_SPEAKER_BACK_RIGHT)	///< 6.0 setup channel mask
#define AK_SPEAKER_SETUP_6POINT1		(AK_SPEAKER_SETUP_6			| AK_SPEAKER_LOW_FREQUENCY)	///< 6.1 setup channel mask
#define AK_SPEAKER_SETUP_7				(AK_SPEAKER_SETUP_6			| AK_SPEAKER_FRONT_CENTER)	///< 7.0 setup channel mask
#define AK_SPEAKER_SETUP_7POINT1		(AK_SPEAKER_SETUP_7			| AK_SPEAKER_LOW_FREQUENCY)	///< 7.1 setup channel mask
#define AK_SPEAKER_SETUP_SURROUND		(AK_SPEAKER_SETUP_STEREO	| AK_SPEAKER_BACK_CENTER)	///< Legacy surround setup channel mask

// Note. DPL2 does not really have 4 channels, but it is used by plugins to differentiate from stereo setup.
#define AK_SPEAKER_SETUP_DPL2			(AK_SPEAKER_SETUP_4)		///< Legacy DPL2 setup channel mask

#define AK_SPEAKER_SETUP_HEIGHT_4		(AK_SPEAKER_HEIGHT_FRONT_LEFT | AK_SPEAKER_HEIGHT_FRONT_RIGHT	| AK_SPEAKER_HEIGHT_BACK_LEFT | AK_SPEAKER_HEIGHT_BACK_RIGHT)	///< 4 speaker height layer.
#define AK_SPEAKER_SETUP_HEIGHT_5		(AK_SPEAKER_SETUP_HEIGHT_4 | AK_SPEAKER_HEIGHT_FRONT_CENTER)																	///< 5 speaker height layer.
#define AK_SPEAKER_SETUP_HEIGHT_ALL		(AK_SPEAKER_SETUP_HEIGHT_5 | AK_SPEAKER_HEIGHT_BACK_CENTER)																		///< All height speaker layer.

// Auro speaker setups
#define AK_SPEAKER_SETUP_AURO_222			(AK_SPEAKER_SETUP_4			| AK_SPEAKER_HEIGHT_FRONT_LEFT	| AK_SPEAKER_HEIGHT_FRONT_RIGHT)	///< Auro-222 setup channel mask
#define AK_SPEAKER_SETUP_AURO_8				(AK_SPEAKER_SETUP_AURO_222	| AK_SPEAKER_HEIGHT_BACK_LEFT	| AK_SPEAKER_HEIGHT_BACK_RIGHT)		///< Auro-8 setup channel mask
#define AK_SPEAKER_SETUP_AURO_9				(AK_SPEAKER_SETUP_AURO_8	| AK_SPEAKER_FRONT_CENTER)											///< Auro-9.0 setup channel mask
#define AK_SPEAKER_SETUP_AURO_9POINT1		(AK_SPEAKER_SETUP_AURO_9	| AK_SPEAKER_LOW_FREQUENCY)											///< Auro-9.1 setup channel mask
#define AK_SPEAKER_SETUP_AURO_10			(AK_SPEAKER_SETUP_AURO_9	| AK_SPEAKER_TOP)													///< Auro-10.0 setup channel mask		
#define AK_SPEAKER_SETUP_AURO_10POINT1		(AK_SPEAKER_SETUP_AURO_10	| AK_SPEAKER_LOW_FREQUENCY)											///< Auro-10.1 setup channel mask	
#define AK_SPEAKER_SETUP_AURO_11			(AK_SPEAKER_SETUP_AURO_10	| AK_SPEAKER_HEIGHT_FRONT_CENTER)									///< Auro-11.0 setup channel mask
#define AK_SPEAKER_SETUP_AURO_11POINT1		(AK_SPEAKER_SETUP_AURO_11	| AK_SPEAKER_LOW_FREQUENCY)											///< Auro-11.1 setup channel mask	
#define AK_SPEAKER_SETUP_AURO_11_740		(AK_SPEAKER_SETUP_7			| AK_SPEAKER_SETUP_HEIGHT_4)										///< Auro-11.0 (7+4) setup channel mask
#define AK_SPEAKER_SETUP_AURO_11POINT1_740	(AK_SPEAKER_SETUP_AURO_11_740	| AK_SPEAKER_LOW_FREQUENCY)										///< Auro-11.1 (7+4) setup channel mask
#define AK_SPEAKER_SETUP_AURO_13_751		(AK_SPEAKER_SETUP_7			| AK_SPEAKER_SETUP_HEIGHT_5 | AK_SPEAKER_TOP)						///< Auro-13.0 setup channel mask
#define AK_SPEAKER_SETUP_AURO_13POINT1_751	(AK_SPEAKER_SETUP_AURO_13_751	| AK_SPEAKER_LOW_FREQUENCY)										///< Auro-13.1 setup channel mask

// Dolby speaker setups: in Dolby nomenclature, [#plane].[lfe].[#height]
#define AK_SPEAKER_SETUP_DOLBY_5_0_2		(AK_SPEAKER_SETUP_5			| AK_SPEAKER_HEIGHT_FRONT_LEFT	| AK_SPEAKER_HEIGHT_FRONT_RIGHT )	///< Dolby 5.0.2 setup channel mask
#define AK_SPEAKER_SETUP_DOLBY_5_1_2		(AK_SPEAKER_SETUP_DOLBY_5_0_2	| AK_SPEAKER_LOW_FREQUENCY )									///< Dolby 5.1.2 setup channel mask
#define AK_SPEAKER_SETUP_DOLBY_6_0_2		(AK_SPEAKER_SETUP_6			| AK_SPEAKER_HEIGHT_FRONT_LEFT	| AK_SPEAKER_HEIGHT_FRONT_RIGHT )	///< Dolby 6.0.2 setup channel mask
#define AK_SPEAKER_SETUP_DOLBY_6_1_2		(AK_SPEAKER_SETUP_DOLBY_6_0_2	| AK_SPEAKER_LOW_FREQUENCY )									///< Dolby 6.1.2 setup channel mask
#define AK_SPEAKER_SETUP_DOLBY_6_0_4		(AK_SPEAKER_SETUP_DOLBY_6_0_2	| AK_SPEAKER_HEIGHT_BACK_LEFT | AK_SPEAKER_HEIGHT_BACK_RIGHT )	///< Dolby 6.0.4 setup channel mask
#define AK_SPEAKER_SETUP_DOLBY_6_1_4		(AK_SPEAKER_SETUP_DOLBY_6_0_4	| AK_SPEAKER_LOW_FREQUENCY )									///< Dolby 6.1.4 setup channel mask
#define AK_SPEAKER_SETUP_DOLBY_7_0_2		(AK_SPEAKER_SETUP_7			| AK_SPEAKER_HEIGHT_FRONT_LEFT	| AK_SPEAKER_HEIGHT_FRONT_RIGHT )	///< Dolby 7.0.2 setup channel mask
#define AK_SPEAKER_SETUP_DOLBY_7_1_2		(AK_SPEAKER_SETUP_DOLBY_7_0_2	| AK_SPEAKER_LOW_FREQUENCY )									///< Dolby 7.1.2 setup channel mask
#define AK_SPEAKER_SETUP_DOLBY_7_0_4		(AK_SPEAKER_SETUP_DOLBY_7_0_2	| AK_SPEAKER_HEIGHT_BACK_LEFT | AK_SPEAKER_HEIGHT_BACK_RIGHT )	///< Dolby 7.0.4 setup channel mask
#define AK_SPEAKER_SETUP_DOLBY_7_1_4		(AK_SPEAKER_SETUP_DOLBY_7_0_4	| AK_SPEAKER_LOW_FREQUENCY )									///< Dolby 7.1.4 setup channel mask

#define AK_SPEAKER_SETUP_ALL_SPEAKERS		(AK_SPEAKER_SETUP_7POINT1 | AK_SPEAKER_BACK_CENTER | AK_SPEAKER_SETUP_HEIGHT_ALL | AK_SPEAKER_TOP)	///< All speakers.

// Channel indices.
// ------------------------------------------------

// Channel indices for standard setups on the plane.
#define AK_IDX_SETUP_FRONT_LEFT				(0)	///< Index of front-left channel in all configurations.
#define AK_IDX_SETUP_FRONT_RIGHT			(1)	///< Index of front-right channel in all configurations.
#define AK_IDX_SETUP_CENTER					(2)	///< Index of front-center channel in all configurations.

#define AK_IDX_SETUP_NOCENTER_BACK_LEFT		(2)	///< Index of back-left channel in configurations with no front-center channel.
#define AK_IDX_SETUP_NOCENTER_BACK_RIGHT	(3)	///< Index of back-right channel in configurations with no front-center channel.
#define AK_IDX_SETUP_NOCENTER_SIDE_LEFT		(4)	///< Index of side-left channel in configurations with no front-center channel.
#define AK_IDX_SETUP_NOCENTER_SIDE_RIGHT	(5)	///< Index of side-right channel in configurations with no front-center channel.

#define AK_IDX_SETUP_WITHCENTER_BACK_LEFT	(3)	///< Index of back-left channel in configurations with a front-center channel.
#define AK_IDX_SETUP_WITHCENTER_BACK_RIGHT	(4)	///< Index of back-right channel in configurations with a front-center channel.
#define AK_IDX_SETUP_WITHCENTER_SIDE_LEFT	(5)	///< Index of side-left channel in configurations with a front-center channel.
#define AK_IDX_SETUP_WITHCENTER_SIDE_RIGHT	(6)	///< Index of side-right channel in configurations with a front-center channel.

// Channel indices for specific setups.
#define AK_IDX_SETUP_0_LFE			(0)	///< Index of low-frequency channel in 0.1 setup (use with AkAudioBuffer::GetChannel())

#define AK_IDX_SETUP_1_CENTER		(0)	///< Index of center channel in 1.x setups (use with AkAudioBuffer::GetChannel())
#define AK_IDX_SETUP_1_LFE			(1)	///< Index of low-frequency channel in 1.1 setup (use with AkAudioBuffer::GetChannel())

#define AK_IDX_SETUP_2_LEFT			(0)	///< Index of left channel in 2.x setups (use with AkAudioBuffer::GetChannel())
#define AK_IDX_SETUP_2_RIGHT		(1)	///< Index of right channel in 2.x setups (use with AkAudioBuffer::GetChannel())
#define AK_IDX_SETUP_2_LFE			(2) ///< Index of low-frequency channel in 2.1 setup (use with AkAudioBuffer::GetChannel())

#define AK_IDX_SETUP_3_LEFT			(0)	///< Index of left channel in 3.x setups (use with AkAudioBuffer::GetChannel())
#define AK_IDX_SETUP_3_RIGHT		(1)	///< Index of right channel in 3.x setups (use with AkAudioBuffer::GetChannel())
#define AK_IDX_SETUP_3_CENTER		(2)	///< Index of center channel in 3.x setups (use with AkAudioBuffer::GetChannel())
#define AK_IDX_SETUP_3_LFE			(3)	///< Index of low-frequency channel in 3.1 setup (use with AkAudioBuffer::GetChannel())

#define AK_IDX_SETUP_4_FRONTLEFT	(0)	///< Index of front left channel in 4.x setups (use with AkAudioBuffer::GetChannel())
#define AK_IDX_SETUP_4_FRONTRIGHT	(1)	///< Index of front right channel in 4.x setups (use with AkAudioBuffer::GetChannel())
#define AK_IDX_SETUP_4_REARLEFT		(2)	///< Index of rear left channel in 4.x setups (use with AkAudioBuffer::GetChannel())
#define AK_IDX_SETUP_4_REARRIGHT	(3)	///< Index of rear right channel in 4.x setups (use with AkAudioBuffer::GetChannel())
#define AK_IDX_SETUP_4_LFE			(4)	///< Index of low-frequency channel in 4.1 setup (use with AkAudioBuffer::GetChannel())

#define AK_IDX_SETUP_5_FRONTLEFT	(0)	///< Index of front left channel in 5.x setups (use with AkAudioBuffer::GetChannel())
#define AK_IDX_SETUP_5_FRONTRIGHT	(1)	///< Index of front right channel in 5.x setups (use with AkAudioBuffer::GetChannel())
#define AK_IDX_SETUP_5_CENTER		(2)	///< Index of center channel in 5.x setups (use with AkAudioBuffer::GetChannel())
#define AK_IDX_SETUP_5_REARLEFT		(3)	///< Index of rear left channel in 5.x setups (use with AkAudioBuffer::GetChannel())
#define AK_IDX_SETUP_5_REARRIGHT	(4)	///< Index of rear right channel in 5.x setups (use with AkAudioBuffer::GetChannel())
#define AK_IDX_SETUP_5_LFE			(5)	///< Index of low-frequency channel in 5.1 setup (use with AkAudioBuffer::GetChannel())

#ifdef AK_71AUDIO
#define AK_IDX_SETUP_6_FRONTLEFT	(0)	///< Index of front left channel in 6.x setups (use with AkAudioBuffer::GetChannel())
#define AK_IDX_SETUP_6_FRONTRIGHT	(1)	///< Index of front right channel in 6x setups (use with AkAudioBuffer::GetChannel())
#define AK_IDX_SETUP_6_REARLEFT		(2)	///< Index of rear left channel in 6.x setups (use with AkAudioBuffer::GetChannel())
#define AK_IDX_SETUP_6_REARRIGHT	(3)	///< Index of rear right channel in 6.x setups (use with AkAudioBuffer::GetChannel())
#define AK_IDX_SETUP_6_SIDELEFT		(4)	///< Index of side left channel in 6.x setups (use with AkAudioBuffer::GetChannel())
#define AK_IDX_SETUP_6_SIDERIGHT	(5)	///< Index of side right channel in 6.x setups (use with AkAudioBuffer::GetChannel())
#define AK_IDX_SETUP_6_LFE			(6)	///< Index of low-frequency channel in 6.1 setup (use with AkAudioBuffer::GetChannel())

#define AK_IDX_SETUP_7_FRONTLEFT	(0)	///< Index of front left channel in 7.x setups (use with AkAudioBuffer::GetChannel())
#define AK_IDX_SETUP_7_FRONTRIGHT	(1)	///< Index of front right channel in 7.x setups (use with AkAudioBuffer::GetChannel())
#define AK_IDX_SETUP_7_CENTER		(2)	///< Index of center channel in 7.x setups (use with AkAudioBuffer::GetChannel())
#define AK_IDX_SETUP_7_REARLEFT		(3)	///< Index of rear left channel in 7.x setups (use with AkAudioBuffer::GetChannel())
#define AK_IDX_SETUP_7_REARRIGHT	(4)	///< Index of rear right channel in 7.x setups (use with AkAudioBuffer::GetChannel())
#define AK_IDX_SETUP_7_SIDELEFT		(5)	///< Index of side left channel in 7.x setups (use with AkAudioBuffer::GetChannel())
#define AK_IDX_SETUP_7_SIDERIGHT	(6)	///< Index of side right channel in 7.x setups (use with AkAudioBuffer::GetChannel())
#define AK_IDX_SETUP_7_LFE			(7)	///< Index of low-frequency channel in 7.1 setup (use with AkAudioBuffer::GetChannel())
#endif

//
// Extra speaker setups. This is a more exhaustive list of speaker setups, which might not all be supported
// by the Wwise Sound Engine audio pipeline.
//

#define AK_SPEAKER_SETUP_0_1		( AK_SPEAKER_LOW_FREQUENCY )							//0.1

#define AK_SPEAKER_SETUP_1_0_CENTER	( AK_SPEAKER_FRONT_CENTER )							//1.0 (C)
#define AK_SPEAKER_SETUP_1_1_CENTER ( AK_SPEAKER_FRONT_CENTER	| AK_SPEAKER_LOW_FREQUENCY )	//1.1 (C)

#define AK_SPEAKER_SETUP_2_0		( AK_SPEAKER_FRONT_LEFT	| AK_SPEAKER_FRONT_RIGHT )							//2.0
#define AK_SPEAKER_SETUP_2_1		( AK_SPEAKER_FRONT_LEFT	| AK_SPEAKER_FRONT_RIGHT | AK_SPEAKER_LOW_FREQUENCY )	//2.1

#define AK_SPEAKER_SETUP_3_0		( AK_SPEAKER_FRONT_LEFT	| AK_SPEAKER_FRONT_RIGHT | AK_SPEAKER_FRONT_CENTER )	//3.0
#define AK_SPEAKER_SETUP_3_1		( AK_SPEAKER_SETUP_3_0	| AK_SPEAKER_LOW_FREQUENCY )	//3.1

#define AK_SPEAKER_SETUP_FRONT		( AK_SPEAKER_SETUP_3_0 )

#define AK_SPEAKER_SETUP_4_0		( AK_SPEAKER_SETUP_4 )
#define AK_SPEAKER_SETUP_4_1		( AK_SPEAKER_SETUP_4POINT1 )
#define AK_SPEAKER_SETUP_5_0		( AK_SPEAKER_SETUP_5 )
#define AK_SPEAKER_SETUP_5_1		( AK_SPEAKER_SETUP_5POINT1 )

#define AK_SPEAKER_SETUP_6_0		( AK_SPEAKER_SETUP_6 )
#define AK_SPEAKER_SETUP_6_1		( AK_SPEAKER_SETUP_6POINT1 )
#define AK_SPEAKER_SETUP_7_0		( AK_SPEAKER_SETUP_7 )
#define AK_SPEAKER_SETUP_7_1		( AK_SPEAKER_SETUP_7POINT1 )

// Per-platform standard/largest setup definitions.
#if defined(AK_71AUDIO)
#define AK_SPEAKER_SETUP_DEFAULT_PLANE			(AK_SPEAKER_SETUP_7POINT1)	///< All speakers on the plane, supported on this platform.
#define AK_SUPPORTED_STANDARD_CHANNEL_MASK		(AK_SPEAKER_SETUP_ALL_SPEAKERS)	///< Platform supports all standard channels.
#define AK_STANDARD_MAX_NUM_CHANNELS			(8)							///< Legacy: Platform supports at least 7.1
#elif defined(AK_LFECENTER) && defined(AK_REARCHANNELS)
#define AK_SPEAKER_SETUP_DEFAULT_PLANE			(AK_SPEAKER_SETUP_5POINT1)	///< All speakers on the plane, supported on this platform.
#define AK_SUPPORTED_STANDARD_CHANNEL_MASK		(AK_SPEAKER_SETUP_DEFAULT_PLANE)	///< Platform supports 5.1
#define AK_VOICE_MAX_NUM_CHANNELS				(6)							///< Legacy: Platform supports up to 5.1 configuration.
#define AK_STANDARD_MAX_NUM_CHANNELS			(AK_VOICE_MAX_NUM_CHANNELS)	///< Legacy: Platform supports 5.1
#elif defined(AK_REARCHANNELS)
#define AK_SPEAKER_SETUP_DEFAULT_PLANE	(AK_SPEAKER_SETUP_4 | AK_SPEAKER_FRONT_CENTER)		///< All speakers on the plane, supported on this platform.
#define AK_VOICE_MAX_NUM_CHANNELS		(4)										///< Legacy: Platform supports up to 4.0 configuration.
#define AK_SUPPORTED_STANDARD_CHANNEL_MASK	(AK_SPEAKER_SETUP_DEFAULT_PLANE)	///< Most complete speaker configuration supported on this platform.
#else
#define AK_SPEAKER_SETUP_DEFAULT_PLANE			(AK_SPEAKER_SETUP_STEREO | AK_SPEAKER_FRONT_CENTER)	///< All speakers on the plane, supported on this platform.
#define AK_SUPPORTED_STANDARD_CHANNEL_MASK		(AK_SPEAKER_SETUP_STEREO)	///< Most complete speaker configuration supported on this platform.
#define AK_VOICE_MAX_NUM_CHANNELS				(2)							///< Legacy: Platform supports up to stereo configuration.
#define AK_STANDARD_MAX_NUM_CHANNELS			(AK_VOICE_MAX_NUM_CHANNELS)	///< Legacy: Platform supports stereo.

#endif

// Helpers.
inline void AK_SPEAKER_SETUP_FIX_LEFT_TO_CENTER( AkUInt32 &io_uChannelMask )
{
	if( !(io_uChannelMask & AK_SPEAKER_FRONT_CENTER) 
		&& !(io_uChannelMask & AK_SPEAKER_FRONT_RIGHT)
		&& (io_uChannelMask & AK_SPEAKER_FRONT_LEFT) )
	{
		io_uChannelMask &= ~AK_SPEAKER_FRONT_LEFT;		// remove left
		io_uChannelMask |= AK_SPEAKER_FRONT_CENTER;	// add center
	}
}

inline void AK_SPEAKER_SETUP_FIX_REAR_TO_SIDE( AkUInt32 &io_uChannelMask )
{
	if( io_uChannelMask & ( AK_SPEAKER_BACK_LEFT ) && !( io_uChannelMask & AK_SPEAKER_SIDE_LEFT ) )
	{
		io_uChannelMask &= ~( AK_SPEAKER_BACK_LEFT | AK_SPEAKER_BACK_RIGHT );	// remove rears
		io_uChannelMask |= ( AK_SPEAKER_SIDE_LEFT | AK_SPEAKER_SIDE_RIGHT );	// add sides
	}
}

inline void AK_SPEAKER_SETUP_CONVERT_TO_SUPPORTED( AkUInt32 &io_uChannelMask )
{
	AK_SPEAKER_SETUP_FIX_LEFT_TO_CENTER( io_uChannelMask );
	AK_SPEAKER_SETUP_FIX_REAR_TO_SIDE( io_uChannelMask );
}

/// Ambisonics configurations (corresponding to AkChannelConfig::eConfigType == AK_ChannelConfigType_Ambisonic).
/// Convention: X points towards the front, and XYZ follow a right-hand rule, so Y is the side vector (pointing to the left).
/// Channel presence and ordering are predefined according to the number of channels. The ordering convention is ACN,
/// with the mapping of components to number of channels detailed below (source: https://en.wikipedia.org/wiki/Ambisonic_data_exchange_formats).
/// Normalization natively used in Wwise is SN3D.
///
/// <table cellspacing="0" cellpadding="1" border="1" width="800px">
/// <tr><td rowspan="2" align="center"><b>Number of channels</b></td> <td colspan="2" align="center"><b>Order</b></td><td rowspan="2" align="center"><b>Description</b></td><td rowspan="2" align="center"><b>Layout of components</b></td></tr>
/// <tr><td align="center">Horizontal</td><td align="center">Vertical</td></tr>
/// <tr><td align="right">1 &nbsp;&nbsp;&nbsp;</td> <td align="right">0 &nbsp;&nbsp;&nbsp;</td><td align="right">0 &nbsp;&nbsp;&nbsp;</td> <td>&nbsp;&nbsp;mono</td><td>&nbsp;</td></tr>
/// <tr><td align="right">4 &nbsp;&nbsp;&nbsp;</td> <td align="right">1 &nbsp;&nbsp;&nbsp;</td><td align="right">1 &nbsp;&nbsp;&nbsp;</td> <td>&nbsp;&nbsp;first-order full sphere</td><td>&nbsp;&nbsp;WYZX</td></tr>
/// <tr><td align="right">9 &nbsp;&nbsp;&nbsp;</td> <td align="right">2 &nbsp;&nbsp;&nbsp;</td><td align="right">2 &nbsp;&nbsp;&nbsp;</td> <td>&nbsp;&nbsp;second-order full sphere</td><td>&nbsp;&nbsp;WYZXVTRSU</td></tr>
/// <tr><td align="right">16 &nbsp;&nbsp;&nbsp;</td> <td align="right">3 &nbsp;&nbsp;&nbsp;</td><td align="right">3 &nbsp;&nbsp;&nbsp;</td> <td>&nbsp;&nbsp;third-order full sphere</td><td>&nbsp;&nbsp;WYZXVTRSUQOMKLNP</td></tr>
/// </table>

namespace AK
{

/// Returns the number of channels of a given channel configuration.
static inline AkUInt8 ChannelMaskToNumChannels( AkChannelMask in_uChannelMask )
{
	AkUInt8 num = 0;
	while( in_uChannelMask ){ ++num; in_uChannelMask &= in_uChannelMask-1; } // iterate max once per channel.
	return num;
}

/// Returns a 'best guess' channel configuration from a given number of channels.
/// Will return 0 if no guess can be made.
static inline AkChannelMask ChannelMaskFromNumChannels( unsigned int in_uNumChannels )
{
	AkChannelMask uChannelMask = 0;

	switch ( in_uNumChannels )
	{
	case 1:
		uChannelMask = AK_SPEAKER_SETUP_1_0_CENTER;
		break;
	case 2:
		uChannelMask = AK_SPEAKER_SETUP_2_0;
		break;
	case 3:
		uChannelMask = AK_SPEAKER_SETUP_2_1;
		break;
	case 4:
		uChannelMask = AK_SPEAKER_SETUP_4_0;
		break;
	case 5:
		uChannelMask = AK_SPEAKER_SETUP_5_0;
		break;
	case 6:
		uChannelMask = AK_SPEAKER_SETUP_5_1;
		break;
	case 7:
		uChannelMask = AK_SPEAKER_SETUP_7;
		break;
	case 8:
		uChannelMask = AK_SPEAKER_SETUP_7POINT1;
		break;
	}

	return uChannelMask;
}

/// Converts a channel it to a channel index (in Wwise pipeline ordering - LFE at the end), given a channel mask in_uChannelMask.
/// \return Channel index.
static inline AkUInt8 ChannelBitToIndex(AkChannelMask in_uChannelBit, AkChannelMask in_uChannelMask)
{
#ifdef AKASSERT
	AKASSERT(ChannelMaskToNumChannels(in_uChannelBit) == 1);
#endif
	if (in_uChannelBit == AK_SPEAKER_LOW_FREQUENCY)
		return ChannelMaskToNumChannels(in_uChannelMask) - 1;
	return ChannelMaskToNumChannels(in_uChannelMask & ((in_uChannelBit & ~AK_SPEAKER_LOW_FREQUENCY) - 1));
}

/// Returns true when the LFE channel is present in a given channel configuration.
/// \return True if the LFE channel is present.
AkForceInline bool HasLFE(AkChannelMask in_uChannelMask)
{
	return (in_uChannelMask & AK_SPEAKER_LOW_FREQUENCY) > 0;
}

/// Returns true when the center channel is present in a given channel configuration.
/// Note that mono configurations have one channel which is arbitrary set to AK_SPEAKER_FRONT_CENTER,
/// so HasCenter() returns true for mono signals.
/// \return True if the center channel is present.
AkForceInline bool HasCenter(AkChannelMask in_uChannelMask)
{
	// All supported non-mono configurations have an AK_SPEAKER_FRONT_LEFT.
	return (in_uChannelMask & AK_SPEAKER_FRONT_CENTER) > 0;
}

/// Returns the number of angle values required to represent the given channel configuration.
/// Use this function with supported 2D standard channel configurations only.
/// \sa AK::SoundEngine::SetSpeakerAngles().
AkForceInline AkUInt32 GetNumberOfAnglesForConfig(AkChannelMask in_uChannelMask)
{
#ifdef AKASSERT
	AKASSERT((in_uChannelMask & ~AK_SPEAKER_SETUP_DEFAULT_PLANE) == 0);
#endif

	// LFE is irrelevant.
	in_uChannelMask &= ~AK_SPEAKER_LOW_FREQUENCY;
	// Center speaker is always in the center and thus does not require an angle.
	in_uChannelMask &= ~AK_SPEAKER_FRONT_CENTER;
	// We should have complete pairs at this point, unless there is a speaker at 180 degrees, 
	// in which case we need one more angle to specify it.
#ifdef AKASSERT
	AKASSERT((in_uChannelMask & AK_SPEAKER_BACK_CENTER) || ((ChannelMaskToNumChannels(in_uChannelMask) % 2) == 0));
#endif
	return ChannelMaskToNumChannels(in_uChannelMask) >> 1;
}

/// Channel ordering type. 
enum AkChannelOrdering
{
	ChannelOrdering_Standard,	// L-R-C-LFE-RL-RR-RC-SL-SR-HL-HR-HC-HRL-HRR-HRC-T
	ChannelOrdering_RunTime		// L-R-C-RL-RR-RC-SL-SR-HL-HR-HC-HRL-HRR-HRC-T-LFE
};

/// Returns true if standard configuration represented by channel mask has surround
/// channels, either defined as side or back channels.
AkForceInline bool HasSurroundChannels( AkChannelMask in_uChannelMask )
{
	return ( in_uChannelMask & AK_SPEAKER_BACK_LEFT || in_uChannelMask & AK_SPEAKER_SIDE_LEFT );
}

/// Returns true if standard configuration represented by channel mask has strictly one
/// pair of surround channels, either defined as side or back channels. 7.1 has two pairs
/// of surround channels and would thus return false.
AkForceInline bool HasStrictlyOnePairOfSurroundChannels( AkChannelMask in_uChannelMask )
{
	return ( ( ( in_uChannelMask & AK_SPEAKER_BACK_LEFT ) != 0 ) ^ ( ( in_uChannelMask & AK_SPEAKER_SIDE_LEFT ) != 0 ) );
}

/// Returns true if standard configuration represented by channel mask has two
/// pair of surround channels, that is, side and back channels. 7.1 has two pairs
/// of surround channels and would thus return true, whereas 5.1 would return false.
AkForceInline bool HasSideAndRearChannels( AkChannelMask in_uChannelMask )
{
	return ( in_uChannelMask & AK_SPEAKER_BACK_LEFT && in_uChannelMask & AK_SPEAKER_SIDE_LEFT );
}

/// Returns true if standard configuration represented by channel mask has at least one "height" channel (above the plane).
AkForceInline bool HasHeightChannels(AkChannelMask in_uChannelMask)
{
	return (in_uChannelMask & ~AK_SPEAKER_SETUP_DEFAULT_PLANE) > 0;
}

/// Takes a channel mask and swap back channels with side channels if there is just
/// one pair of surround channels.
AkForceInline AkChannelMask BackToSideChannels( AkChannelMask in_uChannelMask )
{
	if ( HasStrictlyOnePairOfSurroundChannels( in_uChannelMask ) )
	{
		in_uChannelMask &= ~( AK_SPEAKER_BACK_LEFT | AK_SPEAKER_BACK_RIGHT );	// remove rears
		in_uChannelMask |= ( AK_SPEAKER_SIDE_LEFT | AK_SPEAKER_SIDE_RIGHT );	// add sides
	}
	return in_uChannelMask;
}

/// Convert channel indices as they are ordered in standard (WAV) or Wwise sound engine (WEM) wave files 
/// (which follow channel mask bit values, except that the LFE is at the end in the case of WEMs) 
/// into display indices. Desired display order is L-R-C-SL-SR-RL-RR-HL-HR-HC-HRL-HRR-HRC-T-LFE. Note that 4-5.x configurations 
/// may define back or side channels. Either way they are "Surround" channels and are assigned to "SL, SR" names.
static inline unsigned int StdChannelIndexToDisplayIndex( AkChannelOrdering in_eOrdering, unsigned int in_uChannelMask, unsigned int in_uChannelIdx )
{
	if ( in_eOrdering == ChannelOrdering_Standard )
	{
		unsigned int uNumChannelsFront = ChannelMaskToNumChannels( in_uChannelMask & AK_SPEAKER_SETUP_FRONT );
		if ( ( in_uChannelMask & AK_SPEAKER_LOW_FREQUENCY )
			&& ( in_uChannelIdx == uNumChannelsFront ) )
		{
			// Lfe. Return penultimate channel.
			in_uChannelIdx = ChannelMaskToNumChannels( in_uChannelMask ) - 1;
		}
		else if ( in_uChannelIdx >= uNumChannelsFront )	// strictly greater than uNumChannelsFront (lfe index) if lfe is present, greater or equal otherwise.
		{
			// Back channel. Return index or index-1 if there is an LFE (uLfeOffset==1).
			unsigned int uLfeOffset = ( in_uChannelMask & AK_SPEAKER_LOW_FREQUENCY ) ? 1 : 0;

			// 6-7.x: Need to swap back and sides.
			if ( HasSideAndRearChannels( in_uChannelMask ) )
			{
				unsigned int uRearIdx = uNumChannelsFront + uLfeOffset;
				unsigned int uSideIdx = uRearIdx + 2;
				unsigned int uAfterSideIdx = uSideIdx + 2;
				if ( in_uChannelIdx < uAfterSideIdx )
				{				
					if ( in_uChannelIdx >= uSideIdx )
						in_uChannelIdx -= 2;	// input is side, swap it with back.
					else
						in_uChannelIdx += 2;	// input is back, swap it with side.
				}
			}
			in_uChannelIdx -= uLfeOffset;	// compensate for LFE if it was skipped above.
		}
	}
	else
	{
		// 6-7.x: Need to swap back and sides.
		if ( HasSideAndRearChannels( in_uChannelMask ) )
		{
			unsigned int uRearIdx = ChannelMaskToNumChannels( in_uChannelMask & AK_SPEAKER_SETUP_FRONT );
			unsigned int uMaxIdx = uRearIdx + 4;	// Side and rear channels.

			if ( in_uChannelIdx >= uRearIdx  
				&& in_uChannelIdx < uMaxIdx )
			{
				// Surround channel (not LFE).
				unsigned int uSideIdx = uRearIdx + 2;
				if ( in_uChannelIdx >= uSideIdx )
					in_uChannelIdx -= 2;	// input is side, swap it with back.
				else
					in_uChannelIdx += 2;	// input is back, swap it with side.
			}
		}
	}

	return in_uChannelIdx;
}

} // namespace AK

/// Channel configuration type. 
enum AkChannelConfigType
{
	AK_ChannelConfigType_Anonymous = 0x0,	///< Channel mask == 0 and channels are anonymous.
	AK_ChannelConfigType_Standard = 0x1,	///< Channels must be identified with standard defines in AkSpeakerConfigs.	
	AK_ChannelConfigType_Ambisonic = 0x2	///< Ambisonics. Channel mask == 0 and channels follow standard ambisonic order.
};

/// Defines a channel configuration.
/// Examples:
/// \code
/// AkChannelConfig cfg;
/// 
/// // Create a stereo configuration.
/// cfg.SetStandard(AK_SPEAKER_SETUP_STEREO);
///
/// // Create a 7.1.4 configuration (7.1 plus 4 height channels).
/// cfg.SetStandard(AK_SPEAKER_SETUP_AURO_11POINT1_740);
/// // or
/// cfg.SetStandard(AK_SPEAKER_SETUP_DOLBY_7_1_4);
///
/// // Create a 3rd order ambisonic configuration.
/// cfg.SetAmbisonic(16);	// pass in the number of spherical harmonics, (N+1)^2, where N is the ambisonics order.
///
/// // Invalidate (usually means "As Parent")
/// cfg.Clear();
/// \endcode
struct AkChannelConfig
{
	// Channel config: 
	// - uChannelMask is a bit field, whose channel identifiers depend on AkChannelConfigType (up to 20). Channel bits are defined in AkSpeakerConfig.h.
	// - eConfigType is a code that completes the identification of channels by uChannelMask.
	// - uNumChannels is the number of channels, identified (deduced from channel mask) or anonymous (set directly). 
	AkUInt32	uNumChannels : 8;	///< Number of channels.
	AkUInt32	eConfigType : 4;	///< Channel config type (AkChannelConfigType).
	AkUInt32	uChannelMask : 20;///< Channel mask (configuration). 

	/// Constructor. Clears / sets the channel config in "invalid" state (IsValid() returns false).
	AkForceInline AkChannelConfig()
		: uNumChannels(0)
		, eConfigType(0)
		, uChannelMask(0)
	{
	}

	/// Constructor. Sets number of channels, and config type according to whether channel mask is defined or not. If defined, it must be consistent with the number of channels.
	AkForceInline AkChannelConfig(AkUInt32 in_uNumChannels, AkUInt32 in_uChannelMask)
	{
		// Input arguments should be consistent.
		SetStandardOrAnonymous(in_uNumChannels, in_uChannelMask);
	}

	/// Operator != with a 32-bit word.
	AkForceInline bool operator!=(AkUInt32 in_uBitField)
	{
		return (*((AkUInt32*)this) != in_uBitField);
	}

	/// Clear the channel config. Becomes "invalid" (IsValid() returns false).
	AkForceInline void Clear()
	{
		uNumChannels = 0;
		eConfigType = 0;
		uChannelMask = 0;
	}

	/// Set channel config as a standard configuration specified with given channel mask.
	AkForceInline void SetStandard(AkUInt32 in_uChannelMask)
	{
		uNumChannels = AK::ChannelMaskToNumChannels(in_uChannelMask);
		eConfigType = AK_ChannelConfigType_Standard;
		uChannelMask = in_uChannelMask;
	}

	/// Set channel config as either a standard or an anonymous configuration, specified with both a given channel mask (0 if anonymous) and a number of channels (which must match the channel mask if standard).
	AkForceInline void SetStandardOrAnonymous(AkUInt32 in_uNumChannels, AkUInt32 in_uChannelMask)
	{
#ifdef AKASSERT
		AKASSERT(in_uChannelMask == 0 || in_uNumChannels == AK::ChannelMaskToNumChannels(in_uChannelMask));
#endif
		uNumChannels = in_uNumChannels;
		eConfigType = (in_uChannelMask) ? AK_ChannelConfigType_Standard : AK_ChannelConfigType_Anonymous;
		uChannelMask = in_uChannelMask;
	}

	/// Set channel config as an anonymous configuration specified with given number of channels.
	AkForceInline void SetAnonymous(AkUInt32 in_uNumChannels)
	{
		uNumChannels = in_uNumChannels;
		eConfigType = AK_ChannelConfigType_Anonymous;
		uChannelMask = 0;
	}

	/// Set channel config as an ambisonic configuration specified with given number of channels.
	AkForceInline void SetAmbisonic(AkUInt32 in_uNumChannels)
	{
		uNumChannels = in_uNumChannels;
		eConfigType = AK_ChannelConfigType_Ambisonic;
		uChannelMask = 0;
	}

	/// Returns true if valid, false otherwise (as when it is constructed, or invalidated using Clear()).
	AkForceInline bool IsValid() const
	{
		return uNumChannels != 0;
	}

	/// Serialize channel config into a 32-bit word.
	AkForceInline AkUInt32 Serialize() const
	{
		return uNumChannels | (eConfigType << 8) | (uChannelMask << 12);
	}

	/// Deserialize channel config from a 32-bit word.
	AkForceInline void Deserialize(AkUInt32 in_uChannelConfig)
	{
		uNumChannels = in_uChannelConfig & 0x000000ff;
		eConfigType = (in_uChannelConfig >> 8) & 0x0000000f;
		uChannelMask = (in_uChannelConfig >> 12) & 0x000fffff;
	}

	/// Returns a new config based on 'this' with no LFE.
	AkForceInline AkChannelConfig RemoveLFE() const
	{
		AkChannelConfig newConfig = *this;
#ifdef AK_LFECENTER
		AkUInt32 uNewChannelMask = newConfig.uChannelMask & ~AK_SPEAKER_LOW_FREQUENCY;
		AkUInt32 uNumLFEChannel = (newConfig.uChannelMask - uNewChannelMask) >> 3; // 0 or 1
#ifdef AKASSERT
		AKASSERT(uNumLFEChannel == 0 || uNumLFEChannel == 1);
#endif
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
		AkUInt32 uNumCenterChannel = (newConfig.uChannelMask - uNewChannelMask) >> 2;	// 0 or 1.
#ifdef AKASSERT
		AKASSERT(uNumCenterChannel == 0 || uNumCenterChannel == 1);
#endif
		newConfig.uNumChannels -= uNumCenterChannel;
		newConfig.uChannelMask = uNewChannelMask;
#endif
		return newConfig;
	}

	/// Operator ==
	AkForceInline bool operator==(const AkChannelConfig & in_other) const
	{
		return uNumChannels == in_other.uNumChannels
			&& eConfigType == in_other.eConfigType
			&& uChannelMask == in_other.uChannelMask;
	}

	/// Operator !=
	AkForceInline bool operator!=(const AkChannelConfig & in_other) const
	{
		return uNumChannels != in_other.uNumChannels
			|| eConfigType != in_other.eConfigType
			|| uChannelMask != in_other.uChannelMask;
	}

	/// Checks if the channel configuration is supported by the source pipeline.
	/// \return The interleaved type
	AkForceInline bool IsChannelConfigSupported() const
	{
#ifdef AK_71AUDIO
		return true;
#else
		if (eConfigType == AK_ChannelConfigType_Standard)
		{
			switch (uChannelMask)
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
				return true;
			}
		}
		return false;
#endif
	}

	/// Query if LFE channel is present.
	/// \return True when LFE channel is present
	AkForceInline bool HasLFE() const
	{
#ifdef AK_LFECENTER
		return AK::HasLFE(uChannelMask);
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
		return AK::HasCenter(uChannelMask);
#else
		return false;
#endif
	}
};

#endif //_AK_SPEAKERCONFIG_H_
