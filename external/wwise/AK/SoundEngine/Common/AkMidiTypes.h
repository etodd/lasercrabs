//////////////////////////////////////////////////////////////////////
//
// Copyright (c) 2006 Audiokinetic Inc. / All Rights Reserved
//
//////////////////////////////////////////////////////////////////////

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

// Macros to convert 32-bit MIDI event to 8-bit components
#define AK_MIDI_EVENT_GET_TYPE( in_dwEvent ) (AkUInt8)((in_dwEvent >> 0) & 0xf0)
#define AK_MIDI_EVENT_GET_CHANNEL( in_dwEvent ) (AkUInt8)((in_dwEvent >> 0) & 0x0f)
#define AK_MIDI_EVENT_GET_PARAM1( in_dwEvent ) (AkUInt8)(in_dwEvent >> 8)
#define AK_MIDI_EVENT_GET_PARAM2( in_dwEvent ) (AkUInt8)(in_dwEvent >> 16)

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



struct AkMidiNoteChannelPair
{
	AkMidiNoteChannelPair(): note(AK_INVALID_MIDI_NOTE), channel(AK_INVALID_MIDI_CHANNEL) {}

	bool operator == ( const AkMidiNoteChannelPair& in_rhs ) const { return note == in_rhs.note && channel == in_rhs.channel; }
	bool operator < ( const AkMidiNoteChannelPair& in_rhs ) const { return channel < in_rhs.channel || (channel == in_rhs.channel && note < in_rhs.note ); }
	bool operator > ( const AkMidiNoteChannelPair& in_rhs ) const { return channel > in_rhs.channel || (channel == in_rhs.channel && note > in_rhs.note ); }

	bool IsChannelValid() const	{ return channel != AK_INVALID_MIDI_CHANNEL; }
	bool IsNoteValid() const 	{ return note != AK_INVALID_MIDI_NOTE; }

	AkMidiNoteNo		note;
	AkMidiChannelNo		channel;
};

struct AkMidiEvent
{
	inline AkMidiEvent()
		: byType( AK_MIDI_EVENT_TYPE_INVALID )
		, byChan( AK_INVALID_MIDI_CHANNEL )
	{}

	inline bool IsValid() const { return byType != AK_MIDI_EVENT_TYPE_INVALID; }
	inline void MakeInvalid()	{ *this = AkMidiEvent(); }

	inline bool IsTypeOk() const { return ( (byType & 0x80) == 0x80 ) && ( (byType & 0xf0) != 0xf0 ); }

	inline bool IsSameChannel ( const AkMidiEvent& in_other ) const 
	{
		return byChan == in_other.byChan; 
	}

	inline bool IsNoteEvent () const
	{
		return byType == AK_MIDI_EVENT_TYPE_NOTE_ON ||
			byType == AK_MIDI_EVENT_TYPE_NOTE_OFF ||
			byType == AK_MIDI_EVENT_TYPE_NOTE_AFTERTOUCH;
	}

	inline bool IsCcEvent() const
	{
		return byType == AK_MIDI_EVENT_TYPE_CONTROLLER;
	}

	inline bool IsSameCc( const AkMidiEvent& in_other ) const
	{ 
		return IsCcEvent() && in_other.IsCcEvent() && 
			Cc.byCc == in_other.Cc.byCc; 
	}

	inline bool IsSameNote ( const AkMidiEvent& in_other ) const
	{ 
		return IsNoteEvent() && in_other.IsNoteEvent() && 
			NoteOnOff.byNote == in_other.NoteOnOff.byNote; 
	}

	inline bool IsSameChannelAndNote ( const AkMidiEvent& in_other ) const
	{
		return IsSameChannel( in_other ) && IsSameNote( in_other );
	}

	inline bool IsNoteOn() const
	{
		return ( byType == AK_MIDI_EVENT_TYPE_NOTE_ON && NoteOnOff.byVelocity > 0 );
	}

	inline bool IsNoteOff() const
	{
		return byType == AK_MIDI_EVENT_TYPE_NOTE_OFF || 
			(byType == AK_MIDI_EVENT_TYPE_NOTE_ON && NoteOnOff.byVelocity == 0 );
	}

	inline bool IsNoteOnOff() const
	{
		return IsNoteOn() || IsNoteOff();
	}

	bool IsPitchBend() const
	{
		return byType == AK_MIDI_EVENT_TYPE_PITCH_BEND;
	}

	inline AkMidiNoteChannelPair GetNoteAndChannel() const
	{
		AkMidiNoteChannelPair noteAndCh;
		noteAndCh.channel = byChan;
		noteAndCh.note =  IsNoteEvent() ? NoteOnOff.byNote : AK_INVALID_MIDI_NOTE;
		return noteAndCh;
	}

	inline void MakeSustainPedalOff( AkUInt32 in_uChan )
	{
		byType = AK_MIDI_EVENT_TYPE_CONTROLLER;
		byChan = (AkUInt8)in_uChan;
		Cc.byCc = AK_MIDI_CC_HOLD_PEDAL;
		Cc.byValue = 0;
	}

	inline void MakeNoteOn()
	{
		byType = AK_MIDI_EVENT_TYPE_NOTE_ON;
		NoteOnOff.byVelocity = 0;
	}

	inline void MakeNoteOff()
	{
		byType = AK_MIDI_EVENT_TYPE_NOTE_OFF;
		NoteOnOff.byVelocity = 0;
	}

	inline void SetNoteNumber( const AkMidiNoteNo in_note )
	{
		AKASSERT( IsNoteOnOff() );
		NoteOnOff.byNote = in_note;
	}

	inline AkMidiNoteNo GetNoteNumber() const
	{
		AKASSERT( IsNoteOnOff() );
		return (AkMidiNoteNo)NoteOnOff.byNote;
	}

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

#endif  //_AK_MIDI_TYPES_H_
