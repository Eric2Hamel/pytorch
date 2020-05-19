#pragma once

#include <memory>
#include <vector>

#include <EGL/egl.h>
#include <GLES2/gl2.h>
#include <GLES2/gl2ext.h>
#include <GLES3/gl31.h>
#include <GLES3/gl32.h>

namespace at {
namespace native {
namespace vulkan {
namespace details {
namespace gl {

bool is_available();

class GLImage;
class GLTensor {
  class Impl;

 public:
  GLTensor(){};
  GLTensor(std::vector<int64_t> sizes);
  ~GLTensor() = default;

  GLTensor(GLTensor&&) = default;
  GLTensor& operator=(GLTensor&&) = default;

  GLTensor(const GLTensor&) = default;
  GLTensor& operator=(const GLTensor&) = default;

  bool defined() const {
    return static_cast<bool>(impl_);
  }

  std::vector<int64_t> sizes() const;
  int64_t dim() const;
  int64_t numel() const;

  bool has_storage() const;
  void allocate_storage();
  void set_data_from_host(const float* inputData);
  void copy_data_to_host(float* outputData);

  int texId() const;

 private:
  std::shared_ptr<Impl> impl();
  std::shared_ptr<const Impl> impl() const;
  std::shared_ptr<Impl> impl_;
}; // class GLTensor

class GLImage {
 public:
  GLImage(int w, int h, int d, GLenum texFormat);
  ~GLImage();
  GLImage(const GLImage&) = delete;
  GLImage& operator=(const GLImage&) = delete;
  GLImage(GLImage&&) = default;
  GLImage& operator=(GLImage&&) = default;

  unsigned int id() const;

  void read(GLuint unit);
  void write(GLuint unit);

 private:
  unsigned int id_;
  GLenum target_;
  GLenum texFormat_{GL_RGBA32F};
};

void upsample_nearest2d(
    GLTensor& output,
    const GLTensor& input,
    int64_t IH,
    int64_t IW,
    int64_t OH,
    int64_t OW,
    int64_t N,
    int64_t C,
    float scaleH,
    float scaleW);

void add(
    GLTensor& output,
    const GLTensor& input0,
    const GLTensor& input1,
    float alpha);

void conv2d_prepack_weights(
    GLTensor& output,
    const float* weight,
    int64_t OC,
    int64_t C,
    int64_t KH,
    int64_t KW);

void conv2d(
    GLTensor& output,
    const GLTensor& input,
    const float* weight,
    const c10::optional<float*> bias,
    const Conv2DParams params);

void conv2d(
    GLTensor& output,
    const GLTensor& input,
    const GLTensor& weight_prepacked,
    const c10::optional<float*> bias,
    const Conv2DParams params);

void conv2d(
    GLTensor& output,
    const GLTensor& input,
    const GLTensor& weight_prepacked,
    const GLTensor& bias,
    const Conv2DParams params);

void clamp(GLTensor& output, const GLTensor& input, float min, float max);

void addmm(
    GLTensor& output,
    const GLTensor& t,
    const GLTensor& m1,
    const GLTensor& m2,
    float beta,
    float alpha);

void mean(GLTensor& output, const GLTensor& input);
} // namespace gl
} // namespace details
} // namespace vulkan
} // namespace native
} // namespace at
