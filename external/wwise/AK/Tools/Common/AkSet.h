/***********************************************************************
The content of this file includes source code for the sound engine
portion of the AUDIOKINETIC Wwise Technology and constitutes "Level
Two Source Code" as defined in the Source Code Addendum attached
with this file.  Any use of the Level Two Source Code shall be
subject to the terms and conditions outlined in the Source Code
Addendum and the End User License Agreement for Wwise(R).

Version: v2017.1.0  Build: 6302
Copyright (c) 2006-2017 Audiokinetic Inc.
***********************************************************************/

//////////////////////////////////////////////////////////////////////
//
// AkSet.h
//
//////////////////////////////////////////////////////////////////////
#ifndef _AKSET_H_
#define _AKSET_H_

#include <AK/Tools/Common/AkKeyArray.h>

// AkSetType
//	- An optional set type specifier which is passed into some set operations.  If it is not included, SetType_Inclusion is assumed.
//
enum AkSetType
{
	SetType_Inclusion,	// <- An AkSet object with type SetType_Inclusion is a set where each element in the array 
						//		represents an element in the set.  An empty array represents the empty set.
	SetType_Exclusion   // <- An AkSet object with type SetType_Exclusion is an 'inverted' set, where each element in the array 
						//		represents and element NOT in the set. An empty array represents the universal set.  
};

template<typename T>
struct AkSetGetKey{ static AkForceInline T& Get(T& in_item){ return in_item; } };

// AkSet
//
//	Set container type, implemented as a sorted array of unique items
//
template< typename T, class U_POOL, AkUInt32 uGrowBy = 1 >
class AkSet : public AkSortedKeyArray < T, T, U_POOL, AkSetGetKey<T>, uGrowBy >
{
public:
	bool Contains(T in_item) const { return AkSortedKeyArray < T, T, U_POOL, AkSetGetKey<T>, uGrowBy >::Exists(in_item) != NULL; }
};

// AkDisjoint
//	- Returns true if the intersection of A and B is the empty set.
//
template< typename T, class U_POOL, AkUInt32 uGrowBy >
static bool AkDisjoint(const AkSet<T, U_POOL, uGrowBy>& in_A, const AkSet<T, U_POOL, uGrowBy>& in_B)
{
	typename AkSet<T, U_POOL, uGrowBy>::Iterator itA = in_A.Begin();
	typename AkSet<T, U_POOL, uGrowBy>::Iterator itB = in_B.Begin();
	while (itA != in_A.End() && itB != in_B.End())
	{
		if (*itA == *itB)
			return false;
		else if (*itA < *itB)
			++itA;
		else
			++itB;
	}
	return true;
}

// AkIntersect
//  - Return true if the intersection of A and B is not the empty set.
//
template< typename T, class U_POOL, AkUInt32 uGrowBy >
static bool AkIntersect(const AkSet<T, U_POOL, uGrowBy>& in_A, const AkSet<T, U_POOL, uGrowBy>& in_B)
{
	return !AkDisjoint(in_A, in_B);
}

// AkIsSubset
//	- Return true if in_A is a subset of in_B
//
template< typename T, class U_POOL, AkUInt32 uGrowBy >
static bool AkIsSubset(const AkSet<T, U_POOL, uGrowBy>& in_A, const AkSet<T, U_POOL, uGrowBy>& in_B)
{
	typename AkSet<T, U_POOL, uGrowBy>::Iterator itA = in_A.Begin();
	typename AkSet<T, U_POOL, uGrowBy>::Iterator itB = in_B.Begin();
	while (itA != in_A.End() && itB != in_B.End())
	{
		if (*itA == *itB)
		{
			++itA; ++itB;
		}
		else if (*itA < *itB)
		{
			return false;//an element of A is not in B
		}
		else
			++itB;
	}
	return (itA == in_A.End());
}

// AkCountIntersection
//	- Helper function to count the number of elements that are in both in_A and in_B.
//
template< typename T, class U_POOL, AkUInt32 uGrowBy >
static AkUInt32 AkCountIntersection(const AkSet<T, U_POOL, uGrowBy>& in_A, const AkSet<T, U_POOL, uGrowBy>& in_B)
{
	AkUInt32 uSize = 0;
	typename AkSet<T, U_POOL, uGrowBy>::Iterator itA = in_A.Begin();
	typename AkSet<T, U_POOL, uGrowBy>::Iterator itB = in_B.Begin();
	while (itA != in_A.End() && itB != in_B.End())
	{
		if (*itA == *itB)
		{
			++uSize; ++itA;	++itB;
		}
		else if (*itA < *itB)
		{
			++itA;
		}
		else
		{
			++itB;
		}
	}
	return uSize;
}

// AkSubtraction
//  - In-place set subtraction ( A = A - B )
//
template< typename T, class U_POOL, AkUInt32 uGrowBy >
static bool AkSubtraction(AkSet<T, U_POOL, uGrowBy>& in_A, const AkSet<T, U_POOL, uGrowBy>& in_B)
{
	typename AkSet<T, U_POOL, uGrowBy>::Iterator itAr, itAw;
	itAr = itAw = in_A.Begin();
	typename AkSet<T, U_POOL, uGrowBy>::Iterator itB = in_B.Begin();
	while (itAr != in_A.End())
	{
		if (itB == in_B.End() || *itAr < *itB)
		{
			if (itAw != itAr)
				*itAw = *itAr;

			++itAw;
			++itAr;
		}
		else if (*itAr == *itB)
		{
			++itB;
			++itAr;
		}
		else
		{
			++itB;
		}
	}
	in_A.Resize((AkUInt32)(itAw.pItem - in_A.Begin().pItem));
	return true;
}

// AkIntersection
//	- In-place set intersection ( A = A n B )
//
template< typename T, class U_POOL, AkUInt32 uGrowBy >
static bool AkIntersection(AkSet<T, U_POOL, uGrowBy>& in_A, const AkSet<T, U_POOL, uGrowBy>& in_B)
{
	typename AkSet<T, U_POOL, uGrowBy>::Iterator itAr, itAw;
	itAr = itAw = in_A.Begin();
	typename AkSet<T, U_POOL, uGrowBy>::Iterator itB = in_B.Begin();
	while (itAr != in_A.End() && itB != in_B.End())
	{
		if (*itAr == *itB)
		{
			if (itAw != itAr)
				*itAw = *itAr;

			++itAw;
			++itAr;
			++itB;
		}
		else if (*itAr < *itB)
		{
			++itAr;
		}
		else
		{
			++itB;
		}
	}
	in_A.Resize((AkUInt32)(itAw.pItem - in_A.Begin().pItem));
	return true;
}

// AkUnion
//  - Set union ( A = A U B ).  
//	NOTE: Preforms a memory allocation and may fail.
//
template< typename T, class U_POOL, AkUInt32 uGrowBy >
static bool AkUnion(AkSet<T, U_POOL, uGrowBy>& io_A, const AkSet<T, U_POOL, uGrowBy>& in_B)
{
	AkInt32 uSizeNeeded = io_A.Length() + in_B.Length() - AkCountIntersection(io_A, in_B);
	AkSet<T, U_POOL, uGrowBy> result;

	if (result.Resize(uSizeNeeded))
	{
		typename AkSet<T, U_POOL, uGrowBy>::Iterator itRes = result.Begin();
		typename AkSet<T, U_POOL, uGrowBy>::Iterator itA = io_A.Begin();
		typename AkSet<T, U_POOL, uGrowBy>::Iterator itB = in_B.Begin();

		while (itB != in_B.End() || itA != io_A.End())
		{
			if ( itB != in_B.End() && (itA == io_A.End() || *itB < *itA))
			{
				*itRes = *itB;
				++itB;
			}
			else if (itB == in_B.End() || *itA < *itB)
			{
				*itRes = *itA;
				++itA;
			}
			else //if ( *itA == *itC)
			{
				*itRes = *itA;
				++itA;
				++itB;
			}

			++itRes;
		}

		io_A.Transfer(result);
		return true;
	}

	return false;
}

typedef AkSet< AkUniqueID, ArrayPoolDefault >  AkUniqueIDSet;

// AkIntersect
//  - Return true if the intersection of in_A (a set of type in_typeA), and in_B (a set of type in_typeB) is not the empty set.
//
template< typename T, class U_POOL, AkUInt32 uGrowBy >
static inline bool AkIntersect(const AkSet<T, U_POOL, uGrowBy>& in_A, AkSetType in_typeA, const AkSet<T, U_POOL, uGrowBy>& in_B, AkSetType in_typeB)
{
	if (in_typeA == SetType_Inclusion)
	{
		if (in_typeB == SetType_Inclusion)
			return !AkDisjoint(in_A, in_B);
		else//(in_typeB == SetType_Exclusion)
			return !AkIsSubset(in_A, in_B);
	}
	else//(in_typeA == SetType_Exclusion)
	{
		if (in_typeB == SetType_Inclusion)
			return !AkIsSubset(in_B, in_A);
		else//(in_typeB == SetType_Exclusion)
			return true;//Assuming an infinite space of possible elements.
	}
}

// AkContains
//  - Return true if the element in_item is contained in in_Set, a set of type in_type.
//
template< typename T, class U_POOL, AkUInt32 uGrowBy >
static inline bool AkContains(const AkSet<T, U_POOL, uGrowBy>& in_Set, AkSetType in_type, T in_item)
{
	return	(in_type == SetType_Inclusion && in_Set.Contains(in_item)) ||
		(in_type == SetType_Exclusion && !in_Set.Contains(in_item));
}

// AkSubtraction
//	- pseudo in-place set subtraction (A = A - B) with set type specifiers.
//	NOTE: Memory may be allocated (in AkUnion) so prepare for failure.
//
template< typename T, class U_POOL, AkUInt32 uGrowBy >
static inline bool AkSubtraction(AkSet<T, U_POOL, uGrowBy>& in_A, AkSetType in_typeA, const AkSet<T, U_POOL, uGrowBy>& in_B, AkSetType in_typeB)
{
	if (in_typeA == SetType_Inclusion)
	{
		if (in_typeB == SetType_Inclusion)
			return AkSubtraction(in_A, in_B);
		else//(in_typeB == SetType_Exclusion)
			return AkIntersection(in_A, in_B);
	}
	else//(in_typeA == SetType_Exclusion)
	{
		if (in_typeB == SetType_Inclusion)
			return AkUnion(in_A, in_B);
		else//(in_typeB == SetType_Exclusion)
			return AkIntersection(in_A, in_B);
	}
}

// AkUnion
//  - Pseudo in-place set union (A = A + B)
//	NOTE: Memory may be allocated (in AkUnion) so prepare for failure.
//
template< typename T, class U_POOL, AkUInt32 uGrowBy >
static inline bool AkUnion(AkSet<T, U_POOL, uGrowBy>& io_A, AkSetType& io_typeA, const AkSet<T, U_POOL, uGrowBy>& in_B, AkSetType in_typeB)
{
	if (io_typeA == SetType_Inclusion)
	{
		if (in_typeB == SetType_Inclusion)
			return AkUnion(io_A, in_B);
		else//(in_typeB == SetType_Exclusion)
		{
			AkSet<T, U_POOL, uGrowBy> temp;
			temp.Transfer(io_A);
			if (io_A.Copy(in_B) == AK_Success)
			{
				io_typeA = SetType_Exclusion;
				AkSubtraction(io_A, temp);
				temp.Term();
				return true;
			}
			else
			{
				io_A.Transfer(temp);
				return false;
			}
		}
	}
	else//(in_typeA == SetType_Exclusion)
	{
		if (in_typeB == SetType_Inclusion)
			return AkSubtraction(io_A, in_B);
		else//(in_typeB == SetType_Exclusion)
			return AkIntersection(io_A, in_B);
	}
}

#endif

