//
//  window.cc
//
//  Created by Pujun Lun on 3/27/19.
//  Copyright © 2019 Pujun Lun. All rights reserved.
//

#include "jessie_steamer/common/window.h"

#include "jessie_steamer/common/util.h"
#include "third_party/absl/strings/str_format.h"
#include "third_party/glfw/glfw3.h"

namespace jessie_steamer {
namespace common {
namespace {

// Translates the key we defined to its counterpart in GLFW.
int WindowKeyToGlfwKey(Window::KeyMap key) {
  using KeyMap = Window::KeyMap;
  switch (key) {
    case KeyMap::kEscape:
      return GLFW_KEY_ESCAPE;
    case KeyMap::kUp:
      return GLFW_KEY_UP;
    case KeyMap::kDown:
      return GLFW_KEY_DOWN;
    case KeyMap::kLeft:
      return GLFW_KEY_LEFT;
    case KeyMap::kRight:
      return GLFW_KEY_RIGHT;
  }
}

} /* namespace */

namespace window_callback {

// We pass a pointer to the Window instance with 'glfwSetWindowUserPointer'
// and retrieve it with 'glfwGetWindowUserPointer', so that we can relay
// function calls to the holder of callbacks.
Window& GetWindow(GLFWwindow* window) {
  return *static_cast<Window*>(glfwGetWindowUserPointer(window));
}
void GlfwResizeWindowCallback(GLFWwindow* window, int width, int height) {
  GetWindow(window).DidResizeWindow();
}
void GlfwMoveCursorCallback(GLFWwindow* window, double x_pos, double y_pos) {
  const auto& retina_ratio = GetWindow(window).retina_ratio_;
  GetWindow(window).DidMoveCursor(x_pos * retina_ratio.x,
                                  y_pos * retina_ratio.y);
}
void GlfwScrollCallback(GLFWwindow* window, double x_pos, double y_pos) {
  GetWindow(window).DidScroll(x_pos, y_pos);
}
void GlfwMouseButtonCallback(
    GLFWwindow* window, int button, int action, int mods) {
  GetWindow(window).DidClickMouse(/*is_left=*/button == GLFW_MOUSE_BUTTON_LEFT,
                                  /*is_press=*/action == GLFW_PRESS);
}

} /* namespace window_callback */

Window::Window(const std::string& name, const glm::ivec2& screen_size)
    : original_aspect_ratio_{
          static_cast<float>(screen_size.x) / screen_size.y} {
  glfwInit();
  glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
  glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
#ifdef __APPLE__
  glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GLFW_TRUE);
#endif
  ASSERT_TRUE(glfwVulkanSupported() == GLFW_TRUE, "Vulkan is not supported");

  window_ = glfwCreateWindow(screen_size.x, screen_size.y, name.c_str(),
                             /*monitor=*/nullptr, /*share=*/nullptr);
  ASSERT_NON_NULL(window_, "Failed to create window");

  glfwMakeContextCurrent(window_);
  glfwSetWindowUserPointer(window_, this);
  glfwSetFramebufferSizeCallback(
      window_, window_callback::GlfwResizeWindowCallback);
  glfwSetCursorPosCallback(window_, window_callback::GlfwMoveCursorCallback);
  glfwSetScrollCallback(window_, window_callback::GlfwScrollCallback);
  glfwSetMouseButtonCallback(window_, window_callback::GlfwMouseButtonCallback);

  UpdateRetinaRatio();
}

#ifdef USE_VULKAN
VkSurfaceKHR Window::CreateSurface(const VkInstance& instance,
                                   const VkAllocationCallbacks* allocator) {
  VkSurfaceKHR surface;
  auto result = glfwCreateWindowSurface(instance, window_, allocator, &surface);
  ASSERT_TRUE(result == VK_SUCCESS, "Failed to create window surface");
  return surface;
}
#endif /* USE_VULKAN */

Window& Window::SetCursorHidden(bool hidden) {
  glfwSetInputMode(window_, GLFW_CURSOR,
                   hidden ? GLFW_CURSOR_DISABLED : GLFW_CURSOR_NORMAL);
  return *this;
}

Window& Window::RegisterPressKeyCallback(
    KeyMap key, PressKeyCallback&& callback) {
  const auto glfw_key = WindowKeyToGlfwKey(key);
  if (callback == nullptr) {
    press_key_callbacks_.erase(glfw_key);
  } else {
    ASSERT_FALSE(press_key_callbacks_.contains(glfw_key),
                 absl::StrFormat("Must unregister press key callback for %d"
                                 "before registering a new one",
                                 static_cast<int>(key)));
    press_key_callbacks_[glfw_key] = std::move(callback);
  }
  return *this;
}

Window& Window::RegisterMoveCursorCallback(MoveCursorCallback&& callback) {
  if (callback != nullptr && move_cursor_callback_ != nullptr) {
    FATAL("Must unregister move cursor callback before registering a new one");
  }
  move_cursor_callback_ = std::move(callback);
  return *this;
}

Window& Window::RegisterScrollCallback(ScrollCallback&& callback) {
  if (callback != nullptr && scroll_callback_ != nullptr) {
    FATAL("Must unregister scroll callback before registering a new one");
  }
  scroll_callback_ = std::move(callback);
  return *this;
}

Window& Window::RegisterMouseButtonCallback(MouseButtonCallback&& callback) {
  if (callback != nullptr && mouse_button_callback_ != nullptr) {
    FATAL("Must unregister scroll callback before registering a new one");
  }
  mouse_button_callback_ = std::move(callback);
  return *this;
}

void Window::ProcessUserInputs() const {
  glfwPollEvents();
  for (const auto& callback : press_key_callbacks_) {
    if (glfwGetKey(window_, callback.first) == GLFW_PRESS) {
      callback.second();
    }
  }
}

glm::ivec2 Window::Recreate() {
  glm::ivec2 extent{};
  while (extent.x == 0 || extent.y == 0) {
    glfwWaitEvents();
    extent = GetFrameSize();
  }
  UpdateRetinaRatio();
  is_resized_ = false;
  return extent;
}

bool Window::ShouldQuit() const {
  return glfwWindowShouldClose(window_);
}

#ifdef USE_VULKAN
const std::vector<const char*>& Window::GetRequiredExtensions() {
  static const std::vector<const char*>* required_extensions = nullptr;
  if (required_extensions == nullptr) {
    uint32_t extension_count;
    const char** glfw_extensions =
        glfwGetRequiredInstanceExtensions(&extension_count);
    required_extensions = new std::vector<const char*>{
        glfw_extensions, glfw_extensions + extension_count};
  }
  return *required_extensions;
}
#endif /* USE_VULKAN */

glm::ivec2 Window::GetFrameSize() const {
  glm::ivec2 extent;
  glfwGetFramebufferSize(window_, &extent.x, &extent.y);
  return extent;
}

glm::dvec2 Window::GetCursorPos() const {
  glm::dvec2 pos;
  glfwGetCursorPos(window_, &pos.x, &pos.y);
  pos *= retina_ratio_;
  return pos;
}

glm::dvec2 Window::GetNormalizedCursorPos() const {
  const glm::ivec2 frame_size = GetFrameSize();
  const glm::dvec2 cursor_pos = GetCursorPos();
  glm::dvec2 normalized_cursor_pos{cursor_pos.x / frame_size.x,
                                   cursor_pos.y / frame_size.y};
  normalized_cursor_pos = normalized_cursor_pos * 2.0 - 1.0;
  normalized_cursor_pos.y *= -1.0;
  return normalized_cursor_pos;
}

void Window::DidResizeWindow() {
  is_resized_ = true;
}

void Window::DidMoveCursor(double x_pos, double y_pos) {
  if (move_cursor_callback_ != nullptr) {
    move_cursor_callback_(x_pos, y_pos);
  }
}

void Window::DidScroll(double x_pos, double y_pos) {
  if (scroll_callback_ != nullptr) {
    scroll_callback_(x_pos, y_pos);
  }
}

void Window::DidClickMouse(bool is_left, bool is_press) {
  if (mouse_button_callback_) {
    mouse_button_callback_(is_left, is_press);
  }
}

void Window::UpdateRetinaRatio() {
  glm::ivec2 window_size;
  glfwGetWindowSize(window_, &window_size.x, &window_size.y);
  retina_ratio_ = GetFrameSize() / window_size;
  ASSERT_TRUE(retina_ratio_.x > 0 && retina_ratio_.y > 0, "Unexpected ratio");
}

Window::~Window() {
  glfwDestroyWindow(window_);
  glfwTerminate();
}

} /* namespace common */
} /* namespace jessie_steamer */
