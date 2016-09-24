//////////////////////////////////////////////////////////////////////
//
// Copyright 2014 Audiokinetic Inc. / All Rights Reserved
//
//////////////////////////////////////////////////////////////////////

// AkVectors.h
//

#pragma once

#include <AK/SoundEngine/Common/AkTypes.h>
#include <AK/SoundEngine/Common/AkSpeakerVolumes.h>
#include <AK/SoundEngine/Common/IAkPluginMemAlloc.h>
#include <AK/Tools/Common/AkArray.h>
#include <AK/Tools/Common/AkObject.h>
#include "AkMath.h"
#include <stdio.h>

//#define AKVBAP_DEBUG 1

//class Ak4DMatrix

#define PI					(3.1415926535897932384626433832795f)
#define TWOPI				(6.283185307179586476925286766559f)
#define PIOVERTWO			(1.5707963267948966192313216916398f)

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
	Ak3DVector():
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
	~Ak3DVector(){}

	void Zero()
	{
		X = 0.f;
		Y = 0.f;
		Z = 0.f;
	}


	//-----------------------------------------------------------
	// Basic vector operators
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

	Ak3DVector operator/(const AkReal32 f)
	{
		Ak3DVector v;
		AkReal32 oneoverf = 1.f / f;

		v.X = X * oneoverf;
		v.Y = Y * oneoverf;
		v.Z = Z * oneoverf;

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

	operator AkVector()
	{
		AkVector v;
		v.X = X; v.Y = Y; v.Z = Z;

		return v;
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
		const AkReal32				elevation )
	{
		AkReal32 cosElevation = cosf(elevation);
		X = cosf(azimuth) *	cosElevation;
		Y = sinf(azimuth) *	cosElevation;
		Z =					sinf(elevation);

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

		if (d < AK_EPSILON && d > -AK_EPSILON)
		{
			v.X = 0.0f; v.Y = 0.0f; v.Z = 0.0f;
			return v;
		}
		
		// http://mathworld.wolfram.com/MatrixInverse.html
		Ak3DVector invA = Ak3DVector(	B.Y*C.Z - B.Z*C.Y,		A.Z*C.Y-A.Y*C.Z,		A.Y*B.Z-A.Z*B.Y		);
		Ak3DVector invB = Ak3DVector(	B.Z*C.X - B.X*C.Z,		A.X*C.Z-A.Z*C.X,		A.Z*B.X-A.X*B.Z		);
		Ak3DVector invC = Ak3DVector(	B.X*C.Y - B.Y*C.X,		A.Y*C.X-A.X*C.Y,		A.X*B.Y-A.Y*B.X		);

		AkReal32 oneover_d = 1.f / d;
		invA *= oneover_d;
		invB *= oneover_d;
		invC *= oneover_d;

		// Project coordinates using a vector to matrix multiplication
		v.X = X * invA.X	+ Y * invB.X	+ Z * invC.X;
		v.Y = X * invA.Y	+ Y * invB.Y	+ Z * invC.Y;
		v.Z = X * invA.Z	+ Y * invB.Z	+ Z * invC.Z;

		// v /= v.Length();

		return v;
	}

	void Normalize()
	{
		AkReal32 l = Length();
		AKASSERT(l != 0.0f);
		X /= l;
		Y /= l;
		Z /= l;
	}

	AkReal32 DotProduct(Ak3DVector v2)
	{
		return X*v2.X + Y*v2.Y + Z*v2.Z;
	}

	AkForceInline AkReal32 Length() const
	{
		return sqrtf(X*X+Y*Y+Z*Z);
	}

	// Usefull in VBAP algorithm, only points that are a positive linear composition matters.
	AkForceInline bool IsAllPositive() const
	{
		const AkReal32 POSITIVE_TEST_EPSILON = 0.00001f;
		return X >= -POSITIVE_TEST_EPSILON &&
			Y >= -POSITIVE_TEST_EPSILON &&
			Z >= -POSITIVE_TEST_EPSILON;
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

	AkForceInline Ak2DVector operator-(const Ak2DVector& b) const
	{
		Ak2DVector v;

		v.X = X - b.X;
		v.Y = Y - b.Y;

		return v;
	}

	AkForceInline Ak2DVector operator*=(const AkReal32 f)
	{
		X = X * f;
		Y = Y * f;

		return *this;
	}

	AkForceInline Ak2DVector operator/=(const AkReal32 f)
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

		if (d < AK_EPSILON && d > -AK_EPSILON)
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

		if (X > PI)
			v.X = X - TWOPI;

		if (X < -PI)
			v.X = X + TWOPI;
		
		if (Y > PIOVERTWO)
			v.Y = Y - PI;

		if (Y < -PIOVERTWO)
			v.Y = Y + PI;

		AKASSERT(X<PI);
		AKASSERT(Y<PIOVERTWO);

		return v;
	}

	AkForceInline void NormalizeSpherical() 
	{
		/*
			Normalise spherical coordinates. 
				X (azimuthal)	-> [-PI, PI],		circle lies on xy plan,		0 is on X axix
				Y (elevation)	-> [-PI/2, PI/2],	half circle on Z axis,		0 on XY plan, PI/2 straigt up on Z axis.
		*/

		if (X > PI)
			X = X - TWOPI;

		if (X < -PI)
			X = X + TWOPI;
		
		if (Y > PIOVERTWO)
			Y = Y - PI;

		if (Y < -PIOVERTWO)
			Y = Y + PI;
	}

	// Useful in VBAP algorithm, only points that are a positive linear composition matters.
	AkForceInline bool IsAllPositive() const
	{
		const AkReal32 POSITIVE_TEST_EPSILON = 0.000001f;
		return X >= -POSITIVE_TEST_EPSILON &&
			Y >= -POSITIVE_TEST_EPSILON;
	}

    AkReal32						X;
    AkReal32						Y;
};
