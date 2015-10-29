#pragma once

#include <cstddef>
#include <math.h>
#include <cstring>
#include "LinearMath/btVector3.h"
#include "LinearMath/btQuaternion.h"

namespace VI
{

#define PI 3.1415926535897f
#define HALF_PI (3.1415926535897f * 0.5f)

// Mostly stolen from Ogre3D

struct Vec2
{
	float x, y;

	inline Vec2() : x(0), y(0) {}

	static const Vec2 zero;

	inline Vec2(const float fX, const float fY)
		: x(fX), y(fY)
	{
	}

	inline explicit Vec2(const float scaler)
		: x(scaler), y(scaler)
	{
	}

	inline explicit Vec2(const float afCoordinate[2])
		: x(afCoordinate[0]),
		  y(afCoordinate[1])
	{
	}

	inline explicit Vec2(const int afCoordinate[2])
	{
		x = (float)afCoordinate[0];
		y = (float)afCoordinate[1];
	}

	inline explicit Vec2(float* const r)
		: x(r[0]), y(r[1])
	{
	}

	inline float operator [] (const int i) const
	{
		return *(&x+i);
	}

	inline float& operator [] (const int i)
	{
		return *(&x+i);
	}

	inline bool operator == (const Vec2& rkVector) const
	{
		return (x == rkVector.x && y == rkVector.y);
	}

	inline bool operator != (const Vec2& rkVector) const
	{
		return (x != rkVector.x || y != rkVector.y);
	}

	inline Vec2 operator + (const Vec2& rkVector) const
	{
		return Vec2(
			x + rkVector.x,
			y + rkVector.y);
	}

	inline Vec2 operator - (const Vec2& rkVector) const
	{
		return Vec2(
			x - rkVector.x,
			y - rkVector.y);
	}

	inline Vec2 operator * (const float fScalar) const
	{
		return Vec2(
			x * fScalar,
			y * fScalar);
	}

	inline Vec2 operator * (const Vec2& rhs) const
	{
		return Vec2(
			x * rhs.x,
			y * rhs.y);
	}

	inline Vec2 operator / (const float fScalar) const
	{
		float fInv = 1.0f / fScalar;

		return Vec2(
			x * fInv,
			y * fInv);
	}

	inline Vec2 operator / (const Vec2& rhs) const
	{
		return Vec2(
			x / rhs.x,
			y / rhs.y);
	}

	inline const Vec2& operator + () const
	{
		return *this;
	}

	inline Vec2 operator - () const
	{
		return Vec2(-x, -y);
	}

	inline friend Vec2 operator * (const float fScalar, const Vec2& rkVector)
	{
		return Vec2(
			fScalar * rkVector.x,
			fScalar * rkVector.y);
	}

	inline friend Vec2 operator / (const float fScalar, const Vec2& rkVector)
	{
		return Vec2(
			fScalar / rkVector.x,
			fScalar / rkVector.y);
	}

	inline friend Vec2 operator + (const Vec2& lhs, const float rhs)
	{
		return Vec2(
			lhs.x + rhs,
			lhs.y + rhs);
	}

	inline friend Vec2 operator + (const float lhs, const Vec2& rhs)
	{
		return Vec2(
			lhs + rhs.x,
			lhs + rhs.y);
	}

	inline friend Vec2 operator - (const Vec2& lhs, const float rhs)
	{
		return Vec2(
			lhs.x - rhs,
			lhs.y - rhs);
	}

	inline friend Vec2 operator - (const float lhs, const Vec2& rhs)
	{
		return Vec2(
			lhs - rhs.x,
			lhs - rhs.y);
	}

	inline Vec2& operator += (const Vec2& rkVector)
	{
		x += rkVector.x;
		y += rkVector.y;

		return *this;
	}

	inline Vec2& operator += (const float fScaler)
	{
		x += fScaler;
		y += fScaler;

		return *this;
	}

	inline Vec2& operator -= (const Vec2& rkVector)
	{
		x -= rkVector.x;
		y -= rkVector.y;

		return *this;
	}

	inline Vec2& operator -= (const float fScaler)
	{
		x -= fScaler;
		y -= fScaler;

		return *this;
	}

	inline Vec2& operator *= (const float fScalar)
	{
		x *= fScalar;
		y *= fScalar;

		return *this;
	}

	inline Vec2& operator *= (const Vec2& rkVector)
	{
		x *= rkVector.x;
		y *= rkVector.y;

		return *this;
	}

	inline Vec2& operator /= (const float fScalar)
	{
		float fInv = 1.0f / fScalar;

		x *= fInv;
		y *= fInv;

		return *this;
	}

	inline Vec2& operator /= (const Vec2& rkVector)
	{
		x /= rkVector.x;
		y /= rkVector.y;

		return *this;
	}

	inline float length() const
	{
		return sqrt(x * x + y * y);
	}

	inline float length_squared() const
	{
		return x * x + y * y;
	}

	inline float dot(const Vec2& vec) const
	{
		return x * vec.x + y * vec.y;
	}

	inline float normalize()
	{
		float fLength = sqrt(x * x + y * y);

		float fInvLength = 1.0f / fLength;
		x *= fInvLength;
		y *= fInvLength;

		return fLength;
	}

	inline static Vec2 normalize(const Vec2& v)
	{
		float len = sqrt(v.x * v.x + v.y * v.y);
		return Vec2(v.x / len, v.y / len);
	}

	inline Vec2 perpendicular(void) const
	{
		return Vec2 (-y, x);
	}

	inline float cross(const Vec2& rkVector) const
	{
		return x * rkVector.y - y * rkVector.x;
	}

	inline Vec2 reflect(const Vec2& normal) const
	{
		return *this - (2.0f * this->dot(normal) * normal);
	}

	static inline Vec2 lerp(float x, const Vec2& a, const Vec2& b)
	{
		return (a * (1.0f - x)) + (b * x);
	}
};

struct Vec3
{
	float x, y, z;

	static const Vec3 zero;

	inline Vec3() : x(0), y(0), z(0) {}

	inline Vec3(const Vec2& v, const float fZ)
		: x(v.x), y(v.y), z(fZ)
	{
	}

	inline Vec3(const float fX, const float fY, const float fZ)
		: x(fX), y(fY), z(fZ)
	{
	}

	inline Vec3(const btVector3& v)
		: x(v.getX()), y(v.getY()), z(v.getZ())
	{
	}

	operator btVector3() const { return btVector3(x, y, z); }

	inline explicit Vec3(const float afCoordinate[3])
		: x(afCoordinate[0]),
		  y(afCoordinate[1]),
		  z(afCoordinate[2])
	{
	}

	inline explicit Vec3(const int afCoordinate[3])
	{
		x = (float)afCoordinate[0];
		y = (float)afCoordinate[1];
		z = (float)afCoordinate[2];
	}

	inline explicit Vec3(float* const r)
		: x(r[0]), y(r[1]), z(r[2])
	{
	}

	inline explicit Vec3(const float scaler)
		: x(scaler)
		, y(scaler)
		, z(scaler)
	{
	}

	inline float operator [] (const int i) const
	{
		return *(&x + i);
	}

	inline float& operator [] (const int i)
	{
		return *(&x + i);
	}

	inline bool operator == (const Vec3& rkVector) const
	{
		return (x == rkVector.x && y == rkVector.y && z == rkVector.z);
	}

	inline bool operator != (const Vec3& rkVector) const
	{
		return (x != rkVector.x || y != rkVector.y || z != rkVector.z);
	}

	inline Vec3 operator + (const Vec3& rkVector) const
	{
		return Vec3(
			x + rkVector.x,
			y + rkVector.y,
			z + rkVector.z);
	}

	inline Vec3 operator - (const Vec3& rkVector) const
	{
		return Vec3(
			x - rkVector.x,
			y - rkVector.y,
			z - rkVector.z);
	}

	inline Vec3 operator * (const float fScalar) const
	{
		return Vec3(
			x * fScalar,
			y * fScalar,
			z * fScalar);
	}

	inline Vec3 operator * (const Vec3& rhs) const
	{
		return Vec3(
			x * rhs.x,
			y * rhs.y,
			z * rhs.z);
	}

	inline Vec3 operator / (const float fScalar) const
	{
		float fInv = 1.0f / fScalar;

		return Vec3(
			x * fInv,
			y * fInv,
			z * fInv);
	}

	inline Vec3 operator / (const Vec3& rhs) const
	{
		return Vec3(
			x / rhs.x,
			y / rhs.y,
			z / rhs.z);
	}

	inline const Vec3& operator + () const
	{
		return *this;
	}

	inline Vec3 operator - () const
	{
		return Vec3(-x, -y, -z);
	}

	inline friend Vec3 operator * (const float fScalar, const Vec3& rkVector)
	{
		return Vec3(
			fScalar * rkVector.x,
			fScalar * rkVector.y,
			fScalar * rkVector.z);
	}

	inline friend Vec3 operator / (const float fScalar, const Vec3& rkVector)
	{
		return Vec3(
			fScalar / rkVector.x,
			fScalar / rkVector.y,
			fScalar / rkVector.z);
	}

	inline friend Vec3 operator + (const Vec3& lhs, const float rhs)
	{
		return Vec3(
			lhs.x + rhs,
			lhs.y + rhs,
			lhs.z + rhs);
	}

	inline friend Vec3 operator + (const float lhs, const Vec3& rhs)
	{
		return Vec3(
			lhs + rhs.x,
			lhs + rhs.y,
			lhs + rhs.z);
	}

	inline friend Vec3 operator - (const Vec3& lhs, const float rhs)
	{
		return Vec3(
			lhs.x - rhs,
			lhs.y - rhs,
			lhs.z - rhs);
	}

	inline friend Vec3 operator - (const float lhs, const Vec3& rhs)
	{
		return Vec3(
			lhs - rhs.x,
			lhs - rhs.y,
			lhs - rhs.z);
	}

	// arithmetic updates
	inline Vec3& operator += (const Vec3& rkVector)
	{
		x += rkVector.x;
		y += rkVector.y;
		z += rkVector.z;

		return *this;
	}

	inline Vec3& operator += (const float fScalar)
	{
		x += fScalar;
		y += fScalar;
		z += fScalar;
		return *this;
	}

	inline Vec3& operator -= (const Vec3& rkVector)
	{
		x -= rkVector.x;
		y -= rkVector.y;
		z -= rkVector.z;

		return *this;
	}

	inline Vec3& operator -= (const float fScalar)
	{
		x -= fScalar;
		y -= fScalar;
		z -= fScalar;
		return *this;
	}

	inline Vec3& operator *= (const float fScalar)
	{
		x *= fScalar;
		y *= fScalar;
		z *= fScalar;
		return *this;
	}

	inline Vec3& operator *= (const Vec3& rkVector)
	{
		x *= rkVector.x;
		y *= rkVector.y;
		z *= rkVector.z;

		return *this;
	}

	inline Vec3& operator /= (const float fScalar)
	{
		float fInv = 1.0f / fScalar;

		x *= fInv;
		y *= fInv;
		z *= fInv;

		return *this;
	}

	inline Vec3& operator /= (const Vec3& rkVector)
	{
		x /= rkVector.x;
		y /= rkVector.y;
		z /= rkVector.z;

		return *this;
	}

	inline float length() const
	{
		return sqrt(x * x + y * y + z * z);
	}

	inline float length_squared() const
	{
		return x * x + y * y + z * z;
	}

	inline float dot(const Vec3& vec) const
	{
		return x * vec.x + y * vec.y + z * vec.z;
	}

	inline float normalize()
	{
		float fLength = sqrt(x * x + y * y + z * z);

		float fInvLength = 1.0f / fLength;
		x *= fInvLength;
		y *= fInvLength;
		z *= fInvLength;

		return fLength;
	}

	inline static Vec3 normalize(const Vec3& v)
	{
		float len = sqrt(v.x * v.x + v.y * v.y + v.z * v.z);
		return Vec3(v.x / len, v.y / len, v.z / len);
	}

	inline Vec3 cross(const Vec3& rkVector) const
	{
		return Vec3(
			y * rkVector.z - z * rkVector.y,
			z * rkVector.x - x * rkVector.z,
			x * rkVector.y - y * rkVector.x);
	}

	inline Vec3 reflect(const Vec3& normal) const
	{
		return *this - (2.0f * this->dot(normal) * normal);
	}

	static inline Vec3 lerp(float x, const Vec3& a, const Vec3& b)
	{
		return (a * (1.0f - x)) + (b * x);
	}
};

struct Ray
{
	Vec3 dir;
	Vec3 pos;
};

struct Vec4
{
	float x, y, z, w;

	static const Vec4 zero;

	inline Vec4() : x(0), y(0), z(0), w(0) {}

	inline Vec4(const Vec3& v, const float fW)
		: x(v.x), y(v.y), z(v.z), w(fW)
	{
	}

	inline Vec4(const float fX, const float fY, const float fZ, const float fW)
		: x(fX), y(fY), z(fZ), w(fW)
	{
	}

	inline explicit Vec4(const float afCoordinate[4])
		: x(afCoordinate[0]),
		  y(afCoordinate[1]),
		  z(afCoordinate[2]),
		  w(afCoordinate[3])
	{
	}

	inline explicit Vec4(const int afCoordinate[4])
	{
		x = (float)afCoordinate[0];
		y = (float)afCoordinate[1];
		z = (float)afCoordinate[2];
		w = (float)afCoordinate[3];
	}

	inline explicit Vec4(float* const r)
		: x(r[0]), y(r[1]), z(r[2]), w(r[3])
	{
	}

	inline explicit Vec4(const float scaler)
		: x(scaler)
		, y(scaler)
		, z(scaler)
		, w(scaler)
	{
	}

	inline explicit Vec4(const Vec3& rhs)
		: x(rhs.x), y(rhs.y), z(rhs.z), w(1.0f)
	{
	}

	inline float operator [] (const int i) const
	{
		return *(&x + i);
	}

	inline float& operator [] (const int i)
	{
		return *(&x + i);
	}

	inline Vec4& operator = (const float fScalar)
	{
		x = fScalar;
		y = fScalar;
		z = fScalar;
		w = fScalar;
		return *this;
	}

	inline Vec3 xyz() const
	{
		return Vec3(x, y, z);
	}

	inline bool operator == (const Vec4& rkVector) const
	{
		return x == rkVector.x && y == rkVector.y && z == rkVector.z && w == rkVector.w;
	}

	inline bool operator != (const Vec4& rkVector) const
	{
		return x != rkVector.x || y != rkVector.y || z != rkVector.z || w != rkVector.w;
	}

	inline Vec4& operator = (const Vec3& rhs)
	{
		x = rhs.x;
		y = rhs.y;
		z = rhs.z;
		w = 1.0f;
		return *this;
	}

	inline Vec4 operator + (const Vec4& rkVector) const
	{
		return Vec4(
			x + rkVector.x,
			y + rkVector.y,
			z + rkVector.z,
			w + rkVector.w);
	}

	inline Vec4 operator - (const Vec4& rkVector) const
	{
		return Vec4(
			x - rkVector.x,
			y - rkVector.y,
			z - rkVector.z,
			w - rkVector.w);
	}

	inline Vec4 operator * (const float fScalar) const
	{
		return Vec4(
			x * fScalar,
			y * fScalar,
			z * fScalar,
			w * fScalar);
	}

	inline Vec4 operator * (const Vec4& rhs) const
	{
		return Vec4(
			rhs.x * x,
			rhs.y * y,
			rhs.z * z,
			rhs.w * w);
	}

	inline Vec4 operator / (const float fScalar) const
	{
		float fInv = 1.0f / fScalar;

		return Vec4(
			x * fInv,
			y * fInv,
			z * fInv,
			w * fInv);
	}

	inline Vec4 operator / (const Vec4& rhs) const
	{
		return Vec4(
			x / rhs.x,
			y / rhs.y,
			z / rhs.z,
			w / rhs.w);
	}

	inline const Vec4& operator + () const
	{
		return *this;
	}

	inline Vec4 operator - () const
	{
		return Vec4(-x, -y, -z, -w);
	}

	inline friend Vec4 operator * (const float fScalar, const Vec4& rkVector)
	{
		return Vec4(
			fScalar * rkVector.x,
			fScalar * rkVector.y,
			fScalar * rkVector.z,
			fScalar * rkVector.w);
	}

	inline friend Vec4 operator / (const float fScalar, const Vec4& rkVector)
	{
		return Vec4(
			fScalar / rkVector.x,
			fScalar / rkVector.y,
			fScalar / rkVector.z,
			fScalar / rkVector.w);
	}

	inline friend Vec4 operator + (const Vec4& lhs, const float rhs)
	{
		return Vec4(
			lhs.x + rhs,
			lhs.y + rhs,
			lhs.z + rhs,
			lhs.w + rhs);
	}

	inline friend Vec4 operator + (const float lhs, const Vec4& rhs)
	{
		return Vec4(
			lhs + rhs.x,
			lhs + rhs.y,
			lhs + rhs.z,
			lhs + rhs.w);
	}

	inline friend Vec4 operator - (const Vec4& lhs, float rhs)
	{
		return Vec4(
			lhs.x - rhs,
			lhs.y - rhs,
			lhs.z - rhs,
			lhs.w - rhs);
	}

	inline friend Vec4 operator - (const float lhs, const Vec4& rhs)
	{
		return Vec4(
			lhs - rhs.x,
			lhs - rhs.y,
			lhs - rhs.z,
			lhs - rhs.w);
	}

	// arithmetic updates
	inline Vec4& operator += (const Vec4& rkVector)
	{
		x += rkVector.x;
		y += rkVector.y;
		z += rkVector.z;
		w += rkVector.w;

		return *this;
	}

	inline Vec4& operator -= (const Vec4& rkVector)
	{
		x -= rkVector.x;
		y -= rkVector.y;
		z -= rkVector.z;
		w -= rkVector.w;

		return *this;
	}

	inline Vec4& operator *= (const float fScalar)
	{
		x *= fScalar;
		y *= fScalar;
		z *= fScalar;
		w *= fScalar;
		return *this;
	}

	inline Vec4& operator += (const float fScalar)
	{
		x += fScalar;
		y += fScalar;
		z += fScalar;
		w += fScalar;
		return *this;
	}

	inline Vec4& operator -= (const float fScalar)
	{
		x -= fScalar;
		y -= fScalar;
		z -= fScalar;
		w -= fScalar;
		return *this;
	}

	inline Vec4& operator *= (const Vec4& rkVector)
	{
		x *= rkVector.x;
		y *= rkVector.y;
		z *= rkVector.z;
		w *= rkVector.w;

		return *this;
	}

	inline Vec4& operator /= (const float fScalar)
	{
		float fInv = 1.0f / fScalar;

		x *= fInv;
		y *= fInv;
		z *= fInv;
		w *= fInv;

		return *this;
	}

	inline Vec4& operator /= (const Vec4& rkVector)
	{
		x /= rkVector.x;
		y /= rkVector.y;
		z /= rkVector.z;
		w /= rkVector.w;

		return *this;
	}

	inline float dot(const Vec4& vec) const
	{
		return x * vec.x + y * vec.y + z * vec.z + w * vec.w;
	}

	static inline Vec4 lerp(float x, const Vec4& a, const Vec4& b)
	{
		return (a * (1.0f - x)) + (b * x);
	}
};

struct Plane
{
	Vec3 normal;
	float d;

	Plane();
	Plane(const Plane& rhs);
	Plane(const Vec3& rkNormal, float fConstant);
	Plane(float a, float b, float c, float d);
	Plane(const Vec3& rkNormal, const Vec3& rkPoint);
	Plane(const Vec3& rkPoint0, const Vec3& rkPoint1, const Vec3& rkPoint2);

	float distance(const Vec3& rkPoint) const;

	void redefine(const Vec3& rkPoint0, const Vec3& rkPoint1, const Vec3& rkPoint2);

	void redefine(const Vec3& rkNormal, const Vec3& rkPoint);

	Vec3 project(const Vec3& v) const;

	float normalize(void);

	bool operator==(const Plane& rhs) const
	{
		return (rhs.d == d && rhs.normal == normal);
	}

	bool operator!=(const Plane& rhs) const
	{
		return (rhs.d != d || rhs.normal != normal);
	}
};

struct Mat3
{
	/// Indexed by [row][col].
	float m[3][3];

	static const Mat3 zero;
	static const Mat3 identity;

	inline Mat3()
	{
	}

	inline explicit Mat3 (const float arr[3][3])
	{
		memcpy(m, arr, 9 * sizeof(float));
	}

	inline Mat3 (const Mat3& rkMatrix)
	{
		memcpy(m, rkMatrix.m, 9 * sizeof(float));
	}

	Mat3 (float fEntry00, float fEntry01, float fEntry02,
				float fEntry10, float fEntry11, float fEntry12,
				float fEntry20, float fEntry21, float fEntry22)
	{
		m[0][0] = fEntry00;
		m[0][1] = fEntry01;
		m[0][2] = fEntry02;
		m[1][0] = fEntry10;
		m[1][1] = fEntry11;
		m[1][2] = fEntry12;
		m[2][0] = fEntry20;
		m[2][1] = fEntry21;
		m[2][2] = fEntry22;
	}

	inline const float* operator[] (int iRow) const
	{
		return m[iRow];
	}

	inline float* operator[] (int iRow)
	{
		return m[iRow];
	}

	Vec3 get_column (int iCol) const;
	void set_column(int iCol, const Vec3& vec);
	void from_axes(const Vec3& xAxis, const Vec3& yAxis, const Vec3& zAxis);

	inline Mat3& operator= (const Mat3& rkMatrix)
	{
		memcpy(m, rkMatrix.m, 9 * sizeof(float));
		return *this;
	}

	bool operator== (const Mat3& rkMatrix) const;

	inline bool operator!= (const Mat3& rkMatrix) const
	{
		return !operator==(rkMatrix);
	}

	Mat3 operator+ (const Mat3& rkMatrix) const;

	Mat3 operator- (const Mat3& rkMatrix) const;

	Mat3 operator* (const Mat3& rkMatrix) const;
	Mat3 operator- () const;

	Vec3 operator* (const Vec3& rkVector) const;

	friend Vec3 operator* (const Vec3& rkVector, const Mat3& rkMatrix);

	Mat3 operator* (float fScalar) const;

	friend Mat3 operator* (float fScalar, const Mat3& rkMatrix);

	Mat3 transpose() const;
	bool inverse(Mat3& rkInverse, float fTolerance = 1e-06) const;
	Mat3 inverse(float fTolerance = 1e-06) const;
	float determinant() const;

	void orthonormalize();

	void qdu_decomposition(Mat3& rkQ, Vec3& rkD, Vec3& rkU) const;

	void to_angle_axis(Vec3& rkAxis, float& rfAngle) const;

	void from_angle_axis(const Vec3& rkAxis, const float& ffloats);

	bool to_euler_angles_xyz (float& rfYAngle, float& rfPAngle, float& rfRAngle) const;
	bool to_euler_angles_xzy (float& rfYAngle, float& rfPAngle, float& rfRAngle) const;
	bool to_euler_angles_yxz (float& rfYAngle, float& rfPAngle, float& rfRAngle) const;
	bool to_euler_angles_yzx (float& rfYAngle, float& rfPAngle, float& rfRAngle) const;
	bool to_euler_angles_zxy (float& rfYAngle, float& rfPAngle, float& rfRAngle) const;
	bool to_euler_angles_zyx (float& rfYAngle, float& rfPAngle, float& rfRAngle) const;
	void from_euler_angles_xyz (const float& fYAngle, const float& fPAngle, const float& fRAngle);
	void from_euler_angles_xzy (const float& fYAngle, const float& fPAngle, const float& fRAngle);
	void from_euler_angles_yxz (const float& fYAngle, const float& fPAngle, const float& fRAngle);
	void from_euler_angles_yzx (const float& fYAngle, const float& fPAngle, const float& fRAngle);
	void from_euler_angles_zxy (const float& fYAngle, const float& fPAngle, const float& fRAngle);
	void from_euler_angles_zyx (const float& fYAngle, const float& fPAngle, const float& fRAngle);

	static void tensor_product (const Vec3& rkU, const Vec3& rkV, Mat3& rkProduct);
};

struct Quat
{
	float w, x, y, z;

	static const Quat zero;
	static const Quat identity;
	static const float epsilon;

	inline Quat()
		: w(1), x(0), y(0), z(0)
	{
	}

	operator btQuaternion() const { return btQuaternion(x, y, z, w); }

	inline Quat(const btQuaternion& q)
		: w(q.getW()), x(q.getX()), y(q.getY()), z(q.getZ())
	{
	}

	inline Quat (
		float fW,
		float fX, float fY, float fZ)
		: w(fW), x(fX), y(fY), z(fZ)
	{
	}

	inline Quat(const Mat3& rot)
	{
		this->from_rotation_matrix(rot);
	}

	inline Quat(const float& rfAngle, const Vec3& rkAxis)
	{
		this->from_angle_axis(rfAngle, rkAxis);
	}

	inline Quat(const Vec3& xaxis, const Vec3& yaxis, const Vec3& zaxis)
	{
		this->from_axes(xaxis, yaxis, zaxis);
	}
	inline Quat(const Vec3* akAxis)
	{
		this->from_axes(akAxis);
	}

	inline Quat(float* valptr)
	{
		memcpy(&w, valptr, sizeof(float)*4);
	}

	inline float operator [] (const int i) const
	{
		return *(&w + i);
	}

	inline float& operator [] (const int i)
	{
		return *(&w + i);
	}

	void from_rotation_matrix(const Mat3& kRot);
	void to_rotation_matrix(Mat3& kRot) const;
	void from_angle_axis(const float& rfAngle, const Vec3& rkAxis);
	void to_angle_axis(float& rfAngle, Vec3& rkAxis) const;
	void from_axes (const Vec3* akAxis);
	void from_axes (const Vec3& xAxis, const Vec3& yAxis, const Vec3& zAxis);
	void to_axes (Vec3* akAxis) const;
	void to_axes (Vec3& xAxis, Vec3& yAxis, Vec3& zAxis) const;

	Vec3 x_axis(void) const;

	Vec3 y_axis(void) const;

	Vec3 z_axis(void) const;

	Quat operator+ (const Quat& rkQ) const;
	Quat operator- (const Quat& rkQ) const;
	Quat operator* (const Quat& rkQ) const;
	Quat operator* (float fScalar) const;
	friend Quat operator* (float fScalar, const Quat& rkQ);
	Quat operator- () const;

	inline bool operator== (const Quat& rhs) const
	{
		return (rhs.x == x) && (rhs.y == y) && (rhs.z == z) && (rhs.w == w);
	}

	inline bool operator!= (const Quat& rhs) const
	{
		return !operator==(rhs);
	}

	float dot(const Quat& rkQ) const;
	float length() const;
	float normalize(void); 
	static Quat normalize(const Quat& q);
	Quat inverse() const;  /// Apply to non-zero Quat
	Quat unit_inverse() const;  /// Apply to unit-length Quat
	Quat exp() const;
	Quat log() const;

	/// Rotation of a vector by a Quat
	Vec3 operator* (const Vec3& rkVector) const;

	static Quat euler(float pitch, float yaw, float roll);

	static float angle(const Quat& a, const Quat& b);

	static Quat look(const Vec3& dir);

	static Quat slerp(float fT, const Quat& rkP, const Quat& rkQ);

	static Quat slerp_extra_spins(float fT, const Quat& rkP, const Quat& rkQ, int iExtraSpins);

	/// Setup for spherical quadratic interpolation
	static void intermediate(const Quat& rkQ0, const Quat& rkQ1, const Quat& rkQ2, Quat& rka, Quat& rkB);

	/// Spherical quadratic interpolation
	static Quat squad(float fT, const Quat& rkP, const Quat& rkA, const Quat& rkB, const Quat& rkQ);

	static Quat nlerp(float fT, const Quat& rkP, const Quat& rkQ, bool shortestPath = false);
};

struct Mat4
{
	/// Indexed by [row][col].
	union {
		float m[4][4];
		float _m[16];
	};

	static const Mat4 zero;
	static const Mat4 identity;

	inline Mat4()
	{
	}

	inline Mat4(
		float m00, float m01, float m02, float m03,
		float m10, float m11, float m12, float m13,
		float m20, float m21, float m22, float m23,
		float m30, float m31, float m32, float m33 )
	{
		m[0][0] = m00;
		m[0][1] = m01;
		m[0][2] = m02;
		m[0][3] = m03;
		m[1][0] = m10;
		m[1][1] = m11;
		m[1][2] = m12;
		m[1][3] = m13;
		m[2][0] = m20;
		m[2][1] = m21;
		m[2][2] = m22;
		m[2][3] = m23;
		m[3][0] = m30;
		m[3][1] = m31;
		m[3][2] = m32;
		m[3][3] = m33;
	}

	inline Mat4(const Mat3& m3x3)
	{
		operator=(identity);
		operator=(m3x3);
	}
	
	inline Mat4(const Quat& rot)
	{
		Mat3 m3x3;
		rot.to_rotation_matrix(m3x3);
		operator=(identity);
		operator=(m3x3);
	}

	inline float* operator [] (int column)
	{
		return m[column];
	}

	inline const float* operator [] (int column) const
	{
		return m[column];
	}

	inline Mat4 concatenate(const Mat4 &m2) const
	{
		Mat4 r;
		r.m[0][0] = m[0][0] * m2.m[0][0] + m[0][1] * m2.m[1][0] + m[0][2] * m2.m[2][0] + m[0][3] * m2.m[3][0];
		r.m[0][1] = m[0][0] * m2.m[0][1] + m[0][1] * m2.m[1][1] + m[0][2] * m2.m[2][1] + m[0][3] * m2.m[3][1];
		r.m[0][2] = m[0][0] * m2.m[0][2] + m[0][1] * m2.m[1][2] + m[0][2] * m2.m[2][2] + m[0][3] * m2.m[3][2];
		r.m[0][3] = m[0][0] * m2.m[0][3] + m[0][1] * m2.m[1][3] + m[0][2] * m2.m[2][3] + m[0][3] * m2.m[3][3];

		r.m[1][0] = m[1][0] * m2.m[0][0] + m[1][1] * m2.m[1][0] + m[1][2] * m2.m[2][0] + m[1][3] * m2.m[3][0];
		r.m[1][1] = m[1][0] * m2.m[0][1] + m[1][1] * m2.m[1][1] + m[1][2] * m2.m[2][1] + m[1][3] * m2.m[3][1];
		r.m[1][2] = m[1][0] * m2.m[0][2] + m[1][1] * m2.m[1][2] + m[1][2] * m2.m[2][2] + m[1][3] * m2.m[3][2];
		r.m[1][3] = m[1][0] * m2.m[0][3] + m[1][1] * m2.m[1][3] + m[1][2] * m2.m[2][3] + m[1][3] * m2.m[3][3];

		r.m[2][0] = m[2][0] * m2.m[0][0] + m[2][1] * m2.m[1][0] + m[2][2] * m2.m[2][0] + m[2][3] * m2.m[3][0];
		r.m[2][1] = m[2][0] * m2.m[0][1] + m[2][1] * m2.m[1][1] + m[2][2] * m2.m[2][1] + m[2][3] * m2.m[3][1];
		r.m[2][2] = m[2][0] * m2.m[0][2] + m[2][1] * m2.m[1][2] + m[2][2] * m2.m[2][2] + m[2][3] * m2.m[3][2];
		r.m[2][3] = m[2][0] * m2.m[0][3] + m[2][1] * m2.m[1][3] + m[2][2] * m2.m[2][3] + m[2][3] * m2.m[3][3];

		r.m[3][0] = m[3][0] * m2.m[0][0] + m[3][1] * m2.m[1][0] + m[3][2] * m2.m[2][0] + m[3][3] * m2.m[3][0];
		r.m[3][1] = m[3][0] * m2.m[0][1] + m[3][1] * m2.m[1][1] + m[3][2] * m2.m[2][1] + m[3][3] * m2.m[3][1];
		r.m[3][2] = m[3][0] * m2.m[0][2] + m[3][1] * m2.m[1][2] + m[3][2] * m2.m[2][2] + m[3][3] * m2.m[3][2];
		r.m[3][3] = m[3][0] * m2.m[0][3] + m[3][1] * m2.m[1][3] + m[3][2] * m2.m[2][3] + m[3][3] * m2.m[3][3];

		return r;
	}

	inline Mat4 operator * (const Mat4 &m2) const
	{
		return concatenate( m2 );
	}

	inline Vec4 operator * (const Vec4& v) const
	{
		return Vec4(
			m[0][0] * v.x + m[1][0] * v.y + m[2][0] * v.z + m[3][0] * v.w, 
			m[0][1] * v.x + m[1][1] * v.y + m[2][1] * v.z + m[3][1] * v.w,
			m[0][2] * v.x + m[1][2] * v.y + m[2][2] * v.z + m[3][2] * v.w,
			m[0][3] * v.x + m[1][3] * v.y + m[2][3] * v.z + m[3][3] * v.w
			);
	}

	inline Plane operator * (const Plane& p) const
	{
		Plane ret;
		Mat4 invTrans = inverse().transpose();
		Vec4 v4( p.normal.x, p.normal.y, p.normal.z, p.d );
		v4 = invTrans * v4;
		ret.normal.x = v4.x; 
		ret.normal.y = v4.y; 
		ret.normal.z = v4.z;
		ret.d = v4.w / ret.normal.normalize();

		return ret;
	}

	inline Mat4 operator + (const Mat4 &m2) const
	{
		Mat4 r;

		r.m[0][0] = m[0][0] + m2.m[0][0];
		r.m[0][1] = m[0][1] + m2.m[0][1];
		r.m[0][2] = m[0][2] + m2.m[0][2];
		r.m[0][3] = m[0][3] + m2.m[0][3];

		r.m[1][0] = m[1][0] + m2.m[1][0];
		r.m[1][1] = m[1][1] + m2.m[1][1];
		r.m[1][2] = m[1][2] + m2.m[1][2];
		r.m[1][3] = m[1][3] + m2.m[1][3];

		r.m[2][0] = m[2][0] + m2.m[2][0];
		r.m[2][1] = m[2][1] + m2.m[2][1];
		r.m[2][2] = m[2][2] + m2.m[2][2];
		r.m[2][3] = m[2][3] + m2.m[2][3];

		r.m[3][0] = m[3][0] + m2.m[3][0];
		r.m[3][1] = m[3][1] + m2.m[3][1];
		r.m[3][2] = m[3][2] + m2.m[3][2];
		r.m[3][3] = m[3][3] + m2.m[3][3];

		return r;
	}

	inline Mat4 operator - (const Mat4 &m2) const
	{
		Mat4 r;
		r.m[0][0] = m[0][0] - m2.m[0][0];
		r.m[0][1] = m[0][1] - m2.m[0][1];
		r.m[0][2] = m[0][2] - m2.m[0][2];
		r.m[0][3] = m[0][3] - m2.m[0][3];

		r.m[1][0] = m[1][0] - m2.m[1][0];
		r.m[1][1] = m[1][1] - m2.m[1][1];
		r.m[1][2] = m[1][2] - m2.m[1][2];
		r.m[1][3] = m[1][3] - m2.m[1][3];

		r.m[2][0] = m[2][0] - m2.m[2][0];
		r.m[2][1] = m[2][1] - m2.m[2][1];
		r.m[2][2] = m[2][2] - m2.m[2][2];
		r.m[2][3] = m[2][3] - m2.m[2][3];

		r.m[3][0] = m[3][0] - m2.m[3][0];
		r.m[3][1] = m[3][1] - m2.m[3][1];
		r.m[3][2] = m[3][2] - m2.m[3][2];
		r.m[3][3] = m[3][3] - m2.m[3][3];

		return r;
	}

	inline bool operator == ( const Mat4& m2 ) const
	{
		if( 
			m[0][0] != m2.m[0][0] || m[0][1] != m2.m[0][1] || m[0][2] != m2.m[0][2] || m[0][3] != m2.m[0][3] ||
			m[1][0] != m2.m[1][0] || m[1][1] != m2.m[1][1] || m[1][2] != m2.m[1][2] || m[1][3] != m2.m[1][3] ||
			m[2][0] != m2.m[2][0] || m[2][1] != m2.m[2][1] || m[2][2] != m2.m[2][2] || m[2][3] != m2.m[2][3] ||
			m[3][0] != m2.m[3][0] || m[3][1] != m2.m[3][1] || m[3][2] != m2.m[3][2] || m[3][3] != m2.m[3][3] )
			return false;
		return true;
	}

	inline bool operator != ( const Mat4& m2 ) const
	{
		if( 
			m[0][0] != m2.m[0][0] || m[0][1] != m2.m[0][1] || m[0][2] != m2.m[0][2] || m[0][3] != m2.m[0][3] ||
			m[1][0] != m2.m[1][0] || m[1][1] != m2.m[1][1] || m[1][2] != m2.m[1][2] || m[1][3] != m2.m[1][3] ||
			m[2][0] != m2.m[2][0] || m[2][1] != m2.m[2][1] || m[2][2] != m2.m[2][2] || m[2][3] != m2.m[2][3] ||
			m[3][0] != m2.m[3][0] || m[3][1] != m2.m[3][1] || m[3][2] != m2.m[3][2] || m[3][3] != m2.m[3][3] )
			return true;
		return false;
	}

	inline void operator = ( const Mat3& mat3 )
	{
		m[0][0] = mat3.m[0][0]; m[0][1] = mat3.m[0][1]; m[0][2] = mat3.m[0][2];
		m[1][0] = mat3.m[1][0]; m[1][1] = mat3.m[1][1]; m[1][2] = mat3.m[1][2];
		m[2][0] = mat3.m[2][0]; m[2][1] = mat3.m[2][1]; m[2][2] = mat3.m[2][2];
	}

	inline Mat4 transpose(void) const
	{
		return Mat4(m[0][0], m[1][0], m[2][0], m[3][0],
					   m[0][1], m[1][1], m[2][1], m[3][1],
					   m[0][2], m[1][2], m[2][2], m[3][2],
					   m[0][3], m[1][3], m[2][3], m[3][3]);
	}

	inline void translation(const Vec3& v)
	{
		m[3][0] = v.x;
		m[3][1] = v.y;
		m[3][2] = v.z;
	}

	inline void translate(const Vec3& v)
	{
		m[3][0] += v.x;
		m[3][1] += v.y;
		m[3][2] += v.z;
	}

	inline void rotation(const Quat& q)
	{
		Mat3 r;
		q.to_rotation_matrix(r);
		operator=(r);
	}

	inline void rotation(const Mat3& r)
	{
		operator=(r);
	}

	inline Vec3 translation() const
	{
		return Vec3(m[3][0], m[3][1], m[3][2]);
	}
	
	inline void make_translate(const Vec3& v)
	{
		m[0][0] = 1.0; m[1][0] = 0.0; m[2][0] = 0.0; m[3][0] = v.x;
		m[0][1] = 0.0; m[1][1] = 1.0; m[2][1] = 0.0; m[3][1] = v.y;
		m[0][2] = 0.0; m[1][2] = 0.0; m[2][2] = 1.0; m[3][2] = v.z;
		m[0][3] = 0.0; m[1][3] = 0.0; m[2][3] = 0.0; m[3][3] = 1.0;
	}

	inline void make_translate(float tx, float ty, float tz)
	{
		m[0][0] = 1.0; m[1][0] = 0.0; m[2][0] = 0.0; m[3][0] = tx;
		m[0][1] = 0.0; m[1][1] = 1.0; m[2][1] = 0.0; m[3][1] = ty;
		m[0][2] = 0.0; m[1][2] = 0.0; m[2][2] = 1.0; m[3][2] = tz;
		m[0][3] = 0.0; m[1][3] = 0.0; m[2][3] = 0.0; m[3][3] = 1.0;
	}

	inline static Mat4 make_translation(const Vec3& v)
	{
		Mat4 r;

		r.m[0][0] = 1.0; r.m[1][0] = 0.0; r.m[2][0] = 0.0; r.m[3][0] = v.x;
		r.m[0][1] = 0.0; r.m[1][1] = 1.0; r.m[2][1] = 0.0; r.m[3][1] = v.y;
		r.m[0][2] = 0.0; r.m[1][2] = 0.0; r.m[2][2] = 1.0; r.m[3][2] = v.z;
		r.m[0][3] = 0.0; r.m[1][3] = 0.0; r.m[2][3] = 0.0; r.m[3][3] = 1.0;

		return r;
	}

	inline static Mat4 make_translation(float t_x, float t_y, float t_z)
	{
		Mat4 r;

		r.m[0][0] = 1.0; r.m[1][0] = 0.0; r.m[2][0] = 0.0; r.m[3][0] = t_x;
		r.m[0][1] = 0.0; r.m[1][1] = 1.0; r.m[2][1] = 0.0; r.m[3][1] = t_y;
		r.m[0][2] = 0.0; r.m[1][2] = 0.0; r.m[2][2] = 1.0; r.m[3][2] = t_z;
		r.m[0][3] = 0.0; r.m[1][3] = 0.0; r.m[2][3] = 0.0; r.m[3][3] = 1.0;

		return r;
	}

	inline void scale(const Vec3& v)
	{
		m[0][0] *= v.x; m[1][0] *= v.y; m[2][0] *= v.z;
		m[0][1] *= v.x; m[1][1] *= v.y; m[2][1] *= v.z;
		m[0][2] *= v.x; m[1][2] *= v.y; m[2][2] *= v.z;
	}

	inline static Mat4 make_scale(const Vec3& v)
	{
		Mat4 r;
		r.m[0][0] = v.x; r.m[1][0] = 0.0; r.m[2][0] = 0.0; r.m[3][0] = 0.0;
		r.m[0][1] = 0.0; r.m[1][1] = v.y; r.m[2][1] = 0.0; r.m[3][1] = 0.0;
		r.m[0][2] = 0.0; r.m[1][2] = 0.0; r.m[2][2] = v.z; r.m[3][2] = 0.0;
		r.m[0][3] = 0.0; r.m[1][3] = 0.0; r.m[2][3] = 0.0; r.m[3][3] = 1.0;

		return r;
	}

	inline static Mat4 make_scale(float s_x, float s_y, float s_z)
	{
		Mat4 r;
		r.m[0][0] = s_x; r.m[1][0] = 0.0; r.m[2][0] = 0.0; r.m[3][0] = 0.0;
		r.m[0][1] = 0.0; r.m[1][1] = s_y; r.m[2][1] = 0.0; r.m[3][1] = 0.0;
		r.m[0][2] = 0.0; r.m[1][2] = 0.0; r.m[2][2] = s_z; r.m[3][2] = 0.0;
		r.m[0][3] = 0.0; r.m[1][3] = 0.0; r.m[2][3] = 0.0; r.m[3][3] = 1.0;

		return r;
	}

	inline void extract_mat3(Mat3& m3x3) const
	{
		m3x3.m[0][0] = m[0][0];
		m3x3.m[0][1] = m[0][1];
		m3x3.m[0][2] = m[0][2];
		m3x3.m[1][0] = m[1][0];
		m3x3.m[1][1] = m[1][1];
		m3x3.m[1][2] = m[1][2];
		m3x3.m[2][0] = m[2][0];
		m3x3.m[2][1] = m[2][1];
		m3x3.m[2][2] = m[2][2];
	}

	inline Quat extract_quat() const
	{
		Mat3 m3x3;
		extract_mat3(m3x3);
		return Quat(m3x3);
	}

	inline Mat4 operator*(float scalar) const
	{
		return Mat4(
			scalar*m[0][0], scalar*m[0][1], scalar*m[0][2], scalar*m[0][3],
			scalar*m[1][0], scalar*m[1][1], scalar*m[1][2], scalar*m[1][3],
			scalar*m[2][0], scalar*m[2][1], scalar*m[2][2], scalar*m[2][3],
			scalar*m[3][0], scalar*m[3][1], scalar*m[3][2], scalar*m[3][3]);
	}

	Mat4 adjoint() const;
	float determinant() const;
	Mat4 inverse() const;

	static Mat4 perspective(const float fov, const float aspect, const float near, const float far);
	static Mat4 orthographic(const float fov, const float aspect, const float near, const float far);
	static Mat4 look(const Vec3& eye, const Vec3& forward, const Vec3& up);

	void make_transform(const Vec3& position, const Vec3& scale, const Quat& orientation);

	void make_inverse_transform(const Vec3& position, const Vec3& scale, const Quat& orientation);

	void decomposition(Vec3& position, Vec3& scale, Quat& orientation) const;

	inline bool is_affine(void) const
	{
		return m[3][0] == 0 && m[3][1] == 0 && m[3][2] == 0 && m[3][3] == 1;
	}

	Mat4 inverse_affine(void) const;

	inline Mat4 concatenate_affine(const Mat4 &m2) const
	{
		return Mat4(
			m[0][0] * m2.m[0][0] + m[0][1] * m2.m[1][0] + m[0][2] * m2.m[2][0],
			m[0][0] * m2.m[0][1] + m[0][1] * m2.m[1][1] + m[0][2] * m2.m[2][1],
			m[0][0] * m2.m[0][2] + m[0][1] * m2.m[1][2] + m[0][2] * m2.m[2][2],
			m[0][0] * m2.m[0][3] + m[0][1] * m2.m[1][3] + m[0][2] * m2.m[2][3] + m[0][3],

			m[1][0] * m2.m[0][0] + m[1][1] * m2.m[1][0] + m[1][2] * m2.m[2][0],
			m[1][0] * m2.m[0][1] + m[1][1] * m2.m[1][1] + m[1][2] * m2.m[2][1],
			m[1][0] * m2.m[0][2] + m[1][1] * m2.m[1][2] + m[1][2] * m2.m[2][2],
			m[1][0] * m2.m[0][3] + m[1][1] * m2.m[1][3] + m[1][2] * m2.m[2][3] + m[1][3],

			m[2][0] * m2.m[0][0] + m[2][1] * m2.m[1][0] + m[2][2] * m2.m[2][0],
			m[2][0] * m2.m[0][1] + m[2][1] * m2.m[1][1] + m[2][2] * m2.m[2][1],
			m[2][0] * m2.m[0][2] + m[2][1] * m2.m[1][2] + m[2][2] * m2.m[2][2],
			m[2][0] * m2.m[0][3] + m[2][1] * m2.m[1][3] + m[2][2] * m2.m[2][3] + m[2][3],

			0, 0, 0, 1);
	}

	inline Vec3 transform_affine(const Vec3& v) const
	{
		return Vec3(
				m[0][0] * v.x + m[0][1] * v.y + m[0][2] * v.z + m[0][3], 
				m[1][0] * v.x + m[1][1] * v.y + m[1][2] * v.z + m[1][3],
				m[2][0] * v.x + m[2][1] * v.y + m[2][2] * v.z + m[2][3]);
	}

	inline Vec4 transform_affine(const Vec4& v) const
	{
		return Vec4(
			m[0][0] * v.x + m[0][1] * v.y + m[0][2] * v.z + m[0][3] * v.w, 
			m[1][0] * v.x + m[1][1] * v.y + m[1][2] * v.z + m[1][3] * v.w,
			m[2][0] * v.x + m[2][1] * v.y + m[2][2] * v.z + m[2][3] * v.w,
			v.w);
	}
};

inline Vec4 operator * (const Vec4& v, const Mat4& mat)
{
	return Vec4(
		v.x*mat[0][0] + v.y*mat[1][0] + v.z*mat[2][0] + v.w*mat[3][0],
		v.x*mat[0][1] + v.y*mat[1][1] + v.z*mat[2][1] + v.w*mat[3][1],
		v.x*mat[0][2] + v.y*mat[1][2] + v.z*mat[2][2] + v.w*mat[3][2],
		v.x*mat[0][3] + v.y*mat[1][3] + v.z*mat[2][3] + v.w*mat[3][3]
		);
}

namespace LMath
{
	Vec3 triangle_closest_point(const Vec3&, const Vec3&, const Vec3&, const Vec3&);

	inline float clampf(float t, float a, float b)
	{
		return fmin(b, fmax(a, t));
	}

	inline float lerpf(float t, float a, float b)
	{
		return a + (b - a) * t;
	}
}

}
