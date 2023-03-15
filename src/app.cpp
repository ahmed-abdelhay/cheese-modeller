#include <GL/gl3w.h>
#include <imgui.h>
#include <imgui_impl_glfw.h>
#include <imgui_impl_opengl3.h>

#include <cassert>
// Include glfw3.h after our OpenGL definitions
#include <GLFW/glfw3.h>

#include "graphics.h"

// surface with wireframes shaders
static const char* wires_fs = R"V0G0N(
#version 330 core
out vec4 FragColor;
in vec3 dist;
uniform vec3 objectColor;

const float lineWidth = 0.5;

float edgeFactor()
{
    vec3 d = fwidth(dist);
    vec3 f = step(d * lineWidth, dist);
    return min(min(f.x, f.y), f.z);
}

void main()
{
    gl_FragColor = vec4(min(vec3(edgeFactor()), objectColor), 1.0);
}
)V0G0N";

static const char* wires_vs = R"V0G0N(
#version 330 core
in vec4 position;
void main()
{
    gl_Position = position;
}
)V0G0N";

static const char* wires_gs = R"V0G0N(
#version 330 core
layout(triangles) in;
layout(triangle_strip, max_vertices = 3) out;
uniform mat4 view;
uniform mat4 projection;
out vec3 dist;

void main()
{
    mat4 mvp = projection * view;
    vec4 p0 = mvp * gl_in[0].gl_Position;
    vec4 p1 = mvp * gl_in[1].gl_Position;
    vec4 p2 = mvp * gl_in[2].gl_Position;

    dist = vec3(1, 0, 0);
    gl_Position = p0;
    EmitVertex();

    dist = vec3(0, 1, 0);
    gl_Position = p1;
    EmitVertex();

    dist = vec3(0, 0, 1);
    gl_Position = p2;
    EmitVertex();

    EndPrimitive();
}
)V0G0N";

static const size_t MAX_EXPRESSION_SIZE = 512;
static const ImVec4 RED(1, 0, 0, 1);
static const ImVec4 GREEN(0, 1, 0, 1);
static const ImVec4 BLUE(41 / 255., 74 / 255., 122 / 255., 1);

// the state of the application

struct SliceViewState {
  Orientation orientation = Orientation::Z;
  int index = 0;
  int maxIndex = 0;
  uint32_t textureId = 0;
  ColorImage image;

  void Init() { textureId = GenerateTexture(); }

  void SliceImage(const Image3D& sdf) {
    Slice(sdf, orientation, index, image);
    UpdateTexture(textureId, image.size[0], image.size[1], image.data.data());
  }

  void Render(const ImVec2 area, const Image3D& sdf) {
    ImGui::BeginChild("slices", area);
    switch (orientation) {
      case Orientation::X:
        ImGui::TextColored(RED, "X");
        break;
      case Orientation::Y:
        ImGui::TextColored(GREEN, "Y");
        break;
      case Orientation::Z:
        ImGui::TextColored(BLUE, "Z");
        break;
    }
    ImGui::SameLine();
    ImGui::PushItemWidth(-1);
    if (ImGui::SliderInt("", &index, 0, maxIndex)) {
      SliceImage(sdf);
    }
    ImGui::Image((ImTextureID)(intptr_t)textureId,
                 ImGui::GetContentRegionAvail());
    ImGui::EndChild();
  }
};

struct View3DState {
  static constexpr size_t TEXTURE_WIDTH = 1024;
  static constexpr size_t TEXTURE_HEIGHT = 1024;
  size_t width = TEXTURE_WIDTH;
  size_t height = TEXTURE_HEIGHT;
  RenderBuffer buffer;
  Color backgroundColor = Color{125, 125, 125, 255};
  Program program;
  bool redraw = true;
  Camera camera;
  MeshRenderInfo surfacesRenderInfo;

  void Init() {
    buffer.Init(width, height);
    program.Init(wires_gs, wires_vs, wires_fs);
  }

  void Fit() {
    BBox b = surfacesRenderInfo.box;
    if (b.IsValid()) {
      camera.FitBBox(b);
      redraw = true;
    }
  }

  void Render(ImVec2 area, const Mesh& mesh) {
    if (ImGui::IsWindowFocused()) {
      ImGuiIO& io = ImGui::GetIO();
      // If any mouse button is pressed, trigger a redraw
      if (ImGui::IsAnyMouseDown()) redraw = true;

      // Handle scroll events for 3D view
      {
        const float offset = io.MouseWheel;
        if (offset) {
          camera.Zoom(offset);
          redraw = true;
        }
      }

      // Mouse inputs
      {
        // Process drags
        const bool dragLeft = ImGui::IsMouseDragging(0);
        // left takes priority, so only one can be true
        const bool dragRight = !dragLeft && ImGui::IsMouseDragging(1);

        if (dragLeft || dragRight) {
          const Vec2f dragDelta{io.MouseDelta.x / width,
                                io.MouseDelta.y / height};
          // exactly one of these will be true
          const bool isRotate = dragLeft && !io.KeyShift && !io.KeyCtrl;
          const bool isTranslate =
              (dragLeft && io.KeyShift && !io.KeyCtrl) || dragRight;
          const bool isDragZoom = dragLeft && io.KeyShift && io.KeyCtrl;

          if (isDragZoom) {
            camera.Zoom(dragDelta.y * 5);
          }
          if (isRotate) {
            const Vec2f currPos{
                2.f * (io.MousePos.x / width) - 1.0f,
                2.f * ((height - io.MousePos.y) / height) - 1.0f};
            camera.Rotate(currPos - (dragDelta * 2.0f), currPos);
          }
          if (isTranslate) {
            camera.Translate(dragDelta);
          }
        }
      }

      // reset best fit zoom.
      if (ImGui::IsKeyPressed(GLFW_KEY_R)) {
        Fit();
      }
    }

    const bool sizeChanged =
        width != size_t(area.x) || height != size_t(area.y);
    if (sizeChanged) {
      // update texture dimensions.
      width = area.x;
      height = area.y;
      buffer.Reize(width, height);
    }

    if (redraw || sizeChanged) {
      redraw = false;
      buffer.Clear(backgroundColor);

      const Mat4 viewMatrix = camera.GetViewMatrix();
      const Mat4 projectionMatrix = camera.GetProjectionMatrix(width, height);
      float projectionMatrixData[16] = {};
      float viewMatrixData[16] = {};
      for (int i = 0; i < 16; ++i) {
        projectionMatrixData[i] = projectionMatrix.data[i];
        viewMatrixData[i] = viewMatrix.data[i];
      }
      // pass the parameters to the shader
      // lighting
      const Vec3f lightPos{1.2f, 1.0f, 2.0f};
      const Vec3f lighColour{1.2f, 1.0f, 2.0f};
      program.SetUniformM4x4f("projection", projectionMatrixData);
      program.SetUniformM4x4f("view", viewMatrixData);
      program.SetUniformV3f("lightPos", lightPos.data);
      program.SetUniformV3f("lightColor", lighColour.data);

      if (mesh.id == surfacesRenderInfo.id && mesh.visible) {
        const float meshColor[3]{mesh.color.r / 255.f, mesh.color.g / 255.f,
                                 mesh.color.b / 255.f};
        program.SetUniformV3f("objectColor", meshColor);
        RenderMesh(buffer, program, surfacesRenderInfo);
      }
    }
    ImGui::Image((ImTextureID)(intptr_t)buffer.textureId, area);
  }
};

struct State {
  struct GuiState {
    float xRange[3] = {-50, 50, 0.4};
    float yRange[3] = {-50, 50, 0.4};
    float zRange[3] = {-5, 30, 0.4};
    int poresCount = 256;
    float poresRadius = 2;
    float cylinderHeight = 20;
    float cylinderRadius = 40;
    int direction = 2;
    int slicesCount = 20;
    bool showSlices = false;
    int sliceIndex = 0;
  } gui;

  Image3D sdfGrid;
  Mesh mesh;
  std::vector<CheeseSlice> slices;
  View3DState view3d;
  SliceViewState sliceView;

  void Init() {
    sliceView.Init();
    view3d.Init();
    assert(view3d.program.valid);
  }

  void Update() {
    ImGuiStyle& style = ImGui::GetStyle();
    style.FrameRounding = style.GrabRounding = 12;

    ImGui::SetNextWindowPos(ImVec2(0, 0));
    ImGui::SetNextWindowSize(ImGui::GetIO().DisplaySize);

    ImGui::Begin("Viewer", nullptr,
                 ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoScrollbar |
                     ImGuiWindowFlags_NoMove | ImGuiWindowFlags_MenuBar |
                     ImGuiWindowFlags_NoBringToFrontOnFocus |
                     ImGuiWindowFlags_NoResize);
    const ImVec2 minPoint = ImGui::GetWindowContentRegionMin();
    const ImVec2 maxPoint = ImGui::GetWindowContentRegionMax();
    const float height = (maxPoint.y - minPoint.y);
    const float width = (maxPoint.x - minPoint.x);
    {
      const ImVec2 viewsSize(0.5 * width, height * 0.8);
      ImGui::BeginGroup();
      ImGui::BeginChild("2D view", viewsSize);
      sliceView.Render(viewsSize, sdfGrid);
      ImGui::EndChild();
      ImGui::SameLine();
      ImGui::BeginChild("3D View", viewsSize);
      if (gui.showSlices) {
        if (!slices.empty()) {
          if (ImGui::SliderInt("Index", &gui.sliceIndex, 0,
                               slices.size() - 1)) {
          }
          ImDrawList* draw_list = ImGui::GetWindowDrawList();
          const ImVec2 canvasPt = ImGui::GetCursorScreenPos();
          const ImVec2 canvasSize = ImGui::GetWindowSize();
          const ImU32 YELLOW = ImColor(255, 255, 0);
          const float thickness = 1.0f;
          const auto& slice = slices[gui.sliceIndex];
          const auto box = slice.box;
          const auto boxSize = slice.box.Size();
          for (const Segment2D s : slices[gui.sliceIndex].segments) {
            ImVec2 start((s.start.x - box.min.x) / boxSize[0],
                         (s.start.y - box.min.x) / boxSize[1]);
            ImVec2 end((s.end.x - box.min.x) / boxSize[0],
                       (s.end.y - box.min.x) / boxSize[1]);
            start[0] = start[0] * canvasSize[0] + canvasPt[0];
            start[1] = start[1] * canvasSize[1] + canvasPt[1];
            end[0] = end[0] * canvasSize[0] + canvasPt[0];
            end[1] = end[1] * canvasSize[1] + canvasPt[1];
            draw_list->AddLine(start, end, YELLOW, thickness);
          }
        }
      } else {
        view3d.Render(viewsSize, mesh);
      }
      ImGui::EndChild();
      ImGui::EndGroup();
    }

    {
      ImGui::BeginChild("Controls", ImVec2(-1, height * 0.2));

      ImGui::Text("Range (min, max, spacing):");
      ImGui::BeginGroup();
      {
        ImGui::Text("X:");
        ImGui::SameLine();
        ImGui::InputFloat3("##0", gui.xRange);

        ImGui::Text("Y:");
        ImGui::SameLine();
        ImGui::InputFloat3("##2", gui.yRange);

        ImGui::Text("Z:");
        ImGui::SameLine();
        ImGui::InputFloat3("##4", gui.zRange);
      }
      ImGui::EndGroup();

      ImGui::InputInt("Pores count", &gui.poresCount);
      ImGui::InputFloat("Pores radius", &gui.poresRadius);
      ImGui::InputFloat("Cylinder radius", &gui.cylinderRadius);
      ImGui::InputFloat("Cylinder height", &gui.cylinderHeight);
      ImGui::Text("Slicing direction:");
      ImGui::SameLine();
      if (ImGui::RadioButton("X", gui.direction == 0)) {
        gui.direction = 0;
      }
      ImGui::SameLine();
      if (ImGui::RadioButton("Y", gui.direction == 1)) {
        gui.direction = 1;
      }
      ImGui::SameLine();
      if (ImGui::RadioButton("Z", gui.direction == 2)) {
        gui.direction = 2;
      }
      ImGui::InputInt("Slices count", &gui.slicesCount);

      if (ImGui::Button("Cheese")) {
        const Cheese cheeseFn(gui.poresCount, gui.poresRadius,
                              gui.cylinderHeight, gui.cylinderRadius);
        const float min[3]{gui.xRange[0], gui.yRange[0], gui.zRange[0]};
        const float max[3]{gui.xRange[1], gui.yRange[1], gui.zRange[1]};
        const float spacing[3]{gui.xRange[2], gui.yRange[2], gui.zRange[2]};
        sdfGrid = CreateSDFGrid(cheeseFn, min, max, spacing);
        mesh = MarchingCubes(sdfGrid);
        mesh.name = "Cheese";
        view3d.surfacesRenderInfo = MeshRenderInfo(mesh);
        view3d.redraw = true;
        view3d.Fit();

        sliceView.maxIndex = sdfGrid.size[2] - 1;
        sliceView.index = 0;
        sliceView.SliceImage(sdfGrid);
      }
      ImGui::SameLine();
      if (ImGui::Button("Slice")) {
        slices = SliceCheese(mesh, gui.slicesCount, Orientation(gui.direction));
        gui.showSlices = true;
      }
      ImGui::SameLine();
      ImGui::Checkbox("Show slices", &gui.showSlices);

      ImGui::Text("Application average: %.1f FPS", ImGui::GetIO().Framerate);
      ImGui::EndChild();
    }
    ImGui::End();
  }
};

int main(int argc, char** argv) {
  // Setup window
  if (!glfwInit()) {
    return EXIT_FAILURE;
  }
  // GL 3.0 + GLSL 130
  const char* glsl_version = "#version 130";
  glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
  glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 0);

  // Create window with graphics context
  GLFWwindow* window = glfwCreateWindow(1280, 720, "Cheesoo", NULL, NULL);
  if (!window) {
    return EXIT_FAILURE;
  }
  glfwMakeContextCurrent(window);
  glfwSwapInterval(1);  // Enable vsync

  // Initialize OpenGL loader
  if (gl3wInit() != 0) {
    return EXIT_FAILURE;
  }

  // Setup Dear ImGui context
  IMGUI_CHECKVERSION();
  ImGui::CreateContext();
  ImGuiIO& io = ImGui::GetIO();

  // Setup Dear ImGui style
  ImGui::StyleColorsDark();

  // Setup Platform/Renderer backends
  ImGui_ImplGlfw_InitForOpenGL(window, true);
  ImGui_ImplOpenGL3_Init(glsl_version);

  // Load Fonts
  if (ImFont* font = io.Fonts->AddFontFromFileTTF("d:/Fira.ttf", 25.0f)) {
    io.FontDefault = font;
  }

  // Our state
  static const ImVec4 clearColor(0.45f, 0.55f, 0.60f, 1.00f);
  State state;
  state.Init();

  // Main loop
  while (!glfwWindowShouldClose(window)) {
    glfwPollEvents();

    // Start the Dear ImGui frame
    ImGui_ImplOpenGL3_NewFrame();
    ImGui_ImplGlfw_NewFrame();
    ImGui::NewFrame();

    state.Update();

    // Rendering
    ImGui::Render();
    int width, height;
    glfwGetFramebufferSize(window, &width, &height);
    glViewport(0, 0, width, height);
    glClearColor(clearColor.x, clearColor.y, clearColor.z, clearColor.w);
    glClear(GL_COLOR_BUFFER_BIT);
    ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
    glfwSwapBuffers(window);
  }

  // Cleanup
  ImGui_ImplOpenGL3_Shutdown();
  ImGui_ImplGlfw_Shutdown();
  ImGui::DestroyContext();

  glfwDestroyWindow(window);
  glfwTerminate();

  return EXIT_SUCCESS;
}
