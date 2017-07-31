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
	AkMemSettings()
	{
		uMaxNumPools = 32;				// Default number of pools.
		uDebugFlags = 0;
	}
    AkUInt32 uMaxNumPools;              ///< Maximum number of memory pools.  32 by default, increase as needed.
	AkUInt32 uDebugFlags;				///< Debug flags from AK::MemoryMgr::DebugFlags enum.  Should be 0.  This flag is ignored when not in DEBUG.  Memory usage will be higher when this debug tool is enabled.
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
		AK_EXTERNAPIFUNC(AKRESULT, Init)(
			AkMemSettings * in_pSettings        ///< Memory manager initialization settings.
			);
	}
    //@}
}

