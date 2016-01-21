//////////////////////////////////////////////////////////////////////
//
// Copyright (c) 2006 Audiokinetic Inc. / All Rights Reserved
//
//////////////////////////////////////////////////////////////////////

#ifndef _FNVHASH_H
#define _FNVHASH_H

// http://www.isthe.com/chongo/tech/comp/fnv/

//////////////////////////////////////////////////////////////////
//
// ***************************************************************
//
// IMPORTANT: The Migration Utility contains a C# version of this
// class, to assign Short IDs to objects created during migration.
// If you modify this class, be sure to update its C# counterpart,
// ShortIDGenerator, at the same time.
//
// ***************************************************************
//
//////////////////////////////////////////////////////////////////

namespace AK
{
	struct Hash32
	{
		typedef unsigned int HashType;
		static inline unsigned int Bits() {return 32;}
		static inline HashType Prime() {return 16777619;}
		static const HashType s_offsetBasis = 2166136261U;
	};
	
	struct Hash30 : public Hash32
	{
		static inline unsigned int Bits() {return 30;}
	};
	
	struct Hash64
	{
		typedef unsigned long long HashType;
		static inline unsigned int Bits() {return 64;}
		static inline HashType Prime() {return 1099511628211ULL;}
		static const HashType s_offsetBasis = 14695981039346656037ULL;
	};

	template <class HashParams> 
	class FNVHash
	{
	public:
		inline FNVHash( typename HashParams::HashType in_uBase = HashParams::s_offsetBasis );	///< Constructor

		/// Turn the provided data into a hash value.
		/// When Wwise uses this hash with strings, it always provides lower case strings only.
		/// Call this repeatedly on the same instance to build a hash incrementally.
		inline typename HashParams::HashType Compute( const void* in_pData, unsigned int in_dataSize );
		inline typename HashParams::HashType Get() const { return m_uHash; }

		template <typename T>
		inline typename HashParams::HashType Compute(const T& in_pData) { return Compute(&in_pData, sizeof(T)); }

	private:
		typename HashParams::HashType m_uHash;
	};

	#if defined(_MSC_VER)
	#pragma warning(push)
	#pragma warning(disable:4127)
	#endif


	template <class HashParams> 
	FNVHash<HashParams>::FNVHash( typename HashParams::HashType in_uBase )
		: m_uHash( in_uBase )
	{
	}


	template <class HashParams> 
	typename HashParams::HashType FNVHash<HashParams>::Compute( const void* in_pData, unsigned int in_dataSize )
	{
		const unsigned char* pData = (const unsigned char*) in_pData;
		const unsigned char* pEnd = pData + in_dataSize;		/* beyond end of buffer */

		typename HashParams::HashType hval = m_uHash;

		// FNV-1 hash each octet in the buffer
		while( pData < pEnd ) 
		{
			hval *= HashParams::Prime(); // multiply by the 32 bit FNV magic prime mod 2^32
			hval ^= *pData++; // xor the bottom with the current octet
		}

		m_uHash = hval;

		// XOR-Fold to the required number of bits
		if( HashParams::Bits() >= sizeof(typename HashParams::HashType) * 8 )
			return hval;

		typename HashParams::HashType mask = static_cast<typename HashParams::HashType>(((typename HashParams::HashType)1 << HashParams::Bits())-1);
		return (typename HashParams::HashType)(hval >> HashParams::Bits()) ^ (hval & mask);
	}

	#if defined(_MSC_VER)
	#pragma warning(pop)
	#endif

	typedef FNVHash<Hash32> FNVHash32;
	typedef FNVHash<Hash30> FNVHash30;
	typedef FNVHash<Hash64> FNVHash64;

}

#endif
