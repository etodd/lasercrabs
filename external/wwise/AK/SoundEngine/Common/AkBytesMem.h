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

// AkBytesMem.h

/// \file 
/// IReadBytes / IWriteBytes implementation on a growing memory buffer. This
/// version uses the AK::MemoryMgr memory pools.

#pragma once

#include <AK/IBytes.h>
#include <AK/SoundEngine/Common/AkSoundEngineExport.h>

namespace AK
{
	class ReadBytesMem
		: public AK::IReadBytes
	{
	public:

		AKSOUNDENGINE_API ReadBytesMem();
		AKSOUNDENGINE_API ReadBytesMem(
			const void * in_pBytes, 
			AkInt32 in_cBytes
			);
		AKSOUNDENGINE_API virtual ~ReadBytesMem();

		// IReadBytes implementation

		AKSOUNDENGINE_API virtual bool ReadBytes(
			void * in_pData, 
			AkInt32 in_cBytes, 
			AkInt32 & out_cRead
			);

		// Public methods

		AKSOUNDENGINE_API void Attach(
			const void * in_pBytes, 
			AkInt32 in_cBytes
			);

	private:
		AkInt32   m_cBytes;
		const AkUInt8 * m_pBytes;

		AkInt32   m_cPos;
	};

	class WriteBytesMem
		: public AK::IWriteBuffer
	{
	public:
	
		AKSOUNDENGINE_API WriteBytesMem();
		AKSOUNDENGINE_API virtual ~WriteBytesMem();
	
		// IWriteBytes implementation

		AKSOUNDENGINE_API virtual bool WriteBytes(
			const void * in_pData, 
			AkInt32 in_cBytes, 
			AkInt32& out_cWritten);
	
		// IWriteBuffer implementation

		AKSOUNDENGINE_API virtual bool Reserve(
			AkInt32 in_cBytes
			);

		AKSOUNDENGINE_API virtual AkInt32 Count() const;

		AKSOUNDENGINE_API virtual void SetCount(
			AkInt32 in_cBytes
			);

		AKSOUNDENGINE_API virtual AkUInt8 * Bytes() const;

		AKSOUNDENGINE_API virtual AkUInt8 * Detach();
	
		AKSOUNDENGINE_API virtual void Clear();
	
		// Public methods

		AKSOUNDENGINE_API void SetMemPool( AkMemPoolId in_pool );
	
	private:
		AkInt32		m_cBytes;
		AkUInt8 *	m_pBytes;
	
		AkInt32	m_cPos;
	
		AkMemPoolId m_pool;
	};
}
