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

#pragma once

#include <stdint.h>
#include <string>
#include <locale>
#include <codecvt>

#include "AkGuid.h"

#include <AK/SoundEngine/Common/AkTypes.h>

namespace AK
{
	namespace WwiseAuthoringAPI
	{
		enum AkVariantTypeBase
		{
			AkVariantType_none = 0,

			AkVariantType_uint8,
			AkVariantType_uint16,
			AkVariantType_uint32,
			AkVariantType_uint64,

			AkVariantType_int8,
			AkVariantType_int16,
			AkVariantType_int32,
			AkVariantType_int64,

			AkVariantType_real32,
			AkVariantType_real64,

			AkVariantType_bool,

			AkVariantType_string,
			AkVariantType_wstring,
			AkVariantType_guid
		};

		// Do not use AkVariantBase directly, you must inherit from it.
		class AkVariantBase
		{
		private:

			static const int k_iExampleGuidLen = 38;

			// Constructors and destructor are protected to prevent accidental leak when deleting the class with the wrong type.
			// The class is not using a virtual destructor on purpose to avoid having a vtable.
		protected:

			inline ~AkVariantBase()
			{
				Clear();
			}

			inline AkVariantBase()
			{
				Nullify();
			}

			inline AkVariantBase(const AkVariantBase& other)
			{
				Nullify();
				CopyFrom(other);
			}

			inline AkVariantBase(AkVariantBase&& other)
			{
				Nullify();
				CopyFrom(other);
			}

			inline AkVariantBase(uint8_t in_val) { m_uint8 = in_val; m_eType = AkVariantType_uint8; }
			inline AkVariantBase(uint16_t in_val) { m_uint16 = in_val; m_eType = AkVariantType_uint16; }
			inline AkVariantBase(uint32_t in_val) { m_uint32 = in_val; m_eType = AkVariantType_uint32; }
			inline AkVariantBase(uint64_t in_val) { m_uint64 = in_val; m_eType = AkVariantType_uint64; }

			inline AkVariantBase(int8_t in_val) { m_uint8 = in_val; m_eType = AkVariantType_int8; }
			inline AkVariantBase(int16_t in_val) { m_uint16 = in_val; m_eType = AkVariantType_int16; }
			inline AkVariantBase(int32_t in_val) { m_uint32 = in_val; m_eType = AkVariantType_int32; }
			inline AkVariantBase(int64_t in_val) { m_uint64 = in_val; m_eType = AkVariantType_int64; }

			inline AkVariantBase(float in_val) { m_real32 = in_val; m_eType = AkVariantType_real32; }
			inline AkVariantBase(double in_val) { m_real64 = in_val; m_eType = AkVariantType_real64; }

			inline AkVariantBase(bool in_val) { m_boolean = in_val; m_eType = AkVariantType_bool; }

			inline AkVariantBase(const AkGuid& in_val)
			{
				m_data = new AkGuid();
				memcpy(m_data, &in_val, sizeof(AkGuid));
				m_eType = AkVariantType_guid;
			}

#ifdef _MFC_VER
			inline AkVariantBase(const GUID& in_val)
			{
				m_data = new AkGuid();
				memcpy(m_data, &in_val, sizeof(AkGuid));
				m_eType = AkVariantType_guid;
			}
#endif

			inline AkVariantBase(const wchar_t* in_val)
			{
				m_data = new std::wstring(in_val);
				m_eType = AkVariantType_wstring;
			}

			inline AkVariantBase(const std::wstring& in_val)
				: AkVariantBase(in_val.c_str())
			{
			}

			inline AkVariantBase(const char* in_val)
			{
				m_data = new std::string(in_val);
				m_eType = AkVariantType_string;
			}

			inline AkVariantBase(const std::string& in_val)
				: AkVariantBase(in_val.c_str())
			{
			}

#ifdef _MFC_VER
			inline AkVariantBase(const VARIANT& in_var)
			{
				m_eType = AkVariantType_none;
				*this = in_var;
			}
#endif

			void Nullify()
			{
				m_uint64 = 0;
				m_eType = AkVariantType_none;
			}

		public:

			// Assignment ----------------------------------------------------------------------------

			inline AkVariantBase& operator=(const AkVariantBase& in_val)
			{
				Clear();
				CopyFrom(in_val);
				return *this;
			}

			inline AkVariantBase& operator=(const AkGuid& in_val)
			{
				Clear();
				AkGuid*guid = new AkGuid();
				memcpy(guid, &in_val, sizeof(AkGuid));
				m_eType = AkVariantType_guid;
				return *this;
			}

#ifdef _MFC_VER
			inline AkVariantBase& operator=(const GUID& in_val)
			{
				Clear();
				m_data = new AkGuid();
				memcpy(m_data, &in_val, sizeof(AkGuid));
				m_eType = AkVariantType_guid;
				return *this;
			}
#endif

			inline AkVariantBase& operator=(const wchar_t* in_val)
			{
				Clear();
				m_data = new std::wstring(in_val);
				m_eType = AkVariantType_wstring;
				return *this;
			}

			inline AkVariantBase& operator=(const std::wstring& in_val)
			{
				Clear();
				m_data = new std::wstring(in_val);
				m_eType = AkVariantType_wstring;
				return *this;
			}

			inline AkVariantBase& operator=(const char* in_val)
			{
				Clear();
				m_data = new std::string(in_val);
				m_eType = AkVariantType_string;
				return *this;
			}

			inline AkVariantBase& operator=(const std::string& in_val)
			{
				Clear();
				m_data = new std::string(in_val);
				m_eType = AkVariantType_string;
				return *this;
			}
			
#ifdef _MFC_VER
			inline AkVariantBase& operator=(const VARIANT& in_var)
			{
				switch (in_var.vt)
				{
					// Integer
				case VT_UI1: operator=(static_cast<uint8_t>(in_var.bVal)); break;

				case VT_UI2: operator=(static_cast<uint16_t>(in_var.uiVal)); break;
				case VT_UI4: operator=(static_cast<uint32_t>(in_var.ulVal)); break;
				case VT_UI8: operator=(static_cast<uint64_t>(in_var.ullVal)); break;
				case VT_I1: operator=(static_cast<int8_t>(in_var.cVal)); break;
				case VT_I2: operator=(static_cast<int16_t>(in_var.iVal)); break;
				case VT_I4: operator=(static_cast<int32_t>(in_var.lVal)); break;
				case VT_I8: operator=(static_cast<int64_t>(in_var.llVal)); break;

				case VT_INT: operator=(static_cast<int32_t>(in_var.intVal)); break;
				case VT_UINT: operator=(static_cast<uint32_t>(in_var.uintVal)); break;

				case VT_R4: operator=(static_cast<float>(in_var.fltVal)); break;
				case VT_R8: operator=(static_cast<double>(in_var.dblVal)); break;

				case VT_BOOL: operator=(in_var.boolVal == VARIANT_TRUE); break;
				case VT_BSTR: operator=(in_var.bstrVal); break;
				case VT_EMPTY: Clear(); Nullify(); break;

				default:
					AKASSERT(!"Unsupported type");
				}

				return *this;
			}
#endif

			// Typecast ----------------------------------------------------------------------------

#ifdef _MFC_VER
			inline operator VARIANT() const
			{
				switch ( m_eType )
				{
				case AkVariantType_uint8: return CComVariant( m_uint8 );
				case AkVariantType_uint16: return CComVariant( m_uint16 );
				case AkVariantType_uint32: return CComVariant( m_uint32 );
				case AkVariantType_uint64: return CComVariant( m_uint64 );

				case AkVariantType_int8: return CComVariant( m_int8 );
				case AkVariantType_int16: return CComVariant( m_int16 );
				case AkVariantType_int32: return CComVariant( m_int32 );
				case AkVariantType_int64: return CComVariant( m_int64 );

				case AkVariantType_real32: return CComVariant( m_real32 );
				case AkVariantType_real64: return CComVariant( m_real64 );

				case AkVariantType_bool: return CComVariant( m_boolean );

				case AkVariantType_string: return CComVariant( GetString().c_str() );
				case AkVariantType_wstring: return CComVariant( GetWString().c_str() );

				case AkVariantType_guid:
				{
					std::wstring strTemp;
					AkGuidToWStr( GetGuid(), strTemp );
					return CComVariant( strTemp.c_str() );
				}

				default:
					AKASSERT(false && "Trying to convert an AkVariant that doesn't contain a basic type");
				}
				return CComVariant();
			}
#endif

			inline bool IsString() const
			{
				return (m_eType == AkVariantType_string);
			}

			inline bool IsGuid() const
			{
				return (m_eType == AkVariantType_guid);
			}

			inline bool IsWString() const
			{
				return (m_eType == AkVariantType_wstring);
			}

			// Helpers ----------------------------------------------------------------------------

			const std::wstring& GetWString() const
			{
				AKASSERT(m_eType == AkVariantType_wstring && "AkVariant: illegal typecast");
				return *static_cast<const std::wstring*>(m_data);
			}

			const std::string& GetString() const
			{
				AKASSERT(m_eType == AkVariantType_string && "AkVariant: illegal typecast");
				return *static_cast<const std::string*>(m_data);
			}

			const AkGuid& GetGuid() const
			{
				AKASSERT(m_eType == AkVariantType_guid && "AkVariant: illegal typecast");
				return *static_cast<const AkGuid*>(m_data);
			}

#define _SET_VALUE( _member_, _value_, _type_ ) { \
				Clear(); \
				(_member_) = (_value_); \
				m_eType = _type_; \
				return *this; \
										}

			inline AkVariantBase& operator=(int64_t in_val)
			{
				_SET_VALUE(m_int64, in_val, AkVariantType_int64)
			}
			inline AkVariantBase& operator=(int32_t in_val)
			{
				_SET_VALUE(m_int32, in_val, AkVariantType_int32)
			}
			inline AkVariantBase& operator=(int16_t in_val)
			{
				_SET_VALUE(m_int16, in_val, AkVariantType_int16)
			}
			inline AkVariantBase& operator=(int8_t in_val)
			{
				_SET_VALUE(m_int8, in_val, AkVariantType_int8)
			}

			inline AkVariantBase& operator=(uint64_t in_val)
			{
				_SET_VALUE(m_uint64, in_val, AkVariantType_uint64)
			}
			inline AkVariantBase& operator=(uint32_t in_val)
			{
				_SET_VALUE(m_uint32, in_val, AkVariantType_uint32)
			}
			inline AkVariantBase& operator=(uint16_t in_val)
			{
				_SET_VALUE(m_uint16, in_val, AkVariantType_uint16)
			}
			inline AkVariantBase& operator=(uint8_t in_val)
			{
				_SET_VALUE(m_uint8, in_val, AkVariantType_uint8)
			}

			inline AkVariantBase& operator=(double in_val)
			{
				_SET_VALUE(m_real64, in_val, AkVariantType_real64)
			}
			inline AkVariantBase& operator=(float in_val)
			{
				_SET_VALUE(m_real32, in_val, AkVariantType_real32)
			}

			inline AkVariantBase& operator=(bool in_val)
			{
				_SET_VALUE(m_boolean, in_val, AkVariantType_bool)
			}

			// Typecast ----------------------------------------------------------------------------

			inline operator bool() const
			{
				switch (m_eType)
				{
				case AkVariantType_bool: return m_boolean;
				default: break;
				}

				AKASSERT(false && "AkVariantBase: illegal typecast");
				return 0;
			}

			inline operator int64_t() const
			{
				switch (m_eType)
				{
				case AkVariantType_int8: return static_cast<int64_t>(m_int8);
				case AkVariantType_int16: return static_cast<int64_t>(m_int16);
				case AkVariantType_int32: return static_cast<int64_t>(m_int32);
				case AkVariantType_int64: return m_int64;
				default: break;
				}

				AKASSERT(false && "AkVariantBase: illegal typecast");
				return 0;
			}

			template<typename T> T ToEnum() const
			{
				return static_cast<T>(static_cast<int32_t>(*this));
			}
			
			inline operator int32_t() const
			{
				switch (m_eType)
				{
				case AkVariantType_int8: return static_cast<int32_t>(m_int8);
				case AkVariantType_int16: return static_cast<int32_t>(m_int16);
				case AkVariantType_int32: return m_int32;
				case AkVariantType_uint8: return static_cast<int32_t>(m_uint8);
				case AkVariantType_uint16: return static_cast<int32_t>(m_uint16);
				default: break;
				}

				AKASSERT(false && "AkVariantBase: illegal typecast");
				return 0;
			}

			inline operator uint32_t() const
			{
				switch (m_eType)
				{
				case AkVariantType_int8: return static_cast<uint32_t>(m_int8);
				case AkVariantType_int16: return static_cast<uint32_t>(m_int16);
				case AkVariantType_int32: return static_cast<uint32_t>(m_int32);
				case AkVariantType_uint8: return static_cast<uint32_t>(m_uint8);
				case AkVariantType_uint16: return static_cast<uint32_t>(m_uint16);
				case AkVariantType_uint32: return m_uint32;
				default: break;
				}

				AKASSERT(false && "AkVariantBase: illegal typecast");
				return 0;
			}

			inline operator uint64_t() const
			{
				switch (m_eType)
				{
				case AkVariantType_int8: return static_cast<uint64_t>(m_int8);
				case AkVariantType_int16: return static_cast<uint64_t>(m_int16);
				case AkVariantType_int32: return static_cast<uint64_t>(m_int32);
				case AkVariantType_int64: return static_cast<uint64_t>(m_int64);
				case AkVariantType_uint8: return static_cast<uint64_t>(m_uint8);
				case AkVariantType_uint16: return static_cast<uint64_t>(m_uint16);
				case AkVariantType_uint32: return static_cast<uint64_t>(m_uint32);
				case AkVariantType_uint64: return m_uint64;
				default: break;
				}

				AKASSERT(false && "AkVariantBase: illegal typecast");
				return 0;
			}

			inline operator double() const
			{
				switch (m_eType)
				{
				case AkVariantType_int8: return static_cast<double>(m_int8);
				case AkVariantType_int16: return static_cast<double>(m_int16);
				case AkVariantType_int32: return static_cast<double>(m_int32);
				case AkVariantType_int64: return static_cast<double>(m_int64);

				case AkVariantType_uint8: return static_cast<double>(m_uint8);
				case AkVariantType_uint16: return static_cast<double>(m_uint16);
				case AkVariantType_uint32: return static_cast<double>(m_uint32);
				case AkVariantType_uint64: return static_cast<double>(m_uint64);

				case AkVariantType_real32: return static_cast<double>(m_real32);
				case AkVariantType_real64: return m_real64;

				default: break;
				}

				AKASSERT(false && "AkVariantBase: illegal typecast");
				return 0;
			}

			inline operator AkGuid() const
			{
				if (m_eType == AkVariantType_guid)
					return *static_cast<const AkGuid*>(m_data);

				AKASSERT(false && "AkVariantBase: illegal typecast");
				return AkGuid();
			}

			inline operator std::wstring() const
			{
				if(m_eType == AkVariantType_wstring )
					return *static_cast<const std::wstring*>(m_data);

				AKASSERT(false && "AkVariantBase: illegal typecast");
				return std::wstring();
			}

			inline operator std::string() const
			{
				if (m_eType == AkVariantType_string)
					return *static_cast<const std::string*>(m_data);

				AKASSERT(false && "AkVariantBase: illegal typecast");
				return std::string();
			}

			// Helpers ----------------------------------------------------------------------------

			inline bool IsNumber() const
			{
				return (m_eType >= AkVariantType_int8 && m_eType <= AkVariantType_int64)
					|| (m_eType >= AkVariantType_uint8 && m_eType <= AkVariantType_uint64)
					|| (m_eType >= AkVariantType_real32 && m_eType <= AkVariantType_real64);
			}

			inline bool IsEmpty() const
			{
				return (m_eType == AkVariantType_none);
			}

		private:

			static inline uint8_t WCharToUInt(wchar_t wc)
			{
				if (wc >= '0' && wc <= '9')
					return (uint8_t)(wc - '0');
				else if (wc >= 'a' && wc <= 'f')
					return (uint8_t)(wc - 'a') + 10;
				else if (wc >= 'A' && wc <= 'F')
					return (uint8_t)(wc - 'A') + 10;
				else
					return (uint8_t)-1;
			}

			static inline char UIntToChar(uint8_t by)
			{
				if (by < 0xa)
					return '0' + by;
				else
					return 'A' + (by - 10);
			}

		public:

			static AkGuid AkGuidFromWStr(const std::wstring& in_wstr)
			{
				AkGuid guid;
				WStrToAkGuid(in_wstr, guid);
				return guid;
			}

			static bool WStrToAkGuid(const std::wstring& in_wstr, AkGuid& out_guid)
			{
				const wchar_t* psz = in_wstr.c_str();
				const size_t len = wcslen(psz);

				// GUID must have format: 01234567-ABCD-0123-abcd-01234567ABCD, with MANDATORY brackets around
				if (len != k_iExampleGuidLen)
					return false;

				const bool brackets = psz[0] == L'{' && psz[len - 1] == L'}';
				if (!brackets)
					return false;

				psz += 1;

				// Note: we must convert the string into this struct
				//       uint32_t data1;
				//		 uint16_t data2;
				//		 uint16_t data3;
				//		 uint8_t  data4[8];
				// We only support little endian for now!

				uint8_t* pby = (uint8_t*)&out_guid;

				// Convert into uint32
				for (int i = 0; i < 4; i++) // 01234567
				{
					uint8_t u1 = WCharToUInt(*psz++);
					uint8_t u2 = WCharToUInt(*psz++);
					if (u1 == (uint8_t)-1 || u2 == (uint8_t)-1)
						return false;
					pby[3 - i] = (u1 << 4) | u2; // store backward as little endian
				}

				if (*psz != L'-')
					return false;

				++psz; // -
				pby += 4;

				// Convert into uint16
				for (int i = 0; i < 2; i++) // ABCD
				{
					uint8_t u1 = WCharToUInt(*psz++);
					uint8_t u2 = WCharToUInt(*psz++);
					if (u1 == (uint8_t)-1 || u2 == (uint8_t)-1)
						return false;
					pby[1 - i] = (u1 << 4) | u2;
				}

				if (*psz != L'-')
					return false;

				++psz; // -
				pby += 2;

				// Convert into uint16
				for (int i = 0; i < 2; i++) // 0123
				{
					uint8_t u1 = WCharToUInt(*psz++);
					uint8_t u2 = WCharToUInt(*psz++);
					if (u1 == (uint8_t)-1 || u2 == (uint8_t)-1)
						return false;
					pby[1 - i] = (u1 << 4) | u2;
				}

				if (*psz != L'-')
					return false;

				++psz; // -
				pby += 2;

				// Convert into uint8[2]
				for (int i = 0; i < 2; i++) // abcd
				{
					uint8_t u1 = WCharToUInt(*psz++);
					uint8_t u2 = WCharToUInt(*psz++);
					if (u1 == (uint8_t)-1 || u2 == (uint8_t)-1)
						return false;
					*pby++ = (u1 << 4) | u2;
				}

				if (*psz != L'-')
					return false;

				++psz; // -	

				// Convert into uint8[6]
				for (int i = 0; i < 6; i++) // 01234567ABCD
				{
					uint8_t u1 = WCharToUInt(*psz++);
					uint8_t u2 = WCharToUInt(*psz++);
					if (u1 == (uint8_t)-1 || u2 == (uint8_t)-1)
						return false;
					*pby++ = (u1 << 4) | u2;
				}

				return true;
			}

			static void AkGuidToStr(const AkGuid& in_guid, std::string& out_str)
			{
				uint8_t* pby = (uint8_t*)&in_guid;

				if (out_str.size() < (k_iExampleGuidLen))
					out_str.resize(k_iExampleGuidLen);

				char* psz = (char*)&out_str[0];

				*psz++ = '{';

				// Note: we must convert the string into this struct
				//       uint32_t data1;
				//		 uint16_t data2;
				//		 uint16_t data3;
				//		 uint8_t  data4[8];
				// We only support little endian for now!

				// Convert uint32
				for (int i = 0; i < 4; ++i)
				{
					*psz++ = UIntToChar(pby[3 - i] >> 4);
					*psz++ = UIntToChar(pby[3 - i] & 0xf);
				}
				pby += 4;
				*psz++ = '-';

				// Convert uint16
				for (int i = 0; i < 2; ++i)
				{
					*psz++ = UIntToChar(pby[1 - i] >> 4);
					*psz++ = UIntToChar(pby[1 - i] & 0xf);
				}
				pby += 2;
				*psz++ = '-';

				// Convert uint16
				for (int i = 0; i < 2; ++i)
				{
					*psz++ = UIntToChar(pby[1 - i] >> 4);
					*psz++ = UIntToChar(pby[1 - i] & 0xf);
				}
				pby += 2;
				*psz++ = '-';

				// Convert uint8[2]
				for (int i = 0; i < 2; ++i)
				{
					*psz++ = UIntToChar(pby[i] >> 4);
					*psz++ = UIntToChar(pby[i] & 0xf);
				}
				pby += 2;
				*psz++ = '-';

				// Convert uint8[6]
				for (int i = 0; i < 6; ++i)
				{
					*psz++ = UIntToChar(pby[i] >> 4);
					*psz++ = UIntToChar(pby[i] & 0xf);
				}

				*psz++ = '}';
			}

			static void AkGuidToWStr(const AkGuid& in_guid, std::wstring& out_wstr)
			{
				std::string str;
				AkGuidToStr(in_guid, str);

				static std::wstring_convert<std::codecvt_utf8_utf16<wchar_t>> converter;
				out_wstr = converter.from_bytes(str);
			}

		private:

			void Clear()
			{
				switch (m_eType)
				{
				case AkVariantType_string:
					delete static_cast<std::string*>(m_data);
					break;
				case AkVariantType_wstring:
					delete static_cast<std::wstring*>(m_data);
					break;
				case AkVariantType_guid:
					delete static_cast<AkGuid*>(m_data);
					break;
				}
			}

			void CopyFrom(const AkVariantBase& in_var)
			{
				m_eType = in_var.m_eType;

				switch (in_var.m_eType)
				{
				case AkVariantType_uint8:
					m_uint8 = in_var.m_uint8;
					break;
				case AkVariantType_uint16:
					m_uint16 = in_var.m_uint16;
					break;
				case AkVariantType_uint32:
					m_uint32 = in_var.m_uint32;
					break;
				case AkVariantType_uint64:
					m_uint64 = in_var.m_uint64;
					break;
				case AkVariantType_int8:
					m_int8 = in_var.m_int8;
					break;
				case AkVariantType_int16:
					m_int16 = in_var.m_int16;
					break;
				case AkVariantType_int32:
					m_int32 = in_var.m_int32;
					break;
				case AkVariantType_int64:
					m_int64 = in_var.m_int64;
					break;
				case AkVariantType_real32:
					m_real32 = in_var.m_real32;
					break;
				case AkVariantType_real64:
					m_real64 = in_var.m_real64;
					break;
				case AkVariantType_bool:
					m_boolean = in_var.m_boolean;
					break;
				case AkVariantType_string:
					m_data = new std::string(*static_cast<const std::string*>(in_var.m_data));
					break;
				case AkVariantType_wstring:
					m_data = new std::wstring(*static_cast<const std::wstring*>(in_var.m_data));
					break;
				case AkVariantType_guid:
					m_data = new AkGuid();
					memcpy(m_data, in_var.m_data, sizeof(AkGuid));
					break;
				default:
					break;
				}
			}

		public:

			int GetType() const
			{
				return m_eType;
			}

			int32_t GetInt32() const
			{
				AKASSERT(m_eType == AkVariantType_int32 && "AkVariant: illegal typecast");
				return m_int32;
			}

			bool GetBoolean() const
			{
				AKASSERT(m_eType == AkVariantType_bool && "AkVariant: illegal typecast");
				return m_boolean;
			}

			// Implicit interface supporting conversion to rapidjson. Does not require rapidjson dependencies
			// if the function is not called.
			template<typename RapidJsonValueType, typename RapidJsonAllocator, typename RapidJsonSizeType>
			bool toRapidJsonValue(RapidJsonValueType out_rapidJson, RapidJsonAllocator in_allocator) const
			{
				switch (m_eType)
				{
				case AkVariantType_none: out_rapidJson.SetNull(); break;

				case AkVariantType_bool: out_rapidJson.SetBool(m_boolean); break;

				case AkVariantType_int8: out_rapidJson.SetInt(m_int8); break;
				case AkVariantType_int16: out_rapidJson.SetInt(m_int16); break;
				case AkVariantType_int32: out_rapidJson.SetInt(m_int32); break;
				case AkVariantType_int64: out_rapidJson.SetInt64(m_int64); break;

				case AkVariantType_uint8: out_rapidJson.SetUint(m_uint8); break;
				case AkVariantType_uint16: out_rapidJson.SetUint(m_uint16); break;
				case AkVariantType_uint32: out_rapidJson.SetUint(m_uint32); break;
				case AkVariantType_uint64: out_rapidJson.SetUint64(m_uint64); break;

				case AkVariantType_real32: out_rapidJson.SetDouble(m_real32); break;
				case AkVariantType_real64: out_rapidJson.SetDouble(m_real64); break;

				case AkVariantType_guid:
				{
					std::string str;
					AkVariantBase::AkGuidToStr(*static_cast<const AkGuid*>(m_data), str);
					out_rapidJson.SetString(str.c_str(), static_cast<RapidJsonSizeType>(str.length()), in_allocator);
				}
				break;

				default:
					return false;
				}

				return true;
			}
			
		protected:

			int m_eType;

			union
			{
				uint8_t m_uint8;
				uint16_t m_uint16;
				uint32_t m_uint32;
				uint64_t m_uint64;

				int8_t m_int8;
				int16_t m_int16;
				int32_t m_int32;
				int64_t m_int64;

				float m_real32;
				double m_real64;

				bool m_boolean;

				void* m_data;
			};
		};
	}
}
