//////////////////////////////////////////////////////////////////////
//
// Copyright (c) 2006 Audiokinetic Inc. / All Rights Reserved
//
//////////////////////////////////////////////////////////////////////

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
