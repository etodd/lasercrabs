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

// AkMidiTypes.h

/// \file 
/// Data type definitions.

#ifndef _AK_MIDI_TYPES_H_
#define _AK_MIDI_TYPES_H_

// Include standard types
#include <AK/SoundEngine/Common/AkTypes.h>

//-----------------------------------------------------------------------------
// Types.
//-----------------------------------------------------------------------------

typedef AkUInt8			AkMidiChannelNo;			///< MIDI channel number, usually 0-15.  
typedef AkUInt8			AkMidiNoteNo;				///< MIDI note number.  

//-----------------------------------------------------------------------------
// Constants.
//-----------------------------------------------------------------------------

// Invalid values
static const AkMidiChannelNo			AK_INVALID_MIDI_CHANNEL				=  (AkMidiChannelNo)-1;		///< Not a valid midi channel
static const AkMidiNoteNo				AK_INVALID_MIDI_NOTE				=  (AkUInt8)-1;				///< Not a valid midi note

// List of event types
#define AK_MIDI_EVENT_TYPE_INVALID					0x00
#define AK_MIDI_EVENT_TYPE_NOTE_OFF					0x80
#define AK_MIDI_EVENT_TYPE_NOTE_ON					0x90
#define AK_MIDI_EVENT_TYPE_NOTE_AFTERTOUCH			0xa0
#define AK_MIDI_EVENT_TYPE_CONTROLLER				0xb0
#define AK_MIDI_EVENT_TYPE_PROGRAM_CHANGE			0xc0
#define AK_MIDI_EVENT_TYPE_CHANNEL_AFTERTOUCH		0xd0
#define AK_MIDI_EVENT_TYPE_PITCH_BEND				0xe0
#define AK_MIDI_EVENT_TYPE_SYSEX					0xf0
#define AK_MIDI_EVENT_TYPE_ESCAPE					0xf7
#define AK_MIDI_EVENT_TYPE_META						0xff

// List of Continuous Controller (cc) values
#define AK_MIDI_CC_BANK_SELECT_COARSE		0
#define AK_MIDI_CC_MOD_WHEEL_COARSE			1
#define AK_MIDI_CC_BREATH_CTRL_COARSE		2
#define AK_MIDI_CC_CTRL_3_COARSE			3
#define AK_MIDI_CC_FOOT_PEDAL_COARSE		4
#define AK_MIDI_CC_PORTAMENTO_COARSE		5
#define AK_MIDI_CC_DATA_ENTRY_COARSE		6
#define AK_MIDI_CC_VOLUME_COARSE			7
#define AK_MIDI_CC_BALANCE_COARSE			8
#define AK_MIDI_CC_CTRL_9_COARSE			9
#define AK_MIDI_CC_PAN_POSITION_COARSE		10
#define AK_MIDI_CC_EXPRESSION_COARSE		11
#define AK_MIDI_CC_EFFECT_CTRL_1_COARSE		12
#define AK_MIDI_CC_EFFECT_CTRL_2_COARSE		13
#define AK_MIDI_CC_CTRL_14_COARSE			14
#define AK_MIDI_CC_CTRL_15_COARSE			15
#define AK_MIDI_CC_GEN_SLIDER_1				16
#define AK_MIDI_CC_GEN_SLIDER_2				17
#define AK_MIDI_CC_GEN_SLIDER_3				18
#define AK_MIDI_CC_GEN_SLIDER_4				19
#define AK_MIDI_CC_CTRL_20_COARSE			20
#define AK_MIDI_CC_CTRL_21_COARSE			21
#define AK_MIDI_CC_CTRL_22_COARSE			22
#define AK_MIDI_CC_CTRL_23_COARSE			23
#define AK_MIDI_CC_CTRL_24_COARSE			24
#define AK_MIDI_CC_CTRL_25_COARSE			25
#define AK_MIDI_CC_CTRL_26_COARSE			26
#define AK_MIDI_CC_CTRL_27_COARSE			27
#define AK_MIDI_CC_CTRL_28_COARSE			28
#define AK_MIDI_CC_CTRL_29_COARSE			29
#define AK_MIDI_CC_CTRL_30_COARSE			30
#define AK_MIDI_CC_CTRL_31_COARSE			31
#define AK_MIDI_CC_BANK_SELECT_FINE			32
#define AK_MIDI_CC_MOD_WHEEL_FINE			33
#define AK_MIDI_CC_BREATH_CTRL_FINE			34
#define AK_MIDI_CC_CTRL_3_FINE				35
#define AK_MIDI_CC_FOOT_PEDAL_FINE			36
#define AK_MIDI_CC_PORTAMENTO_FINE			37
#define AK_MIDI_CC_DATA_ENTRY_FINE			38
#define AK_MIDI_CC_VOLUME_FINE				39
#define AK_MIDI_CC_BALANCE_FINE				40
#define AK_MIDI_CC_CTRL_9_FINE				41
#define AK_MIDI_CC_PAN_POSITION_FINE		42
#define AK_MIDI_CC_EXPRESSION_FINE			43
#define AK_MIDI_CC_EFFECT_CTRL_1_FINE		44
#define AK_MIDI_CC_EFFECT_CTRL_2_FINE		45
#define AK_MIDI_CC_CTRL_14_FINE				46
#define AK_MIDI_CC_CTRL_15_FINE				47

#define AK_MIDI_CC_CTRL_20_FINE				52
#define AK_MIDI_CC_CTRL_21_FINE				53
#define AK_MIDI_CC_CTRL_22_FINE				54
#define AK_MIDI_CC_CTRL_23_FINE				55
#define AK_MIDI_CC_CTRL_24_FINE				56
#define AK_MIDI_CC_CTRL_25_FINE				57
#define AK_MIDI_CC_CTRL_26_FINE				58
#define AK_MIDI_CC_CTRL_27_FINE				59
#define AK_MIDI_CC_CTRL_28_FINE				60
#define AK_MIDI_CC_CTRL_29_FINE				61
#define AK_MIDI_CC_CTRL_30_FINE				62
#define AK_MIDI_CC_CTRL_31_FINE				63

#define AK_MIDI_CC_HOLD_PEDAL				64
#define AK_MIDI_CC_PORTAMENTO_ON_OFF		65
#define AK_MIDI_CC_SUSTENUTO_PEDAL			66
#define AK_MIDI_CC_SOFT_PEDAL				67
#define AK_MIDI_CC_LEGATO_PEDAL				68
#define AK_MIDI_CC_HOLD_PEDAL_2				69

#define AK_MIDI_CC_SOUND_VARIATION			70
#define AK_MIDI_CC_SOUND_TIMBRE				71
#define AK_MIDI_CC_SOUND_RELEASE_TIME		72
#define AK_MIDI_CC_SOUND_ATTACK_TIME		73
#define AK_MIDI_CC_SOUND_BRIGHTNESS			74
#define AK_MIDI_CC_SOUND_CTRL_6				75
#define AK_MIDI_CC_SOUND_CTRL_7				76
#define AK_MIDI_CC_SOUND_CTRL_8				77
#define AK_MIDI_CC_SOUND_CTRL_9				78
#define AK_MIDI_CC_SOUND_CTRL_10			79

#define AK_MIDI_CC_GENERAL_BUTTON_1			80
#define AK_MIDI_CC_GENERAL_BUTTON_2			81
#define AK_MIDI_CC_GENERAL_BUTTON_3			82
#define AK_MIDI_CC_GENERAL_BUTTON_4			83

#define AK_MIDI_CC_REVERB_LEVEL				91
#define AK_MIDI_CC_TREMOLO_LEVEL			92
#define AK_MIDI_CC_CHORUS_LEVEL				93
#define AK_MIDI_CC_CELESTE_LEVEL			94
#define AK_MIDI_CC_PHASER_LEVEL				95
#define AK_MIDI_CC_DATA_BUTTON_P1			96
#define AK_MIDI_CC_DATA_BUTTON_M1			97

#define AK_MIDI_CC_NON_REGISTER_COARSE		98
#define AK_MIDI_CC_NON_REGISTER_FINE		99

#define AK_MIDI_CC_ALL_SOUND_OFF			120
#define AK_MIDI_CC_ALL_CONTROLLERS_OFF		121
#define AK_MIDI_CC_LOCAL_KEYBOARD			122
#define AK_MIDI_CC_ALL_NOTES_OFF			123
#define AK_MIDI_CC_OMNI_MODE_OFF			124
#define AK_MIDI_CC_OMNI_MODE_ON				125
#define AK_MIDI_CC_OMNI_MONOPHONIC_ON		126
#define AK_MIDI_CC_OMNI_POLYPHONIC_ON		127

//-----------------------------------------------------------------------------
// Structs.
//-----------------------------------------------------------------------------

struct AkMIDIEvent
{
	AkUInt8			byType;		// (Ak_MIDI_EVENT_TYPE_)
	AkMidiChannelNo	byChan;

	struct tGen
	{
		AkUInt8		byParam1;
		AkUInt8		byParam2;
	};
	struct tNoteOnOff
	{
		AkMidiNoteNo	byNote;
		AkUInt8			byVelocity;
	};
	struct tCc
	{
		AkUInt8		byCc;
		AkUInt8		byValue;
	};
	struct tPitchBend
	{
		AkUInt8		byValueLsb;
		AkUInt8		byValueMsb;
	};
	struct tNoteAftertouch
	{
		AkUInt8		byNote;
		AkUInt8		byValue;
	};
	struct tChanAftertouch
	{
		AkUInt8		byValue;
	};
	struct tProgramChange
	{
		AkUInt8		byProgramNum;
	};

	union
	{
		tGen Gen;
		tCc Cc;
		tNoteOnOff NoteOnOff;
		tPitchBend PitchBend;
		tNoteAftertouch NoteAftertouch;
		tChanAftertouch ChanAftertouch;
		tProgramChange ProgramChange;
	};
};

struct AkMIDIPost : public AkMIDIEvent
{
	AkUInt32 uOffset; // Frame offset for MIDI event post
};

#endif  //_AK_MIDI_TYPES_H_
