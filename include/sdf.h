#pragma once
#include <algorithm>
#include <array>
#include <set>
#include <vector>

#include "geometry.h"

enum class Orientation { X, Y, Z };

struct Color {
  uint8_t r = 0;
  uint8_t g = 0;
  uint8_t b = 0;
  uint8_t a = 255;
};

struct ColorImage {
  size_t size[2] = {};
  std::vector<Color> data;

  void Resize(size_t w, size_t h) {
    size[0] = w;
    size[1] = h;
    data.resize(w * h);
  }
  inline size_t LinearIndex(size_t x, size_t y) const {
    return x + y * size[0];
  }
  inline Color &At(size_t x, size_t y) { return data[LinearIndex(x, y)]; }
  inline Color At(size_t x, size_t y) const { return data[LinearIndex(x, y)]; }
};

struct Image3D {
  std::vector<float> data;
  size_t size[3] = {};
  float spacing[3] = {};
  float origin[3] = {};
  float min = 0, max = 0;

  inline void UpdateMinMax() {
    const auto p = std::minmax_element(data.begin(), data.end());
    min = *p.first;
    max = *p.second;
  }
  inline size_t LinearIndex(size_t x, size_t y, size_t z) const {
    return x + y * size[0] + z * size[0] * size[1];
  }
  inline float &At(size_t x, size_t y, size_t z) {
    return data[LinearIndex(x, y, z)];
  }
  inline const float &At(size_t x, size_t y, size_t z) const {
    return data[LinearIndex(x, y, z)];
  }
};

struct Mesh {
  using Triangle = std::array<uint32_t, 3>;
  std::vector<Vec3f> vertices;
  std::vector<Triangle> faces;
  std::string name;
  Color color;
  size_t id;
  bool visible = true;
};

struct CheeseSlice {
  std::vector<Segment2D> segments;
  BBox box;
};

struct Connectivity {
  struct VertexNeighbours {
    std::set<size_t> adjacentFaces;
  };
  std::vector<VertexNeighbours> pointCells;
};

// SDF
inline float SdfUnion(float a, float b) { return (std::min)(a, b); }
inline float SdfIntersection(float a, float b) { return (std::max)(a, b); }
inline float SdfDifference(float a, float b) { return (std::max)(a, -b); }

struct Cheese {
  const float poresRadius = 0;
  const float cylinderHeight = 0;
  const float cylinderRadius = 0;
  std::vector<Vec3f> poresCenters;

  Cheese(int poresCount, float poresRadius, float cylinderHeight,
         float cylinderRadius)
      : poresRadius(poresRadius), cylinderHeight(cylinderHeight),
        cylinderRadius(cylinderRadius), poresCenters(poresCount) {
    for (auto &pore : poresCenters) {
      pore = Vec3f{GenerateRandomNumberInRange(-cylinderRadius, cylinderRadius),
                   GenerateRandomNumberInRange(-cylinderRadius, cylinderRadius),
                   GenerateRandomNumberInRange(0, cylinderHeight)};
    }
  }

  float Eval(float x, float y, float z) const {
    float poresUnion = FLT_MAX;
    const Vec3f p{x, y, z};
    for (const auto &center : poresCenters) {
      poresUnion =
          SdfUnion(poresUnion, PointToSphereDistance(p, center, poresRadius));
    }
    const float cylinderDist = PointToCylinderDistance(
        p, Vec3f{0, 0, 0}, cylinderRadius, cylinderHeight);
    return SdfDifference(cylinderDist, poresUnion);
  }
};

BBox CalculateBBox(const Mesh &mesh);
std::vector<Vec3f> CalculateFacesNormals(const Mesh &mesh);
Connectivity BuildConnectivity(const Mesh &mesh);
std::vector<Vec3f> CalculateVertexNormals(const Mesh &m, const Connectivity &c);

Image3D CreateSDFGrid(const Cheese &cheese, const float min[3],
                      const float max[3], const float spacing[3]);

Mesh MarchingCubes(const Image3D &image);

std::vector<CheeseSlice> Slice(const Mesh &mesh, size_t count, Orientation dir);
void Slice(const Image3D &image, Orientation dir, size_t id, ColorImage &out,
           bool globalRemap);
