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

Version: v2017.2.1  Build: 6524
Copyright (c) 2006-2018 Audiokinetic Inc.
*******************************************************************************/

#ifndef _AK_REFLECT_GAMEDATA_H_
#define _AK_REFLECT_GAMEDATA_H_

#include <AK/SoundEngine/Common/AkTypes.h>

#define AK_MAX_NUM_TEXTURE 4

/// Data used to describe one image source in Wwise Reflect.
struct AkImageSourceName
{
	AkImageSourceName()
		: uNumChar(0)
		, pName(NULL)
	{
	}

	void SetName(const char * in_pName)
	{
		pName = in_pName;
		if (pName)
		{
			uNumChar = (AkUInt32)strlen(in_pName);
		}
		else
		{
			uNumChar = 0;
		}
	}

	AkUInt32 uNumChar;							///< Number of characters in image source name.
	const char * pName;							///< Optional image source name. Appears in Wwise Reflect's editor when profiling.
};

struct AkImageSourceTexture
{
	AkImageSourceTexture()
		: uNumTexture(1)
	{
		arTextureID[0] = AK_INVALID_UNIQUE_ID;
	}

	AkUInt32 uNumTexture;						///< Number of valid textures in the texture array.
	AkUniqueID arTextureID[AK_MAX_NUM_TEXTURE];	///< Unique IDs of the Acoustics Texture ShareSets used to filter this image source.
};

struct AkImageSourceParams
{
	AkImageSourceParams()
		: fDistanceScalingFactor(1.f)
		, fLevel(1.f)
	{
		sourcePosition.X = 0.f;
		sourcePosition.Y = 0.f;
		sourcePosition.Z = 0.f;
	}

	AkImageSourceParams(AkVector in_sourcePosition, AkReal32 in_fDistanceScalingFactor, AkReal32 in_fLevel)
		: sourcePosition(in_sourcePosition)
		, fDistanceScalingFactor(in_fDistanceScalingFactor)
		, fLevel(in_fLevel)
	{
	}

	AkVector sourcePosition;					///< Image source position, relative to the world.
	AkReal32 fDistanceScalingFactor;			///< Image source distance scaling. This number effectively scales the sourcePosition vector with respect to the listener and, consequently, scales distance and preserves orientation.
	AkReal32 fLevel;							///< Game-controlled level for this source, linear.
};

struct AkReflectImageSource
{
	AkReflectImageSource()
		: uID((AkImageSourceID)-1)
		, params()
		, texture()
		, name()
	{}

	AkReflectImageSource(AkImageSourceID in_uID, AkVector in_sourcePosition, AkReal32 in_fDistanceScalingFactor, AkReal32 in_fLevel)
		: uID(in_uID) 
		, params(in_sourcePosition, in_fDistanceScalingFactor, in_fLevel)
		, texture()
		, name()
	{
	}

	void SetName(const char * in_pName)
	{
		name.SetName(in_pName);
	}

	AkImageSourceID uID;						///< Image source ID (for matching delay lines across frames)
	AkImageSourceParams params;
	AkImageSourceTexture texture;
	AkImageSourceName name;
};

/// Data structure sent by the game to an instance of the Wwise Reflect plug-in.
struct AkReflectGameData
{
	AkGameObjectID listenerID;					///< ID of the listener used to compute spatialization and distance evaluation from within the targeted Reflect plug-in instance. It needs to be one of the listeners that are listening to the game object associated with the targeted plug-in instance. See AK::SoundEngine::SetListeners and AK::SoundEngine::SetGameObjectAuxSendValues.
	AkUInt32 uNumImageSources;					///< Number of image sources passed in the variable array, below.
	AkReflectImageSource arSources[1];			///< Variable array of image sources. You should allocate storage for the structure by calling AkReflectGameData::GetSize() with the desired number of sources.
	
	/// Default constructor.
	AkReflectGameData()
		: listenerID( AK_INVALID_GAME_OBJECT )
		, uNumImageSources(0)
	{}

	/// Helper function for computing the size required to allocate the AkReflectGameData structure.
	static AkUInt32 GetSize(AkUInt32 in_uNumSources)
	{
		return (in_uNumSources > 0) ? sizeof(AkReflectGameData) + (in_uNumSources - 1) * sizeof(AkReflectImageSource) : sizeof(AkReflectGameData);
	}
};
#endif // _AK_REFLECT_GAMEDATA_H_