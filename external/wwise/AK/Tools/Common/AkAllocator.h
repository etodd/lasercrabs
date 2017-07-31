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

// AkAllocator.h

#ifndef _AK_TOOLS_COMMON_AKALLOCATOR_H
#define _AK_TOOLS_COMMON_AKALLOCATOR_H

#include <AK/SoundEngine/Common/IAkPluginMemAlloc.h>

namespace AK
{
	// Audiokinetic Wwise namespace
	namespace Wwise
	{
		class Mallocator
			: public AK::IAkPluginMemAlloc
		{
		public:
			virtual void* Malloc(size_t in_uSize)
			{
				return malloc(in_uSize);
			}

			virtual void Free(void* in_pMemAddress)
			{
				free(in_pMemAddress);
			}
		};

		template<typename T>
		class SafeAllocator
		{
		public:
			SafeAllocator(AK::IAkPluginMemAlloc* in_pAlloc)
				: m_pAlloc(in_pAlloc),
				  m_pPtr(nullptr)
			{
			}

			~SafeAllocator()
			{
				if (m_pPtr)
				{
					m_pAlloc->Free(m_pPtr);
				}
			}

			T* operator->() { return m_pPtr; }
			T& operator*() { return *m_pPtr; }

			explicit operator bool() const { return m_pPtr != nullptr; }
			operator T*&() { return m_pPtr; }

		private:
			AK::IAkPluginMemAlloc* m_pAlloc;
			T* m_pPtr;
		};
	}
}

#endif // _AK_TOOLS_COMMON_AKALLOCATOR_H
