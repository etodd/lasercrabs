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

  Version: v2016.2.4  Build: 6098
  Copyright (c) 2006-2017 Audiokinetic Inc.
*******************************************************************************/

#ifndef _AKIMAGESOURCE_H_
#define _AKIMAGESOURCE_H_

#include <AK/Tools/Common/AkVectors.h>
#include <AK/Tools/Common/AkListBare.h>

#ifdef _WIN64
#define AK_RUNTIME 1
#endif

const int MIRROR_COUNT				= 6;

class AkPortalLink
{
public:
	AkPortalLink():
		m_BoxID(-1),
		m_FaceID(-1)
	{
	}

	AkPortalLink(
		AkInt32						in_BoxID,
		AkInt32						in_FaceID,
		AkBoundingBox				in_Opening):
		m_BoxID(in_BoxID),
		m_FaceID(in_FaceID),
		m_Opening(in_Opening)
	{
	}

	AkInt32							m_BoxID;
	AkInt32							m_FaceID;
	AkBoundingBox					m_Opening;
};

class AkImageSourceReflector
{
public:
	AkImageSourceReflector();
	AkImageSourceReflector(
		Ak3DVector					in_p1,
		Ak3DVector					in_p2,
		Ak3DVector					in_p4,
		AkUInt32					in_uMaterialID
		);

	~AkImageSourceReflector();

	AKRESULT Set(
		Ak3DVector					in_p1,
		Ak3DVector					in_p2,
		Ak3DVector					in_p4,
		AkUInt32					in_uMaterialID
		);

	AKRESULT Reflect(
		Ak3DVector					in_p,
		Ak3DVector&					out_p
		) const;

	bool UpdatePortal(
		const AkPlane&				in_PlaneB,
		AkBoundingBox&				out_BoundingBox
		);

	AkUInt32 GetMaterialID() const
	{
		return m_uMaterialID;
	}

	const AkPlane GetPlane() const
	{
		return m_Plane;
	}

private:

	AkUInt32						m_uMaterialID;

	AkMatrix4x4						m_pReflectionMatrix;

	AkPlane							m_Plane;
};

enum AkImageSourceBoxState
{
	AkImageSourceBox_Unused,
	AkImageSourceBox_Used
};

struct AkAcousticPortalParams
{
	AkAcousticPortalParams() :
		fDensity(0.f),
		fDiffusion(0.f)
	{}

	AkReal32						fDensity;
	AkReal32						fDiffusion;
};

class AkSynthSpaceAllocManager
{
public:
	static AkSynthSpaceAllocManager* Create(AK::IAkPluginMemAlloc * in_pAllocator)
	{
		AKASSERT(!m_pInstance);
		AKASSERT(in_pAllocator);
		m_pInstance = (AkSynthSpaceAllocManager *)AK_PLUGIN_NEW(in_pAllocator, AkSynthSpaceAllocManager(in_pAllocator));
		return m_pInstance;
	}

	static AkSynthSpaceAllocManager* Instance()
	{
		return m_pInstance;
	}

	static void Destroy(AK::IAkPluginMemAlloc * in_pAllocator = NULL)
	{
		if (m_pInstance)
		{
			AKASSERT(in_pAllocator);
			if (in_pAllocator)
			{
				AK_PLUGIN_DELETE(in_pAllocator, m_pInstance);
				m_pInstance = NULL;
			}
		}
	}

	AK::IAkPluginMemAlloc * GetAllocator(void)
	{
		return m_pAllocator;
	};

private:
	AkSynthSpaceAllocManager(AK::IAkPluginMemAlloc * in_pAllocator):
		m_pAllocator(in_pAllocator)
	{};

	static AkSynthSpaceAllocManager* m_pInstance;
	AK::IAkPluginMemAlloc * m_pAllocator;
};

class AkAcousticPortal
{
public:

	struct AkSynthSpaceAllocator
	{
		static /*AkForceInline*/ void * Alloc(size_t in_uSize)
		{
			return AkSynthSpaceAllocManager::Instance()->GetAllocator()->Malloc(in_uSize);
		}

		static /*AkForceInline*/ void Free(void * in_pAddress)
		{
			AkSynthSpaceAllocManager::Instance()->GetAllocator()->Free(in_pAddress);
		}
	};
	typedef AkArray< void*, void*, AkSynthSpaceAllocator, 2 > BufferArray;

	typedef AkArray<AkPortalLink, AkPortalLink&, AkSynthSpaceAllocator, 10> AkPortalLinkDatabase;
	typedef AkArray<AkBoundingBox, const AkBoundingBox&, AkSynthSpaceAllocator, 10> AkBoundingBoxList;

	AkAcousticPortal() :
		m_ID(0),
		m_State(AkImageSourceBox_Unused)
	{
	}
	~AkAcousticPortal(){};

	void Init(
		const Ak3DVector &		in_center,
		const Ak3DVector &		in_size,
		const Ak3DVector &		in_Front,
		const Ak3DVector &		in_Up,
		const AkAcousticPortalParams & in_LateParams);

	void Release();

	void AddOrUpdateLink(AkPortalLink& in_link)
	{
		bool found = false;
		for (AkPortalLinkDatabase::Iterator it = m_LinkDB.Begin(); it != m_LinkDB.End() && !found; ++it)
		{
			AkPortalLink& link = static_cast<AkPortalLink&>(*it);
			if (link.m_BoxID == in_link.m_BoxID &&
				link.m_FaceID == in_link.m_FaceID)
			{
				link.m_Opening.Update(in_link.m_Opening.m_Min);
				link.m_Opening.Update(in_link.m_Opening.m_Max);
				found = true;
			}
		}

		if (!found)
		{
			m_LinkDB.AddLast(in_link);
		}
	}

	void FindIntersections(AkUInt32 in_ListenerBox, AkUInt32 in_EmmiterBox, AkBoundingBoxList& out_portalList)
	{
		bool emmiterBoxFound = false;

		for (AkPortalLinkDatabase::Iterator it = m_LinkDB.Begin(); it != m_LinkDB.End() && emmiterBoxFound != true; ++it)
		{
			const AkPortalLink& link = static_cast<AkPortalLink>(*it);
			if (link.m_BoxID == (AkInt32)in_EmmiterBox)
			{
				emmiterBoxFound = true;
			}
		}

		if (emmiterBoxFound)
		{
			for (AkPortalLinkDatabase::Iterator it = m_LinkDB.Begin(); it != m_LinkDB.End() && emmiterBoxFound != false; ++it)
			{
				const AkPortalLink& link = static_cast<AkPortalLink>(*it);
				if (link.m_BoxID == (AkInt32)in_ListenerBox)
				{
					out_portalList.AddLast(link.m_Opening);
				}
			}
		}
	}

	bool IsUsed() const { return m_State == AkImageSourceBox_Used; }
	bool IsAvailable() const { return m_State != AkImageSourceBox_Used; }
	AkUInt32 GetID() const { return m_ID; }

	const AkBox	& GetBox() const { return m_Box; }

private:
	AkInt32							m_ID;
	AkBox							m_Box;
	AkImageSourceBoxState			m_State;
	AkAcousticPortalParams			m_PortalParams;

	AkPortalLinkDatabase			m_LinkDB;

};

class AkImageSourceBox
{
public:
	AkImageSourceBox() :
		m_ID(0),
		m_State(AkImageSourceBox_Unused)
	{
	}
	~AkImageSourceBox(){};

	void Init(
		const Ak3DVector &		in_center,
		const Ak3DVector &		in_size,
		const Ak3DVector &		in_Front,
		const Ak3DVector &		in_Up,
		const AkUInt32			in_DiffuseReverberator,
		AkInt32					in_wall_mask,
		AkUInt32*				in_AcousticTextures,
		AkInt32					in_startIndex);

	void Release();

	void ModePosition(
		Ak3DVector				in_p,
		Ak3DVector&				out_p1,
		AkUInt16				in_i1
		) const;

	void ModePosition(
		Ak3DVector				in_p,
		Ak3DVector				in_mode,
		Ak3DVector&				out_p1,
		Ak3DVector&				out_p2,
		AkUInt16				in_i1,
		AkUInt16				in_i2) const;

	void Reflect(
		AkInt32					in_index,
		Ak3DVector				in_p,
		Ak3DVector&				out_p) const;

	AkUInt32 GetMaterialID(
		AkInt32					in_index) const;

	bool FindIntersections(AkAcousticPortal* in_portal);

	void SetCrossSectionsPerFace(AkAcousticPortal* in_portal);

	bool SeparatingAxisExists(
		const Ak3DVector& in_Axis,
		const AkBox& in_Box);

	AkUInt32 GetDiffuseReverberator() const;

	bool IsUsed() const { return m_State == AkImageSourceBox_Used; }
	bool IsAvailable() const { return m_State != AkImageSourceBox_Used; }
	AkUInt32 GetID() const { return m_ID; }

	const AkBox	GetBox() const { return m_Box; };

private:
	AkInt32							m_ID;
	AkBox							m_Box;
	AkImageSourceReflector			m_ImageSourceReflector[MIRROR_COUNT];
	AkImageSourceBoxState			m_State;
	AkUInt32						m_DiffuseReverberator;
};

#endif // _AKIMAGESOURCE_H_