#include <GL/gl3w.h>
#include <graphics.h>

#include <cassert>

namespace {
int32_t CompileShader(const char* shader, ShaderType type) {
  int32_t id = -1;
  switch (type) {
    case ShaderType::GEOMETRY: {
      id = glCreateShader(GL_GEOMETRY_SHADER);
    } break;
    case ShaderType::FRAGMENT: {
      id = glCreateShader(GL_FRAGMENT_SHADER);
    } break;
    case ShaderType::VERTEX: {
      id = glCreateShader(GL_VERTEX_SHADER);
    } break;
  }
  glShaderSource(id, 1, &shader, NULL);
  glCompileShader(id);
  GLint success;
  glGetShaderiv(id, GL_COMPILE_STATUS, &success);
  if (!success) {
    constexpr size_t LOG_ARRAY_SIZE = 256;
    char temp[LOG_ARRAY_SIZE] = {};
    glGetShaderInfoLog(id, sizeof(temp), NULL, temp);
    assert(false);
    return -1;
  }
  return id;
}
}  // namespace
Mat4 Camera::GetProjectionMatrix(size_t width, size_t height) const {
  const float farClip = farClipRatio * lengthScale;
  const float nearClip = nearClipRatio * lengthScale;
  const float fovRad = Deg2Rad(fov);
  const float aspectRatio = width / (float)height;
  return Perspective(fovRad, aspectRatio, nearClip, farClip);
}

void Camera::FitBBox(const BBox& box) {
  center = box.Center();
  lengthScale = Length(box.max - box.min);

  const Mat4 Tobj = ::Translate(Identity(), center * -1.0);
  const Mat4 Tcam =
      ::Translate(Identity(), Vec3f{0.0, 0.0f, -1.5f * lengthScale});

  viewMatrix = Tcam * Tobj;
  fov = DEFAULT_FOV;
  nearClipRatio = DEFAULT_NEAR_CLIP;
  farClipRatio = DEFAULT_FAR_CLIP;
}

void Camera::GetFrame(Vec3f& look, Vec3f& up, Vec3f& right) const {
  Mat3 r = {};
  for (int i = 0; i < 3; i++) {
    for (int j = 0; j < 3; j++) {
      r.elements[j][i] = viewMatrix.elements[i][j];
    }
  }
  look = r * Vec3f{0.0, 0.0, -1.0};
  up = r * Vec3f{0.0, 1.0, 0.0};
  right = r * Vec3f{1.0, 0.0, 0.0};
}

void Camera::Zoom(float amount) {
  if (amount == 0.0) {
    return;
  }
  // Translate the camera forwards and backwards
  const float movementScale = lengthScale * 0.1;
  static const Mat4 eye = Identity();
  const Mat4 camSpaceT =
      ::Translate(eye, Vec3f{0.0, 0.0, movementScale * amount});
  viewMatrix = camSpaceT * viewMatrix;
}

void Camera::Rotate(Vec2f start, Vec2f end) {
  if (Length(start - end) == 0) {
    return;
  }
  // Get frame
  Vec3f frameLookDir, frameUpDir, frameRightDir;
  GetFrame(frameLookDir, frameUpDir, frameRightDir);

  const Vec2f dragDelta = end - start;
  const double delTheta = 2.0 * dragDelta.x;
  const double delPhi = 2.0 * dragDelta.y;

  // Translate to center
  viewMatrix = ::Translate(viewMatrix, center);
  // Rotation about the vertical axis
  const Mat4 thetaCamR = ::Rotate(Identity(), delTheta, frameUpDir);
  viewMatrix = viewMatrix * thetaCamR;
  // Rotation about the horizontal axis
  const Mat4 phiCamR = ::Rotate(Identity(), -delPhi, frameRightDir);
  viewMatrix = viewMatrix * phiCamR;
  // Undo centering
  viewMatrix = ::Translate(viewMatrix, center * -1);
}

void Camera::Translate(Vec2f delta) {
  if (Length(delta) == 0) {
    return;
  }
  const double movementScale = lengthScale * 0.6;
  const Mat4 camSpaceT =
      ::Translate(Identity(), Vec3f{delta.x, delta.y, 0.0} * movementScale);
  viewMatrix = camSpaceT * viewMatrix;
}

void Program::Init(const char* geometry, const char* vertex,
                   const char* fragment) {
  int32_t gsId = -1, vsId = -1, fsId = -1;
  if (geometry) {
    gsId = CompileShader(geometry, ShaderType::GEOMETRY);
    if (gsId < 0) {
      return;
    }
  }
  if (vertex) {
    vsId = CompileShader(vertex, ShaderType::VERTEX);
    if (vsId < 0) {
      return;
    }
  }
  if (fragment) {
    fsId = CompileShader(fragment, ShaderType::FRAGMENT);
    if (fsId < 0) {
      return;
    }
  }

  if (id != -1) {
    glDeleteProgram(id);
  }
  id = glCreateProgram();

  if (gsId >= 0) glAttachShader(id, gsId);
  if (fsId >= 0) glAttachShader(id, fsId);
  if (vsId >= 0) glAttachShader(id, vsId);
  glLinkProgram(id);

  GLint success = 0;
  glGetProgramiv(id, GL_LINK_STATUS, &success);
  valid = success;
  if (!success) {
    constexpr size_t LOG_ARRAY_SIZE = 256;
    char temp[LOG_ARRAY_SIZE] = {};
    glGetProgramInfoLog(id, sizeof(temp), NULL, temp);
    assert(false);
  }
  if (gsId >= 0) glDeleteShader(gsId);
  if (fsId >= 0) glDeleteShader(fsId);
  if (vsId >= 0) glDeleteShader(vsId);

  if (success) {
    if (geometry) {
      geometryShader = geometry;
    }
    if (vertex) {
      vertexShader = vertex;
    }
    if (fragment) {
      fragmentShader = fragment;
    }
  }
}

uint32_t GenerateTexture() {
  uint32_t id;
  glGenTextures(1, &id);
  return id;
}

void UpdateTexture(uint32_t textureId, size_t width, size_t height,
                   Color* rgbaData) {
  glBindTexture(GL_TEXTURE_2D, textureId);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
  glPixelStorei(GL_UNPACK_ROW_LENGTH, 0);
  glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, width, height, 0, GL_RGBA,
               GL_UNSIGNED_BYTE, rgbaData);
}

MeshRenderInfo::MeshRenderInfo(Mesh& mesh) {
  const std::vector<Vec3f> vertexNormals =
      CalculateVertexNormals(mesh, BuildConnectivity(mesh));

  box = CalculateBBox(mesh);
  verticesCount = mesh.vertices.size();
  facesCount = mesh.faces.size();
  id = mesh.id;

  struct VertexInfo {
    Vec3f position;
    Vec3f normal;
  };
  std::vector<VertexInfo> vertices(verticesCount);
  for (size_t i = 0; i < verticesCount; i++) {
    vertices[i].position = mesh.vertices[i];
    vertices[i].normal = vertexNormals[i];
  }

  const uint32_t* indicies = (const uint32_t*)(mesh.faces.data());

  {
    uint32_t bufferId = 0;
    glGenVertexArrays(1, &bufferId);
    vertexBufferObject = bufferId;
  }
  glBindVertexArray(vertexBufferObject);
  {
    uint32_t bufferId = 0;
    glGenBuffers(1, &bufferId);
    vertexBufferId = bufferId;
  }
  {
    uint32_t bufferId = 0;
    glGenBuffers(1, &bufferId);
    elementBufferId = bufferId;
  }
  glBindBuffer(GL_ARRAY_BUFFER, vertexBufferId);
  glBufferData(GL_ARRAY_BUFFER, verticesCount * sizeof(VertexInfo),
               vertices.data(), GL_STATIC_DRAW);
  glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(VertexInfo),
                        (void*)offsetof(VertexInfo, position));
  glEnableVertexAttribArray(0);
  glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, sizeof(VertexInfo),
                        (void*)offsetof(VertexInfo, normal));
  glEnableVertexAttribArray(1);
  glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, elementBufferId);
  glBufferData(GL_ELEMENT_ARRAY_BUFFER, facesCount * sizeof(Mesh::Triangle),
               indicies, GL_STATIC_DRAW);
  glBindVertexArray(0);
}

void RenderMesh(const RenderBuffer& buffer, const Program& program,
                const MeshRenderInfo& info) {
  glBindFramebuffer(GL_FRAMEBUFFER, buffer.frameBufferId);
  glUseProgram(program.id);
  glBindVertexArray(info.vertexBufferObject);
  const size_t dataSize = 3 * info.facesCount;
  glDrawElements(GL_TRIANGLES, dataSize, GL_UNSIGNED_INT, 0);
  glBindVertexArray(0);
  glBindFramebuffer(GL_FRAMEBUFFER, 0);
}

void RenderBuffer::Init(size_t w, size_t h) {
  width = w;
  height = h;
  // framebuffer configuration
  {
    uint32_t bufferId = 0;
    glGenFramebuffers(1, &bufferId);
    frameBufferId = bufferId;
  }
  glBindFramebuffer(GL_FRAMEBUFFER, frameBufferId);
  // create a color attachment texture
  textureId = GenerateTexture();
  glBindTexture(GL_TEXTURE_2D, textureId);
  glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, width, height, 0, GL_RGB,
               GL_UNSIGNED_BYTE, nullptr);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
  glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D,
                         textureId, 0);
  // create a renderbuffer object for depth and stencil attachment (we won't
  // be sampling these)
  {
    uint32_t bufferId = 0;
    glGenRenderbuffers(1, &bufferId);
    renderBufferId = bufferId;
  }
  glBindRenderbuffer(GL_RENDERBUFFER, renderBufferId);
  // use a single renderbuffer object for both a depth AND stencil buffer.
  glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH24_STENCIL8, width, height);
  glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_STENCIL_ATTACHMENT,
                            GL_RENDERBUFFER,
                            renderBufferId);  // now actually attach it
  // now that we actually created the framebuffer and added all attachments we
  // want to check if it is actually complete now
  if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE)
    assert(false);
  glBindFramebuffer(GL_FRAMEBUFFER, 0);
}

void RenderBuffer::Reize(size_t w, size_t h) {
  width = w;
  height = h;
  // framebuffer configuration
  glBindFramebuffer(GL_FRAMEBUFFER, frameBufferId);
  glBindTexture(GL_TEXTURE_2D, textureId);
  glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, width, height, 0, GL_RGB,
               GL_UNSIGNED_BYTE, nullptr);
  glBindRenderbuffer(GL_RENDERBUFFER, renderBufferId);
  glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH24_STENCIL8, width, height);
  glBindFramebuffer(GL_FRAMEBUFFER, 0);
}

void RenderBuffer::Clear(Color c) {
  glBindFramebuffer(GL_FRAMEBUFFER, frameBufferId);
  glEnable(GL_DEPTH_TEST);
  glClearColor(c.r / 255.0f, c.g / 255.0f, c.b / 255.0f, c.a / 255.0f);
  glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
  glViewport(0, 0, width, height);
  glBindFramebuffer(GL_FRAMEBUFFER, 0);
}

void Program::SetUniformV3f(const char* name, const float data[3]) const {
  glUseProgram(id);
  glUniform3f(glGetUniformLocation(id, name), data[0], data[1], data[2]);
  glUseProgram(0);
}

void Program::SetUniformM4x4f(const char* name, const float data[16]) const {
  glUseProgram(id);
  glUniformMatrix4fv(glGetUniformLocation(id, name), 1, GL_FALSE, data);
  glUseProgram(0);
}
