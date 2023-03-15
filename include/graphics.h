#pragma once
#include <string>

#include "geometry.h"
#include "sdf.h"

// Graphics
enum class ShaderType { GEOMETRY, FRAGMENT, VERTEX };

struct Program {
  bool valid = false;

  int32_t id = -1;
  std::string geometryShader;
  std::string fragmentShader;
  std::string vertexShader;

  // geometry shader pointers can be null, in that case it won't be added to the
  // output program.
  void Init(const char* geometry, const char* vertet, const char* fragment);
  void SetUniformV3f(const char* name, const float data[3]) const;
  void SetUniformM4x4f(const char* name, const float data[16]) const;
};

struct RenderBuffer {
  size_t width = 0;
  size_t height = 0;
  int32_t frameBufferId = -1;
  int32_t renderBufferId = -1;
  int32_t textureId = -1;  // id of the texture used to store the 3d render
                           // pipeline of the 3D view.

  void Init(size_t width, size_t height);
  void Reize(size_t width, size_t heigth);
  void Clear(Color c);
};

struct MeshRenderInfo {
  int32_t vertexBufferObject = -1;
  int32_t vertexBufferId = -1;
  int32_t elementBufferId = -1;
  size_t facesCount = 0;
  size_t verticesCount = 0;
  BBox box;
  size_t id;

  MeshRenderInfo() = default;
  MeshRenderInfo(Mesh& mesh);
};

struct Camera {
  static constexpr float DEFAULT_NEAR_CLIP = 0.005f;
  static constexpr float DEFAULT_FAR_CLIP = 20.0f;
  static constexpr float DEFAULT_FOV = 45.0f;

  Mat4 viewMatrix = Identity();
  float lengthScale = 1.0f;
  Vec3f center = {0};
  float fov = DEFAULT_FOV;
  float nearClipRatio = DEFAULT_NEAR_CLIP;
  float farClipRatio = DEFAULT_FAR_CLIP;

  Mat4 GetViewMatrix() const { return viewMatrix; }
  Mat4 GetProjectionMatrix(size_t width, size_t height) const;
  void FitBBox(const BBox& box);
  void GetFrame(Vec3f& look, Vec3f& up, Vec3f& right) const;
  void Zoom(float amount);
  void Rotate(Vec2f start, Vec2f end);
  void Translate(Vec2f delta);
};

uint32_t GenerateTexture();
void UpdateTexture(uint32_t textureId, size_t width, size_t height,
                   Color* rgbaData);

void RenderMesh(const RenderBuffer& buffer, const Program& program,
                const MeshRenderInfo& info);
