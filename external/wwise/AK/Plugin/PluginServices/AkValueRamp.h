//////////////////////////////////////////////////////////////////////
//
// Copyright (c) 2006 Audiokinetic Inc. / All Rights Reserved
//
//////////////////////////////////////////////////////////////////////

// AkValueRamp.h

/// \file 
/// Linear interpolation services for plug-in parameters.

#ifndef _AK_VALUERAMP_H_
#define _AK_VALUERAMP_H_

#include <AK/SoundEngine/Common/AkTypes.h>
#include <AK/Tools/Common/AkAssert.h>
#include <math.h>

namespace AK
{

	/// Platform-independent parameter interpolation service for software plug-ins.
	/// \aknote
	/// Algorithm performs linear interpolation.
	/// \endaknote
	/// \sa
	/// - \ref shared_parameter_interface
	class CAkValueRamp
	{
	public:

		/// Constructor method.
		CAkValueRamp() :
		m_fStepIncrement( 0.f ),	// Step increment sign
			m_fInc( 0.f ),			// Signed increment
			m_fTarget( 0.f ),		// Target gain for ramping
			m_fCurrent( 0.f ),		// Current interpolated value
			m_uRampCount( 0 ),		// Position in interpolation ramp
			m_uRampLength( 0 )		// Total duration of interpolation ramp
		{
		}

		/// Destructor method.
		~CAkValueRamp()
		{
		}

		/// Initial parameter interpolation ramp setup.
		inline void RampSetup( 
			AkReal32 fStepIncrement,	///< Increment to add to the parameter at every Tick() call
			AkReal32 fInitVal			///< Initial ramp value
			)
		{
			AKASSERT( fStepIncrement > 0.f );
			m_fStepIncrement = fStepIncrement;
			m_fCurrent = fInitVal;
			SetTarget( fInitVal );
		}

		/// Set the ramp's target value.
		AkForceInline void SetTarget( 
			AkReal32 fTarget		///< Target ramp value
			) 
		{
			m_fTarget = fTarget;
			m_uRampCount = 0;
			AkReal32 fDiff = m_fTarget - m_fCurrent;
			m_uRampLength = static_cast<AkUInt32>( fabs(fDiff) / m_fStepIncrement );
			m_fInc = fDiff > 0 ? m_fStepIncrement : -m_fStepIncrement;
		}

		/// Process a single interpolation frame.
		/// \return The current interpolated value
		AkForceInline AkReal32 Tick()
		{
			if ( m_uRampCount >= m_uRampLength )
				m_fCurrent = m_fTarget;
			else
			{
				++m_uRampCount;
				m_fCurrent += m_fInc;
			}
			return m_fCurrent;
		}
		
		/// Retrieve the current interpolated value.
		/// \return The current interpolated value
		AkReal32 GetCurrent()						{ return m_fCurrent; }
		
		/// Set the current interpolated value.
		void SetCurrent(AkReal32 in_fCurrent)		{ m_fCurrent = in_fCurrent; }

		/// Retrieve the current interpolation frame count.
		/// \return The current interpolation frame count
		AkUInt32 GetRampCount()						{ return m_uRampCount; }

		/// Set the current interpolation frame count.
		void SetRampCount(AkUInt32 in_uRampCount)	{ m_uRampCount = in_uRampCount; }

		/// The ramp is no longer necessary; set to target
		void StopRamp()
		{
			m_fCurrent = m_fTarget;
			m_uRampCount = m_uRampLength;
		}

	private:

		AkReal32			m_fStepIncrement;		// Step increment size
		AkReal32			m_fInc;					// Signed increment
		AkReal32			m_fTarget;				// Target for interpolation ramp
		AkReal32			m_fCurrent;				// Current interpolated value
		AkUInt32			m_uRampCount;			// Position in interpolation ramp
		AkUInt32			m_uRampLength;			// Total duration of interpolation ramp
	} AK_ALIGN_DMA;
}

#endif  //_AK_VALUERAMP_H_
