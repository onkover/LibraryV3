#pragma once
#include "Vec3f.h"
#include "Matrix44f.h"
#include <cmath>
namespace LibV2 {
    template<typename T>
    struct Quat {
        T w=T(1), x=T(0), y=T(0), z=T(0);
        constexpr Quat() noexcept = default;
        constexpr Quat(T w_,T x_,T y_,T z_) noexcept : w(w_),x(x_),y(y_),z(z_) {}
        static Quat FromEuler(T pitch,T yaw,T roll) noexcept {
            T cp=std::cos(pitch*.5f),sp=std::sin(pitch*.5f);
            T cy=std::cos(yaw*.5f),  sy=std::sin(yaw*.5f);
            T cr=std::cos(roll*.5f), sr=std::sin(roll*.5f);
            return {cr*cp*cy+sr*sp*sy,sr*cp*cy-cr*sp*sy,cr*sp*cy+sr*cp*sy,cr*cp*sy-sr*sp*cy};
        }
        static Quat FromAxisAngle(const Vec3f& ax, T angle) noexcept {
            T h=angle*.5f, s=std::sin(h);
            return {std::cos(h),T(ax.x)*s,T(ax.y)*s,T(ax.z)*s};
        }
        Quat operator*(const Quat& q) const noexcept {
            return {w*q.w-x*q.x-y*q.y-z*q.z, w*q.x+x*q.w+y*q.z-z*q.y,
                    w*q.y-x*q.z+y*q.w+z*q.x, w*q.z+x*q.y-y*q.x+z*q.w};
        }
        Quat conjugate() const noexcept { return {w,-x,-y,-z}; }
        Quat normalize()  const noexcept {
            T i=T(1)/std::sqrt(w*w+x*x+y*y+z*z);
            return {w*i,x*i,y*i,z*i};
        }
        Vec3f rotate(const Vec3f& v) const noexcept {
            Vec3f qv{float(x),float(y),float(z)};
            Vec3f uv=qv.cross(v), uuv=qv.cross(uv);
            return v+(uv*float(2*w))+(uuv*2.f);
        }
        Matrix44f ToMatrix() const noexcept {
            Matrix44f mat=Matrix44f::Identity();
            T xx=x*x,yy=y*y,zz=z*z,xy=x*y,xz=x*z,yz=y*z,wx=w*x,wy=w*y,wz=w*z;
            mat.m[0][0]=T(1)-T(2)*(yy+zz); mat.m[1][0]=T(2)*(xy-wz); mat.m[2][0]=T(2)*(xz+wy);
            mat.m[0][1]=T(2)*(xy+wz); mat.m[1][1]=T(1)-T(2)*(xx+zz); mat.m[2][1]=T(2)*(yz-wx);
            mat.m[0][2]=T(2)*(xz-wy); mat.m[1][2]=T(2)*(yz+wx); mat.m[2][2]=T(1)-T(2)*(xx+yy);
            return mat;
        }
        static Quat Slerp(const Quat& a, const Quat& b, T t) noexcept {
            T dot=a.w*b.w+a.x*b.x+a.y*b.y+a.z*b.z;
            Quat b2=(dot<T(0))?Quat{-b.w,-b.x,-b.y,-b.z}:b;
            if(dot<T(0)) dot=-dot;
            if(dot>T(0.9995f)){
                return Quat{a.w+t*(b2.w-a.w),a.x+t*(b2.x-a.x),
                            a.y+t*(b2.y-a.y),a.z+t*(b2.z-a.z)}.normalize();
            }
            T th0=std::acos(dot),th=th0*t;
            T s0=std::cos(th)-dot*std::sin(th)/std::sin(th0);
            T s1=std::sin(th)/std::sin(th0);
            return {s0*a.w+s1*b2.w,s0*a.x+s1*b2.x,s0*a.y+s1*b2.y,s0*a.z+s1*b2.z};
        }
        static constexpr Quat Identity() noexcept { return {T(1),T(0),T(0),T(0)}; }
    };
    using QuatF = Quat<float>;
} // namespace LibV2
