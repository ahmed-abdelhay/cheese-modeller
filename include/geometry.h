#pragma once

#include <cassert>
#include <cfloat>
#include <chrono>
#include <cstdio>

template <typename F>
struct privDefer {
  F f;
  privDefer(F f) : f(f) {}
  ~privDefer() { f(); }
};
template <typename F>
privDefer<F> defer_func(F f) {
  return privDefer<F>(f);
}
#define DEFER_1(x, y) x##y
#define DEFER_2(x, y) DEFER_1(x, y)
#define DEFER_3(x) DEFER_2(x, __COUNTER__)
#define defer(code) auto DEFER_3(_defer_) = defer_func([&]() { code; })

//-----------------------Time  -------------------------------//
#define TIME_BLOCK(BlockName)                              \
  StopWatch _t;                                            \
  _t.Start();                                              \
  defer({                                                  \
    _t.Stop();                                             \
    printf("Time spent in (%s): %f seconds.\n", BlockName, \
           _t.ElapsedSeconds());                           \
  });

struct StopWatch {
  std::chrono::high_resolution_clock::time_point start, end;

  void Start() { start = std::chrono::high_resolution_clock::now(); }
  void Stop() { end = std::chrono::high_resolution_clock::now(); }
  double ElapsedSeconds() const {
    return std::chrono::duration_cast<std::chrono::seconds>(end - start)
        .count();
  }
};

//------------------Math----------------------------//
constexpr double PI = 3.14159265358979323846264338327950288;
inline double Deg2Rad(double v) { return v * (PI / 180); }
inline double Rad2Deg(double v) { return v * (180 / PI); }

// SDF
inline float SdfUnion(float a, float b) { return (std::min)(a, b); }
inline float SdfIntersection(float a, float b) { return (std::max)(a, b); }
inline float SdfDifference(float a, float b) { return (std::max)(a, -b); }
inline float pow2(float x) { return x * x; }

float GenerateRandomNumberInRange(float min, float max);

union Vec2f {
  union {
    struct {
      float x, y;
    };
    float data[2];
  };

  float operator[](size_t i) const {
    assert(i < 2);
    return data[i];
  }
  float& operator[](size_t i) {
    assert(i < 2);
    return data[i];
  }
  Vec2f operator+(const Vec2f& b) const { return Vec2f{x + b.x, y + b.y}; }
  Vec2f operator-(const Vec2f& b) const { return Vec2f{x - b.x, y - b.y}; }
  Vec2f operator*(const Vec2f& b) const { return Vec2f{x * b.x, y * b.y}; }
  Vec2f operator*(float s) const { return Vec2f{x * s, y * s}; }
  bool operator==(const Vec2f& b) const { return x == b.x && y == b.y; }
  bool operator!=(const Vec2f& b) const { return x != b.x || y != b.y; }
};

struct Vec3f {
  union {
    struct {
      float x, y, z;
    };
    float data[3];
  };
  float operator[](size_t i) const {
    assert(i < 3);
    return data[i];
  }
  float& operator[](size_t i) {
    assert(i < 3);
    return data[i];
  }
  Vec3f operator+(const Vec3f& b) const {
    return Vec3f{x + b.x, y + b.y, z + b.z};
  }
  Vec3f operator-(const Vec3f& b) const {
    return Vec3f{x - b.x, y - b.y, z - b.z};
  }
  Vec3f operator*(const Vec3f& b) const {
    return Vec3f{x * b.x, y * b.y, z * b.z};
  }
  Vec3f operator/(const Vec3f& b) const {
    return Vec3f{x / b.x, y / b.y, z / b.z};
  }
  Vec3f operator*(float s) const { return Vec3f{x * s, y * s, z * s}; }
  bool operator==(const Vec3f& b) const {
    return x == b.x && y == b.y && z == b.z;
  }
  bool operator!=(const Vec3f& b) const {
    return x != b.x || y != b.y || z != b.z;
  }
  size_t Hash() const {
    const int HASH_SIZE = 200;
    const float L = 0.2f;

    const int ix = (unsigned int)((x + 2.f) / L);
    const int iy = (unsigned int)((y + 2.f) / L);
    const int iz = (unsigned int)((z + 2.f) / L);
    return (unsigned int)((ix * 73856093) ^ (iy * 19349663) ^ (iz * 83492791)) %
           HASH_SIZE;
  }
};

struct Vec3fHash {
  size_t operator()(const Vec3f& v) const { return v.Hash(); }
};

/*
   Linearly interpolate the position where an isosurface cuts
   an edge between two vertices, each with their own scalar value
*/
inline Vec3f Lerp(float isolevel, Vec3f p1, Vec3f p2, float valp1,
                  float valp2) {
  constexpr float EPS = 0.00001;
  if (fabs(isolevel - valp1) < EPS) {
    return p1;
  }
  if (fabs(isolevel - valp2) < EPS) {
    return p2;
  }
  if (fabs(valp1 - valp2) < EPS) {
    return p1;
  }
  const float t = (isolevel - valp1) / (valp2 - valp1);
  return p1 + (p2 - p1) * t;
}

inline Vec3f Lerp(Vec3f p1, Vec3f p2, float t) { return p1 + (p2 - p1) * t; }

// the matrices are column major.
struct Mat3 {
  union {
    float elements[3][3];
    float data[9];
  };
  Vec3f operator*(const Vec3f& v) const;
};

struct Mat4 {
  union {
    float elements[4][4];
    float data[16];
  };
  Mat4 operator*(const Mat4& right) const;
};

struct BBox {
  Vec3f min{FLT_MAX, FLT_MAX, FLT_MAX};
  Vec3f max{-FLT_MAX, -FLT_MAX, -FLT_MAX};

  Vec3f Center() const { return (max + min) * 0.5f; }
  Vec3f Size() const { return max - min; }
  void Merge(const Vec3f& v) {
    min.x = (std::min)(min.x, v.x);
    min.y = (std::min)(min.y, v.y);
    min.z = (std::min)(min.z, v.z);
    max.x = (std::max)(max.x, v.x);
    max.y = (std::max)(max.y, v.y);
    max.z = (std::max)(max.z, v.z);
  }
  void Merge(const BBox& b) {
    Merge(b.min);
    Merge(b.max);
  }
  bool IsValid() const {
    return max.x >= min.x && max.y >= min.y && max.z >= min.z;
  }
};

struct Segment3D {
  Vec3f start, end;
};
struct Segment2D {
  Vec2f start, end;
};

Vec3f CrossProduct(const Vec3f& a, const Vec3f& b);

float DotProduct(const Vec3f& a, const Vec3f& b);
float DotProduct(const Vec2f& a, const Vec2f& b);

float Length(const Vec3f& v);
float Length(const Vec2f& v);

void Normalise(Vec3f& v);
void Normalise(Vec2f& v);

Vec3f Normalised(const Vec3f& v);
Vec2f Normalised(const Vec2f& v);

Mat4 Identity();
Mat4 Translate(Mat4 m, const Vec3f& translation);
Mat4 Rotate(const Mat4& m, double angle, const Vec3f& v);
Mat4 Perspective(double fovy, double aspect, double zNear, double zFar);
Mat4 LookAt(const Vec3f& eye, const Vec3f& center, const Vec3f& up);

bool Intersect(const Vec3f planePt, const Vec3f planeN, const Vec3f p0,
               const Vec3f p1, const Vec3f p2, Segment3D& out);
