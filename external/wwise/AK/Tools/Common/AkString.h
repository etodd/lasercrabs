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
#pragma once

#include <AK/Tools/Common/AkFNVHash.h>
#include <AK/Tools/Common/AkHashList.h>

template<typename TAlloc, typename T_CHAR>
class AkStringData : public TAlloc
{
public:
	AkStringData() : pStr(NULL), bOwner(false) {}
	AkStringData(const T_CHAR* in_pStr) : pStr(in_pStr), bOwner(false) {}
	~AkStringData() { Term(); }

	void Term()
	{
		if (pStr && bOwner)
		{
			TAlloc::Free((void*)pStr);
			bOwner = false;
		}
		pStr = NULL;
	}

protected:
	const T_CHAR* pStr;
	bool bOwner;
};

template<typename TAlloc, typename T_CHAR>
class AkStringImpl : public AkStringData<TAlloc, T_CHAR>
{};

template<typename TAlloc, typename T_CHAR>
class AkString: public AkStringImpl<TAlloc, T_CHAR>
{
private:
	typedef AkStringData<TAlloc, T_CHAR> tData;
	typedef AkStringImpl<TAlloc, T_CHAR> tImpl;

public:
	AkString() : AkStringImpl<TAlloc, T_CHAR>() {}

	template<typename T_CHAR2>
	AkString(const T_CHAR2* in_pStr) { tImpl::Set(in_pStr); }

	AkString(const AkString<TAlloc, T_CHAR>& in_other) { tImpl::Set(in_other.Get()); }

	template<typename TAlloc2, typename T_CHAR2>
	AkString(const AkString<TAlloc2, T_CHAR2>& in_other) { tImpl::Set(in_other.Get()); }

	// The default assignment behavior is to not create a local copy, unless it is necessary (due to incompatible string types).
	// Call AllocCopy() if you want to ensure there is a local copy owned by this AkString.  
	AKRESULT AllocCopy()
	{
		if (tData::pStr && !tData::bOwner)
		{
			const T_CHAR* pRefStr = tData::pStr;
			AkUInt32 uLen = tImpl::Length();
			if (uLen > 0)
			{
				tData::pStr = (T_CHAR*)TAlloc::Alloc((uLen + 1) * sizeof(T_CHAR));
				if (tData::pStr == NULL)
					return AK_InsufficientMemory;

				AKPLATFORM::AkMemCpy((void*)tData::pStr, (void*)pRefStr, ((uLen + 1) * sizeof(T_CHAR)));
				tData::bOwner = true;
			}
			else
			{
				tData::pStr = NULL;
			}
		}
		return AK_Success;
	}

	// Transfer memory ownership from in_from to this AkString.
	void Transfer(AkString<TAlloc, T_CHAR>& in_from)
	{
		tData::Term();

		tData::pStr = in_from.tData::pStr;
		tData::bOwner = true;
		
		in_from.tData::pStr = NULL;
		in_from.tData::bOwner = false;
	}

	const T_CHAR* Get() const
	{
		return tData::pStr;
	}

	AkString& operator=(const AkString<TAlloc, T_CHAR>& in_rhs)
	{
		tImpl::Set(in_rhs.Get());
		return *this;
	}

	template<typename TAlloc2, typename T_CHAR2>
	AkString& operator=(const AkString<TAlloc2, T_CHAR2>& in_rhs)
	{
		tImpl::Set(in_rhs.Get());
		return *this;
	}

	template<typename T_CHAR2>
	AkString& operator=(const T_CHAR2* in_pStr)
	{
		tImpl::Set(in_pStr);
		return *this;
	}
};

#ifdef AK_SUPPORT_WCHAR	
template<typename TAlloc>
class AkStringImpl <TAlloc, wchar_t> : public AkStringData<TAlloc, wchar_t>
{
private:
	typedef AkStringData<TAlloc, wchar_t> tData;

public:
	AkStringImpl() : AkStringData<TAlloc, wchar_t>() {}

protected:
	AKRESULT Set(const char* in_pStr)
	{
		tData::Term();

		if (in_pStr != NULL)
		{
			size_t uLen = strlen(in_pStr);
			if (uLen > 0)
			{
				tData::pStr = (wchar_t*)TAlloc::Alloc((uLen + 1) * sizeof(wchar_t));
				if (tData::pStr == NULL)
					return AK_InsufficientMemory;

				AKPLATFORM::AkCharToWideChar(in_pStr, (AkUInt32)(uLen + 1), const_cast<wchar_t*>(tData::pStr));
				tData::bOwner = true;
			}
			else
			{
				tData::pStr = NULL;
			}
		}

		return AK_Success;
	}

	AKRESULT Set(const wchar_t* in_pStr)
	{
		tData::Term();
		tData::pStr = in_pStr;
		return AK_Success;
	}

public:
	AkUInt32 Length() const
	{
		return (AkUInt32)wcslen(tData::pStr);
	}
};
#endif

template<typename TAlloc>
class AkStringImpl <TAlloc, char> : public AkStringData<TAlloc, char>
{
private:
	typedef AkStringData<TAlloc, char> tData;

public:
	AkStringImpl() : AkStringData<TAlloc, char>() {}

protected:
	AKRESULT Set(const wchar_t* in_pStr)
	{
		tData::Term();

		if (in_pStr != NULL)
		{
			size_t uLen = wcslen(in_pStr);
			if (uLen > 0)
			{
				tData::pStr = (char*)TAlloc::Alloc((uLen + 1) * sizeof(char));
				if (tData::pStr == NULL)
					return AK_InsufficientMemory;

				AKPLATFORM::AkWideCharToChar(in_pStr, (AkUInt32)(uLen + 1), const_cast<char*>(tData::pStr));
				tData::bOwner = true;
			}
			else
			{
				tData::pStr = NULL;
			}
		}

		return AK_Success;
	}

	AKRESULT Set(const char* in_pStr)
	{
		tData::Term();
		tData::pStr = in_pStr;
		return AK_Success;
	}

public:
	AkUInt32 Length() const
	{
		return (AkUInt32)strlen(tData::pStr);
	}
};

struct AkNonThreaded
{
	AkForceInline void Lock() {}
	AkForceInline void Unlock() {}
};

template<typename TAlloc, typename T_CHAR>
static AkForceInline AkUInt32 AkHash(const AkString<TAlloc, T_CHAR>& in_str)
{
	AkUInt32 uLen = in_str.Length();
	if (uLen > 0)
	{
		AK::FNVHash32 hash;
		return hash.Compute(in_str.Get(), uLen * sizeof(T_CHAR));
	}
	return 0;
}

//
// AkDbString - A string reference class that stores a hash to a string in a database.  If an identical string is found, the reference count in the database is incremented,
//	so that we do not store duplicate strings.  Database can be made multi thread safe by passing in CAkLock for tLock, or AkNonThreaded if concurrent access is not needed.
//
template<typename TAlloc, typename T_CHAR, typename tLock = AkNonThreaded>
class AkDbString : public TAlloc
{
public: 
	typedef AkDbString<TAlloc, T_CHAR, tLock> tThis;
	typedef AkString<TAlloc, T_CHAR> tString;

	struct Entry
	{
		Entry() : refCount(0) {}

		tString str;
		AkInt32 refCount;
	};

	typedef AkHashList<AkUInt32, Entry, TAlloc> tStringTable;

	struct Instance : public TAlloc
	{
		tStringTable table;
		tLock lock;
	};

public:

	// Must be called to initialize the database.  
	static AKRESULT InitDB()
	{
		if (pInstance == NULL)
		{
			pInstance = (Instance*)pInstance->TAlloc::Alloc(sizeof(Instance));
			AkPlacementNew(pInstance) Instance();
			return AK_Success;
		}
		else
			return AK_Fail;
	}

	// Term the DB.
	static void TermDB()
	{
		if (pInstance != NULL)
		{
			pInstance->~Instance();
			pInstance->TAlloc::Free(pInstance);
			pInstance = NULL;
		}
	}

	static void UnlockDB() { pInstance->lock.Unlock();}
	static void LockDB() { pInstance->lock.Lock(); }

private:

	static Instance* pInstance;

public:
	AkDbString() : m_uHash(0)
	{}

	AkDbString(const tThis& in_fromDbStr) : m_uHash(0) { Aquire(in_fromDbStr.m_uHash); }

	// Construct from AkString
	template<typename TAlloc2, typename T_CHAR2>
	AkDbString(const AkString<TAlloc2, T_CHAR2>& in_fromStr) : m_uHash(0) { Aquire(in_fromStr); }

	tThis& operator=(const tThis& in_rhs)
	{
		Aquire(in_rhs.m_uHash);
		return *this;
	}

	// Assign from AkString
	template<typename TAlloc2, typename T_CHAR2>
	tThis& operator=(const AkString<TAlloc2, T_CHAR2>& in_rhs)
	{
		Aquire(in_rhs);
		return *this;
	}

	~AkDbString()
	{
		Release();
	}

	const T_CHAR* Get()
	{
		if (m_uHash != 0)
		{
			Entry* pEntry = pInstance->table.Exists(m_uHash);
			AKASSERT(pEntry != NULL);
			return pEntry->str.Get();
		}
		return NULL;
	}

protected:
	
	template<typename TAlloc2, typename T_CHAR2>
	AKRESULT Aquire(const AkString<TAlloc2, T_CHAR2>& in_str)
	{
		AKRESULT res = AK_Success;

		Release();

		if (in_str.Get() != NULL)
		{
			m_uHash = AkHash(in_str);

			LockDB();
			{
				Entry* pEntry = pInstance->table.Set(m_uHash);
				if (pEntry != NULL)
				{
					pEntry->refCount++;

					if (pEntry->str.Get() == NULL)
					{
						pEntry->str = in_str;
						pEntry->str.AllocCopy();

						if (pEntry->str.Get() == NULL) // Allocation failure
						{
							pInstance->table.Unset(m_uHash);
							m_uHash = 0;
							res = AK_Fail;
						}
					}
				}
				else
				{
					m_uHash = 0;
					res = AK_Fail;
				}
			}
			UnlockDB();
		}

		return res;
	}

	// in_uHash must have come from another AkDbString, and therefore already exist in the DB.
	AKRESULT Aquire(AkUInt32 in_uHash)
	{
		AKRESULT res = AK_Success;

		Release();

		if (in_uHash != 0)
		{
			m_uHash = in_uHash;
			LockDB();
			{
				Entry* pEntry = pInstance->table.Exists(m_uHash);
				AKASSERT(pEntry != NULL);

				pEntry->refCount++;
				AKASSERT(pEntry->str.Get() != NULL);
			}
			UnlockDB();
		}

		return res;
	}

	void Release()
	{
		if (m_uHash != 0)
		{
			LockDB();
			{
				tStringTable& table = pInstance->table;
				typename tStringTable::IteratorEx it = table.FindEx(m_uHash);
				Entry& entry = (*it).item;
				AKASSERT(it != table.End() && entry.refCount > 0);

				entry.refCount--;
				if (entry.refCount == 0)
				{
					table.Erase(it);
				}
			}
			UnlockDB();

			m_uHash = 0;
		}
	}

	AkUInt32 m_uHash;

};

template<typename TAlloc, typename T_CHAR, typename tLock>
typename AkDbString<TAlloc, T_CHAR, tLock>::Instance* AkDbString<TAlloc, T_CHAR, tLock>::pInstance = NULL;