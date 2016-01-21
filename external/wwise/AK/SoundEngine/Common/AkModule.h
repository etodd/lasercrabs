//////////////////////////////////////////////////////////////////////
//
// Copyright (c) 2006 Audiokinetic Inc. / All Rights Reserved
//
//////////////////////////////////////////////////////////////////////

/// \file 
/// Audiokinetic's definitions and factory of overridable Memory Manager module.

#pragma once

#include <AK/SoundEngine/Common/AkTypes.h>

/// \name Audiokinetic Memory Manager's implementation-specific definitions.
//@{
/// Memory Manager's initialization settings.
/// \sa AK::MemoryMgr
struct AkMemSettings
{
    AkUInt32 uMaxNumPools;              ///< Maximum number of memory pools.
};
//@}

namespace AK
{
    /// \name Audiokinetic implementation-specific modules factories.
    //@{
	namespace MemoryMgr
	{
	    /// Memory Manager initialization.
	    /// \sa AK::MemoryMgr
		AK_EXTERNFUNC( AKRESULT, Init )(
			AkMemSettings * in_pSettings        ///< Memory manager initialization settings.
			);
	}
    //@}
}

