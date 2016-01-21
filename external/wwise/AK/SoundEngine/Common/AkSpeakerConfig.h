///////////////////////////////////////////////////////////////////////
//
// AkSpeakerConfig.h
//
//
// Copyright 2008-2009 Audiokinetic Inc.
//
///////////////////////////////////////////////////////////////////////

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
#define AK_SPEAKER_SETUP_SURROUND		(AK_SPEAKER_SETUP_STEREO	| AK_SPEAKER_BACK_CENTER)	///< Wii surround setup channel mask

// Note. DPL2 does not really have 4 channels, but it is used by plugins to differentiate from stereo setup.
#define AK_SPEAKER_SETUP_DPL2			(AK_SPEAKER_SETUP_4)		///< Wii DPL2 setup channel mask

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

// Dolby speaker setups: in Dolby standard, [#plane].[lfe].[#height]
#define AK_SPEAKER_SETUP_DOLBY_5_0_2		(AK_SPEAKER_SETUP_5			| AK_SPEAKER_HEIGHT_FRONT_LEFT	| AK_SPEAKER_HEIGHT_FRONT_RIGHT )	///< Dolby 5.0.2 setup channel mask
#define AK_SPEAKER_SETUP_DOLBY_5_1_2		(AK_SPEAKER_SETUP_DOLBY_5_0_2	| AK_SPEAKER_LOW_FREQUENCY )									///< Dolby 5.1.2 setup channel mask
#define AK_SPEAKER_SETUP_DOLBY_7_0_2		(AK_SPEAKER_SETUP_7			| AK_SPEAKER_HEIGHT_FRONT_LEFT	| AK_SPEAKER_HEIGHT_FRONT_RIGHT )	///< Dolby 7.0.2 setup channel mask
#define AK_SPEAKER_SETUP_DOLBY_7_1_2		(AK_SPEAKER_SETUP_DOLBY_7_0_2	| AK_SPEAKER_LOW_FREQUENCY )									///< Dolby 7.1.2 setup channel mask

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
#define AK_IDX_SETUP_6_FRONTRIGHT	(1)	///< Index of fornt right channel in 6x setups (use with AkAudioBuffer::GetChannel())
#define AK_IDX_SETUP_6_REARLEFT		(2)	///< Index of rear left channel in 6.x setups (use with AkAudioBuffer::GetChannel())
#define AK_IDX_SETUP_6_REARRIGHT	(3)	///< Index of rear right channel in 6.x setups (use with AkAudioBuffer::GetChannel())
#define AK_IDX_SETUP_6_SIDELEFT		(4)	///< Index of side left channel in 6.x setups (use with AkAudioBuffer::GetChannel())
#define AK_IDX_SETUP_6_SIDERIGHT	(5)	///< Index of side right channel in 6.x setups (use with AkAudioBuffer::GetChannel())
#define AK_IDX_SETUP_6_LFE			(6)	///< Index of low-frequency channel in 6.1 setup (use with AkAudioBuffer::GetChannel())

#define AK_IDX_SETUP_7_FRONTLEFT	(0)	///< Index of front left channel in 7.x setups (use with AkAudioBuffer::GetChannel())
#define AK_IDX_SETUP_7_FRONTRIGHT	(1)	///< Index of fornt right channel in 7.x setups (use with AkAudioBuffer::GetChannel())
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
#define AK_SPEAKER_SETUP_1_0		( AK_SPEAKER_FRONT_LEFT )								//1.0 (L)
#define AK_SPEAKER_SETUP_1_1		( AK_SPEAKER_FRONT_LEFT	| AK_SPEAKER_LOW_FREQUENCY )	//1.1 (L)

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

namespace AK
{

/// Returns the number of channels of a given channel configuration.
static inline unsigned int ChannelMaskToNumChannels( AkChannelMask in_uChannelMask )
{
	unsigned int num = 0;
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

/// Channel ordering type. 
enum AkChannelOrdering
{
	ChannelOrdering_Standard,	// L-R-C-Lfe-RL-RR-RC-SL-SR-HL-HR-HC-HRL-HRR-HRC-T
	ChannelOrdering_RunTime		// L-R-C-RL-RR-RC-SL-SR-HL-HR-HC-HRL-HRR-HRC-T-Lfe
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

/// Convert channel indices as they are ordered in standard (wav) or Wwise sound engine (wem) wave files 
/// (which follow channel mask bit values, except that the Lfe is at the end in the case of wems) 
/// into display indices. Desired display order is L-R-C-SL-SR-RL-RR-HL-HR-HC-HRL-HRR-HRC-T-Lfe. Note that 4-5.x configurations 
/// may define back or side channels. Either way they are "Surround" channels and are assigned to "SL, SR" names.
static inline unsigned int ChannelIndexToDisplayIndex( AkChannelOrdering in_eOrdering, unsigned int in_uChannelMask, unsigned int in_uChannelIdx )
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

#endif //_AK_SPEAKERCONFIG_H_
