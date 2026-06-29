#define VECTOR_LIB
#pragma once

//#include <cmath>
#include <iostream>
//#include <algorithm>

template<typename T>
class Vec2
{
public:
	Vec2() : x(0), y(0) {}
	Vec2(T xx) : x(xx), y(xx) {}
	Vec2(T xx, T yy) : x(xx), y(yy) {}
	Vec2 operator + (const T& r) const
	{
		return Vec2(x + r, y + r);
	}
	Vec2 operator + (const Vec2& v) const
	{
		return Vec2(x + v.x, y + v.y);
	}

	Vec2 operator - () const
	{
		return Vec2(-x, -y);
	}
	Vec2 operator - (const Vec2& v) const
	{
		return Vec2(x - v.x, y - v.y);
	}
	friend Vec2 operator -(const Vec2& v, const T& r)
	{
		return Vec2(v.x - r, v.y - r);
	}

	friend Vec2 operator -(const T& r, const Vec2& v)
	{
		return Vec2(r - v.x, r - v.y);
	}


	Vec2 operator * (const T& r) const
	{
		return Vec2(x * r, y * r);
	}
	Vec2 operator * (const Vec2& v) const
	{
		return Vec2(x * v.x, y * v.y);
	}

	Vec2 operator / (const T &r) const
	{
		return Vec2(x / r, y / r);
	}
	Vec2 operator / (const Vec2& v) const
	{
		return Vec2(x / v.x, y / v.y);
	}
	
	//*************************************************

	Vec2& operator += (const Vec2& v)
	{
		x += v.x, y += v.y; return *this;
	}

	Vec2& operator -= (const Vec2& v)
	{
		x -= v.x, y -= v.y; return *this;
	}

	Vec2& operator /= (const T &r)
	{
		x /= r, y /= r; return *this;
	}

	Vec2& operator *= (const T &r)
	{
		x *= r, y *= r; return *this;
	}

	Vec2& operator *= (const Vec2& v)
	{
		x *= v.x, y *= v.y; return *this;
	}

	Vec2& operator >>= (const Vec2& v)			// ne fonctionnera qu'avec du INT
	{
		x >>= v.x, y >>= v.y; return *this;
	}

	Vec2& operator <<= (const Vec2& v)			// ne fonctionnera qu'avec du INT
	{
		x <<= v.x, y <<= v.y; return *this;
	}
	//*************************************************


	/* un produit vectoriel 2d donne un scalaire e nnon un vecteur 2d
	
	Le résultat est un scalaire représentant l'aire du parallélogramme formé par les deux vecteurs.
	Notez que contrairement au produit scalaire, qui donne un nombre qui mesure la projection d'un vecteur sur un autre, 
		le produit vectoriel donne une quantité qui mesure l'aire du parallélogramme formé par les deux vecteurs, ce qui est une quantité géométrique.	 
	*/
	T crossProduct(const Vec2<T>& v) const
	{
		return x * v.y - y * v.x;
	}

	T dotProduct(const Vec2<T>& v) const
	{
		return x * v.x + y * v.y;
	}
	T norm() const
	{
		return x * x + y * y;
	}
	T length() const	// magnitude
	{
		return sqrt(norm());
	}

	Vec2& normalize()
	{
		T n = norm();
		if (n > 0) {
			T factor = 1 / sqrt(n);
			x *= factor, y *= factor;
		}

		return *this;
	}

	T getAngle() const
	{
		return atan2(y, x);	// retrouve l'angle par rapport à l'axe positif des absisses
	}
	
	Vec2& setMagnitude(const T& r)
	{
		normalize();
		x *= r, y *= r;
		return *this;
	}

	friend std::ostream& operator << (std::ostream &s, const Vec2<T> &v)
	{
		return s << '[' << v.x << ' ' << v.y << ']';
	}
	friend Vec2 operator * (const T &r, const Vec2<T> &v)
	{
		return Vec2(v.x * r, v.y * r);
	}
	T x, y;
};

typedef Vec2<double> Vec2d;
typedef Vec2<float> Vec2f;
typedef Vec2<long> Vec2l;
typedef Vec2<int> Vec2i;

//[comment]
// Implementation of a generic vector class - it will be used to deal with 3D points, vectors and normals.
// The class is implemented as a template. While it may complicate the code a bit, it gives us
// the flexibility later, to specialize the type of the coordinates into anything we want.
// For example: Vec3f if we want the coordinates to be floats or Vec3i if we want the coordinates to be integers.
//
// Vec3 is a standard/common way of naming vectors, points, etc. The OpenEXR and Autodesk libraries
// use this convention for instance.
//[/comment]
template<typename T>
class Vec3
{
public:
	Vec3() : x(T(0)), y(T(0)), z(T(0)) {}
	Vec3(T xx) : x(xx), y(xx), z(xx) {}
	Vec3(T xx, T yy, T zz) : x(xx), y(yy), z(zz) {}
	Vec3 operator + (const Vec3 &v) const
	{
		return Vec3(x + v.x, y + v.y, z + v.z);
	}
	Vec3 operator + (const T& r) const
	{
		return Vec3(x + r, y + r, z + r);
	}

	Vec3 operator - (const Vec3 &v) const
	{
		return Vec3(x - v.x, y - v.y, z - v.z);
	}

	friend Vec3 operator -(const Vec3 &v, const T &r)
	{
		return Vec3(v.x - r, v.y - r, v.z - r);
	}
	
	friend Vec3 operator -(const T& r, const Vec3& v)
	{
		return Vec3(r - v.x, r - v.y, r - v.z);
	}
	
	Vec3 operator - () const
	{
		return Vec3(-x, -y, -z);
	}

	Vec3 operator * (const T &r) const
	{
		return Vec3(x * r, y * r, z * r);
	}
	Vec3 operator * (const Vec3 &v) const
	{
		return Vec3(x * v.x, y * v.y, z * v.z);
	}

	Vec3 operator / (const Vec3& v) const
	{
		return Vec3(x / v.x, y / v.y, z / v.z);
	}

	Vec3 operator / (const T &r) const
	{
		return Vec3(x / r, y / r, z / r);
	}
	
	bool operator < (const Vec3& v) const
	{
		if (x < v.x && y < v.y && z < v.z)
			return true;
		return false;	
	}
	bool operator > (const Vec3& v) const
	{
		if (x > v.x && y > v.y && z > v.z)
			return true;		
		return false;
	}

	//*******************************************************
	Vec3& operator *= (const Vec3& v)
	{
		x *= v.x, y *= v.y, z *= v.z; return *this;
	}
	Vec3& operator *= (const T& r)
	{
		x *= r, y *= r, z *= r; return *this;
	}
	Vec3& operator /= (const Vec3& v)
	{
		x /= v.x, y /= v.y, z /= v.z; return *this;
	}
	Vec3& operator /= (const T &r)
	{
		x /= r, y /= r, z /= r; return *this;
	}

	Vec3& operator += (const Vec3& v)
	{
		x += v.x, y += v.y, z += v.z; return *this;
	}

	Vec3& operator -= (const Vec3& v)
	{
		x -= v.x, y -= v.y, z -= v.z; return *this;
	}
	
	Vec3& operator >>= (const Vec3& v)			// ne fonctionnera qu'avec du INT
	{
		x >>= v.x, y >>= v.y, z >>= v.z; return *this;
	}

	Vec3& operator <<= (const Vec3& v)			// ne fonctionnera qu'avec du INT
	{
		x <<= v.x, y <<= v.y, z <<= v.z; return *this;
	}
	//*******************************************************
	//T dotProductSIMD(const Vec3<T>& v) const
	//{
	//	return x * v.x + y * v.y + z * v.z;
	//}

	inline T dotProduct(const Vec3<T>& v) const
	{
		return x * v.x + y * v.y + z * v.z;
	}
	inline Vec3 crossProduct(const Vec3<T> &v) const
	{
		return Vec3<T>(y * v.z - z * v.y, z * v.x - x * v.z, x * v.y - y * v.x);
	}
	inline T norm() const
	{
		return x * x + y * y + z * z;
	}
	inline T length() const	// magnitude
	{
		return sqrt(norm());
	}
	T getAngle(const Vec3<T>& v) const
	{
		return max(dotProduct(v), 0);
	}

	Vec3& setMagnitude(const T& r)
	{
		normalize();
		x *= r, y *= r, z *= r;
		return *this;
	}

	Vec3& setMin(const Vec3<T>& v)
	{
		x = std::min(x, v.x);
		y = std::min(y, v.y);
		z = std::min(z, v.z);
		return *this;
	}

	Vec3& setMax(const Vec3<T>& v)
	{
		x = std::max(x, v.x);
		y = std::max(y, v.y);
		z = std::max(z, v.z);
		return *this;
	}





	//[comment]
	// The next two operators are sometimes called access operators or
	// accessors. The Vec coordinates can be accessed that way v[0], v[1], v[2],
	// rather than using the more traditional form v.x, v.y, v.z. This useful
	// when vectors are used in loops: the coordinates can be accessed with the
	// loop index (e.g. v[i]).
	//[/comment]
	const T& operator [] (uint8_t i) const { return (&x)[i]; }
	T& operator [] (uint8_t i) { return (&x)[i]; }
	
	inline Vec3& normalize()
	{
		T n = norm();
		if (n > 0) {
			T factor = 1 / sqrt(n);
			x *= factor, y *= factor, z *= factor;
		}

		return *this;
	}

	Vec3 Normalized() const {
		T l = std::sqrt(x * x + y * y + z * z);
		if (l == 0) 
			return Vec3(T(0));
		return Vec3(x / l, y / l, z / l);
	}


	// Limite "max" du vecteur
	Vec3& Limit(const T& max)
	{
		if (norm() > max * max)
		{
			T mag = length();
			Vec3<T> n = *this;
			n /= mag;
			n *= max;
			
			x = n.x;
			y = n.y;
			z = n.z;
		}
		return *this;

	}

	friend Vec3 operator * (const T &r, const Vec3 &v)
	{
		return Vec3<T>(v.x * r, v.y * r, v.z * r);
	}
	friend Vec3 operator / (const T &r, const Vec3 &v)
	{
		return Vec3<T>(r / v.x, r / v.y, r / v.z);
	}

	friend std::ostream& operator << (std::ostream &s, const Vec3<T> &v)
	{
		return s << '[' << v.x << ' ' << v.y << ' ' << v.z << ']';
	}


	static constexpr Vec3 Zero()    noexcept { Vec3<T>return { 0,0,0 }; }
	static constexpr Vec3 One()     noexcept { Vec3<T>return { 1,1,1 }; }
	static constexpr Vec3 Up()      noexcept { Vec3<T>return { 0,1,0 }; }
	static constexpr Vec3 Forward() noexcept { Vec3<T>return { 0,0,1 }; }
	static constexpr Vec3 Right()   noexcept { Vec3<T>return { 1,0,0 }; }


	T x, y, z;
};

//[comment]
// Now you can specialize the class. We are just showing two examples here. In your code
// you can declare a vector either that way: Vec3<float> a, or that way: Vec3f a
//[/comment]
typedef Vec3<double> Vec3d;
typedef Vec3<float> Vec3f;
typedef Vec3<int> Vec3i;


//***********************************************

template<typename T>
class Vec4
{
public:
	Vec4() : x(T(0)), y(T(0)), z(T(0)), w(T(0)) {}
	Vec4(T xx) : x(xx), y(xx), z(xx), w(xx) {}
	Vec4(T xx, T yy, T zz, T ww) : x(xx), y(yy), z(zz), w(ww) {}

	Vec4 operator + (const Vec4& v) const
	{
		return Vec4(x + v.x, y + v.y, z + v.z, w + v.w);
	}

	Vec4 operator - (const Vec4& v) const
	{
		return Vec4(x - v.x, y - v.y, z - v.z, w - v.w);
	}

	Vec4 operator - () const
	{
		return Vec4(-x, -y, -z, -w);
	}

	Vec4 operator * (const T& rr) const
	{
		return Vec4(x * rr, y * rr, z * rr, w * rr);
	}

	Vec4 operator * (const Vec4& v) const
	{
		return Vec4(x * v.x, y * v.y, z * v.z, w * v.w);
	}

	Vec4 operator / (const Vec4& v) const
	{
		return Vec4(x / v.x, y / v.y, z / v.z, w / v.w);
	}

	Vec4 operator / (const T& rr) const
	{
		return Vec4(x / rr, y / rr, z / rr, w / rr);
	}
	Vec4 operator & (const T& rr) const
	{
		return Vec4(x & rr, y & rr, z & rr, w & rr);
	}
	Vec4 operator & (const Vec4& v) const
	{
		return Vec4(x & v.x, y & v.y, z & v.z, w & v.w);
	}

	Vec4& operator /= (const T& rr)
	{
		x /= rr, y /= rr, z /= rr, w /= rr;
		return *this;
	}
	Vec4& operator *= (const Vec4& v)
	{
		x *= v.x, y *= v.y, z *= v.z, w *= v.w; return *this;
	}
	Vec4& operator *= (const T& rr)
	{
		x *= rr, y *= rr, z *= rr, w *= rr; return *this;
	}
	Vec4& operator += (const Vec4& v)
	{
		x += v.x, y += v.y, z += v.z, w += v.w; return *this;
	}
	Vec4& operator &= (const T& rr)
	{
		x &= rr, y &= rr, z &= rr, w &= rr;
		return *this;
	}
	Vec4& operator &= (const Vec4& v)
	{
		x &= v.x, y &= v.y, z &= v.z, w &= v.w; return *this;
	}


	//friend int operator >= (const T & rr)
	//{
	//	return int (x >= rr) & (y >= rr) & (z >= rr) & (w >= rr);
	//}

	friend Vec4 operator * (const T& r, const Vec4& v)
	{
		return Vec4<T>(v.x * r, v.y * r, v.z * r, v.w * r);
	}


	friend Vec4 operator - (const T& r, const Vec4& v)
	{
		return Vec4<T>(r - v.x, r - v.y, r - v.z, r - v.w);
	}


	T x, y, z, w;
};
typedef Vec4<double> Vec4d;
typedef Vec4<float> Vec4f;
typedef Vec4<int> Vec4i;
