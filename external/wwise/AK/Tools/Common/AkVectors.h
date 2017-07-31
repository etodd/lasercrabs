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

// AkVectors.h
//

#pragma once

#include <AK/SoundEngine/Common/AkTypes.h>
#include <AK/SoundEngine/Common/AkSimd.h>
#include <AK/SoundEngine/Common/AkSpeakerVolumes.h>
#include <AK/SoundEngine/Common/IAkPluginMemAlloc.h>
#include <AK/Tools/Common/AkArray.h>
#include <AK/Tools/Common/AkObject.h>

#include <math.h>
#include <stdio.h>
#include <float.h>

//#define AKVBAP_DEBUG 1
//#define AKPORTALS_DEBUG

#define AKVECTORS_PI				(3.1415926535897932384626433832795f)
#define AKVECTORS_TWOPI				(6.283185307179586476925286766559f)
#define AKVECTORS_PIOVERTWO			(1.5707963267948966192313216916398f)
#define	AKVECTORS_EPSILON			(1.0e-38f)									// epsilon value for fast log(0)


class AkMatrix4x4
{
	static const int MAX_SIZE = 16;

	public:
	//-----------------------------------------------------------
	// Constructor/Destructor functions
	AkMatrix4x4(){}
	~AkMatrix4x4(){}

	//-----------------------------------------------------------
	// Basic vector operators
	AkMatrix4x4 operator/=(const AkReal32 f)
	{
		for (int i = 0; i < MAX_SIZE; i++)
			m_Data[i] /= f;

		return *this;
	}

	AkMatrix4x4 operator=(AkReal32 * in_Data)
	{
		for (int i = 0; i < MAX_SIZE; i++)
		{
			m_Data[i] = in_Data[i];
		}

		return *this;
	}

	AkReal32 m_Data[MAX_SIZE];
};

class AkMatrix3x3
{

public:
	//-----------------------------------------------------------
	// Constructor/Destructor functions
	AkMatrix3x3(){}
	~AkMatrix3x3(){}

	//-----------------------------------------------------------
	// Basic vector operators
	AkMatrix3x3 operator/=(const AkReal32 f)
	{
		for (int i = 0; i < 3; i++)
		{
			for (int j = 0; j < 3; j++)
			{
				m_Data[i][j] /= f;
			}
		}
		return *this;
	}

	AkReal32 m_Data[3][3];
};

class Ak4DVector
{
public:
	//-----------------------------------------------------------
	// Constructor/Destructor functions
	Ak4DVector()
	{
		v[0] = 0.0f;
		v[1] = 0.0f;
		v[2] = 0.0f;
		v[3] = 0.0f;
	}

	Ak4DVector(const AkVector& b)
	{
		v[0] = b.X;
		v[1] = b.Y;
		v[2] = b.Z;
		v[3] = 1;
	}

	~Ak4DVector(){}

	//-----------------------------------------------------------
	// Basic vector operators
	Ak4DVector operator=(const Ak4DVector& b)
	{
		v[0] = b.v[0];
		v[1] = b.v[1];
		v[2] = b.v[2];
		v[3] = b.v[3];

		return *this;
	}

	Ak4DVector operator/=(const AkReal32 f)
	{
		v[0] = v[0] / f;
		v[1] = v[1] / f;
		v[2] = v[2] / f;
		v[3] = v[3] / f;

		return *this;
	}

	Ak4DVector operator-(const Ak4DVector& b) const
	{
		Ak4DVector p;

		p.v[0] = v[0] - b.v[0];
		p.v[1] = v[1] - b.v[1];
		p.v[2] = v[2] - b.v[2];
		p.v[3] = v[3] - b.v[3];

		return p;
	}

	AkReal32	v[4];
};

struct Ak3DIntVector
{
public:
	Ak3DIntVector(){}
	Ak3DIntVector(AkInt32 x, AkInt32 y, AkInt32 z)
	{
		X = x;
		Y = y;
		Z = z;
	}

	~Ak3DIntVector(){}

	AkInt32		X;	///< X Position
	AkInt32		Y;	///< Y Position
	AkInt32		Z;	///< Z Position
};

class Ak3DVector
{
public:
	//-----------------------------------------------------------
	// Constructor/Destructor functions
	Ak3DVector() :
		X(0.f),
		Y(0.f),
		Z(0.f)
	{}

	Ak3DVector(
		AkReal32					x,
		AkReal32					y,
		AkReal32					z)
	{
		X = x;
		Y = y;
		Z = z;
	}
	Ak3DVector(const AkVector& b)
	{
		X = b.X;
		Y = b.Y;
		Z = b.Z;
	}
	explicit Ak3DVector(const AKSIMD_V4F32& in_v4f32)
	{
		X = AKSIMD_GETELEMENT_V4F32(in_v4f32, 0);
		Y = AKSIMD_GETELEMENT_V4F32(in_v4f32, 1);
		Z = AKSIMD_GETELEMENT_V4F32(in_v4f32, 2);
	}
	AkForceInline AKSIMD_V4F32 PointV4F32() const
	{
		AKSIMD_V4F32 v4f32;
		AKSIMD_GETELEMENT_V4F32(v4f32, 0) = X;
		AKSIMD_GETELEMENT_V4F32(v4f32, 1) = Y;
		AKSIMD_GETELEMENT_V4F32(v4f32, 2) = Z;
		AKSIMD_GETELEMENT_V4F32(v4f32, 3) = 1.f;
		return v4f32;
	}
	AkForceInline AKSIMD_V4F32 VectorV4F32() const
	{
		AKSIMD_V4F32 v4f32;
		AKSIMD_GETELEMENT_V4F32(v4f32, 0) = X;
		AKSIMD_GETELEMENT_V4F32(v4f32, 1) = Y;
		AKSIMD_GETELEMENT_V4F32(v4f32, 2) = Z;
		AKSIMD_GETELEMENT_V4F32(v4f32, 3) = 0.f;
		return v4f32;
	}
	~Ak3DVector() {}

	void Zero()
	{
		X = 0.f;
		Y = 0.f;
		Z = 0.f;
	}


	//-----------------------------------------------------------
	// Basic vector operators
	AkForceInline bool operator==(const Ak3DVector& b) const
	{
		return X == b.X && Y == b.Y && Z == b.Z;
	}

	AkForceInline bool operator!=(const Ak3DVector& b) const
	{
		return X != b.X || Y != b.Y || Z != b.Z;
	}

	AkForceInline Ak3DVector operator=(const Ak3DVector& b)
	{
		X = b.X;
		Y = b.Y;
		Z = b.Z;

		return *this;
	}

	AkForceInline Ak3DVector operator=(const AkVector& b)
	{
		X = b.X;
		Y = b.Y;
		Z = b.Z;

		return *this;
	}

	AkForceInline bool operator<(const Ak3DVector& b) const
	{
		return X < b.X && Y < b.Y && Z < b.Z;
	}

	AkForceInline bool operator<=(const Ak3DVector& b) const
	{
		return X <= b.X && Y <= b.Y && Z <= b.Z;
	}

	AkForceInline bool operator>(const Ak3DVector b) const
	{
		return X > b.X && Y > b.Y && Z > b.Z;
	}

	AkForceInline bool operator>=(const Ak3DVector& b) const
	{
		return X >= b.X && Y >= b.Y && Z >= b.Z;
	}

	AkForceInline Ak3DVector operator*=(const AkReal32 f)
	{
		X = X * f;
		Y = Y * f;
		Z = Z * f;

		return *this;
	}

	AkForceInline Ak3DVector operator/=(const AkReal32 f)
	{
		AkReal32 oneoverf = 1.f / f;
		X = X * oneoverf;
		Y = Y * oneoverf;
		Z = Z * oneoverf;

		return *this;
	}

	AkForceInline Ak3DVector operator*(const Ak3DVector v2) const
	{
		Ak3DVector v;

		v.X = X * v2.X;
		v.Y = Y * v2.Y;
		v.Z = Z * v2.Z;

		return v;
	}

	AkForceInline Ak3DVector operator*(const AkReal32 f) const
	{
		Ak3DVector v;

		v.X = X * f;
		v.Y = Y * f;
		v.Z = Z * f;

		return v;
	}

	AkForceInline Ak3DVector operator/(const AkReal32 f) const
	{
		Ak3DVector v;
		AkReal32 oneoverf = 1.f / f;

		v.X = X * oneoverf;
		v.Y = Y * oneoverf;
		v.Z = Z * oneoverf;

		return v;
	}

	AkForceInline Ak3DVector operator+(const AkReal32 f) const
	{
		Ak3DVector v;

		v.X = X + f;
		v.Y = Y + f;
		v.Z = Z + f;

		return v;
	}

	AkForceInline Ak3DVector operator-(const AkReal32 f) const
	{
		Ak3DVector v;

		v.X = X - f;
		v.Y = Y - f;
		v.Z = Z - f;

		return v;
	}

	AkForceInline Ak3DVector operator+(const Ak3DVector& b) const
	{
		Ak3DVector v;

		v.X = X + b.X;
		v.Y = Y + b.Y;
		v.Z = Z + b.Z;

		return v;
	}

	AkForceInline Ak3DVector operator-(const Ak3DVector& b) const
	{
		Ak3DVector v;

		v.X = X - b.X;
		v.Y = Y - b.Y;
		v.Z = Z - b.Z;

		return v;
	}

	AkForceInline operator AkVector()
	{
		AkVector v;
		v.X = X; v.Y = Y; v.Z = Z;

		return v;
	}


	AkForceInline static Ak3DVector Min(const Ak3DVector& A, const Ak3DVector& B)
	{
		Ak3DVector min;

		min.X = AkMin(A.X, B.X);
		min.Y = AkMin(A.Y, B.Y);
		min.Z = AkMin(A.Z, B.Z);

		return min;
	}

	AkForceInline static Ak3DVector Max(const Ak3DVector& A, const Ak3DVector& B)
	{
		Ak3DVector max;

		max.X = AkMax(A.X, B.X);
		max.Y = AkMax(A.Y, B.Y);
		max.Z = AkMax(A.Z, B.Z);

		return max;
	}

	//-----------------------------------------------------------
	// Conversion functions
	AkForceInline Ak3DVector Rotate180X_90Y() const
	{
		Ak3DVector v;

		v.X = -X;
		v.Y = Z;
		v.Z = -Y;

		return v;
	}

	AkForceInline Ak3DVector SphericalToCartesian(
		const AkReal32				azimuth,
		const AkReal32				elevation)
	{
		AkReal32 cosElevation = cosf(elevation);
		X = cosf(azimuth) *	cosElevation;
		Y = sinf(azimuth) *	cosElevation;
		Z = sinf(elevation);

		return *this;
	}

	// Determinant of 3 column vectors.
	static AkReal32 Determinant(
		const Ak3DVector &			a,
		const Ak3DVector &			b,
		const Ak3DVector &			c)
	{
		return	(a.X*b.Y*c.Z + a.Y*b.Z*c.X + a.Z*b.X*c.Y) -
			(a.Z*b.Y*c.X + a.Y*b.X*c.Z + a.X*b.Z*c.Y);
	}

	// Convert a vector to a different base
	AkForceInline Ak3DVector LinearCombination(
		const Ak3DVector&			A,
		const Ak3DVector&			B,
		const Ak3DVector&			C) const
	{
		Ak3DVector v;

		AkReal32 d = Determinant(A, B, C);

		if (d < AKVECTORS_EPSILON && d > -AKVECTORS_EPSILON)
		{
			v.X = 0.0f; v.Y = 0.0f; v.Z = 0.0f;
			return v;
		}

		// http://mathworld.wolfram.com/MatrixInverse.html
		Ak3DVector invA = Ak3DVector(B.Y*C.Z - B.Z*C.Y, A.Z*C.Y - A.Y*C.Z, A.Y*B.Z - A.Z*B.Y);
		Ak3DVector invB = Ak3DVector(B.Z*C.X - B.X*C.Z, A.X*C.Z - A.Z*C.X, A.Z*B.X - A.X*B.Z);
		Ak3DVector invC = Ak3DVector(B.X*C.Y - B.Y*C.X, A.Y*C.X - A.X*C.Y, A.X*B.Y - A.Y*B.X);

		AkReal32 oneover_d = 1.f / d;
		invA *= oneover_d;
		invB *= oneover_d;
		invC *= oneover_d;

		// Project coordinates using a vector to matrix multiplication
		v.X = X * invA.X + Y * invB.X + Z * invC.X;
		v.Y = X * invA.Y + Y * invB.Y + Z * invC.Y;
		v.Z = X * invA.Z + Y * invB.Z + Z * invC.Z;

		// v /= v.Length();

		return v;
	}

	AkForceInline void Normalize()
	{
		AkReal32 l = Length();
		//AKASSERT(l != 0.0f);

		if (l == 0.f)
			X = Y = Z = 0;

		X /= l;
		Y /= l;
		Z /= l;
	}

	AkForceInline AkReal32 L2_Norm() const
	{
		return sqrtf(X*X + Y*Y + Z*Z);
	}

	AkForceInline AkReal32 DotProduct(const Ak3DVector& v2) const
	{
		return X*v2.X + Y*v2.Y + Z*v2.Z;
	}

	AkForceInline AkReal32 Dot(const Ak3DVector& v2) const
	{
		return DotProduct(v2);
	}

	AkForceInline Ak3DVector Cross(const Ak3DVector& v) const
	{
		Ak3DVector uxv;
		const Ak3DVector& u = *this;

		uxv.X = u.Y*v.Z - u.Z*v.Y;
		uxv.Y = u.Z*v.X - u.X*v.Z;
		uxv.Z = u.X*v.Y - u.Y*v.X;

		return uxv;
	}
	//
	AkForceInline AkReal32 Length() const
	{
		return sqrtf(X*X + Y*Y + Z*Z);
	}

	AkForceInline AkReal32 LengthSquared() const
	{
		return X*X + Y*Y + Z*Z;
	}

	// Usefull in VBAP algorithm, only points that are a positive linear composition matters.
	AkForceInline bool IsAllPositive() const
	{
		const AkReal32 POSITIVE_TEST_EPSILON = 0.00001f;
		return X >= -POSITIVE_TEST_EPSILON &&
			Y >= -POSITIVE_TEST_EPSILON &&
			Z >= -POSITIVE_TEST_EPSILON;
	}

	AkForceInline Ak3DVector Abs() const
	{
		Ak3DVector abs = *this;
		abs.X = (AkReal32)fabs(abs.X);
		abs.Y = (AkReal32)fabs(abs.Y);
		abs.Z = (AkReal32)fabs(abs.Z);
		return abs;
	}

	AkReal32						X;
    AkReal32						Y;
    AkReal32						Z;
};

class Ak2DVector
{
public:
	//-----------------------------------------------------------
	// Constructor/Destructor functions
	Ak2DVector(){}
	~Ak2DVector(){}

	Ak2DVector(
		AkReal32					x,
		AkReal32					y)
	{
		X = x;
		Y = y;
	}

	//-----------------------------------------------------------
	// Basic vector operators
	AkForceInline Ak2DVector operator=(const Ak2DVector& b)
	{
		X = b.X;
		Y = b.Y;

		return *this;
	}

	AkForceInline Ak2DVector operator=(const AkSphericalCoord& b)
	{
		X = b.theta;
		Y = b.phi;

		return *this;
	}

	Ak2DVector operator-(const Ak2DVector& b) const
	{
		Ak2DVector v;

		v.X = X - b.X;
		v.Y = Y - b.Y;

		return v;
	}

	Ak2DVector operator*=(const AkReal32 f)
	{
		X = X * f;
		Y = Y * f;

		return *this;
	}

	Ak2DVector operator/=(const AkReal32 f)
	{
		AkReal32 oneoverf = 1.f / f;
		X = X * oneoverf;
		Y = Y * oneoverf;

		return *this;
	}

	AkForceInline bool operator==(const Ak2DVector& b) const
	{
		return b.X == X && b.Y == Y;
	}

	AkForceInline bool operator!=(const Ak2DVector& b) const
	{
		return b.X != X && b.Y != Y;
	}

	AkForceInline AkReal32 Length() const
	{
		return sqrtf(X*X+Y*Y);
	}

	//-----------------------------------------------------------
	// Conversion functions
	AkForceInline Ak2DVector CartesianToSpherical( const Ak3DVector& in_Cartesian )
	{
		// (radial, azimuth, elevation)
		AkReal32 r = sqrtf( in_Cartesian.X*in_Cartesian.X + in_Cartesian.Y*in_Cartesian.Y + in_Cartesian.Z*in_Cartesian.Z);
		AKASSERT( r != 0);

		X = atan2f(in_Cartesian.Y, in_Cartesian.X);
		Y = asinf(in_Cartesian.Z / r);

		NormalizeSpherical();

		return *this;
	}

	AkForceInline Ak2DVector LinearCombination(
		const Ak2DVector&			A,
		const Ak2DVector&			B) const
	{
		Ak2DVector v;

		// Project coordinates using a vector to matrix multiplication
		AkReal32 d = (A.X*B.Y - A.Y*B.X);

		if (d < AKVECTORS_EPSILON && d > -AKVECTORS_EPSILON)
		{
			v.X = 0.0f; v.Y = 0.0f;
			return v;
		}

		Ak2DVector invA = Ak2DVector( B.Y, -A.Y );
		Ak2DVector invB = Ak2DVector( -B.X, A.X );

		AkReal32 oneover_d = 1.f / d;
		invA *= oneover_d;
		invB *= oneover_d;

		v.X = X * invA.X + Y * invB.X;
		v.Y = X * invA.Y + Y * invB.Y;
		// v /= v.Length();

		return v;
	}

	AkForceInline Ak2DVector NormalizeSpherical() const
	{
		/*
			Normalise spherical coordinates.
				X (azimuthal)	-> [-PI, PI],		circle lies on xy plan,			0 is on X axix
				Y (elevation)	-> [-PI/2, PI/2],	half circle on Z axis,			0 on XY plan, PI/2 straigt up on Z axis.
		*/

		Ak2DVector v;

		v.X = X;
		v.Y = Y;

		if (X > AKVECTORS_PI)
			v.X = X - AKVECTORS_TWOPI;

		if (X < -AKVECTORS_PI)
			v.X = X + AKVECTORS_TWOPI;

		if (Y > AKVECTORS_PIOVERTWO)
			v.Y = Y - AKVECTORS_PI;

		if (Y < -AKVECTORS_PIOVERTWO)
			v.Y = Y + AKVECTORS_PI;

		AKASSERT(X<AKVECTORS_PI);
		AKASSERT(Y<AKVECTORS_PIOVERTWO);

		return v;
	}

	AkForceInline void NormalizeSpherical()
	{
		/*
			Normalise spherical coordinates.
				X (azimuthal)	-> [-PI, PI],		circle lies on xy plan,		0 is on X axix
				Y (elevation)	-> [-PI/2, PI/2],	half circle on Z axis,		0 on XY plan, PI/2 straigt up on Z axis.
		*/

		if (X > AKVECTORS_PI)
			X = X - AKVECTORS_TWOPI;

		if (X < -AKVECTORS_PI)
			X = X + AKVECTORS_TWOPI;

		if (Y > AKVECTORS_PIOVERTWO)
			Y = Y - AKVECTORS_PI;

		if (Y < -AKVECTORS_PIOVERTWO)
			Y = Y + AKVECTORS_PI;
	}

	// Useful in VBAP algorithm, only points that are a positive linear composition matters.
	AkForceInline bool IsAllPositive() const
	{
		const AkReal32 POSITIVE_TEST_EPSILON = 0.00001f; //0.005f;
		return X >= -POSITIVE_TEST_EPSILON &&
			Y >= -POSITIVE_TEST_EPSILON;
	}

    AkReal32						X;
    AkReal32						Y;
};

struct AkIntersectionPoints
{
	Ak3DVector points[2];
	AkUInt32 count;
};

class AkLine
{
public:
	AkLine()
	{
		mint = 1.175494351e-38F;
		maxt = 3.402823466e+38F;
	}

	AkLine(
		Ak3DVector					in_L,
		Ak3DVector					in_P
		)
	{
		L = in_L;
		P = in_P;
		mint = 1.175494351e-38F;
		maxt = 3.402823466e+38F;
	}

	Ak3DVector PointAt(AkReal32 t) const
	{
		return P + L*t;
	}

	bool Intersect(
		Ak3DVector A,
		Ak3DVector B)
	{
		Ak3DVector L2 = B - A;

		/*
		a (V1 X V2) = (P2 - P1) X V2
		If the lines intersect at a single point, then the resultant vectors
		on each side of this equation must be parallel, and the left side must
		not be the zero vector. We should check to make sure that this is
		true. Once we have checked this, we can solve for 'a' by taking the
		magnitude of each side and dividing. If the resultant vectors are
		parallel, but in opposite directions, then 'a' is the negative of the
		ratio of magnitudes. Once we have 'a' we can go back to the equation
		for L1 to find the intersection point.
		*/
		Ak3DVector V1 = L;
		Ak3DVector V2 = B - A;
		Ak3DVector P1 = P;
		Ak3DVector P2 = A;

		// k(V1 X V2) = (A - P) X V2

		Ak3DVector v1CrossV2 = V1.Cross(V2);
		AkReal32 det = Ak3DVector::Determinant(
			P2 - P1,
			V2,
			v1CrossV2
			);
		AkReal32 t = det / v1CrossV2.LengthSquared();

		det = Ak3DVector::Determinant(
			P2 - P1,
			V1,
			v1CrossV2
			);
		AkReal32 s = det / v1CrossV2.LengthSquared();

		AkReal32 distsqrd = ((P2 + V2*s) - (P1 + V1*t)).LengthSquared();

		if ((AkReal32)fabs(v1CrossV2.L2_Norm()) >= AKVECTORS_EPSILON
			&& distsqrd < 0.001
			&& s <= 1.0f )
		{
#ifdef AKPORTALS_DEBUG
			Ak3DVector minPoint = PointAt(t);

			char msg[256];
			sprintf(msg, "L1a=[%0.2f,%0.2f,%0.2f];\n", P.X, P.Y, P.Z); AKPLATFORM::OutputDebugMsg(msg);
			sprintf(msg, "L1b=[%0.2f,%0.2f,%0.2f];\n", V1.X + P.X, V1.Y + P.Y, V1.Z + P.Z); AKPLATFORM::OutputDebugMsg(msg);
			sprintf(msg, "L2a=[%0.2f,%0.2f,%0.2f];\n", A.X, A.Y, A.Z); AKPLATFORM::OutputDebugMsg(msg);
			sprintf(msg, "L2b=[%0.2f,%0.2f,%0.2f];\n", B.X, B.Y, B.Z); AKPLATFORM::OutputDebugMsg(msg);
			sprintf(msg, "%% t=%0.2f Min t=%0.2f, Max t=%0.2f\n", t, mint, maxt); AKPLATFORM::OutputDebugMsg(msg);
			sprintf(msg, "intrPoint=[%0.2f,%0.2f,%0.2f];\n", minPoint.X, minPoint.Y, minPoint.Z); AKPLATFORM::OutputDebugMsg(msg);
			sprintf(msg, "\n"); AKPLATFORM::OutputDebugMsg(msg);
#endif

			mint = AkMin(mint, t);
			maxt = AkMax(maxt, t);
			
			return true;
		}

#ifdef AKPORTALS_DEBUG
	//	char msg[256];
	//	sprintf(msg, "%% DISCARTED t=%0.2f Min t=%0.2f, Max t=%0.2f\n", t, mint, maxt); AKPLATFORM::OutputDebugMsg(msg);
#endif
		return false;
	}

	Ak3DVector L;
	Ak3DVector P;

	AkReal32 mint;
	AkReal32 maxt;
};

class AkPlane
{
public:
	AkPlane()
	{
	}

	AkPlane(
		Ak3DVector					in_p1,
		Ak3DVector					in_p2,
		Ak3DVector					in_p4
		)
	{
		SetPlane(
			in_p1,
			in_p2,
			in_p4);
	}

	~AkPlane()
	{
	}

	void SetPlane(
		Ak3DVector					in_p1,
		Ak3DVector					in_p2,
		Ak3DVector					in_p4
		)
	{
		// Reorder A-B-C to clockwwise if necessary
		AKASSERT(in_p1.X < 100000 && in_p1.X > -100000);
		AKASSERT(in_p1.Y < 100000 && in_p1.Y > -100000);
		AKASSERT(in_p1.Z < 100000 && in_p1.Z > -100000);

		AKASSERT(in_p2.X < 100000 && in_p2.X > -100000);
		AKASSERT(in_p2.Y < 100000 && in_p2.Y > -100000);
		AKASSERT(in_p2.Z < 100000 && in_p2.Z > -100000);

		AKASSERT(in_p4.X < 100000 && in_p4.X > -100000);
		AKASSERT(in_p4.Y < 100000 && in_p4.Y > -100000);
		AKASSERT(in_p4.Z < 100000 && in_p4.Z > -100000);

		p1 = in_p1;
		p2 = in_p2;
		p4 = in_p4;

		SetNormal();

		// Ax + By + Cz + D = 0
		// Find D using the normal and a point
		D = -(N.X*p1.X) - (N.Y*p1.Y) - (N.Z*p1.Z);
	}

#define EPSILON 0.01f
	bool DoesRayIntersect(
		const Ak3DVector&			in_Origin,
		const Ak3DVector&			in_Destination,
		Ak3DVector&					out_Intersection
		) const
	{
		AkReal32 A = N.X;
		AkReal32 B = N.Y;
		AkReal32 C = N.Z;

		Ak3DVector ray = in_Destination - in_Origin;
		AkReal32 rayLength = ray.Length();

		Ak3DVector intersect;

		// If ray is < EPSILON, use on of the point directly for the test and skip the linear projection
		if (rayLength <= EPSILON)
		{
			Ak3DVector temp = in_Origin - p1;
			AkReal32 dot = temp.DotProduct(N);
			if (dot < EPSILON && dot > -EPSILON)
			{
				intersect = in_Origin;
			}
			else
			{
				// For debug only, to remove
				out_Intersection = p1;
				return false;
			}

		}
		else
		{
			// Normalize ray
			ray.Normalize();

			// TODO: possible fix for all edge cases
			// 1) if ray len ~= 0, only check if one of the point is on target, ie: assign the intersect point

			// Is ray parallel to the plane?
			if ((A*ray.X + B*ray.Y + C*ray.Z) == 0.0f)
			{
				// For debug only, to remove
				AkReal32 t = -(A*in_Origin.X + B*in_Origin.Y + C*in_Origin.Z + D) / (A*ray.X + B*ray.Y + C*ray.Z);
				intersect = Ak3DVector(in_Origin.X + ray.X*t, in_Origin.Y + ray.Y*t, in_Origin.Z + ray.Z*t);
				out_Intersection = intersect; // For debugging
				return false;
			}


			// Distance along the ray where reflector is hit
			AkReal32 t = -(A*in_Origin.X + B*in_Origin.Y + C*in_Origin.Z + D) / (A*ray.X + B*ray.Y + C*ray.Z);

			// Is the ray going towards the plane? Is it long enough?
			if (t < -EPSILON || t >(rayLength + EPSILON))
			{
				// For debug only, to remove
				intersect = Ak3DVector(in_Origin.X + ray.X*t, in_Origin.Y + ray.Y*t, in_Origin.Z + ray.Z*t);
				out_Intersection = intersect; // For debugging
				return false; // The ray doesn't intersect
			}

			// Find the coordinate of intersection on the plane
			intersect = Ak3DVector(in_Origin.X + ray.X*t, in_Origin.Y + ray.Y*t, in_Origin.Z + ray.Z*t);
		}
		///////////////////////////////////////
		//
		//		p2____v3____p3
		//		|	  .   	|
		//		^	inter  	v4
		//		v1	        v
		//		|	    	|
		//		p1__ v2>___p4

		Ak3DVector v1 = p2 - p1;
		Ak3DVector v2 = p4 - p1;
		Ak3DVector vInter1 = intersect - p1;

		Ak3DVector p3 = p4 + v1;
		Ak3DVector v3 = p2 - p3;
		Ak3DVector v4 = p4 - p3;
		Ak3DVector vInter2 = intersect - p3;

		v1.Normalize(); v2.Normalize(); v3.Normalize(); v4.Normalize(); vInter1.Normalize(); vInter2.Normalize();

		// Since it's a square, the angle between the point of intersection and any segment of the pannel should be < 90 degree,
		// therefore the dot product of the two normalized vectors should be > 0
		AkReal32 dot1 = v1.DotProduct(vInter1);
		AkReal32 dot2 = v2.DotProduct(vInter1);
		AkReal32 dot3 = v3.DotProduct(vInter2);
		AkReal32 dot4 = v4.DotProduct(vInter2);

		out_Intersection = intersect;

		return dot1 >= -EPSILON && dot2 >= -EPSILON && dot3 >= -EPSILON && dot4 >= -EPSILON;
	}

	AkReal32 DistPoint_to_Plane(
		Ak3DVector			in_P,
		Ak3DVector&			out_B) const
	{
		AkReal32 distance = (AkReal32)(AkReal32)fabs(N.X * in_P.X + N.Y * in_P.Y + N.Z * in_P.Z + D);

		Ak3DVector pointToPlane = N;
		pointToPlane *= distance;

		out_B = in_P + pointToPlane;

		return (AkReal32)fabs(distance);
	}

	void SetReflection(
		AkReal32*			out_mat) const
	{
		// http://ami.ektf.hu/uploads/papers/finalpdf/AMI_40_from175to186.pd
		/* m_pReflectionMatrix
		reflection on z axis

		P0 (x0, y0, z0), P1 (x1, y1, z1) and P2 (x2, y2, z2),
		normal = (cx, cy, cz)
		d = -CxX0 - CyY0 - CzZ0

		Reflect =	1-2Cx^2		-2CxCy		-2CxCz		-2Cxd
		-2CxCy		1-2Cy^2		-2CyCz		-2Cyd
		-2CxCz		-2CyCz		1-2Cz^2		-2Czd
		0			0			0			1
		*/

		AkReal32 d = -(N.X*p1.X) - (N.Y*p1.Y) - (N.Z*p1.Z);

		out_mat[0] = 1 - 2 * N.X*N.X;		out_mat[1] = -2 * N.X*N.Y;				out_mat[2] = -2 * N.X*N.Z;				out_mat[3] = -2 * N.X*d;
		out_mat[0 + 4] = -2 * N.X*N.Y;		out_mat[1 + 4] = 1 - 2 * N.Y*N.Y;		out_mat[2 + 4] = -2 * N.Y*N.Z;			out_mat[3 + 4] = -2 * N.Y*d;
		out_mat[0 + 8] = -2 * N.X*N.Z;		out_mat[1 + 8] = -2 * N.Y*N.Z;			out_mat[2 + 8] = 1 - 2 * N.Z*N.Z;		out_mat[3 + 8] = -2 * N.Z*d;
		out_mat[0 + 12] = 0;				out_mat[1 + 12] = 0;					out_mat[2 + 12] = 0;					out_mat[3 + 12] = 1;
	}

	Ak3DVector GetN() const { return N; }
	AkReal32 GetD() const { return D; }

	bool FindIntersectionPoints(
		const AkPlane& in_PlaneB,
		AkIntersectionPoints& out_Intrs) const
	{
		out_Intrs.count = 0;

		// Use vector to solve A

		Ak3DVector point;

		Ak3DVector N1 = N;
		Ak3DVector N2 = in_PlaneB.GetN();
		AkReal32 D1 = D;
		AkReal32 D2 = in_PlaneB.GetD();

		Ak3DVector L = N1.Cross(N2);
		if (L.Length() < 0.001f)
		{
			return false; // The two planes are parallel
		}

		AkUInt8 pivotAxis = 0;

		if ((AkReal32)fabs(L.Y) > (AkReal32)fabs(L.X))
		{
			pivotAxis = 1;
			if ((AkReal32)fabs(L.Z) > (AkReal32)fabs(L.Y))
			{
				pivotAxis = 2;
			}
		}
		else if ((AkReal32)fabs(L.Z) > (AkReal32)fabs(L.X))
		{
			pivotAxis = 2;
		}

		/*
		Pu = ( N1v*D2 - N2v*D1 ) / Lw
		Pv = ( N2u*D1 - N1u*D2 ) / Lw
		Pz = 0
		*/

		switch (pivotAxis)
		{
		case 0:
			AKASSERT((AkReal32)fabs(L.X) > AKVECTORS_EPSILON);
			point.X = 0.f;
			point.Y = (N1.Z*D2 - N2.Z*D1) / L.X;
			point.Z = (N2.Y*D1 - N1.Y*D2) / L.X;
			break;
		case 1:
			AKASSERT((AkReal32)fabs(L.Y) > AKVECTORS_EPSILON);
			point.X = (N1.Z*D2 - N2.Z*D1) / L.Y;
			point.Y = 0.f;
			point.Z = (N2.X*D1 - N1.X*D2) / L.Y;
			break;
		case 2:
			AKASSERT((AkReal32)fabs(L.Z) > AKVECTORS_EPSILON);
			point.X = (N1.Y*D2 - N2.Y*D1) / L.Z;
			point.Y = (N2.X*D1 - N1.X*D2) / L.Z;
			point.Z = 0.f;
			break;
		};



		L.Normalize();

		AkLine intrLine = AkLine(L, point);
		AkLine intrLine2 = AkLine(L, point);

		//in_PlaneB.GetP1()

		// find min max
		AkUInt32 cpt = 0;
		AkUInt32 cpt2 = 0;
		Ak3DVector p3 = GetP2() + (GetP4() - GetP1());

#ifdef AKPORTALS_DEBUG
		char msg[256];
		sprintf(msg, "P1a=[%0.2f,%0.2f,%0.2f];\n", GetP1().X, GetP1().Y, GetP1().Z); AKPLATFORM::OutputDebugMsg(msg);
		sprintf(msg, "P2a=[%0.2f,%0.2f,%0.2f];\n", GetP2().X, GetP2().Y, GetP2().Z); AKPLATFORM::OutputDebugMsg(msg);
		sprintf(msg, "P4a=[%0.2f,%0.2f,%0.2f];\n", GetP4().X, GetP4().Y, GetP4().Z); AKPLATFORM::OutputDebugMsg(msg);

		sprintf(msg, "P1b=[%0.2f,%0.2f,%0.2f];\n", in_PlaneB.GetP1().X, in_PlaneB.GetP1().Y, in_PlaneB.GetP1().Z); AKPLATFORM::OutputDebugMsg(msg);
		sprintf(msg, "P2b=[%0.2f,%0.2f,%0.2f];\n", in_PlaneB.GetP2().X, in_PlaneB.GetP2().Y, in_PlaneB.GetP2().Z); AKPLATFORM::OutputDebugMsg(msg);
		sprintf(msg, "P4b=[%0.2f,%0.2f,%0.2f];\n", in_PlaneB.GetP4().X, in_PlaneB.GetP4().Y, in_PlaneB.GetP4().Z); AKPLATFORM::OutputDebugMsg(msg);

		sprintf(msg, "line1=[%0.2f,%0.2f,%0.2f];\n", point.X + L.X*1000.f, point.Y + L.Y*1000.f, point.Z + L.Z*1000.f); AKPLATFORM::OutputDebugMsg(msg);
		sprintf(msg, "line2=[%0.2f,%0.2f,%0.2f];\n", point.X - L.X*1000.f, point.Y - L.Y*500.f, point.Z - L.Z*500.f); AKPLATFORM::OutputDebugMsg(msg);


		sprintf(msg, "%% Plane intersec\n"); AKPLATFORM::OutputDebugMsg(msg);
#endif
		// for the four lines in rectangle
		// Find where the line is crossing with plane A
		if (intrLine.Intersect(GetP1(), GetP2())) cpt++;
		if (intrLine.Intersect(GetP1(), GetP4())) cpt++;
		if (intrLine.Intersect(GetP2(), p3)) cpt++;
		if (intrLine.Intersect(p3, GetP4())) cpt++;
		//AKASSERT(cpt == 2);

#ifdef AKPORTALS_DEBUG
		sprintf(msg, "%% Portal intersec\n"); AKPLATFORM::OutputDebugMsg(msg);
#endif

		// Find where the line is crossing with plane B
		p3 = in_PlaneB.GetP2() + (in_PlaneB.GetP4() - in_PlaneB.GetP1());
		if (intrLine2.Intersect(in_PlaneB.GetP1(), in_PlaneB.GetP2())) cpt2++;
		if (intrLine2.Intersect(in_PlaneB.GetP1(), in_PlaneB.GetP4())) cpt2++;
		if (intrLine2.Intersect(in_PlaneB.GetP2(), p3)) cpt2++;
		if (intrLine2.Intersect(p3, in_PlaneB.GetP4())) cpt2++;
		// **AKASSERT(cpt2 == 2 || cpt == 2);

		if (cpt < 2 || cpt2 < 2)
		{
#ifdef AKPORTALS_DEBUG
			sprintf(msg, "%% NON \n"); AKPLATFORM::OutputDebugMsg(msg);
			sprintf(msg, "%% _______________________\n"); AKPLATFORM::OutputDebugMsg(msg);
#endif
			return false;
		}

		AkReal32 start = AkMax(intrLine.mint, intrLine2.mint);
		AkReal32 end = AkMin(intrLine.maxt, intrLine2.maxt);

		Ak3DVector minPoint = intrLine.PointAt(start);
		Ak3DVector maxPoint = intrLine.PointAt(end);
#ifdef AKPORTALS_DEBUG
		sprintf(msg, "P1a=[%0.2f,%0.2f,%0.2f];\n", GetP1().X, GetP1().Y, GetP1().Z); AKPLATFORM::OutputDebugMsg(msg);
		sprintf(msg, "P2a=[%0.2f,%0.2f,%0.2f];\n", GetP2().X, GetP2().Y, GetP2().Z); AKPLATFORM::OutputDebugMsg(msg);
		sprintf(msg, "P4a=[%0.2f,%0.2f,%0.2f];\n", GetP4().X, GetP4().Y, GetP4().Z); AKPLATFORM::OutputDebugMsg(msg);

		sprintf(msg, "P1b=[%0.2f,%0.2f,%0.2f];\n", in_PlaneB.GetP1().X, in_PlaneB.GetP1().Y, in_PlaneB.GetP1().Z); AKPLATFORM::OutputDebugMsg(msg);
		sprintf(msg, "P2b=[%0.2f,%0.2f,%0.2f];\n", in_PlaneB.GetP2().X, in_PlaneB.GetP2().Y, in_PlaneB.GetP2().Z); AKPLATFORM::OutputDebugMsg(msg);
		sprintf(msg, "P4b=[%0.2f,%0.2f,%0.2f];\n", in_PlaneB.GetP4().X, in_PlaneB.GetP4().Y, in_PlaneB.GetP4().Z); AKPLATFORM::OutputDebugMsg(msg);

		sprintf(msg, "line1=[%0.2f,%0.2f,%0.2f];\n", point.X + L.X*1000.f, point.Y + L.Y*1000.f, point.Z + L.Z*1000.f); AKPLATFORM::OutputDebugMsg(msg);
		sprintf(msg, "line2=[%0.2f,%0.2f,%0.2f];\n", point.X - L.X*1000.f, point.Y - L.Y*500.f, point.Z - L.Z*500.f); AKPLATFORM::OutputDebugMsg(msg);

		sprintf(msg, "intr1=[%0.2f,%0.2f,%0.2f];\n", minPoint.X, minPoint.Y, minPoint.Z); AKPLATFORM::OutputDebugMsg(msg);
		sprintf(msg, "intr2=[%0.2f,%0.2f,%0.2f];\n", maxPoint.X, maxPoint.Y, maxPoint.Z); AKPLATFORM::OutputDebugMsg(msg);

		sprintf(msg, "%% _______________________\n"); AKPLATFORM::OutputDebugMsg(msg);
#endif
		out_Intrs.points[0] = minPoint;
		out_Intrs.points[1] = maxPoint;
		out_Intrs.count = 2;

		return true;
	}

	Ak3DVector GetP1() const { return p1; }
	Ak3DVector GetP2() const { return p2; }
	Ak3DVector GetP4() const { return p4; }

private:
	bool SetNormal()
	{
		//m_pNormal = (B-A) cross (C-A); normalize
		Ak3DVector a = p2 - p1;
		Ak3DVector b = p4 - p1;

		N = Ak3DVector(a.Y*b.Z - a.Z*b.Y, -(a.X*b.Z - a.Z*b.X), a.X*b.Y - a.Y*b.X);

		AkReal32 len = N.Length();
		AKASSERT(len > 0.f);

		if (len > 0)
		{
			N /= len;
		}
		else
		{
			// TODO handle invalid reflectors
			return false;
		}

		return true;
	};

	/*
	p2__________p3
	|	  .   	|
	^	inter  	v3
	v1	        v
	|	    	|
	p1__ v2>___p4
	*/

	Ak3DVector						p1;		// Bottom left
	Ak3DVector						p2;		// Top left
	Ak3DVector						p4;		// Tottom right
	Ak3DVector						N;		// Normal vector
	AkReal32						D;		// Plane equation: Ax + By + Cz = D => N.Xx + N.Yy + N.Zz = D
};

struct AkBoundingBox
{
	AkBoundingBox() :
		m_Min(Ak3DVector(FLT_MAX, FLT_MAX, FLT_MAX)),
		m_Max(Ak3DVector(-FLT_MAX, -FLT_MAX, -FLT_MAX))
	{}

	void Update(
		const Ak3DVector &		in_point
		)
	{
		if (m_Min.X > in_point.X)
			m_Min.X = in_point.X;

		if (m_Min.Y > in_point.Y)
			m_Min.Y = in_point.Y;

		if (m_Min.Z > in_point.Z)
			m_Min.Z = in_point.Z;

		if (m_Max.X < in_point.X)
			m_Max.X = in_point.X;

		if (m_Max.Y < in_point.Y)
			m_Max.Y = in_point.Y;

		if (m_Max.Z < in_point.Z)
			m_Max.Z = in_point.Z;
	}

	AkForceInline bool IsWithin(
		const Ak3DVector &		in_Point
		) const
	{
		return in_Point >= m_Min && in_Point <= m_Max;
	}

	AkForceInline bool IsWithin(
		const AkBoundingBox &	in_BB
		) const
	{
		return (m_Min.X <= in_BB.m_Max.X && m_Max.X >= in_BB.m_Min.X) &&
			(m_Min.Y <= in_BB.m_Max.Y && m_Max.Y >= in_BB.m_Min.Y) &&
			(m_Min.Z <= in_BB.m_Max.Z && m_Max.Z >= in_BB.m_Min.Z);
	}

	AkBoundingBox Intersect(
		const AkBoundingBox &	in_BB
	) const
	{
		AkBoundingBox result;
		
		result.m_Max.X = AkMin(m_Max.X, in_BB.m_Max.X);
		result.m_Max.Y = AkMin(m_Max.Y, in_BB.m_Max.Y);
		result.m_Max.Z = AkMin(m_Max.Z, in_BB.m_Max.Z);

		result.m_Min.X = AkMax(m_Min.X, in_BB.m_Min.X);
		result.m_Min.Y = AkMax(m_Min.Y, in_BB.m_Min.Y);
		result.m_Min.Z = AkMax(m_Min.Z, in_BB.m_Min.Z);
		
		return result;
	}

	// returns acos(in_fAngle)
	AkForceInline AkReal32 ACos(
		AkReal32			in_fAngle
		) const
	{
		AKASSERT((in_fAngle <= 1.0f) && (in_fAngle >= -1.0f));
		return acosf(in_fAngle);
	}

	AkForceInline bool IsEmpty() const
	{
		return (m_Min.X >= m_Max.X) || (m_Min.Y >= m_Max.Y) || (m_Min.Z >= m_Max.Z);
	}

	Ak3DVector						m_Min;
	Ak3DVector						m_Max;
};

class AkBox
{
public:
	AkBox()
	{
	}

	~AkBox()
	{
	}

	void Init(
		const Ak3DVector &		in_center,
		const Ak3DVector &		in_extent,
		const Ak3DVector &		in_Front,
		const Ak3DVector &		in_Up)
	{
		AKASSERT(fabs(in_Front.Length() - 1.f) < 0.001 && fabs(in_Up.Length() - 1.f) < 0.001);//Must be unit vectors.
		AKASSERT(fabs(in_Front.Dot(in_Up) - 0.f) < 0.001); //Must be orthogonal.

		m_Center = in_center;
		m_Extent = in_extent;

		m_Z = in_Front,
		m_Y = in_Up;
		m_X = m_Z.Cross(m_Y);
	}

	bool IsPointInBox(
		const Ak3DVector &		in_Point
		) const
	{
		Ak3DVector pt = in_Point - m_Center;
		return	fabs(pt.Dot(m_X)) <= m_Extent.X && fabs(pt.Dot(m_Y)) <= m_Extent.Y && fabs(pt.Dot(m_Z)) <= m_Extent.Z;
	}

	Ak3DVector GetSize() const { return m_Extent*2.f; }
	Ak3DVector GetCenter() const { return m_Center; }

	Ak3DVector GetUx() const { return m_X; }
	Ak3DVector GetUy() const { return m_Y; }
	Ak3DVector GetUz() const { return m_Z; }

	Ak3DVector GetFront() const { return m_Z; }
	Ak3DVector GetUp() const { return m_Y; }
	Ak3DVector GetSide() const { return m_X; }

	AkReal32 GetVolume() const
	{
		Ak3DVector size = GetSize();
		return size.X * size.Y * size.Z;
	}

	bool SeparatingAxisExists(
		const Ak3DVector&		L,
		const AkBox&			B
		) const
	{
		// Separating Axis Theorem for Oriented Bounding Boxes by Johnny Huynh
		const AkBox& A = *this;
		Ak3DVector T = B.GetCenter() - A.GetCenter();

		AkReal32 WA = A.m_Extent.X;
		AkReal32 HA = A.m_Extent.Y;
		AkReal32 DA = A.m_Extent.Z;

		AkReal32 WB = B.m_Extent.X;
		AkReal32 HB = B.m_Extent.Y;
		AkReal32 DB = B.m_Extent.Z;

		Ak3DVector Ax = A.GetUx();
		Ak3DVector Ay = A.GetUy();
		Ak3DVector Az = A.GetUz();

		Ak3DVector Bx = B.GetUx();
		Ak3DVector By = B.GetUy();
		Ak3DVector Bz = B.GetUz();

		/*
		| T • L | > | (WA*Ax) • L | + | (HA*Ay) • L | + |(DA*Az) • L | +
					| (WB*Bx) • L | +| (HB*By) • L | +| (DB*Bz) • L |*/

		AkReal32 left = (AkReal32)fabs(T.DotProduct(L));
		AkReal32 dpax = (AkReal32)fabs((Ax*WA).DotProduct(L));
		AkReal32 dpay = (AkReal32)fabs((Ay*HA).DotProduct(L));
		AkReal32 dpaz = (AkReal32)fabs((Az*DA).DotProduct(L));
		AkReal32 dpbx = (AkReal32)fabs((Bx*WB).DotProduct(L));
		AkReal32 dpby = (AkReal32)fabs((By*HB).DotProduct(L));
		AkReal32 dpbz = (AkReal32)fabs((Bz*DB).DotProduct(L));

		AkReal32 right = dpax + dpay + dpaz + dpbx + dpby + dpbz;

		return left > right;
	}

	void UpdateBoundingBox(AkBoundingBox& out_aabb) const
	{
		Ak3DVector x = m_X * m_Extent.X;
		out_aabb.Update(m_Center + x);
		out_aabb.Update(m_Center - x);
		Ak3DVector y = m_Y * m_Extent.Y;
		out_aabb.Update(m_Center + y);
		out_aabb.Update(m_Center - y);
		Ak3DVector Z = m_Z * m_Extent.Z;
		out_aabb.Update(m_Center + Z);
		out_aabb.Update(m_Center - Z);
	}


private:

	Ak3DVector						m_Center;
	Ak3DVector						m_Extent;

	//Orthonormal Axes
	Ak3DVector						m_X;
	Ak3DVector						m_Y;
	Ak3DVector						m_Z;
};
