
#include "geometry.h"

#include <random>

float GenerateRandomNumberInRange(float min, float max) {
  std::random_device rd;   // obtain a random number from hardware
  std::mt19937 gen(rd());  // seed the generator
  std::uniform_int_distribution<> distr(min, max);  // define the range
  return distr(gen);
}

float DotProduct(const Vec3f& a, const Vec3f& b) {
  return a.x * b.x + a.y * b.y + a.z * b.z;
}

float DotProduct(const Vec2f& a, const Vec2f& b) {
  return a.x * b.x + a.y * b.y;
}

Vec3f Mat3::operator*(const Vec3f& v) const {
  const Vec3f v0{elements[0][0], elements[0][1], elements[0][2]};
  const Vec3f v1{elements[1][0], elements[1][1], elements[1][2]};
  const Vec3f v2{elements[2][0], elements[2][1], elements[2][2]};
  return Vec3f{DotProduct(v0, v), DotProduct(v1, v), DotProduct(v2, v)};
}

Mat4 Mat4::operator*(const Mat4& b) const {
  Mat4 r = {};
  for (int i = 0; i < 4; ++i) {
    r.elements[0][i] =
        elements[0][i] * b.elements[0][0] + elements[1][i] * b.elements[0][1] +
        elements[2][i] * b.elements[0][2] + elements[3][i] * b.elements[0][3];
    r.elements[1][i] =
        elements[0][i] * b.elements[1][0] + elements[1][i] * b.elements[1][1] +
        elements[2][i] * b.elements[1][2] + elements[3][i] * b.elements[1][3];
    r.elements[2][i] =
        elements[0][i] * b.elements[2][0] + elements[1][i] * b.elements[2][1] +
        elements[2][i] * b.elements[2][2] + elements[3][i] * b.elements[2][3];
    r.elements[3][i] =
        elements[0][i] * b.elements[3][0] + elements[1][i] * b.elements[3][1] +
        elements[2][i] * b.elements[3][2] + elements[3][i] * b.elements[3][3];
  }
  return r;
}

Vec3f CrossProduct(const Vec3f& a, const Vec3f& b) {
  return Vec3f{a.y * b.z - a.z * b.y, a.z * b.x - a.x * b.z,
               a.x * b.y - a.y * b.x};
}

float Length(const Vec2f& v) { return sqrt(v.x * v.x + v.y * v.y); }

float Length(const Vec3f& v) { return sqrt(v.x * v.x + v.y * v.y + v.z * v.z); }

void Normalise(Vec2f& v) {
  const double length = Length(v);
  v.x /= length;
  v.y /= length;
}

void Normalise(Vec3f& v) {
  const double length = Length(v);
  v.x /= length;
  v.y /= length;
  v.z /= length;
}

Vec3f Normalised(const Vec3f& v) {
  Vec3f result = v;
  Normalise(result);
  return result;
}

Vec2f Normalised(const Vec2f& v) {
  Vec2f result = v;
  Normalise(result);
  return result;
}

Mat4 Translate(Mat4 m, const Vec3f& v) {
  // m[3] = m[0] * v + m[1] * v + m[2] * v + m[3];
  for (int i = 0; i < 4; ++i) {
    m.elements[3][i] = m.elements[0][i] * v.data[0] +
                       m.elements[1][i] * v.data[1] +
                       m.elements[2][i] * v.data[2] + m.elements[3][i];
  }
  return m;
}

Mat4 Rotate(const Mat4& m, double angle, const Vec3f& v) {
  const double c = cos(angle);
  const double s = sin(angle);

  const Vec3f axis = Normalised(v);
  const Vec3f temp = axis * (1. - c);

  Mat4 rotate;
  rotate.elements[0][0] = c + temp.data[0] * axis.data[0];
  rotate.elements[0][1] = temp.data[0] * axis.data[1] + s * axis.data[2];
  rotate.elements[0][2] = temp.data[0] * axis.data[2] - s * axis.data[1];

  rotate.elements[1][0] = temp.data[1] * axis.data[0] - s * axis.data[2];
  rotate.elements[1][1] = c + temp.data[1] * axis.data[1];
  rotate.elements[1][2] = temp.data[1] * axis.data[2] + s * axis.data[0];

  rotate.elements[2][0] = temp.data[2] * axis.data[0] + s * axis.data[1];
  rotate.elements[2][1] = temp.data[2] * axis.data[1] - s * axis.data[0];
  rotate.elements[2][2] = c + temp.data[2] * axis.data[2];

  Mat4 result = m;
  for (int i = 0; i < 4; ++i) {
    result.elements[0][i] = m.elements[0][i] * rotate.elements[0][0] +
                            m.elements[1][i] * rotate.elements[0][1] +
                            m.elements[2][i] * rotate.elements[0][2];
    result.elements[1][i] = m.elements[0][i] * rotate.elements[1][0] +
                            m.elements[1][i] * rotate.elements[1][1] +
                            m.elements[2][i] * rotate.elements[1][2];
    result.elements[2][i] = m.elements[0][i] * rotate.elements[2][0] +
                            m.elements[1][i] * rotate.elements[2][1] +
                            m.elements[2][i] * rotate.elements[2][2];
  }
  return result;
}

Mat4 Ortho(double left, double right, double bottom, double top, double nearVal,
           double farVal) {
  const double rl = 1.0f / (right - left);
  const double tb = 1.0f / (top - bottom);
  const double fn = -1.0f / (farVal - nearVal);

  Mat4 dest = {0};
  dest.elements[0][0] = 2.0f * rl;
  dest.elements[1][1] = 2.0f * tb;
  dest.elements[2][2] = 2.0f * fn;
  dest.elements[3][0] = -(right + left) * rl;
  dest.elements[3][1] = -(top + bottom) * tb;
  dest.elements[3][2] = (farVal + nearVal) * fn;
  dest.elements[3][3] = 1.0f;

  return dest;
}

Mat4 Identity() {
  Mat4 m = {0};
  m.elements[0][0] = m.elements[1][1] = m.elements[2][2] = m.elements[3][3] =
      1.0f;
  return m;
}

Mat4 LookAt(const Vec3f& eye, const Vec3f& center, const Vec3f& up) {
  const Vec3f f = Normalised(center - eye);
  const Vec3f s = Normalised(CrossProduct(f, up));
  const Vec3f u = CrossProduct(s, f);

  Mat4 dest = {0};
  dest.elements[0][0] = s.data[0];
  dest.elements[0][1] = u.data[0];
  dest.elements[0][2] = -f.data[0];
  dest.elements[1][0] = s.data[1];
  dest.elements[1][1] = u.data[1];
  dest.elements[1][2] = -f.data[1];
  dest.elements[2][0] = s.data[2];
  dest.elements[2][1] = u.data[2];
  dest.elements[2][2] = -f.data[2];
  dest.elements[3][0] = -DotProduct(s, eye);
  dest.elements[3][1] = -DotProduct(u, eye);
  dest.elements[3][2] = DotProduct(f, eye);
  dest.elements[3][3] = 1.0f;
  return dest;
}

Mat4 Perspective(double fovy, double aspect, double zNear, double zFar) {
  const double f = 1.0f / tan(fovy * 0.5f);
  const double fn = 1.0f / (zNear - zFar);
  Mat4 dest = {0};
  dest.elements[0][0] = f / aspect;
  dest.elements[1][1] = f;
  dest.elements[2][2] = (zNear + zFar) * fn;
  dest.elements[2][3] = -1.0f;
  dest.elements[3][2] = 2.0f * zNear * zFar * fn;
  return dest;
}

bool Intersect(const Vec3f planePt, const Vec3f planeN, const Vec3f triA,
               const Vec3f triB, const Vec3f triC, Segment3D& out)

{
  const float planeD = -DotProduct(planePt, planeN);
  constexpr float eps = 1e-7;
  const float d1 = DotProduct(planeN, triA) + planeD;
  const float d2 = DotProduct(planeN, triB) + planeD;
  const float d3 = DotProduct(planeN, triC) + planeD;

  const bool s1 = d1 * d2 < 0;
  const bool s2 = d2 * d3 < 0;
  const bool s3 = d3 * d1 < 0;

  if (s1 && s2 && s3) {
    return false;
  } else if (s1 && s2) {
    const float t0 = d1 / (d1 - d2);
    const float t1 = d2 / (d2 - d3);
    out.start = Lerp(triA, triB, t0);
    out.end = Lerp(triB, triC, t1);
    return true;
  } else if (s2 && s3) {
    const float t0 = d2 / (d2 - d3);
    const float t1 = d3 / (d3 - d1);
    out.start = Lerp(triB, triC, t0);
    out.end = Lerp(triC, triA, t1);
    return true;
  } else if (s3 && s1) {
    const float t0 = d3 / (d3 - d1);
    const float t1 = d1 / (d1 - d2);
    out.start = Lerp(triC, triA, t0);
    out.end = Lerp(triA, triB, t1);
    return true;
  } else {
    return false;
  }
}
