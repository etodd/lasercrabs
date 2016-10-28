#include "lmath.h"

namespace VI
{

namespace LMath
{
	Vec3 triangle_closest_point(const Vec3& u, const Vec3& v, const Vec3& w, const Vec3& p)
	{
		Vec3 edge0 = v - u;
		Vec3 edge1 = w - u;
		Vec3 v0 = u - p;

		r32 a = edge0.dot(edge0);
		r32 b = edge0.dot(edge1);
		r32 c = edge1.dot(edge1);
		r32 d = edge0.dot(v0);
		r32 e = edge1.dot(v0);

		r32 det = a*c - b*b;
		r32 s = b*e - c*d;
		r32 t = b*d - a*e;

		if (s + t < det)
		{
			if (s < 0.f)
			{
				if (t < 0.f)
				{
					if (d < 0.f)
					{
						s = clampf(-d/a, 0.f, 1.f);
						t = 0.f;
					}
					else
					{
						s = 0.f;
						t = clampf(-e/c, 0.f, 1.f);
					}
				}
				else
				{
					s = 0.f;
					t = clampf(-e/c, 0.f, 1.f);
				}
			}
			else if (t < 0.f)
			{
				s = clampf(-d/a, 0.f, 1.f);
				t = 0.f;
			}
			else
			{
				r32 invDet = 1.f / det;
				s *= invDet;
				t *= invDet;
			}
		}
		else
		{
			if (s < 0.f)
			{
				r32 tmp0 = b+d;
				r32 tmp1 = c+e;
				if (tmp1 > tmp0)
				{
					r32 numer = tmp1 - tmp0;
					r32 denom = a-2*b+c;
					s = clampf(numer/denom, 0.f, 1.f);
					t = 1-s;
				}
				else
				{
					t = clampf(-e/c, 0.f, 1.f);
					s = 0.f;
				}
			}
			else if (t < 0.f)
			{
				if (a+d > b+e)
				{
					r32 numer = c+e-b-d;
					r32 denom = a-2*b+c;
					s = clampf(numer/denom, 0.f, 1.f);
					t = 1-s;
				}
				else
				{
					s = clampf(-e/c, 0.f, 1.f);
					t = 0.f;
				}
			}
			else
			{
				r32 numer = c+e-b-d;
				r32 denom = a-2*b+c;
				s = clampf(numer/denom, 0.f, 1.f);
				t = 1.f - s;
			}
		}

		return u + s * edge0 + t * edge1;
	}

	b8 ray_sphere_intersect(const Vec3& ray_start, const Vec3& ray_end, const Vec3& sphere_pos, r32 sphere_radius, Vec3* intersection)
	{
		Vec3 ray = ray_end - ray_start;
		Vec3 sphere_to_ray_start = ray_start - sphere_pos;

		r32 a = ray.length_squared();
		r32 b = 2.0f * ray.dot(sphere_to_ray_start);
		r32 c = sphere_to_ray_start.length_squared() - (sphere_radius * sphere_radius);

		r32 delta = (b * b) - 4.0f * a * c;

		if (delta > 0.0f)
		{
			r32 distance = (-b - sqrtf(delta)) / (2.0f * a);
			if (distance < 1.0f)
			{
				if (intersection)
					*intersection = ray_start + ray * distance;
				return true;
			}
		}
		return false;
	}

	const r32 INTENSITY_COEFFICIENT = 0.2f;

	Vec3 desaturate(const Vec3& c)
	{
		r32 intensity = c.x * INTENSITY_COEFFICIENT + c.y * INTENSITY_COEFFICIENT + c.z * INTENSITY_COEFFICIENT;
		return Vec3(intensity);
	}

	Vec4 desaturate(const Vec4& c)
	{
		r32 intensity = c.x * INTENSITY_COEFFICIENT + c.y * INTENSITY_COEFFICIENT + c.z * INTENSITY_COEFFICIENT;
		return Vec4(intensity, intensity, intensity, c.w);
	}
}

const Vec2 Vec2::zero(0, 0);
const Vec3 Vec3::zero(0, 0, 0);
const Vec4 Vec4::zero(0, 0, 0, 0);

Rect2 Rect2::outset(r32 padding) const
{
	Vec2 p(padding, padding);
	return
	{
		pos - p,
		size + p * 2.0f,
	};
}

Rect2 Rect2::pad(const Rect2& p) const
{
	return
	{
		pos - p.pos,
		size + p.pos + p.size,
	};
}

b8 Rect2::contains(const Vec2& p) const
{
	return p.x > pos.x
		&& p.y > pos.y
		&& p.x < pos.x + size.x
		&& p.y < pos.y + size.y;
}

const Quat Quat::zero(0, 0, 0, 0);
const Quat Quat::identity(1, 0, 0, 0);
const r32 Quat::epsilon = 1e-03f;

const Mat3 Mat3::zero(
		0, 0, 0,
		0, 0, 0,
		0, 0, 0);

const Mat3 Mat3::identity(
		1, 0, 0,
		0, 1, 0,
		0, 0, 1);

const Mat4 Mat4::zero(
		0, 0, 0, 0,
		0, 0, 0, 0,
		0, 0, 0, 0,
		0, 0, 0, 0);

const Mat4 Mat4::identity(
		1, 0, 0, 0,
		0, 1, 0, 0,
		0, 0, 1, 0,
		0, 0, 0, 1);

Plane::Plane()
{
	normal = Vec3::zero;
	d = 0.0;
}

Plane::Plane(const Plane& rhs)
{
	normal = rhs.normal;
	d = rhs.d;
}

Plane::Plane (const Vec3& rkNormal, r32 fConstant)
{
	normal = rkNormal;
	d = -fConstant;
}

Plane::Plane (r32 a, r32 b, r32 c, r32 _d)
	: normal(a, b, c), d(_d)
{
}

Plane::Plane (const Vec3& rkNormal, const Vec3& rkPoint)
{
	redefine(rkNormal, rkPoint);
}

Plane::Plane (const Vec3& rkPoint0, const Vec3& rkPoint1, const Vec3& rkPoint2)
{
	redefine(rkPoint0, rkPoint1, rkPoint2);
}

r32 Plane::distance (const Vec3& rkPoint) const
{
	return normal.dot(rkPoint) + d;
}

void Plane::redefine(const Vec3& rkPoint0, const Vec3& rkPoint1, const Vec3& rkPoint2)
{
	Vec3 kEdge1 = rkPoint1 - rkPoint0;
	Vec3 kEdge2 = rkPoint2 - rkPoint0;
	normal = kEdge1.cross(kEdge2);
	normal.normalize();
	d = -normal.dot(rkPoint0);
}

void Plane::redefine(const Vec3& rkNormal, const Vec3& rkPoint)
{
	normal = rkNormal;
	d = -rkNormal.dot(rkPoint);
}

Vec3 Plane::project(const Vec3& p) const
{
	// We know plane normal is unit length, so use simple method
	Mat3 xform;
	xform[0][0] = 1.0f - normal.x * normal.x;
	xform[0][1] = -normal.x * normal.y;
	xform[0][2] = -normal.x * normal.z;
	xform[1][0] = -normal.y * normal.x;
	xform[1][1] = 1.0f - normal.y * normal.y;
	xform[1][2] = -normal.y * normal.z;
	xform[2][0] = -normal.z * normal.x;
	xform[2][1] = -normal.z * normal.y;
	xform[2][2] = 1.0f - normal.z * normal.z;
	return xform * p;

}

r32 Plane::normalize(void)
{
	r32 fLength = normal.length();

	// Will also work for zero-sized vectors, but will change nothing
	// We're not using epsilons because we don't need to.
	// Read http://www.ogre3d.org/forums/viewtopic.php?f=4&t=61259
	if (fLength > r32(0.0f))
	{
		r32 fInvLength = 1.0f / fLength;
		normal *= fInvLength;
		d *= fInvLength;
	}

	return fLength;
}

Vec3 Mat3::get_column(s32 iCol) const
{
	return Vec3(m[0][iCol],m[1][iCol],
		m[2][iCol]);
}

void Mat3::set_column(s32 iCol, const Vec3& vec)
{
	m[0][iCol] = vec.x;
	m[1][iCol] = vec.y;
	m[2][iCol] = vec.z;

}

void Mat3::from_axes(const Vec3& xAxis, const Vec3& yAxis, const Vec3& zAxis)
{
	set_column(0,xAxis);
	set_column(1,yAxis);
	set_column(2,zAxis);
}

b8 Mat3::operator== (const Mat3& rkMatrix) const
{
	for (s32 iRow = 0; iRow < 3; iRow++)
	{
		for (s32 iCol = 0; iCol < 3; iCol++)
		{
			if (m[iRow][iCol] != rkMatrix.m[iRow][iCol])
				return false;
		}
	}

	return true;
}

Mat3 Mat3::operator+ (const Mat3& rkMatrix) const
{
	Mat3 kSum;
	for (s32 iRow = 0; iRow < 3; iRow++)
	{
		for (s32 iCol = 0; iCol < 3; iCol++)
			kSum.m[iRow][iCol] = m[iRow][iCol] + rkMatrix.m[iRow][iCol];
	}
	return kSum;
}

Mat3 Mat3::operator- (const Mat3& rkMatrix) const
{
	Mat3 kDiff;
	for (s32 iRow = 0; iRow < 3; iRow++)
	{
		for (s32 iCol = 0; iCol < 3; iCol++)
			kDiff.m[iRow][iCol] = m[iRow][iCol] - rkMatrix.m[iRow][iCol];
	}
	return kDiff;
}

Mat3 Mat3::operator* (const Mat3& rkMatrix) const
{
	Mat3 kProd;
	for (s32 iRow = 0; iRow < 3; iRow++)
	{
		for (s32 iCol = 0; iCol < 3; iCol++)
		{
			kProd.m[iRow][iCol] =
				m[iRow][0]*rkMatrix.m[0][iCol] +
				m[iRow][1]*rkMatrix.m[1][iCol] +
				m[iRow][2]*rkMatrix.m[2][iCol];
		}
	}
	return kProd;
}

Vec3 Mat3::operator* (const Vec3& rkPoint) const
{
	Vec3 kProd;
	for (s32 iRow = 0; iRow < 3; iRow++)
	{
		kProd[iRow] =
			m[iRow][0]*rkPoint[0] +
			m[iRow][1]*rkPoint[1] +
			m[iRow][2]*rkPoint[2];
	}
	return kProd;
}

Vec3 operator* (const Vec3& rkPoint, const Mat3& rkMatrix)
{
	Vec3 kProd;
	for (s32 iRow = 0; iRow < 3; iRow++)
	{
		kProd[iRow] =
			rkPoint[0]*rkMatrix.m[0][iRow] +
			rkPoint[1]*rkMatrix.m[1][iRow] +
			rkPoint[2]*rkMatrix.m[2][iRow];
	}
	return kProd;
}

Mat3 Mat3::operator- () const
{
	Mat3 kNeg;
	for (s32 iRow = 0; iRow < 3; iRow++)
	{
		for (s32 iCol = 0; iCol < 3; iCol++)
			kNeg[iRow][iCol] = -m[iRow][iCol];
	}
	return kNeg;
}

Mat3 Mat3::operator* (r32 fScalar) const
{
	Mat3 kProd;
	for (s32 iRow = 0; iRow < 3; iRow++)
	{
		for (s32 iCol = 0; iCol < 3; iCol++)
			kProd[iRow][iCol] = fScalar*m[iRow][iCol];
	}
	return kProd;
}

Mat3 operator* (r32 fScalar, const Mat3& rkMatrix)
{
	Mat3 kProd;
	for (s32 iRow = 0; iRow < 3; iRow++)
	{
		for (s32 iCol = 0; iCol < 3; iCol++)
			kProd[iRow][iCol] = fScalar*rkMatrix.m[iRow][iCol];
	}
	return kProd;
}

Mat3 Mat3::transpose() const
{
	Mat3 kTranspose;
	for (s32 iRow = 0; iRow < 3; iRow++)
	{
		for (s32 iCol = 0; iCol < 3; iCol++)
			kTranspose[iRow][iCol] = m[iCol][iRow];
	}
	return kTranspose;
}

b8 Mat3::inverse(Mat3& rkInverse, r32 fTolerance) const
{
	// Invert a 3x3 using cofactors.  This is about 8 times faster than
	// the Numerical Recipes code which uses Gaussian elimination.

	rkInverse[0][0] = m[1][1]*m[2][2] - m[1][2]*m[2][1];
	rkInverse[0][1] = m[0][2]*m[2][1] - m[0][1]*m[2][2];
	rkInverse[0][2] = m[0][1]*m[1][2] - m[0][2]*m[1][1];
	rkInverse[1][0] = m[1][2]*m[2][0] - m[1][0]*m[2][2];
	rkInverse[1][1] = m[0][0]*m[2][2] - m[0][2]*m[2][0];
	rkInverse[1][2] = m[0][2]*m[1][0] - m[0][0]*m[1][2];
	rkInverse[2][0] = m[1][0]*m[2][1] - m[1][1]*m[2][0];
	rkInverse[2][1] = m[0][1]*m[2][0] - m[0][0]*m[2][1];
	rkInverse[2][2] = m[0][0]*m[1][1] - m[0][1]*m[1][0];

	r32 fDet =
		m[0][0]*rkInverse[0][0] +
		m[0][1]*rkInverse[1][0]+
		m[0][2]*rkInverse[2][0];

	if (fabs(fDet) <= fTolerance)
		return false;

	r32 fInvDet = 1.0f/fDet;
	for (s32 iRow = 0; iRow < 3; iRow++)
	{
		for (s32 iCol = 0; iCol < 3; iCol++)
			rkInverse[iRow][iCol] *= fInvDet;
	}

	return true;
}

Mat3 Mat3::inverse(r32 fTolerance) const
{
	Mat3 kInverse = Mat3::zero;
	inverse(kInverse,fTolerance);
	return kInverse;
}

r32 Mat3::determinant() const
{
	r32 fCofactor00 = m[1][1]*m[2][2] - m[1][2]*m[2][1];
	r32 fCofactor10 = m[1][2]*m[2][0] - m[1][0]*m[2][2];
	r32 fCofactor20 = m[1][0]*m[2][1] - m[1][1]*m[2][0];

	r32 fDet =
		m[0][0]*fCofactor00 +
		m[0][1]*fCofactor10 +
		m[0][2]*fCofactor20;

	return fDet;
}


void Mat3::orthonormalize()
{
	// Algorithm uses Gram-Schmidt orthogonalization.  If 'this' matrix is
	// M = [m0|m1|m2], then orthonormal output matrix is Q = [q0|q1|q2],
	//
	//   q0 = m0/|m0|
	//   q1 = (m1-(q0*m1)q0)/|m1-(q0*m1)q0|
	//   q2 = (m2-(q0*m2)q0-(q1*m2)q1)/|m2-(q0*m2)q0-(q1*m2)q1|
	//
	// where |V| indicates length of vector V and A*B indicates dot
	// product of vectors A and B.

	// compute q0
	r32 fInvLength = 1.0f / sqrt(m[0][0]*m[0][0]
		+ m[1][0]*m[1][0] +
		m[2][0]*m[2][0]);

	m[0][0] *= fInvLength;
	m[1][0] *= fInvLength;
	m[2][0] *= fInvLength;

	// compute q1
	r32 fDot0 =
		m[0][0]*m[0][1] +
		m[1][0]*m[1][1] +
		m[2][0]*m[2][1];

	m[0][1] -= fDot0*m[0][0];
	m[1][1] -= fDot0*m[1][0];
	m[2][1] -= fDot0*m[2][0];

	fInvLength = 1.0f / sqrt(m[0][1]*m[0][1] +
		m[1][1]*m[1][1] +
		m[2][1]*m[2][1]);

	m[0][1] *= fInvLength;
	m[1][1] *= fInvLength;
	m[2][1] *= fInvLength;

	// compute q2
	r32 fDot1 =
		m[0][1]*m[0][2] +
		m[1][1]*m[1][2] +
		m[2][1]*m[2][2];

	fDot0 =
		m[0][0]*m[0][2] +
		m[1][0]*m[1][2] +
		m[2][0]*m[2][2];

	m[0][2] -= fDot0*m[0][0] + fDot1*m[0][1];
	m[1][2] -= fDot0*m[1][0] + fDot1*m[1][1];
	m[2][2] -= fDot0*m[2][0] + fDot1*m[2][1];

	fInvLength = 1.0f / sqrt(m[0][2]*m[0][2] +
		m[1][2]*m[1][2] +
		m[2][2]*m[2][2]);

	m[0][2] *= fInvLength;
	m[1][2] *= fInvLength;
	m[2][2] *= fInvLength;
}

void Mat3::qdu_decomposition(Mat3& kQ, Vec3& kD, Vec3& kU) const
{
	// Factor M = QR = QDU where Q is orthogonal, D is diagonal,
	// and U is upper triangular with ones on its diagonal.  Algorithm uses
	// Gram-Schmidt orthogonalization (the QR algorithm).
	//
	// If M = [ m0 | m1 | m2 ] and Q = [ q0 | q1 | q2 ], then
	//
	//   q0 = m0/|m0|
	//   q1 = (m1-(q0*m1)q0)/|m1-(q0*m1)q0|
	//   q2 = (m2-(q0*m2)q0-(q1*m2)q1)/|m2-(q0*m2)q0-(q1*m2)q1|
	//
	// where |V| indicates length of vector V and A*B indicates dot
	// product of vectors A and B.  The matrix R has entries
	//
	//   r00 = q0*m0  r01 = q0*m1  r02 = q0*m2
	//   r10 = 0      r11 = q1*m1  r12 = q1*m2
	//   r20 = 0      r21 = 0      r22 = q2*m2
	//
	// so D = diag(r00,r11,r22) and U has entries u01 = r01/r00,
	// u02 = r02/r00, and u12 = r12/r11.

	// Q = rotation
	// D = scaling
	// U = shear

	// D stores the three diagonal entries r00, r11, r22
	// U stores the entries U[0] = u01, U[1] = u02, U[2] = u12

	// build orthogonal matrix Q
	r32 fInvLength = 1.0f / sqrt(m[0][0]*m[0][0] + m[1][0]*m[1][0] + m[2][0]*m[2][0]);

	kQ[0][0] = m[0][0]*fInvLength;
	kQ[1][0] = m[1][0]*fInvLength;
	kQ[2][0] = m[2][0]*fInvLength;

	r32 fDot = kQ[0][0]*m[0][1] + kQ[1][0]*m[1][1] +
		kQ[2][0]*m[2][1];
	kQ[0][1] = m[0][1]-fDot*kQ[0][0];
	kQ[1][1] = m[1][1]-fDot*kQ[1][0];
	kQ[2][1] = m[2][1]-fDot*kQ[2][0];
	fInvLength = 1.0f / sqrt(kQ[0][1]*kQ[0][1] + kQ[1][1]*kQ[1][1] + kQ[2][1]*kQ[2][1]);
	
	kQ[0][1] *= fInvLength;
	kQ[1][1] *= fInvLength;
	kQ[2][1] *= fInvLength;

	fDot = kQ[0][0]*m[0][2] + kQ[1][0]*m[1][2] + kQ[2][0]*m[2][2];
	kQ[0][2] = m[0][2]-fDot*kQ[0][0];
	kQ[1][2] = m[1][2]-fDot*kQ[1][0];
	kQ[2][2] = m[2][2]-fDot*kQ[2][0];
	fDot = kQ[0][1]*m[0][2] + kQ[1][1]*m[1][2] + kQ[2][1]*m[2][2];
	kQ[0][2] -= fDot*kQ[0][1];
	kQ[1][2] -= fDot*kQ[1][1];
	kQ[2][2] -= fDot*kQ[2][1];
	fInvLength = 1 / sqrt(kQ[0][2]*kQ[0][2] + kQ[1][2]*kQ[1][2] + kQ[2][2]*kQ[2][2]);

	kQ[0][2] *= fInvLength;
	kQ[1][2] *= fInvLength;
	kQ[2][2] *= fInvLength;

	// guarantee that orthogonal matrix has determinant 1 (no reflections)
	r32 fDet = kQ[0][0]*kQ[1][1]*kQ[2][2] + kQ[0][1]*kQ[1][2]*kQ[2][0] +
		kQ[0][2]*kQ[1][0]*kQ[2][1] - kQ[0][2]*kQ[1][1]*kQ[2][0] -
		kQ[0][1]*kQ[1][0]*kQ[2][2] - kQ[0][0]*kQ[1][2]*kQ[2][1];

	if (fDet < 0.0)
	{
		for (s32 iRow = 0; iRow < 3; iRow++)
			for (s32 iCol = 0; iCol < 3; iCol++)
				kQ[iRow][iCol] = -kQ[iRow][iCol];
	}

	// build "right" matrix R
	Mat3 kR;
	kR[0][0] = kQ[0][0]*m[0][0] + kQ[1][0]*m[1][0] + kQ[2][0]*m[2][0];
	kR[0][1] = kQ[0][0]*m[0][1] + kQ[1][0]*m[1][1] + kQ[2][0]*m[2][1];
	kR[1][1] = kQ[0][1]*m[0][1] + kQ[1][1]*m[1][1] + kQ[2][1]*m[2][1];
	kR[0][2] = kQ[0][0]*m[0][2] + kQ[1][0]*m[1][2] + kQ[2][0]*m[2][2];
	kR[1][2] = kQ[0][1]*m[0][2] + kQ[1][1]*m[1][2] + kQ[2][1]*m[2][2];
	kR[2][2] = kQ[0][2]*m[0][2] + kQ[1][2]*m[1][2] + kQ[2][2]*m[2][2];

	// the scaling component
	kD[0] = kR[0][0];
	kD[1] = kR[1][1];
	kD[2] = kR[2][2];

	// the shear component
	r32 fInvD0 = 1.0f/kD[0];
	kU[0] = kR[0][1]*fInvD0;
	kU[1] = kR[0][2]*fInvD0;
	kU[2] = kR[1][2]/kD[1];
}

void Mat3::to_angle_axis(Vec3& rkAxis, r32& rfr32s) const
{
	// Let (x,y,z) be the unit-length axis and let A be an angle of rotation.
	// The rotation matrix is R = I + sin(A)*P + (1-cos(A))*P^2 where
	// I is the identity and
	//
	//       +-        -+
	//   P = |  0 -z +y |
	//       | +z  0 -x |
	//       | -y +x  0 |
	//       +-        -+
	//
	// If A > 0, R represents a counterclockwise rotation about the axis in
	// the sense of looking from the tip of the axis vector towards the
	// origin.  Some algebra will show that
	//
	//   cos(A) = (trace(R)-1)/2  and  R - R^t = 2*sin(A)*P
	//
	// In the event that A = pi, R-R^t = 0 which prevents us from extracting
	// the axis through P.  Instead note that R = I+2*P^2 when A = pi, so
	// P^2 = (R-I)/2.  The diagonal entries of P^2 are x^2-1, y^2-1, and
	// z^2-1.  We can solve these for axis (x,y,z).  Because the angle is pi,
	// it does not matter which sign you choose on the square roots.

	r32 fTrace = m[0][0] + m[1][1] + m[2][2];
	r32 fCos = 0.5f*(fTrace-1.0f);
	rfr32s = acos(fCos);  // in [0,PI]

	if (rfr32s > r32(0.0))
	{
		if (rfr32s < r32(PI))
		{
			rkAxis.x = m[2][1]-m[1][2];
			rkAxis.y = m[0][2]-m[2][0];
			rkAxis.z = m[1][0]-m[0][1];
			rkAxis.normalize();
		}
		else
		{
			// angle is PI
			r32 fHalfInverse;
			if (m[0][0] >= m[1][1])
			{
				// r00 >= r11
				if (m[0][0] >= m[2][2])
				{
					// r00 is maximum diagonal term
					rkAxis.x = 0.5f*sqrt(m[0][0] -
						m[1][1] - m[2][2] + 1.0f);
					fHalfInverse = 0.5f/rkAxis.x;
					rkAxis.y = fHalfInverse*m[0][1];
					rkAxis.z = fHalfInverse*m[0][2];
				}
				else
				{
					// r22 is maximum diagonal term
					rkAxis.z = 0.5f*sqrt(m[2][2] -
						m[0][0] - m[1][1] + 1.0f);
					fHalfInverse = 0.5f/rkAxis.z;
					rkAxis.x = fHalfInverse*m[0][2];
					rkAxis.y = fHalfInverse*m[1][2];
				}
			}
			else
			{
				// r11 > r00
				if (m[1][1] >= m[2][2])
				{
					// r11 is maximum diagonal term
					rkAxis.y = 0.5f*sqrt(m[1][1] -
						m[0][0] - m[2][2] + 1.0f);
					fHalfInverse  = 0.5f/rkAxis.y;
					rkAxis.x = fHalfInverse*m[0][1];
					rkAxis.z = fHalfInverse*m[1][2];
				}
				else
				{
					// r22 is maximum diagonal term
					rkAxis.z = 0.5f*sqrt(m[2][2] -
						m[0][0] - m[1][1] + 1.0f);
					fHalfInverse = 0.5f/rkAxis.z;
					rkAxis.x = fHalfInverse*m[0][2];
					rkAxis.y = fHalfInverse*m[1][2];
				}
			}
		}
	}
	else
	{
		// The angle is 0 and the matrix is the identity.  Any axis will
		// work, so just use the x-axis.
		rkAxis.x = 1.0;
		rkAxis.y = 0.0;
		rkAxis.z = 0.0;
	}
}

void Mat3::from_angle_axis(const Vec3& rkAxis, const r32& fr32s)
{
	r32 fCos = cos(fr32s);
	r32 fSin = sin(fr32s);
	r32 fOneMinusCos = 1.0f-fCos;
	r32 fX2 = rkAxis.x*rkAxis.x;
	r32 fY2 = rkAxis.y*rkAxis.y;
	r32 fZ2 = rkAxis.z*rkAxis.z;
	r32 fXYM = rkAxis.x*rkAxis.y*fOneMinusCos;
	r32 fXZM = rkAxis.x*rkAxis.z*fOneMinusCos;
	r32 fYZM = rkAxis.y*rkAxis.z*fOneMinusCos;
	r32 fXSin = rkAxis.x*fSin;
	r32 fYSin = rkAxis.y*fSin;
	r32 fZSin = rkAxis.z*fSin;

	m[0][0] = fX2*fOneMinusCos+fCos;
	m[0][1] = fXYM-fZSin;
	m[0][2] = fXZM+fYSin;
	m[1][0] = fXYM+fZSin;
	m[1][1] = fY2*fOneMinusCos+fCos;
	m[1][2] = fYZM-fXSin;
	m[2][0] = fXZM-fYSin;
	m[2][1] = fYZM+fXSin;
	m[2][2] = fZ2*fOneMinusCos+fCos;
}

b8 Mat3::to_euler_angles_xyz(r32& rfYAngle, r32& rfPAngle, r32& rfRAngle) const
{
	// rot =  cy*cz          -cy*sz           sy
	//        cz*sx*sy+cx*sz  cx*cz-sx*sy*sz -cy*sx
	//       -cx*cz*sy+sx*sz  cz*sx+cx*sy*sz  cx*cy

	rfPAngle = r32(asin(m[0][2]));
	if (rfPAngle < r32(HALF_PI))
	{
		if (rfPAngle > r32(-HALF_PI))
		{
			rfYAngle = atan2f(-m[1][2],m[2][2]);
			rfRAngle = atan2f(-m[0][1],m[0][0]);
			return true;
		}
		else
		{
			// WARNING.  Not a unique solution.
			r32 fRmY = atan2f(m[1][0],m[1][1]);
			rfRAngle = r32(0.0);  // any angle works
			rfYAngle = rfRAngle - fRmY;
			return false;
		}
	}
	else
	{
		// WARNING.  Not a unique solution.
		r32 fRpY = atan2f(m[1][0],m[1][1]);
		rfRAngle = r32(0.0);  // any angle works
		rfYAngle = fRpY - rfRAngle;
		return false;
	}
}

b8 Mat3::to_euler_angles_xzy(r32& rfYAngle, r32& rfPAngle, r32& rfRAngle) const
{
	// rot =  cy*cz          -sz              cz*sy
	//        sx*sy+cx*cy*sz  cx*cz          -cy*sx+cx*sy*sz
	//       -cx*sy+cy*sx*sz  cz*sx           cx*cy+sx*sy*sz

	rfPAngle = asin(-m[0][1]);
	if (rfPAngle < r32(HALF_PI))
	{
		if (rfPAngle > r32(-HALF_PI))
		{
			rfYAngle = atan2f(m[2][1],m[1][1]);
			rfRAngle = atan2f(m[0][2],m[0][0]);
			return true;
		}
		else
		{
			// WARNING.  Not a unique solution.
			r32 fRmY = atan2f(-m[2][0],m[2][2]);
			rfRAngle = r32(0.0);  // any angle works
			rfYAngle = rfRAngle - fRmY;
			return false;
		}
	}
	else
	{
		// WARNING.  Not a unique solution.
		r32 fRpY = atan2f(-m[2][0],m[2][2]);
		rfRAngle = r32(0.0);  // any angle works
		rfYAngle = fRpY - rfRAngle;
		return false;
	}
}

b8 Mat3::to_euler_angles_yxz(r32& rfYAngle, r32& rfPAngle, r32& rfRAngle) const
{
	// rot =  cy*cz+sx*sy*sz  cz*sx*sy-cy*sz  cx*sy
	//        cx*sz           cx*cz          -sx
	//       -cz*sy+cy*sx*sz  cy*cz*sx+sy*sz  cx*cy

	rfPAngle = asin(-m[1][2]);
	if (rfPAngle < r32(HALF_PI))
	{
		if (rfPAngle > r32(-HALF_PI))
		{
			rfYAngle = atan2f(m[0][2],m[2][2]);
			rfRAngle = atan2f(m[1][0],m[1][1]);
			return true;
		}
		else
		{
			// WARNING.  Not a unique solution.
			r32 fRmY = atan2f(-m[0][1],m[0][0]);
			rfRAngle = r32(0.0);  // any angle works
			rfYAngle = rfRAngle - fRmY;
			return false;
		}
	}
	else
	{
		// WARNING.  Not a unique solution.
		r32 fRpY = atan2f(-m[0][1],m[0][0]);
		rfRAngle = r32(0.0);  // any angle works
		rfYAngle = fRpY - rfRAngle;
		return false;
	}
}

b8 Mat3::to_euler_angles_yzx(r32& rfYAngle, r32& rfPAngle, r32& rfRAngle) const
{
	// rot =  cy*cz           sx*sy-cx*cy*sz  cx*sy+cy*sx*sz
	//        sz              cx*cz          -cz*sx
	//       -cz*sy           cy*sx+cx*sy*sz  cx*cy-sx*sy*sz

	rfPAngle = asin(m[1][0]);
	if (rfPAngle < r32(HALF_PI))
	{
		if (rfPAngle > r32(-HALF_PI))
		{
			rfYAngle = atan2f(-m[2][0],m[0][0]);
			rfRAngle = atan2f(-m[1][2],m[1][1]);
			return true;
		}
		else
		{
			// WARNING.  Not a unique solution.
			r32 fRmY = atan2f(m[2][1],m[2][2]);
			rfRAngle = r32(0.0);  // any angle works
			rfYAngle = rfRAngle - fRmY;
			return false;
		}
	}
	else
	{
		// WARNING.  Not a unique solution.
		r32 fRpY = atan2f(m[2][1],m[2][2]);
		rfRAngle = r32(0.0);  // any angle works
		rfYAngle = fRpY - rfRAngle;
		return false;
	}
}

b8 Mat3::to_euler_angles_zxy(r32& rfYAngle, r32& rfPAngle, r32& rfRAngle) const
{
	// rot =  cy*cz-sx*sy*sz -cx*sz           cz*sy+cy*sx*sz
	//        cz*sx*sy+cy*sz  cx*cz          -cy*cz*sx+sy*sz
	//       -cx*sy           sx              cx*cy

	rfPAngle = asin(m[2][1]);
	if (rfPAngle < r32(HALF_PI))
	{
		if (rfPAngle > r32(-HALF_PI))
		{
			rfYAngle = atan2f(-m[0][1],m[1][1]);
			rfRAngle = atan2f(-m[2][0],m[2][2]);
			return true;
		}
		else
		{
			// WARNING.  Not a unique solution.
			r32 fRmY = atan2f(m[0][2],m[0][0]);
			rfRAngle = r32(0.0);  // any angle works
			rfYAngle = rfRAngle - fRmY;
			return false;
		}
	}
	else
	{
		// WARNING.  Not a unique solution.
		r32 fRpY = atan2f(m[0][2],m[0][0]);
		rfRAngle = r32(0.0);  // any angle works
		rfYAngle = fRpY - rfRAngle;
		return false;
	}
}

b8 Mat3::to_euler_angles_zyx(r32& rfYAngle, r32& rfPAngle, r32& rfRAngle) const
{
	// rot =  cy*cz           cz*sx*sy-cx*sz  cx*cz*sy+sx*sz
	//        cy*sz           cx*cz+sx*sy*sz -cz*sx+cx*sy*sz
	//       -sy              cy*sx           cx*cy

	rfPAngle = asin(-m[2][0]);
	if (rfPAngle < r32(HALF_PI))
	{
		if (rfPAngle > r32(-HALF_PI))
		{
			rfYAngle = atan2f(m[1][0],m[0][0]);
			rfRAngle = atan2f(m[2][1],m[2][2]);
			return true;
		}
		else
		{
			// WARNING.  Not a unique solution.
			r32 fRmY = atan2f(-m[0][1],m[0][2]);
			rfRAngle = r32(0.0);  // any angle works
			rfYAngle = rfRAngle - fRmY;
			return false;
		}
	}
	else
	{
		// WARNING.  Not a unique solution.
		r32 fRpY = atan2f(-m[0][1],m[0][2]);
		rfRAngle = r32(0.0);  // any angle works
		rfYAngle = fRpY - rfRAngle;
		return false;
	}
}

void Mat3::from_euler_angles_xyz(const r32& fYAngle, const r32& fPAngle, const r32& fRAngle)
{
	r32 fCos, fSin;

	fCos = cos(fYAngle);
	fSin = sin(fYAngle);
	Mat3 kXMat(1.0,0.0,0.0,0.0,fCos,-fSin,0.0,fSin,fCos);

	fCos = cos(fPAngle);
	fSin = sin(fPAngle);
	Mat3 kYMat(fCos,0.0,fSin,0.0,1.0,0.0,-fSin,0.0,fCos);

	fCos = cos(fRAngle);
	fSin = sin(fRAngle);
	Mat3 kZMat(fCos,-fSin,0.0,fSin,fCos,0.0,0.0,0.0,1.0);

	*this = kXMat*(kYMat*kZMat);
}

void Mat3::from_euler_angles_xzy(const r32& fYAngle, const r32& fPAngle, const r32& fRAngle)
{
	r32 fCos, fSin;

	fCos = cos(fYAngle);
	fSin = sin(fYAngle);
	Mat3 kXMat(1.0,0.0,0.0,0.0,fCos,-fSin,0.0,fSin,fCos);

	fCos = cos(fPAngle);
	fSin = sin(fPAngle);
	Mat3 kZMat(fCos,-fSin,0.0,fSin,fCos,0.0,0.0,0.0,1.0);

	fCos = cos(fRAngle);
	fSin = sin(fRAngle);
	Mat3 kYMat(fCos,0.0,fSin,0.0,1.0,0.0,-fSin,0.0,fCos);

	*this = kXMat*(kZMat*kYMat);
}

void Mat3::from_euler_angles_yxz(const r32& fYAngle, const r32& fPAngle, const r32& fRAngle)
{
	r32 fCos, fSin;

	fCos = cos(fYAngle);
	fSin = sin(fYAngle);
	Mat3 kYMat(fCos,0.0,fSin,0.0,1.0,0.0,-fSin,0.0,fCos);

	fCos = cos(fPAngle);
	fSin = sin(fPAngle);
	Mat3 kXMat(1.0,0.0,0.0,0.0,fCos,-fSin,0.0,fSin,fCos);

	fCos = cos(fRAngle);
	fSin = sin(fRAngle);
	Mat3 kZMat(fCos,-fSin,0.0,fSin,fCos,0.0,0.0,0.0,1.0);

	*this = kYMat*(kXMat*kZMat);
}

void Mat3::from_euler_angles_yzx(const r32& fYAngle, const r32& fPAngle, const r32& fRAngle)
{
	r32 fCos, fSin;

	fCos = cos(fYAngle);
	fSin = sin(fYAngle);
	Mat3 kYMat(fCos,0.0,fSin,0.0,1.0,0.0,-fSin,0.0,fCos);

	fCos = cos(fPAngle);
	fSin = sin(fPAngle);
	Mat3 kZMat(fCos,-fSin,0.0,fSin,fCos,0.0,0.0,0.0,1.0);

	fCos = cos(fRAngle);
	fSin = sin(fRAngle);
	Mat3 kXMat(1.0,0.0,0.0,0.0,fCos,-fSin,0.0,fSin,fCos);

	*this = kYMat*(kZMat*kXMat);
}

void Mat3::from_euler_angles_zxy(const r32& fYAngle, const r32& fPAngle, const r32& fRAngle)
{
	r32 fCos, fSin;

	fCos = cos(fYAngle);
	fSin = sin(fYAngle);
	Mat3 kZMat(fCos,-fSin,0.0,fSin,fCos,0.0,0.0,0.0,1.0);

	fCos = cos(fPAngle);
	fSin = sin(fPAngle);
	Mat3 kXMat(1.0,0.0,0.0,0.0,fCos,-fSin,0.0,fSin,fCos);

	fCos = cos(fRAngle);
	fSin = sin(fRAngle);
	Mat3 kYMat(fCos,0.0,fSin,0.0,1.0,0.0,-fSin,0.0,fCos);

	*this = kZMat*(kXMat*kYMat);
}

void Mat3::from_euler_angles_zyx(const r32& fYAngle, const r32& fPAngle, const r32& fRAngle)
{
	r32 fCos, fSin;

	fCos = cos(fYAngle);
	fSin = sin(fYAngle);
	Mat3 kZMat(fCos,-fSin,0.0,fSin,fCos,0.0,0.0,0.0,1.0);

	fCos = cos(fPAngle);
	fSin = sin(fPAngle);
	Mat3 kYMat(fCos,0.0,fSin,0.0,1.0,0.0,-fSin,0.0,fCos);

	fCos = cos(fRAngle);
	fSin = sin(fRAngle);
	Mat3 kXMat(1.0,0.0,0.0,0.0,fCos,-fSin,0.0,fSin,fCos);

	*this = kZMat*(kYMat*kXMat);
}

void Mat3::tensor_product(const Vec3& rkU, const Vec3& rkV, Mat3& rkProduct)
{
	for (s32 iRow = 0; iRow < 3; iRow++)
	{
		for (s32 iCol = 0; iCol < 3; iCol++)
			rkProduct[iRow][iCol] = rkU[iRow]*rkV[iCol];
	}
}

void Quat::from_rotation_matrix(const Mat3& kRot)
{
	// Algorithm in Ken Shoemake's article in 1987 SIGGRAPH course notes
	// article "Quat Calculus and Fast Animation".

	r32 fTrace = kRot[0][0]+kRot[1][1]+kRot[2][2];
	r32 fRoot;

	if (fTrace > 0.0)
	{
		// |w| > 1/2, may as well choose w > 1/2
		fRoot = sqrt(fTrace + 1.0f);  // 2w
		w = 0.5f*fRoot;
		fRoot = 0.5f/fRoot;  // 1/(4w)
		x = (kRot[1][2]-kRot[2][1])*fRoot;
		y = (kRot[2][0]-kRot[0][2])*fRoot;
		z = (kRot[0][1]-kRot[1][0])*fRoot;
	}
	else
	{
		// |w| <= 1/2
		static s32 s_iNext[3] = { 1, 2, 0 };
		s32 i = 0;
		if (kRot[1][1] > kRot[0][0])
			i = 1;
		if (kRot[2][2] > kRot[i][i])
			i = 2;
		s32 j = s_iNext[i];
		s32 k = s_iNext[j];

		fRoot = sqrt(kRot[i][i]-kRot[j][j]-kRot[k][k] + 1.0f);
		r32* apkQuat[3] = { &x, &y, &z };
		*apkQuat[i] = 0.5f*fRoot;
		fRoot = 0.5f/fRoot;
		w = (kRot[j][k]-kRot[k][j])*fRoot;
		*apkQuat[j] = (kRot[i][j]+kRot[j][i])*fRoot;
		*apkQuat[k] = (kRot[i][k]+kRot[k][i])*fRoot;
	}
}

void Quat::to_rotation_matrix(Mat3& kRot) const
{
	r32 fTx  = x+x;
	r32 fTy  = y+y;
	r32 fTz  = z+z;
	r32 fTwx = fTx*w;
	r32 fTwy = fTy*w;
	r32 fTwz = fTz*w;
	r32 fTxx = fTx*x;
	r32 fTxy = fTy*x;
	r32 fTxz = fTz*x;
	r32 fTyy = fTy*y;
	r32 fTyz = fTz*y;
	r32 fTzz = fTz*z;

	kRot[0][0] = 1.0f-(fTyy+fTzz);
	kRot[1][0] = fTxy-fTwz;
	kRot[2][0] = fTxz+fTwy;
	kRot[0][1] = fTxy+fTwz;
	kRot[1][1] = 1.0f-(fTxx+fTzz);
	kRot[2][1] = fTyz-fTwx;
	kRot[0][2] = fTxz-fTwy;
	kRot[1][2] = fTyz+fTwx;
	kRot[2][2] = 1.0f-(fTxx+fTyy);
}

void Quat::from_angle_axis(const r32& rfAngle, const Vec3& rkAxis)
{
	// assert:  axis[] is unit length
	//
	// The Quat representing the rotation is
	//   q = cos(A/2)+sin(A/2)*(x*i+y*j+z*k)

	r32 fHalfAngle (0.5f*rfAngle);
	r32 fSin = sin(fHalfAngle);
	w = cos(fHalfAngle);
	x = fSin*rkAxis.x;
	y = fSin*rkAxis.y;
	z = fSin*rkAxis.z;
}

void Quat::to_angle_axis(r32& rfAngle, Vec3& rkAxis) const
{
	// The Quat representing the rotation is
	//   q = cos(A/2)+sin(A/2)*(x*i+y*j+z*k)

	r32 fSqrLength = x*x+y*y+z*z;
	if (fSqrLength > 0.0f)
	{
		rfAngle = 2.0f*acos(w);
		r32 fInvLength = 1.0f / sqrt(fSqrLength);
		rkAxis.x = x*fInvLength;
		rkAxis.y = y*fInvLength;
		rkAxis.z = z*fInvLength;
	}
	else
	{
		// angle is 0 (mod 2*pi), so any axis will do
		rfAngle = r32(0.0);
		rkAxis.x = 1.0;
		rkAxis.y = 0.0;
		rkAxis.z = 0.0;
	}
}

void Quat::from_axes(const Vec3* akAxis)
{
	Mat3 kRot;

	for (s32 iCol = 0; iCol < 3; iCol++)
	{
		kRot[0][iCol] = akAxis[iCol].x;
		kRot[1][iCol] = akAxis[iCol].y;
		kRot[2][iCol] = akAxis[iCol].z;
	}

	from_rotation_matrix(kRot);
}

void Quat::from_axes(const Vec3& xaxis, const Vec3& yaxis, const Vec3& zaxis)
{
	Mat3 kRot;

	kRot[0][0] = xaxis.x;
	kRot[1][0] = xaxis.y;
	kRot[2][0] = xaxis.z;

	kRot[0][1] = yaxis.x;
	kRot[1][1] = yaxis.y;
	kRot[2][1] = yaxis.z;

	kRot[0][2] = zaxis.x;
	kRot[1][2] = zaxis.y;
	kRot[2][2] = zaxis.z;

	from_rotation_matrix(kRot);
}

void Quat::to_axes(Vec3* akAxis) const
{
	Mat3 kRot;

	to_rotation_matrix(kRot);

	for (s32 iCol = 0; iCol < 3; iCol++)
	{
		akAxis[iCol].x = kRot[0][iCol];
		akAxis[iCol].y = kRot[1][iCol];
		akAxis[iCol].z = kRot[2][iCol];
	}
}

Vec3 Quat::x_axis(void) const
{
	//r32 fTx  = 2.0*x;
	r32 fTy  = 2.0f*y;
	r32 fTz  = 2.0f*z;
	r32 fTwy = fTy*w;
	r32 fTwz = fTz*w;
	r32 fTxy = fTy*x;
	r32 fTxz = fTz*x;
	r32 fTyy = fTy*y;
	r32 fTzz = fTz*z;

	return Vec3(1.0f-(fTyy+fTzz), fTxy+fTwz, fTxz-fTwy);
}

Vec3 Quat::y_axis(void) const
{
	r32 fTx  = 2.0f*x;
	r32 fTy  = 2.0f*y;
	r32 fTz  = 2.0f*z;
	r32 fTwx = fTx*w;
	r32 fTwz = fTz*w;
	r32 fTxx = fTx*x;
	r32 fTxy = fTy*x;
	r32 fTyz = fTz*y;
	r32 fTzz = fTz*z;

	return Vec3(fTxy-fTwz, 1.0f-(fTxx+fTzz), fTyz+fTwx);
}

Vec3 Quat::z_axis(void) const
{
	r32 fTx  = 2.0f*x;
	r32 fTy  = 2.0f*y;
	r32 fTz  = 2.0f*z;
	r32 fTwx = fTx*w;
	r32 fTwy = fTy*w;
	r32 fTxx = fTx*x;
	r32 fTxz = fTz*x;
	r32 fTyy = fTy*y;
	r32 fTyz = fTz*y;

	return Vec3(fTxz+fTwy, fTyz-fTwx, 1.0f-(fTxx+fTyy));
}

void Quat::to_axes(Vec3& xaxis, Vec3& yaxis, Vec3& zaxis) const
{
	Mat3 kRot;

	to_rotation_matrix(kRot);

	xaxis.x = kRot[0][0];
	xaxis.y = kRot[1][0];
	xaxis.z = kRot[2][0];

	yaxis.x = kRot[0][1];
	yaxis.y = kRot[1][1];
	yaxis.z = kRot[2][1];

	zaxis.x = kRot[0][2];
	zaxis.y = kRot[1][2];
	zaxis.z = kRot[2][2];
}

Quat Quat::operator+ (const Quat& rkQ) const
{
	return Quat(w+rkQ.w,x+rkQ.x,y+rkQ.y,z+rkQ.z);
}

Quat Quat::operator- (const Quat& rkQ) const
{
	return Quat(w-rkQ.w,x-rkQ.x,y-rkQ.y,z-rkQ.z);
}

Quat Quat::operator* (const Quat& rkQ) const
{
	// NOTE:  Multiplication is not generally commutative, so in most
	// cases p*q != q*p.

	return Quat
	(
		w * rkQ.w - x * rkQ.x - y * rkQ.y - z * rkQ.z,
		w * rkQ.x + x * rkQ.w + y * rkQ.z - z * rkQ.y,
		w * rkQ.y + y * rkQ.w + z * rkQ.x - x * rkQ.z,
		w * rkQ.z + z * rkQ.w + x * rkQ.y - y * rkQ.x
	);
}

Quat Quat::operator* (r32 fScalar) const
{
	return Quat(fScalar*w,fScalar*x,fScalar*y,fScalar*z);
}

Quat operator* (r32 fScalar, const Quat& rkQ)
{
	return Quat(fScalar*rkQ.w,fScalar*rkQ.x,fScalar*rkQ.y,
		fScalar*rkQ.z);
}

Quat Quat::operator- () const
{
	return Quat(-w,-x,-y,-z);
}

r32 Quat::dot(const Quat& rkQ) const
{
	return w*rkQ.w+x*rkQ.x+y*rkQ.y+z*rkQ.z;
}

r32 Quat::length() const
{
	return sqrtf(w * w + x * x + y * y + z * z);
}

Quat Quat::inverse() const
{
	r32 fNorm = w*w+x*x+y*y+z*z;
	if (fNorm > 0.0)
	{
		r32 fInvNorm = 1.0f/fNorm;
		return Quat(w*fInvNorm,-x*fInvNorm,-y*fInvNorm,-z*fInvNorm);
	}
	else
	{
		// return an invalid result to flag the error
		return zero;
	}
}

Quat Quat::unit_inverse() const
{
	// assert:  'this' is unit length
	return Quat(w,-x,-y,-z);
}

Quat Quat::exp() const
{
	// If q = A*(x*i+y*j+z*k) where (x,y,z) is unit length, then
	// exp(q) = cos(A)+sin(A)*(x*i+y*j+z*k).  If sin(A) is near zero,
	// use exp(q) = cos(A)+A*(x*i+y*j+z*k) since A/sin(A) has limit 1.

	r32 fAngle (sqrt(x*x+y*y+z*z));
	r32 fSin = sin(fAngle);

	Quat kResult;
	kResult.w = cos(fAngle);

	if (fabs(fSin) >= epsilon)
	{
		r32 fCoeff = fSin/(fAngle);
		kResult.x = fCoeff*x;
		kResult.y = fCoeff*y;
		kResult.z = fCoeff*z;
	}
	else
	{
		kResult.x = x;
		kResult.y = y;
		kResult.z = z;
	}

	return kResult;
}

Quat Quat::log() const
{
	// If q = cos(A)+sin(A)*(x*i+y*j+z*k) where (x,y,z) is unit length, then
	// log(q) = A*(x*i+y*j+z*k).  If sin(A) is near zero, use log(q) =
	// sin(A)*(x*i+y*j+z*k) since sin(A)/A has limit 1.

	Quat kResult;
	kResult.w = 0.0;

	if (fabs(w) < 1.0)
	{
		r32 fAngle (acos(w));
		r32 fSin = sin(fAngle);
		if (fabs(fSin) >= epsilon)
		{
			r32 fCoeff = fAngle/fSin;
			kResult.x = fCoeff*x;
			kResult.y = fCoeff*y;
			kResult.z = fCoeff*z;
			return kResult;
		}
	}

	kResult.x = x;
	kResult.y = y;
	kResult.z = z;

	return kResult;
}

Vec3 Quat::operator* (const Vec3& v) const
{
	// nVidia SDK implementation
	Vec3 uv, uuv;
	Vec3 qvec(x, y, z);
	uv = qvec.cross(v);
	uuv = qvec.cross(uv);
	uv *= (2.0f * w);
	uuv *= 2.0f;

	return v + uv + uuv;

}

Quat Quat::euler(r32 roll, r32 yaw, r32 pitch)
{
	// Assuming the angles are in radians.
	r32 cy = cos(yaw * 0.5f);
	r32 sy = sin(yaw * 0.5f);
	r32 cr = cos(roll * 0.5f);
	r32 sr = sin(roll * 0.5f);
	r32 cp = cos(pitch * 0.5f);
	r32 sp = sin(pitch * 0.5f);
	r32 cycr = cy*cr;
	r32 sysr = sy*sr;

	// yaw, roll, pitch
	/*
	return Quat
	(
		cp*cycr  - sp*sysr,
		cp*sysr  + sp*cycr,
		cp*sy*cr + sp*cy*sr,
		cp*cy*sr - sp*sy*cr
	);
	*/

	// yaw, pitch, roll
	return Quat
	(
		cp*cycr  + sp*sysr,
		sp*cycr  + cp*sysr,
		cp*sy*cr - sp*cy*sr,
		cp*cy*sr - sp*sy*cr
	);
}

r32 Quat::angle(const Quat& a, const Quat& b)
{
	Quat c = a.inverse() * b;
	r32 angle;
	Vec3 axis;
	c.to_angle_axis(angle, axis);
	if (angle > PI)
		angle = fabs(angle - (PI * 2.0f));
	return angle;
}

Quat Quat::look(const Vec3& dir)
{
	if (fabs(dir.y - 1.0f) < epsilon)
		return Quat::euler(0, 0, PI * -0.5f);
	else if (fabs(dir.y + 1.0f) < epsilon)
		return Quat::euler(0, 0, PI * 0.5f);
	else
		return Quat::euler(0, atan2f(dir.x, dir.z), -asinf(dir.y));
}

Quat Quat::slerp(r32 amount, const Quat& quaternion1, const Quat& quaternion2)
{
	r32 num = quaternion1.x * quaternion2.x + quaternion1.y * quaternion2.y + quaternion1.z * quaternion2.z + quaternion1.w * quaternion2.w;
	b8 flag = false;
	if (num < 0.0f)
	{
		flag = true;
		num = -num;
	}
	r32 num2;
	r32 num3;
	if (num > 0.999999f)
	{
		num2 = 1.0f - amount;
		num3 = (flag ? (-amount) : amount);
	}
	else
	{
		r32 num4 = acosf(num);
		r32 num5 = 1.0f / sinf(num4);
		num2 = sinf((1.0f - amount) * num4) * num5;
		num3 = flag ? (-sinf(amount * num4) * num5) : (sinf(amount * num4) * num5);
	}
	Quat result;
	result.x = num2 * quaternion1.x + num3 * quaternion2.x;
	result.y = num2 * quaternion1.y + num3 * quaternion2.y;
	result.z = num2 * quaternion1.z + num3 * quaternion2.z;
	result.w = num2 * quaternion1.w + num3 * quaternion2.w;
	return result;
}

Quat Quat::slerp_extra_spins(r32 fT, const Quat& rkP, const Quat& rkQ, s32 iExtraSpins)
{
	r32 fCos = rkP.dot(rkQ);
	r32 fAngle (acos(fCos));

	if (fabs(fAngle) < epsilon)
		return rkP;

	r32 fSin = sin(fAngle);
	r32 fPhase (PI*iExtraSpins*fT);
	r32 fInvSin = 1.0f/fSin;
	r32 fCoeff0 = sin((1.0f-fT)*fAngle - fPhase)*fInvSin;
	r32 fCoeff1 = sin(fT*fAngle + fPhase)*fInvSin;
	return fCoeff0*rkP + fCoeff1*rkQ;
}

void Quat::intermediate(const Quat& rkQ0, const Quat& rkQ1, const Quat& rkQ2, Quat& rkA, Quat& rkB)
{
	// assert:  q0, q1, q2 are unit Quats

	Quat kQ0inv = rkQ0.unit_inverse();
	Quat kQ1inv = rkQ1.unit_inverse();
	Quat rkP0 = kQ0inv*rkQ1;
	Quat rkP1 = kQ1inv*rkQ2;
	Quat kArg = 0.25*(rkP0.log()-rkP1.log());
	Quat kMinusArg = -kArg;

	rkA = rkQ1*kArg.exp();
	rkB = rkQ1*kMinusArg.exp();
}

Quat Quat::squad(r32 fT, const Quat& rkP, const Quat& rkA, const Quat& rkB, const Quat& rkQ)
{
	r32 fSlerpT = 2.0f*fT*(1.0f-fT);
	Quat kSlerpP = slerp(fT, rkP, rkQ);
	Quat kSlerpQ = slerp(fT, rkA, rkB);
	return slerp(fSlerpT, kSlerpP ,kSlerpQ);
}

r32 Quat::normalize(void)
{
	r32 len = length();
	*this = *this * (1.0f / len);
	return len;
}

Quat Quat::normalize(const Quat& q)
{
	r32 factor = 1.0f / q.length();
	return q * factor;
}

Quat Quat::nlerp(r32 fT, const Quat& rkP, const Quat& rkQ, b8 shortestPath)
{
	Quat result;
	r32 fCos = rkP.dot(rkQ);
	if (fCos < 0.0f && shortestPath)
	{
		result = rkP + fT * ((-rkQ) - rkP);
	}
	else
	{
		result = rkP + fT * (rkQ - rkP);
	}
	result.normalize();
	return result;
}

inline static r32
	MINOR(const Mat4& m, const s32 r0, const s32 r1, const s32 r2, 
							const s32 c0, const s32 c1, const s32 c2)
{
	return m[r0][c0] * (m[r1][c1] * m[r2][c2] - m[r2][c1] * m[r1][c2]) -
		m[r0][c1] * (m[r1][c0] * m[r2][c2] - m[r2][c0] * m[r1][c2]) +
		m[r0][c2] * (m[r1][c0] * m[r2][c1] - m[r2][c0] * m[r1][c1]);
}

Mat4 Mat4::adjoint() const
{
	return Mat4(MINOR(*this, 1, 2, 3, 1, 2, 3),
		-MINOR(*this, 0, 2, 3, 1, 2, 3),
		MINOR(*this, 0, 1, 3, 1, 2, 3),
		-MINOR(*this, 0, 1, 2, 1, 2, 3),

		-MINOR(*this, 1, 2, 3, 0, 2, 3),
		MINOR(*this, 0, 2, 3, 0, 2, 3),
		-MINOR(*this, 0, 1, 3, 0, 2, 3),
		MINOR(*this, 0, 1, 2, 0, 2, 3),

		MINOR(*this, 1, 2, 3, 0, 1, 3),
		-MINOR(*this, 0, 2, 3, 0, 1, 3),
		MINOR(*this, 0, 1, 3, 0, 1, 3),
		-MINOR(*this, 0, 1, 2, 0, 1, 3),

		-MINOR(*this, 1, 2, 3, 0, 1, 2),
		MINOR(*this, 0, 2, 3, 0, 1, 2),
		-MINOR(*this, 0, 1, 3, 0, 1, 2),
		MINOR(*this, 0, 1, 2, 0, 1, 2));
}

r32 Mat4::determinant() const
{
	return m[0][0] * MINOR(*this, 1, 2, 3, 1, 2, 3) -
		m[0][1] * MINOR(*this, 1, 2, 3, 0, 2, 3) +
		m[0][2] * MINOR(*this, 1, 2, 3, 0, 1, 3) -
		m[0][3] * MINOR(*this, 1, 2, 3, 0, 1, 2);
}

Mat4 Mat4::inverse() const
{
	r32 m00 = m[0][0], m01 = m[0][1], m02 = m[0][2], m03 = m[0][3];
	r32 m10 = m[1][0], m11 = m[1][1], m12 = m[1][2], m13 = m[1][3];
	r32 m20 = m[2][0], m21 = m[2][1], m22 = m[2][2], m23 = m[2][3];
	r32 m30 = m[3][0], m31 = m[3][1], m32 = m[3][2], m33 = m[3][3];

	r32 v0 = m20 * m31 - m21 * m30;
	r32 v1 = m20 * m32 - m22 * m30;
	r32 v2 = m20 * m33 - m23 * m30;
	r32 v3 = m21 * m32 - m22 * m31;
	r32 v4 = m21 * m33 - m23 * m31;
	r32 v5 = m22 * m33 - m23 * m32;

	r32 t00 = + (v5 * m11 - v4 * m12 + v3 * m13);
	r32 t10 = - (v5 * m10 - v2 * m12 + v1 * m13);
	r32 t20 = + (v4 * m10 - v2 * m11 + v0 * m13);
	r32 t30 = - (v3 * m10 - v1 * m11 + v0 * m12);

	r32 invDet = 1 / (t00 * m00 + t10 * m01 + t20 * m02 + t30 * m03);

	r32 d00 = t00 * invDet;
	r32 d10 = t10 * invDet;
	r32 d20 = t20 * invDet;
	r32 d30 = t30 * invDet;

	r32 d01 = - (v5 * m01 - v4 * m02 + v3 * m03) * invDet;
	r32 d11 = + (v5 * m00 - v2 * m02 + v1 * m03) * invDet;
	r32 d21 = - (v4 * m00 - v2 * m01 + v0 * m03) * invDet;
	r32 d31 = + (v3 * m00 - v1 * m01 + v0 * m02) * invDet;

	v0 = m10 * m31 - m11 * m30;
	v1 = m10 * m32 - m12 * m30;
	v2 = m10 * m33 - m13 * m30;
	v3 = m11 * m32 - m12 * m31;
	v4 = m11 * m33 - m13 * m31;
	v5 = m12 * m33 - m13 * m32;

	r32 d02 = + (v5 * m01 - v4 * m02 + v3 * m03) * invDet;
	r32 d12 = - (v5 * m00 - v2 * m02 + v1 * m03) * invDet;
	r32 d22 = + (v4 * m00 - v2 * m01 + v0 * m03) * invDet;
	r32 d32 = - (v3 * m00 - v1 * m01 + v0 * m02) * invDet;

	v0 = m21 * m10 - m20 * m11;
	v1 = m22 * m10 - m20 * m12;
	v2 = m23 * m10 - m20 * m13;
	v3 = m22 * m11 - m21 * m12;
	v4 = m23 * m11 - m21 * m13;
	v5 = m23 * m12 - m22 * m13;

	r32 d03 = - (v5 * m01 - v4 * m02 + v3 * m03) * invDet;
	r32 d13 = + (v5 * m00 - v2 * m02 + v1 * m03) * invDet;
	r32 d23 = - (v4 * m00 - v2 * m01 + v0 * m03) * invDet;
	r32 d33 = + (v3 * m00 - v1 * m01 + v0 * m02) * invDet;

	return Mat4(
		d00, d01, d02, d03,
		d10, d11, d12, d13,
		d20, d21, d22, d23,
		d30, d31, d32, d33);
}

Mat4 Mat4::inverse_affine(void) const
{
	r32 m10 = m[1][0], m11 = m[1][1], m12 = m[1][2];
	r32 m20 = m[2][0], m21 = m[2][1], m22 = m[2][2];

	r32 t00 = m22 * m11 - m21 * m12;
	r32 t10 = m20 * m12 - m22 * m10;
	r32 t20 = m21 * m10 - m20 * m11;

	r32 m00 = m[0][0], m01 = m[0][1], m02 = m[0][2];

	r32 invDet = 1 / (m00 * t00 + m01 * t10 + m02 * t20);

	t00 *= invDet; t10 *= invDet; t20 *= invDet;

	m00 *= invDet; m01 *= invDet; m02 *= invDet;

	r32 r00 = t00;
	r32 r01 = m02 * m21 - m01 * m22;
	r32 r02 = m01 * m12 - m02 * m11;

	r32 r10 = t10;
	r32 r11 = m00 * m22 - m02 * m20;
	r32 r12 = m02 * m10 - m00 * m12;

	r32 r20 = t20;
	r32 r21 = m01 * m20 - m00 * m21;
	r32 r22 = m00 * m11 - m01 * m10;

	r32 m03 = m[0][3], m13 = m[1][3], m23 = m[2][3];

	r32 r03 = - (r00 * m03 + r01 * m13 + r02 * m23);
	r32 r13 = - (r10 * m03 + r11 * m13 + r12 * m23);
	r32 r23 = - (r20 * m03 + r21 * m13 + r22 * m23);

	return Mat4(
		r00, r01, r02, r03,
		r10, r11, r12, r13,
		r20, r21, r22, r23,
		  0,   0,   0,   1);
}

void Mat4::make_transform(const Vec3& position, const Vec3& scale, const Quat& orientation)
{
	// Ordering:
	//    1. Scale
	//    2. Rotate
	//    3. Translate

	Mat3 rot3x3;
	orientation.to_rotation_matrix(rot3x3);

	// Set up final matrix with scale, rotation and translation
	m[0][0] = scale.x * rot3x3[0][0]; m[1][0] = scale.y * rot3x3[1][0]; m[2][0] = scale.z * rot3x3[2][0]; m[3][0] = position.x;
	m[0][1] = scale.x * rot3x3[0][1]; m[1][1] = scale.y * rot3x3[1][1]; m[2][1] = scale.z * rot3x3[2][1]; m[3][1] = position.y;
	m[0][2] = scale.x * rot3x3[0][2]; m[1][2] = scale.y * rot3x3[1][2]; m[2][2] = scale.z * rot3x3[2][2]; m[3][2] = position.z;

	// No projection term
	m[0][3] = 0; m[1][3] = 0; m[2][3] = 0; m[3][3] = 1;
}

void Mat4::make_inverse_transform(const Vec3& position, const Vec3& scale, const Quat& orientation)
{
	// Invert the parameters
	Vec3 invTranslate = -position;
	Vec3 invScale(1 / scale.x, 1 / scale.y, 1 / scale.z);
	Quat invRot = orientation.inverse();

	// Because we're inverting, order is translation, rotation, scale
	// So make translation relative to scale & rotation
	invTranslate = invRot * invTranslate; // rotate
	invTranslate *= invScale; // scale

	// Next, make a 3x3 rotation matrix
	Mat3 rot3x3;
	invRot.to_rotation_matrix(rot3x3);

	// Set up final matrix with scale, rotation and translation
	m[0][0] = invScale.x * rot3x3[0][0]; m[1][0] = invScale.x * rot3x3[1][0]; m[2][0] = invScale.x * rot3x3[2][0]; m[3][0] = invTranslate.x;
	m[0][1] = invScale.y * rot3x3[0][1]; m[1][1] = invScale.y * rot3x3[1][1]; m[2][1] = invScale.y * rot3x3[2][1]; m[3][1] = invTranslate.y;
	m[0][2] = invScale.z * rot3x3[0][2]; m[1][2] = invScale.z * rot3x3[1][2]; m[2][2] = invScale.z * rot3x3[2][2]; m[3][2] = invTranslate.z;        

	// No projection term
	m[0][3] = 0; m[1][3] = 0; m[2][3] = 0; m[3][3] = 1;
}

void Mat4::decomposition(Vec3& position, Vec3& scale, Quat& orientation) const
{
	Mat3 m3x3;
	extract_mat3(m3x3);

	Mat3 matQ;
	Vec3 vecU;
	m3x3.qdu_decomposition(matQ, scale, vecU); 

	orientation = Quat(matQ);
	position = Vec3(m[3][0], m[3][1], m[3][2]);
}

Mat4 Mat4::perspective(const r32 fov, const r32 aspect, const r32 near, const r32 far)
{
	Mat4 result = Mat4::zero;

	r32 depth = far - near;
	r32 inverse_depth = 1.0f / depth;

	result[1][1] = 1.0f / tan(fov);

	result[0][0] = -1.0f * result[1][1] / aspect;
	result[2][2] = far * inverse_depth;
	result[3][2] = (-far * near) * inverse_depth;
	result[2][3] = 1;

	return result;
}

Mat4 Mat4::orthographic(const r32 width, const r32 height, const r32 near, const r32 far)
{
	Mat4 result = Mat4::zero;

	result[0][0] = 2.0f / width;
	result[1][1] = 2.0f / height;
	r32 inverse_depth = 1.0f / (near - far);
	result[2][2] = inverse_depth;
	result[3][2] = near * inverse_depth;
	result[3][3] = 1;

	return result;
}

Mat4 Mat4::look(const Vec3& eye, const Vec3& forward, const Vec3& up)
{
	Vec3 const f(Vec3::normalize(-forward));
	Vec3 const s(Vec3::normalize(f.cross(up)));
	Vec3 u(Vec3::normalize(up));

	Mat4 result = Mat4::identity;
	result[0][0] = s.x;
	result[1][0] = s.y;
	result[2][0] = s.z;
	result[0][1] = u.x;
	result[1][1] = u.y;
	result[2][1] = u.z;
	result[0][2] = -f.x;
	result[1][2] = -f.y;
	result[2][2] = -f.z;
	result[3][0] = -s.dot(eye);
	result[3][1] = -u.dot(eye);
	result[3][2] = f.dot(eye);
	return result;
}

}
