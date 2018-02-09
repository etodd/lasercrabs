/*******************************************************************************
The content of this file includes portions of the AUDIOKINETIC Wwise Technology
released in source code form as part of the SDK installer package.

Commercial License Usage

Licensees holding valid commercial licenses to the AUDIOKINETIC Wwise Technology
may use this file in accordance with the end user license agreement provided
with the software or, alternatively, in accordance with the terms contained in a
written agreement between you and Audiokinetic Inc.

Version: v2017.2.1  Build: 6524
Copyright (c) 2006-2018 Audiokinetic Inc.
*******************************************************************************/

#pragma once

#include <cassert>

#ifndef AKASSERT
#define AKASSERT assert
#endif

#include <AK/WwiseAuthoringAPI/AkVariantBase.h>

namespace AK
{
	namespace WwiseAuthoringAPI
	{
		class AkVariant : public AkVariantBase
		{
		public:

			inline ~AkVariant()
			{
			}
			
			inline AkVariant() : AkVariantBase() {}

			inline AkVariant(const AkVariant& in_val) : AkVariantBase(in_val) {}

			inline AkVariant(uint8_t in_val) : AkVariantBase(in_val) {}
			inline AkVariant(uint16_t in_val) : AkVariantBase(in_val) {}
			inline AkVariant(uint32_t in_val) : AkVariantBase(in_val) {}
			inline AkVariant(uint64_t in_val) : AkVariantBase(in_val) {}
			inline AkVariant(int8_t in_val) : AkVariantBase(in_val) {}
			inline AkVariant(int16_t in_val) : AkVariantBase(in_val) {}
			inline AkVariant(int32_t in_val) : AkVariantBase(in_val) {}
			inline AkVariant(int64_t in_val) : AkVariantBase(in_val) {}
			inline AkVariant(float in_val) : AkVariantBase(in_val) {}
			inline AkVariant(double in_val) : AkVariantBase(in_val) {}
			inline AkVariant(bool in_val) : AkVariantBase(in_val) {}

			inline AkVariant(const char* in_val) : AkVariantBase(in_val) {}
			inline AkVariant(const std::string& in_val) : AkVariantBase(in_val) {}

			template<typename RapidJsonValueType, typename RapidJsonAllocator, typename RapidJsonSizeType>
			bool toRapidJsonValue(RapidJsonValueType out_rapidJson, RapidJsonAllocator in_allocator) const
			{
				switch (GetType())
				{
				case AkVariantType_string:
				{
					const std::string* value = static_cast<const std::string*>(m_data);
					out_rapidJson.SetString(value->c_str(), static_cast<RapidJsonSizeType>(value->length()), in_allocator);
				}
				break;

				default:
					return AkVariantBase::toRapidJsonValue<RapidJsonValueType, RapidJsonAllocator, RapidJsonSizeType>(out_rapidJson, in_allocator);
				}

				return true;
			}
			
			template<typename RapidJsonValueType>
			static AkVariant FromRapidJsonValue(RapidJsonValueType in_rapidJson)
			{
				if (in_rapidJson.IsBool())
					return in_rapidJson.IsTrue();
				else if (in_rapidJson.IsInt())
					return in_rapidJson.GetInt();
				else if (in_rapidJson.IsUint())
					return in_rapidJson.GetUint();
				else if (in_rapidJson.IsInt64())
					return in_rapidJson.GetInt64();
				else if (in_rapidJson.IsUint64())
					return in_rapidJson.GetUint64();
				else if (in_rapidJson.IsNumber())
					return in_rapidJson.GetDouble();
				else if (in_rapidJson.IsString())
					return in_rapidJson.GetString();

				return AkVariant();
			}
		};
	}
}
