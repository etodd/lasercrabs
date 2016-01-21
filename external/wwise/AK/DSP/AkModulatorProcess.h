/***********************************************************************
The content of this file includes source code for the sound engine
portion of the AUDIOKINETIC Wwise Technology and constitutes "Level
Two Source Code" as defined in the Source Code Addendum attached
with this file.  Any use of the Level Two Source Code shall be
subject to the terms and conditions outlined in the Source Code
Addendum and the End User License Agreement for Wwise(R).

Version: <VERSION>  Build: <BUILDNUMBER>
Copyright (c) <COPYRIGHTYEAR> Audiokinetic Inc.
***********************************************************************/

#ifndef _AK_MODULATOR_PROCESS_H_
#define _AK_MODULATOR_PROCESS_H_

#include "AkModulatorParams.h"
#ifdef AK_PS3
	#include <AK/Plugin/PluginServices/PS3/MultiCoreServices.h>
#endif

class CAkEnvelopeProcess
{
public:
	inline static void Process(const AkModulatorParams& in_Params, AkUInt32 in_uSamples, AkModulatorOutput& out_pOutput, AkReal32* out_pOutputBuffer);

#ifdef AK_PS3
	static AK::MultiCoreServices::BinData JobBin;
#endif

private:
	template< typename tPolicy >
	inline static void _Process(const AkModulatorParams& in_Params, AkUInt32 in_uFrameSize, AkModulatorOutput& out_pOutput, AkReal32* out_pOutputBuffer );
};

class CAkLFOProcess
{
public:
	inline static void Process(const AkModulatorParams& in_Params, AkUInt32 in_uSamples, AkModulatorOutput& out_pOutput, AkReal32* out_pOutputBuffer);

#ifdef AK_PS3
	static AK::MultiCoreServices::BinData JobBin;
#endif

};

inline void CAkLFOProcess::Process(const AkModulatorParams& in_Params, const AkUInt32 in_uFrameSize, AkModulatorOutput& out_pOutput, AkReal32* out_pOutputBuffer)
{
	const AkLFOParams& params = (const AkLFOParams&) in_Params;

	ValidateState(params.m_eState);
	out_pOutput.m_eNextState = AkModulatorState_Invalid;

	AkLFOOutput& output = static_cast<AkLFOOutput&>(out_pOutput);
	DSP::LFO::State state;
	params.m_lfo.GetState(state);
	output.m_lfo.PutState(state);

	AkReal32 fDepthBegin = params.m_fDepth;
	AkUInt32 uStartFrame = in_Params.m_uElapsedFrames - in_uFrameSize;
	if (uStartFrame < params.m_uAttack)
		fDepthBegin = params.m_fDepth * ((AkReal32)uStartFrame)/((AkReal32)params.m_uAttack);

	DSP::ModulatorLFOOutputPolicy outputPolicy;
	if (in_Params.m_uBufferSize > 0)
	{
		AkReal32 fDepthEnd = params.m_fDepth;
		if (in_Params.m_uElapsedFrames < params.m_uAttack)
			fDepthEnd = params.m_fDepth * ((AkReal32)in_Params.m_uElapsedFrames)/((AkReal32)params.m_uAttack);

		output.m_lfo.ProduceBuffer(out_pOutputBuffer, in_uFrameSize, fDepthEnd, fDepthBegin, params.m_DspParams.fPWM, outputPolicy );
		out_pOutput.m_fOutput = out_pOutputBuffer[in_uFrameSize-1];
		out_pOutput.m_fPeak = fDepthEnd;
	}
	else
	{
		out_pOutput.m_pBuffer = NULL;
		output.m_lfo.SkipFrames(in_uFrameSize-1);
		output.m_lfo.ProduceBuffer(&out_pOutput.m_fOutput, 1, fDepthBegin, fDepthBegin, params.m_DspParams.fPWM, outputPolicy );
		out_pOutput.m_fPeak = out_pOutput.m_fOutput;
	}
}

class CommonOutputPolicy
{
public:
	inline static AkReal32 CalcDelta( AkReal32 in_fLevel, AkUInt32 in_uFrames )
	{
		AkReal32 fFrames = (AkReal32)in_uFrames;
		return AK_FSEL( -fFrames, 0.f, (in_fLevel/fFrames) );
	}
};

class BufferOutputPolicy: public CommonOutputPolicy
{
public:
	inline static AkUInt32 RoundNumFrames(AkUInt32 in_uNumFrames)
	{
		return ((in_uNumFrames + 2) & ~3);
	}

	inline static AkReal32 Ramp( AkReal32*& io_pBuffer, AkUInt32 in_uNumFrames, AkReal32 fStart, AkReal32 fDelta, AkReal32& io_fPeak )
	{	
		if (in_uNumFrames != 0)
		{
			AKASSERT((AkUIntPtr)io_pBuffer % 4 == 0);
			AKASSERT(in_uNumFrames % 4 == 0);

			AK_ALIGN_SIMD( AkReal32 f4Ramp[4] ) = {0.f, fDelta, 2.f*fDelta, 3.f*fDelta};

			AKSIMD_V4F32* pStart = (AKSIMD_V4F32*)io_pBuffer;
			AKSIMD_V4F32* pEnd = (AKSIMD_V4F32*)(io_pBuffer + in_uNumFrames);

			AKSIMD_V4F32 s4Start = AKSIMD_LOAD1_V4F32(fStart);

			AkReal32 f4Delta = fDelta*4.f;
			AKSIMD_V4F32 s4Delta = AKSIMD_LOAD1_V4F32(f4Delta);

			*pStart = AKSIMD_ADD_V4F32( AKSIMD_LOAD_V4F32( f4Ramp ), s4Start); 
			AKSIMD_V4F32* pPrev = pStart;

			pStart++;

			while(pStart < pEnd)
			{
				*pStart = AKSIMD_ADD_V4F32(s4Delta, *pPrev);

				pStart++;
				pPrev++;
			}

			union{ AkReal32 f4Value[4]; AKSIMD_V4F32 vValue; };
			vValue = AKSIMD_ADD_V4F32(s4Delta, *pPrev);
			//AKASSERT( f4Value[0] >= 0.0f && f4Value[0] <= 1.0f );

			io_pBuffer += in_uNumFrames;
			AkReal32 fOutput = AkClamp( f4Value[0], 0.0f, 1.0f);
			io_fPeak = AkMax( AkMax( fOutput, fStart ), io_fPeak);
			return fOutput;
		}

		return fStart;
	}

	inline static AkReal32 Set( AkReal32*& io_pBuffer, AkUInt32 in_uNumFrames, AkReal32 fVal, AkReal32& io_fPeak )
	{
		AKASSERT((AkUIntPtr)io_pBuffer % 4 == 0);
		AKASSERT(in_uNumFrames % 4 == 0);

		const AKSIMD_V4F32 vValue = AKSIMD_LOAD1_V4F32( fVal );
		for(AKSIMD_V4F32 *pStart = (AKSIMD_V4F32*)io_pBuffer, *pEnd = (AKSIMD_V4F32*)(io_pBuffer+in_uNumFrames); 
			pStart < pEnd; pStart++)
		{
			*pStart = vValue;
		}

		io_fPeak = AkMax( fVal, io_fPeak);
		io_pBuffer += in_uNumFrames;
		return fVal;
	}
};


class SingleOutputPolicy: public CommonOutputPolicy
{
public:
	inline static AkUInt32 RoundNumFrames(AkUInt32 in_uNumFrames)
	{
		return in_uNumFrames;
	}

	inline static AkReal32 Ramp( AkReal32*, AkUInt32 in_uFrames, AkReal32 fStart, AkReal32 fDelta, AkReal32& io_fPeak )
	{	
		AkReal32 fOutput = fStart + fDelta * (AkReal32)(in_uFrames);
		io_fPeak = AkMax( AkMax(fOutput, fStart), io_fPeak);
		return fOutput;
	}

	inline static AkReal32 Set( AkReal32*, AkUInt32, AkReal32 fVal, AkReal32& io_fPeak )
	{
		io_fPeak = AkMax( fVal, io_fPeak);
		return fVal;
	}
};

template< typename tPolicy >
inline void CAkEnvelopeProcess::_Process(const AkModulatorParams& in_Params, const AkUInt32 in_uFrameSize, AkModulatorOutput& out_pOutput, AkReal32* out_pOutputBuffer )
{
	const AkEnvelopeParams& params = (const AkEnvelopeParams&) in_Params;
	AKASSERT(params.m_fStartValue >= -0.0f && params.m_fStartValue <= 1.0f);
	AKASSERT(params.m_fCurve >= -0.0f && params.m_fCurve <= 1.0f);

	ValidateState(params.m_eState);
	out_pOutput.m_eNextState = AkModulatorState_Invalid;
	out_pOutput.m_fPeak = params.m_fStartValue;
	
	const AkUInt32 uStartOffset = (params.m_uStartOffsetFrames & ~(0x3));
	const AkUInt32 uHalfAttack = tPolicy::RoundNumFrames(params.m_uAttack / 2);
	const AkUInt32 uDecay = tPolicy::RoundNumFrames(params.m_uDecay);
	const AkUInt32 uRelease = tPolicy::RoundNumFrames(params.m_uRelease);
	AkUInt32 uReleaseFrame = uStartOffset + tPolicy::RoundNumFrames(params.m_uReleaseFrame);

	AkUInt32 uCurrentFrame = in_Params.m_uElapsedFrames - in_uFrameSize;

	if (params.m_fStartValue > 0.f)
	{
		AkUInt32 uEffectiveStartFrame = 0;
		if (params.m_fStartValue < params.m_fCurve && params.m_fCurve > 0.f)
		{
			uEffectiveStartFrame = (AkUInt32)((AkReal32)uHalfAttack * (params.m_fStartValue/params.m_fCurve));
		}
		else 
		{
			uEffectiveStartFrame = uHalfAttack + (AkUInt32)((AkReal32)uHalfAttack * ((params.m_fStartValue - params.m_fCurve)/( 1.0f - params.m_fCurve )));
		}

		uEffectiveStartFrame = tPolicy::RoundNumFrames(uEffectiveStartFrame);
		uCurrentFrame += uEffectiveStartFrame;
		uReleaseFrame += uEffectiveStartFrame;
	}

	AkReal32 fValue = uCurrentFrame > 0 ? params.m_fPreviousOutput : params.m_fStartValue;

	AkReal32* pBuffer = out_pOutputBuffer;
	AkUInt32 uRemainingFrames = in_uFrameSize;

	// START OFFSET
	if ( uCurrentFrame < uStartOffset )
	{
		//Zero out the offset part of the first buffer
		AkUInt32 uStartOffsetFrames = AkMin( uStartOffset-uCurrentFrame, in_uFrameSize );
		tPolicy::Set( pBuffer, uStartOffsetFrames, 0.f, out_pOutput.m_fPeak );
		uCurrentFrame += uStartOffsetFrames;
		uRemainingFrames -= uStartOffsetFrames;
	}

	//ATTACK -- part 1
	AkUInt32 uAttackP1End = AkMin( uStartOffset + uHalfAttack, uReleaseFrame );
	if (uCurrentFrame < uAttackP1End)
	{
		AkReal32 fAttackP1Delta = tPolicy::CalcDelta( params.m_fCurve , uHalfAttack );
		AkUInt32 uNumAttackP1Frames = AkMin(uRemainingFrames, uAttackP1End-uCurrentFrame);
		fValue = tPolicy::Ramp( pBuffer, uNumAttackP1Frames, fValue, fAttackP1Delta, out_pOutput.m_fPeak);
		uCurrentFrame += uNumAttackP1Frames;
		uRemainingFrames -= uNumAttackP1Frames;
	}

	//ATTACK -- part 2
	AkUInt32 uAttackP2End = AkMin( uStartOffset + 2*uHalfAttack, uReleaseFrame );
	if (uCurrentFrame < uAttackP2End)
	{
		AkReal32 fAttackP2Delta = tPolicy::CalcDelta( (1.0f - params.m_fCurve), uHalfAttack );
		AkUInt32 uNumAttackP2Frames = AkMin(uRemainingFrames, uAttackP2End-uCurrentFrame);
		fValue = tPolicy::Ramp( pBuffer, uNumAttackP2Frames, fValue, fAttackP2Delta, out_pOutput.m_fPeak);
		uCurrentFrame += uNumAttackP2Frames;
		uRemainingFrames -= uNumAttackP2Frames;
	}

	//DECAY -- pre release
	AkUInt32 uDecayEndNormal = (uAttackP2End + uDecay);
	AkUInt32 uDecayEndPreRelease = AkMin( uDecayEndNormal, uReleaseFrame );
	if (uCurrentFrame < uDecayEndPreRelease)
	{
		AkReal32 fDecayDelta = tPolicy::CalcDelta( params.m_fSustain - 1.0f, uDecay );

		AkUInt32 uNumDecayFrames = AkMin(uRemainingFrames, uDecayEndPreRelease-uCurrentFrame);
		fValue = tPolicy::Ramp( pBuffer, uNumDecayFrames, fValue, fDecayDelta, out_pOutput.m_fPeak);
		uCurrentFrame += uNumDecayFrames;
		uRemainingFrames -= uNumDecayFrames;
	}

	AkReal32 fLevelAtDecayEnd = params.m_fSustain;

	//DECAY -- post release
	AkUInt32 uPostReleaseDecayDur = 0;
	if ( uDecayEndNormal > uReleaseFrame ) 
	{
		AkReal32 fLvlAfterAttack = 1.f;
		if ( uHalfAttack > 0 )
		{
			// See how far the attack progressed, before we got the note-off.
			AkReal32 fAttackP1Delta = tPolicy::CalcDelta(params.m_fCurve, uHalfAttack);
			AkReal32 fAttackP2Delta = tPolicy::CalcDelta(1.0f - params.m_fCurve, uHalfAttack);

			fLvlAfterAttack = (fAttackP1Delta)*((AkReal32)(uAttackP1End - uStartOffset)) + (fAttackP2Delta)*((AkReal32)(uAttackP2End - uAttackP1End));
		}

		AkReal32 fLvlAboveSusAtAttackEnd = fLvlAfterAttack - params.m_fSustain;

		// See if we got to the point in the attack where we are above the sustain level.
		if (fLvlAboveSusAtAttackEnd > 0.0f)
		{
			AkReal32 fDecayDelta = tPolicy::CalcDelta( params.m_fSustain - 1.0f, uDecay );

			AkUInt32 uReleaseFromAttack = fDecayDelta != 0.f ? ((AkUInt32)(-fLvlAboveSusAtAttackEnd / fDecayDelta)) : 0;// release time taking into account a possibly truncated attack

			// the divide the release duration by two and use a steeper slope to get to the release slope faster.
			uPostReleaseDecayDur = tPolicy::RoundNumFrames( AkMin( uRelease, AkMin( (uDecayEndNormal-uReleaseFrame), uReleaseFromAttack )) / 2 ); 

			AkUInt32 uDecayEndPostRelease = uDecayEndPreRelease + uPostReleaseDecayDur;

			AkReal32 fReleaseDelta = tPolicy::CalcDelta( -(params.m_fSustain), uRelease );
			AkReal32 fLvlBelowSusAtDecayEnd = -1.f*(fReleaseDelta)*((AkReal32)(uPostReleaseDecayDur));

			//Update this for release slope calc
			fLevelAtDecayEnd = params.m_fSustain + fLvlBelowSusAtDecayEnd;

			if (uCurrentFrame < uDecayEndPostRelease )
			{
				AkReal32 fLvlAboveSusAtRelease = fLvlAboveSusAtAttackEnd + (fDecayDelta)*((AkReal32)uDecayEndPreRelease - (AkReal32)uAttackP2End);
				AkReal32 fDecayPostReleaseDelta = tPolicy::CalcDelta( -(fLvlAboveSusAtRelease + fLvlBelowSusAtDecayEnd), uPostReleaseDecayDur );

				AkUInt32 uNumDecayFrames = AkMin(uRemainingFrames, uDecayEndPostRelease-uCurrentFrame);
				fValue = tPolicy::Ramp( pBuffer, uNumDecayFrames, fValue, fDecayPostReleaseDelta, out_pOutput.m_fPeak);

				uCurrentFrame += uNumDecayFrames;
				uRemainingFrames -= uNumDecayFrames;
			}

		}
		else
		{
			fLevelAtDecayEnd = params.m_fSustain + fLvlAboveSusAtAttackEnd;
		}
	}

	//SUSTAIN
	AkUInt32 uNumSustainFrames = AkMax( (AkInt32)0, AkMin((AkInt32)uRemainingFrames, (AkInt32)uReleaseFrame-(AkInt32)uCurrentFrame) );
	if( uNumSustainFrames > 0 )
	{
		fValue = tPolicy::Set( pBuffer, uNumSustainFrames, params.m_fSustain, out_pOutput.m_fPeak  );
		uCurrentFrame += uNumSustainFrames;
		uRemainingFrames -= uNumSustainFrames;
	}

	//RELEASE
	AkUInt32 uActualReleaseDur = uRelease - uPostReleaseDecayDur;
	AkUInt32 uReleaseEnd = uReleaseFrame + uActualReleaseDur;
	if (uCurrentFrame < uReleaseEnd)
	{
		AkReal32 fReleaseDelta = tPolicy::CalcDelta( -(fLevelAtDecayEnd), uActualReleaseDur );
		AkUInt32 uNumReleaseFrames = AkMin(uRemainingFrames, uReleaseEnd-uCurrentFrame);
		fValue = tPolicy::Ramp( pBuffer, uNumReleaseFrames, fValue, fReleaseDelta, out_pOutput.m_fPeak);
		uCurrentFrame += uNumReleaseFrames;
		uRemainingFrames -= uNumReleaseFrames;
	}

	//END
	if (uRemainingFrames > 0 && uCurrentFrame >= uReleaseEnd)
	{
		fValue = tPolicy::Set(pBuffer, uRemainingFrames, 0.f, out_pOutput.m_fPeak );
		out_pOutput.m_eNextState = AkModulatorState_Finished;
	}

	out_pOutput.m_fOutput = fValue;

}

inline void CAkEnvelopeProcess::Process(const AkModulatorParams& in_Params, const AkUInt32 in_uFrameSize, AkModulatorOutput& out_pOutput, AkReal32* out_pOutputBuffer)
{
	if ( in_Params.m_uBufferSize == 0 )
		_Process<SingleOutputPolicy>(in_Params, in_uFrameSize, out_pOutput, out_pOutputBuffer );
	else
		_Process<BufferOutputPolicy>(in_Params, in_uFrameSize, out_pOutput, out_pOutputBuffer );
}

#endif
