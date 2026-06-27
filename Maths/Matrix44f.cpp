#include "pch.h"
#include "Matrix44f.h"
#include <cstring>
namespace LibV2 {

Matrix44f Matrix44f::Identity() noexcept {
    Matrix44f r; r.m[0][0]=r.m[1][1]=r.m[2][2]=r.m[3][3]=1.f; return r;
}
Matrix44f Matrix44f::Translation(const Vec3f& t) noexcept {
    Matrix44f r=Identity(); r.m[3][0]=t.x; r.m[3][1]=t.y; r.m[3][2]=t.z; return r;
}
Matrix44f Matrix44f::Scale(const Vec3f& s) noexcept {
    Matrix44f r; r.m[0][0]=s.x; r.m[1][1]=s.y; r.m[2][2]=s.z; r.m[3][3]=1.f; return r;
}
Matrix44f Matrix44f::RotationX(float a) noexcept {
    Matrix44f r=Identity(); float c=std::cos(a),s=std::sin(a);
    r.m[1][1]=c; r.m[2][1]=-s; r.m[1][2]=s; r.m[2][2]=c; return r;
}
Matrix44f Matrix44f::RotationY(float a) noexcept {
    Matrix44f r=Identity(); float c=std::cos(a),s=std::sin(a);
    r.m[0][0]=c; r.m[2][0]=s; r.m[0][2]=-s; r.m[2][2]=c; return r;
}
Matrix44f Matrix44f::RotationZ(float a) noexcept {
    Matrix44f r=Identity(); float c=std::cos(a),s=std::sin(a);
    r.m[0][0]=c; r.m[1][0]=-s; r.m[0][1]=s; r.m[1][1]=c; return r;
}
Matrix44f Matrix44f::LookAt(const Vec3f& eye, const Vec3f& target, const Vec3f& up) noexcept {
    Vec3f f=(target-eye).normalize(), r=f.cross(up).normalize(), u=r.cross(f);
    Matrix44f m=Identity();
    m.m[0][0]= r.x; m.m[1][0]= r.y; m.m[2][0]= r.z; m.m[3][0]=-r.dot(eye);
    m.m[0][1]= u.x; m.m[1][1]= u.y; m.m[2][1]= u.z; m.m[3][1]=-u.dot(eye);
    m.m[0][2]=-f.x; m.m[1][2]=-f.y; m.m[2][2]=-f.z; m.m[3][2]= f.dot(eye);
    return m;
}
Matrix44f Matrix44f::Perspective(float fov, float aspect, float near_, float far_) noexcept {
    float th=std::tan(fov*0.5f); Matrix44f r;
    r.m[0][0]=1.f/(aspect*th); r.m[1][1]=1.f/th;
    r.m[2][2]=-(far_+near_)/(far_-near_); r.m[3][2]=-(2.f*far_*near_)/(far_-near_);
    r.m[2][3]=-1.f; return r;
}
Matrix44f Matrix44f::Orthographic(float l, float r_, float b, float t, float n, float f) noexcept {
    Matrix44f m=Identity();
    m.m[0][0]=2.f/(r_-l); m.m[1][1]=2.f/(t-b); m.m[2][2]=-2.f/(f-n);
    m.m[3][0]=-(r_+l)/(r_-l); m.m[3][1]=-(t+b)/(t-b); m.m[3][2]=-(f+n)/(f-n);
    return m;
}
Matrix44f Matrix44f::operator*(const Matrix44f& o) const noexcept {
    Matrix44f res;
    for (int col=0;col<4;++col)
        for (int row=0;row<4;++row)
            for (int k=0;k<4;++k)
                res.m[col][row]+=m[k][row]*o.m[col][k];
    return res;
}
Vec3f Matrix44f::operator*(const Vec3f& v) const noexcept {
    float w=m[0][3]*v.x+m[1][3]*v.y+m[2][3]*v.z+m[3][3];
    float iw=(w!=0.f)?1.f/w:1.f;
    return {(m[0][0]*v.x+m[1][0]*v.y+m[2][0]*v.z+m[3][0])*iw,
            (m[0][1]*v.x+m[1][1]*v.y+m[2][1]*v.z+m[3][1])*iw,
            (m[0][2]*v.x+m[1][2]*v.y+m[2][2]*v.z+m[3][2])*iw};
}
Vec4f Matrix44f::operator*(const Vec4f& v) const noexcept {
    return {m[0][0]*v.x+m[1][0]*v.y+m[2][0]*v.z+m[3][0]*v.w,
            m[0][1]*v.x+m[1][1]*v.y+m[2][1]*v.z+m[3][1]*v.w,
            m[0][2]*v.x+m[1][2]*v.y+m[2][2]*v.z+m[3][2]*v.w,
            m[0][3]*v.x+m[1][3]*v.y+m[2][3]*v.z+m[3][3]*v.w};
}
Vec3f Matrix44f::transformDir(const Vec3f& v) const noexcept {
    return {m[0][0]*v.x+m[1][0]*v.y+m[2][0]*v.z,
            m[0][1]*v.x+m[1][1]*v.y+m[2][1]*v.z,
            m[0][2]*v.x+m[1][2]*v.y+m[2][2]*v.z};
}
Matrix44f Matrix44f::transpose() const noexcept {
    Matrix44f r; for(int i=0;i<4;++i) for(int j=0;j<4;++j) r.m[i][j]=m[j][i]; return r;
}
float Matrix44f::determinant() const noexcept {
    // Laplace expansion on first row
    auto sub3=[&](int r0,int r1,int r2,int c0,int c1,int c2)->float{
        return m[c0][r0]*(m[c1][r1]*m[c2][r2]-m[c2][r1]*m[c1][r2])
              -m[c1][r0]*(m[c0][r1]*m[c2][r2]-m[c2][r1]*m[c0][r2])
              +m[c2][r0]*(m[c0][r1]*m[c1][r2]-m[c1][r1]*m[c0][r2]);
    };
    return m[0][0]*sub3(1,2,3,1,2,3)-m[1][0]*sub3(1,2,3,0,2,3)
          +m[2][0]*sub3(1,2,3,0,1,3)-m[3][0]*sub3(1,2,3,0,1,2);
}
Matrix44f Matrix44f::inverse() const noexcept {
    // Gauss-Jordan
    float a[4][8]={};
    for(int i=0;i<4;++i){for(int j=0;j<4;++j) a[i][j]=m[j][i]; a[i][i+4]=1.f;}
    for(int i=0;i<4;++i){
        float piv=a[i][i]; if(std::abs(piv)<1e-8f) return Identity();
        for(int j=0;j<8;++j) a[i][j]/=piv;
        for(int r=0;r<4;++r) if(r!=i){float f=a[r][i]; for(int j=0;j<8;++j) a[r][j]-=f*a[i][j];}
    }
    Matrix44f res;
    for(int i=0;i<4;++i) for(int j=0;j<4;++j) res.m[j][i]=a[i][j+4];
    return res;
}

} // namespace LibV2
