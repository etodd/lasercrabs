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

// AkListBareLight.h

#ifndef _AKLISTBARELIGHT_H
#define _AKLISTBARELIGHT_H

#include <AK/Tools/Common/AkListBare.h>

// this one lets you define the structure
// only requirement is that T must have member pNextLightItem,
// or use the template getter.
// client is responsible for allocation/deallocation of T.

// WATCH OUT !
// - remember that removeall/term can't delete the elements for you.
// - be sure to destroy elements AFTER removing them from the list, as remove will
// access members of the element.
// WARNING : Each AkListBareLight item can be used in only one AkListBareLight at 
//           once since the member pNextLightItem cannot be re-used.

/// Next item name policy.
template <class T> struct AkListBareLightNextItem
{
	/// Default policy.
	static AkForceInline T *& Get( T * in_pItem ) 
	{
		return in_pItem->pNextLightItem;
	}
};

/// Implementation of List Bare Light.
template <class T, template <class> class U_NEXTITEM = AkListBareLightNextItem > class AkListBareLight : public AkListBare< T, U_NEXTITEM, AkCountPolicyNoCount, AkLastPolicyNoLast > {};

#endif // _AKLISTBARELIGHT_H
