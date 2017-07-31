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

#ifndef _AK_WWISE_MOTION_PLUGIN
#define _AK_WWISE_MOTION_PLUGIN

#include <AK\Wwise\AudioPlugin.h>
#include <AK\SoundEngine\Common\AkTypes.h>

struct AkAudioFormat;

namespace AK
{
	namespace Wwise
	{
		/// MotionDataType
		enum MotionDataType 
		{
			TypePositionSamples = 1, 
			TypeSpeedSamples
		};

		/// Motion device bus plugin interface.  This represent a device bus in Wwise.
		/// \warning The functions in this interface are not thread-safe, unless stated otherwise.
		/// \warning For internal use only.
		class IMotionBusPlugin : public DefaultAudioPluginImplementation
		{
		public:
			/// Returns if the data type is compatible with the device
			/// \return True if supported, False otherwise
			virtual bool	IsTypeCompatible(MotionDataType in_eType) const = 0;
		};

		///Interfaces for motion device source plugins.  Currently used only to discriminate with audio plugins.
		class IMotionSourcePlugin : public DefaultAudioPluginImplementation
		{
		};		
	}
}

#endif