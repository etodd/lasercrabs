#pragma once
#include "types.h"
#include "lmath.h"

struct cJSON;

namespace VI
{


namespace Json
{
	cJSON* load(const char*);
	void save(cJSON*, const char*);
	void json_free(cJSON*);
	Vec3 get_vec3(cJSON*, const char*, const Vec3& = Vec3::zero);
	Vec4 get_vec4(cJSON*, const char*, const Vec4& = Vec4::zero);
	Quat get_quat(cJSON*, const char*, const Quat& = Quat::identity);
	r32 get_r32(cJSON*, const char*, const r32 = 0.0f);
	s32 get_s32(cJSON*, const char*, const s32 = 0);
	s64 get_s64(cJSON*, const char*, const s64 = 0);
	const char* get_string(cJSON*, const char*, const char* = 0);
	s32 get_enum(cJSON*, const char*, const char**, const s32 = 0);
};


}
