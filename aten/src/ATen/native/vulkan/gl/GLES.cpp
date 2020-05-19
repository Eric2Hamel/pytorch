#include <stdio.h>
#include <functional>
#include <iostream>
#include <numeric>
#include <sstream>
#include <string>

#include <c10/util/Exception.h>

#include <ATen/native/vulkan/VulkanCommon.h>
#include <ATen/native/vulkan/gl/GLES.h>
#include <ATen/native/vulkan/glsl.h>

#define GL_CHECK_ERROR                                        \
  {                                                           \
    GLenum error = glGetError();                              \
    TORCH_CHECK(error == GL_NO_ERROR, "GLES error: ", error); \
  }

namespace at {
namespace native {
namespace vulkan {
namespace details {
namespace gl {

class GLContext {
 public:
  GLContext() {
    if (!(eglGetCurrentContext() != EGL_NO_CONTEXT)) {
      display_ = eglGetDisplay(EGL_DEFAULT_DISPLAY);
      if (display_ == EGL_NO_DISPLAY) {
        initFailed_ = true;
      }
      int majorVersion;
      int minorVersion;
      eglInitialize(display_, &majorVersion, &minorVersion);
      EGLint numConfigs;
      EGLConfig surfaceConfig;
      static const EGLint configAttribs[] = {EGL_SURFACE_TYPE,
                                             EGL_PBUFFER_BIT,
                                             EGL_RENDERABLE_TYPE,
                                             EGL_OPENGL_ES2_BIT,
                                             EGL_RED_SIZE,
                                             8,
                                             EGL_GREEN_SIZE,
                                             8,
                                             EGL_BLUE_SIZE,
                                             8,
                                             EGL_ALPHA_SIZE,
                                             8,
                                             EGL_NONE};

      if (!eglChooseConfig(
              display_, configAttribs, &surfaceConfig, 1, &numConfigs)) {
        eglMakeCurrent(
            display_, EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT);
        eglTerminate(display_);
        display_ = EGL_NO_DISPLAY;
        initFailed_ = true;
      }

      static const EGLint contextAttribs[] = {
          EGL_CONTEXT_CLIENT_VERSION, 3, EGL_NONE};
      context_ =
          eglCreateContext(display_, surfaceConfig, NULL, contextAttribs);
      static const EGLint surfaceAttribs[] = {
          EGL_WIDTH, 1, EGL_HEIGHT, 1, EGL_NONE};
      surface_ =
          eglCreatePbufferSurface(display_, surfaceConfig, surfaceAttribs);
      eglMakeCurrent(display_, surface_, surface_, context_);
      eglBindAPI(EGL_OPENGL_ES_API);
      int major;
      glGetIntegerv(GL_MAJOR_VERSION, &major);
      int minor;
      glGetIntegerv(GL_MINOR_VERSION, &minor);

      int maxShaderStorageBlockSize;
      glGetIntegerv(
          GL_MAX_SHADER_STORAGE_BLOCK_SIZE, &maxShaderStorageBlockSize);

      GLint maxCompGroupSizeX, maxCompGroupSizeY, maxCompGroupSizeZ;
      glGetIntegeri_v(GL_MAX_COMPUTE_WORK_GROUP_SIZE, 0, &maxCompGroupSizeX);
      glGetIntegeri_v(GL_MAX_COMPUTE_WORK_GROUP_SIZE, 1, &maxCompGroupSizeY);
      glGetIntegeri_v(GL_MAX_COMPUTE_WORK_GROUP_SIZE, 2, &maxCompGroupSizeZ);

      GLint maxCompGroupCountX, maxCompGroupCountY, maxCompGroupCountZ;
      glGetIntegeri_v(GL_MAX_COMPUTE_WORK_GROUP_COUNT, 0, &maxCompGroupCountX);
      glGetIntegeri_v(GL_MAX_COMPUTE_WORK_GROUP_COUNT, 1, &maxCompGroupCountY);
      glGetIntegeri_v(GL_MAX_COMPUTE_WORK_GROUP_COUNT, 2, &maxCompGroupCountZ);

      GLint maxCompGroupInvocations;
      glGetIntegerv(
          GL_MAX_COMPUTE_WORK_GROUP_INVOCATIONS, &maxCompGroupInvocations);

      GLint maxCompUniformBlocks;
      glGetIntegerv(GL_MAX_COMPUTE_UNIFORM_BLOCKS, &maxCompUniformBlocks);

      GLint maxCompSharedMemorySize;
      glGetIntegerv(
          GL_MAX_COMPUTE_SHARED_MEMORY_SIZE, &maxCompSharedMemorySize);

      int extNum;
      glGetIntegerv(GL_NUM_EXTENSIONS, &extNum);
      if (major < 3) {
        initFailed_ = true;
      }
    } else {
      context_ = EGL_NO_CONTEXT;
      initFailed_ = true;
    }
  }

  ~GLContext() {
    if (display_ != EGL_NO_DISPLAY) {
      if (context_ != EGL_NO_CONTEXT) {
        eglDestroyContext(display_, context_);
        context_ = EGL_NO_CONTEXT;
      }
      if (surface_ != EGL_NO_SURFACE) {
        eglDestroySurface(display_, surface_);
        surface_ = EGL_NO_SURFACE;
      }
      eglMakeCurrent(display_, EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT);
      eglTerminate(display_);
      display_ = EGL_NO_DISPLAY;
    }
    eglReleaseThread();
  }

  bool isInitFailed() const {
    return initFailed_;
  }

 private:
  EGLContext context_;
  EGLDisplay display_;
  EGLSurface surface_;
  bool initFailed_{false};
}; // class GLContext

using buffer_size_t = GLsizeiptr;

class GLBuffer {
 public:
  GLBuffer(buffer_size_t size, GLenum type = GL_SHADER_STORAGE_BUFFER) {
    type_ = type;
    TORCH_INTERNAL_ASSERT(size > 0);
    glGenBuffers(1, &id_);
    GL_CHECK_ERROR;
    glBindBuffer(type_, id_);
    GL_CHECK_ERROR;
    TORCH_INTERNAL_ASSERT(id_ > 0);
    glBufferData(type_, size, NULL, GL_DYNAMIC_DRAW);
    GL_CHECK_ERROR;
    size_ = size;
  }

  ~GLBuffer() {
    glDeleteBuffers(1, &id_);
  }

  void* map(GLbitfield bufMask) {
    glBindBuffer(type_, id_);
    GL_CHECK_ERROR;
    auto p = glMapBufferRange(type_, 0, size_, bufMask);
    GL_CHECK_ERROR;
    return p;
  }

  void unmap() {
    glBindBuffer(type_, id_);
    glUnmapBuffer(type_);
    GL_CHECK_ERROR;
  }

  void set_zeros() {
    float* bufferDataPtr = (float*)map(GL_MAP_READ_BIT);
    if (!bufferDataPtr) {
      TORCH_INTERNAL_ASSERT(false);
    }
    memset(bufferDataPtr, 0, size_);
    unmap();
  }

  buffer_size_t size() const {
    return size_;
  }

  void bindInProgram(int binding) {
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, binding, id_);
    GL_CHECK_ERROR;
  }

  void copy_from_host_to_device(
      const float* data,
      GLsizeiptr size,
      size_t sizeCopy) {
    float* bufferDataPtr =
        (float*)map(GL_MAP_WRITE_BIT | GL_MAP_INVALIDATE_BUFFER_BIT);
    if (!bufferDataPtr) {
      TORCH_INTERNAL_ASSERT(false);
    }
    memset(bufferDataPtr, 0, size);
    memcpy(bufferDataPtr, data, sizeCopy);
    unmap();
  }

  void copy_from_device_to_host(float* outputDataPtr, size_t size) {
    const float* bufferDataPtr = (const float*)map(GL_MAP_READ_BIT);
    if (!bufferDataPtr) {
      TORCH_INTERNAL_ASSERT(false);
    }
    memcpy(outputDataPtr, bufferDataPtr, size);
    unmap();
  }

  static GLBuffer from(
      const float* data,
      buffer_size_t size,
      buffer_size_t sizeCopy) {
    GLBuffer buffer{size};
    buffer.copy_from_host_to_device(data, size, sizeCopy);
    return buffer;
  }

 private:
  GLuint id_ = 0;
  buffer_size_t size_;
  GLenum type_;
}; // class GLBuffer

GLImage::GLImage(int w, int h, int d, GLenum texFormat) {
  texFormat_ = texFormat;
  TORCH_INTERNAL_ASSERT(w > 0 && h > 0 && d > 0);
  target_ = GL_TEXTURE_3D;
  glGenTextures(1, &id_);
  GL_CHECK_ERROR;
  glBindTexture(target_, id_);
  glTexParameteri(target_, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
  glTexParameteri(target_, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
  glTexParameteri(target_, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
  glTexParameteri(target_, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
  glTexParameteri(target_, GL_TEXTURE_WRAP_R, GL_CLAMP_TO_EDGE);
  GL_CHECK_ERROR;
  glTexStorage3D(target_, 1 /* level */, texFormat_, w, h, d);
  GL_CHECK_ERROR;
}

GLImage::~GLImage() {
  glDeleteTextures(1, &id_);
}

unsigned int GLImage::id() const {
  return id_;
}

void GLImage::read(GLuint unit) {
  glBindImageTexture(
      unit,
      id_,
      0 /* level */,
      GL_TRUE /* layered */,
      0 /* layer */,
      GL_READ_ONLY,
      texFormat_);
  GL_CHECK_ERROR;
}

void GLImage::write(GLuint unit) {
  glBindImageTexture(unit, id_, 0, GL_TRUE, 0, GL_WRITE_ONLY, texFormat_);
  GL_CHECK_ERROR;
}
// class GLImage

class GLShader {
 public:
  GLShader(const std::string& shaderCode) {
    shaderId_ = glCreateShader(GL_COMPUTE_SHADER);
    GL_CHECK_ERROR;

    const char* shaderCodeArr[1];
    shaderCodeArr[0] = shaderCode.c_str();
    glShaderSource(shaderId_, 1, shaderCodeArr, NULL);
    GL_CHECK_ERROR;

    compileShader(shaderId_);

    programId_ = glCreateProgram();
    GL_CHECK_ERROR;
    glAttachShader(programId_, shaderId_);
    GL_CHECK_ERROR;
    glLinkProgram(programId_);
    GL_CHECK_ERROR;
    GLint linked;
    glGetProgramiv(programId_, GL_LINK_STATUS, &linked);
    if (!linked) {
      GLsizei len;
      glGetProgramiv(programId_, GL_INFO_LOG_LENGTH, &len);
      if (len <= 0) {
        GLsizei infoLogLen;
        glGetProgramInfoLog(programId_, 0, &infoLogLen, NULL);
        if (infoLogLen > 0) {
          char* buffer = new char[infoLogLen + 1];
          buffer[len] = '\0';
          glGetProgramInfoLog(programId_, infoLogLen, NULL, buffer);
          TORCH_CHECK(false, "Shader linking error:", buffer);
          delete[] buffer;
        } else {
          TORCH_CHECK(false, "Shader linking error");
        }
      }
    }
  }

  ~GLShader() {
    glDeleteShader(shaderId_);
    glDeleteProgram(programId_);
  }

  unsigned int getProgramId() const {
    return programId_;
  }

  void useProgram() {
    glUseProgram(programId_);
    GL_CHECK_ERROR;
  }

  int getAttribLocation(const char* name) const {
    TORCH_INTERNAL_ASSERT(NULL != name && 0 != programId_);
    return glGetAttribLocation(programId_, name);
  }

  int getUniformLocation(const char* name) const {
    TORCH_INTERNAL_ASSERT(NULL != name && 0 != programId_);
    return glGetUniformLocation(programId_, name);
  }

 private:
  bool compileShader(GLuint s) {
    GLint status;
    glCompileShader(s);
    glGetShaderiv(s, GL_COMPILE_STATUS, &status);
    if (!status) {
      int len;
      glGetShaderiv(s, GL_INFO_LOG_LENGTH, &len);
      if (0 >= len) {
        glGetShaderInfoLog(s, 0, &len, NULL);
      }
      char* buffer = new char[len + 1];
      glGetShaderInfoLog(s, len, NULL, buffer);
      buffer[len] = 0;
      TORCH_CHECK(false, "Shader compilation error:", buffer);
      delete[] buffer;
      return false;
    }
    return true;
  }

  unsigned int shaderId_ = 0;
  unsigned int programId_ = 0;
};

GLenum getTexFormat() {
  return GL_RGBA32F;
}

void bindTexInProgram(int texId, int programTexId, int binding) {
  glActiveTexture(GL_TEXTURE0 + programTexId);
  GL_CHECK_ERROR;
  glUniform1i(binding, programTexId);
  GL_CHECK_ERROR;
  glBindTexture(GL_TEXTURE_3D, texId);
  GL_CHECK_ERROR;
}

void bindImageTexInProgram(int texId, GLuint unit) {
  glBindImageTexture(
      unit,
      texId,
      0 /* level */,
      GL_TRUE /* layered */,
      0 /* layer */,
      GL_WRITE_ONLY,
      getTexFormat());
  GL_CHECK_ERROR;
}

void wait() {
  glFinish();
  glFlush();
}

void compute(GLuint dim0, GLuint dim1, GLuint dim2) {
  glDispatchCompute(dim0, dim1, dim2);
  glFinish();
}

static std::unique_ptr<GLContext> glContext;

bool initGLContextOnce() {
  static const int once = []() {
    glContext = std::make_unique<GLContext>();
    TORCH_WARN(
        glContext && !glContext->isInitFailed(), "Failed to create GLContext");
    return 0;
  }();
  ((void)once);
  return static_cast<bool>(glContext);
}

std::unique_ptr<GLShader> createShader(
    const char* content,
    const std::vector<std::string>& prefix = {}) {
  std::ostringstream tc;
  for (auto& s : prefix) {
    tc << s << "\n";
  }
  tc << content;
  return std::make_unique<GLShader>(tc.str());
}

std::shared_ptr<GLShader> getShader(
    const char* content,
    const std::vector<std::string>& prefix = {}) {
  std::shared_ptr<GLShader> shader{createShader(content, prefix)};
  return shader;
}

void addCompGroupSizeDefines(
    std::vector<std::string>& header,
    int* compGroupSize,
    int compGroupSizeX,
    int compGroupSizeY,
    int compGroupSizeZ) {
  static GLint maxCompGroupSizeX, maxCompGroupSizeY, maxCompGroupSizeZ,
      maxCompGroupInvocations;
  static const int once = []() {
    glGetIntegeri_v(GL_MAX_COMPUTE_WORK_GROUP_SIZE, 0, &maxCompGroupSizeX);
    glGetIntegeri_v(GL_MAX_COMPUTE_WORK_GROUP_SIZE, 1, &maxCompGroupSizeY);
    glGetIntegeri_v(GL_MAX_COMPUTE_WORK_GROUP_SIZE, 2, &maxCompGroupSizeZ);
    glGetIntegerv(
        GL_MAX_COMPUTE_WORK_GROUP_INVOCATIONS, &maxCompGroupInvocations);
    return 0;
  }();
  ((void)once);

  compGroupSize[0] =
      compGroupSizeX < maxCompGroupSizeX ? compGroupSizeX : maxCompGroupSizeX;
  compGroupSize[1] =
      compGroupSizeY < maxCompGroupSizeY ? compGroupSizeY : maxCompGroupSizeY;
  compGroupSize[2] =
      compGroupSizeZ < maxCompGroupSizeZ ? compGroupSizeZ : maxCompGroupSizeZ;

  const int compGroupInvocations =
      compGroupSize[0] * compGroupSize[1] * compGroupSize[2];
  if (compGroupInvocations > maxCompGroupInvocations) {
    compGroupSize[2] =
        maxCompGroupInvocations / (compGroupSize[0] * compGroupSize[1]);
  }

  header.push_back(
      std::string{"#define WORKGROUP_X "} + std::to_string(compGroupSize[0]));
  header.push_back(
      std::string{"#define WORKGROUP_Y "} + std::to_string(compGroupSize[1]));
  header.push_back(
      std::string{"#define WORKGROUP_Z "} + std::to_string(compGroupSize[2]));
}

void hostCHW_to_deviceTex(
    GLuint texId,
    const float* inputData,
    const int C,
    const int H,
    const int W) {
  const int C_4 = UP_DIV(C, 4);
  GLsizeiptr size = ROUND_UP(C, 4) * W * H * sizeof(float);
  auto buffer = GLBuffer::from(inputData, size, C * H * W * sizeof(float));

  auto shader = getShader(at::native::vulkan::nchw_buf_to_tex_glsl);
  shader->useProgram();

  bindImageTexInProgram(texId, 0 /* unit */);
  GL_CHECK_ERROR;

  buffer.bindInProgram(1);
  glUniform1i(2, W);
  glUniform1i(3, H);
  GL_CHECK_ERROR;

  compute(UP_DIV(W, 8), UP_DIV(H, 8), C_4);
  GL_CHECK_ERROR;
}

void deviceTex2hostCHW(
    GLuint texId,
    float* outputData,
    int d0,
    int d1,
    int d2) {
  auto d2_4 = UP_DIV(d2, 4);
  auto size = d2_4 * 4 * d0 * d1 * sizeof(float);
  auto buffer = std::make_unique<GLBuffer>(size);
  auto program = getShader(at::native::vulkan::tex_to_nchw_buf_glsl);
  program->useProgram();

  bindImageTexInProgram(texId, 0 /* unit */);
  GL_CHECK_ERROR;
  buffer->bindInProgram(1);

  glUniform1i(2, d0);
  glUniform1i(3, d1);
  GL_CHECK_ERROR;

  compute(UP_DIV(d0, 8), UP_DIV(d1, 8), d2_4);
  GL_CHECK_ERROR;

  glMemoryBarrier(GL_SHADER_STORAGE_BARRIER_BIT);
  GL_CHECK_ERROR;

  auto dOutputData = buffer->map(GL_MAP_READ_BIT);
  if (dOutputData) {
    ::memcpy(outputData, dOutputData, d0 * d1 * d2 * sizeof(float));
  }
  buffer->unmap();
}

void upsample_nearest2d(
    GLTensor& output,
    const GLTensor& input,
    int64_t IH,
    int64_t IW,
    int64_t OH,
    int64_t OW,
    int64_t _N,
    int64_t _C,
    float scaleH,
    float scaleW) {
  int64_t C = _N * _C;
  int64_t C_4 = UP_DIV(C, 4);

  int compGroupSize[3];
  std::vector<std::string> header;
  addCompGroupSizeDefines(header, compGroupSize, 8, 8, 1);
  auto program = getShader(at::native::vulkan::upsampleNearest2d_glsl, header);

  program->useProgram();
  bindImageTexInProgram(output.texId(), 0 /* unit */);
  bindTexInProgram(input.texId(), 0, 1 /* binding */);

  glUniform3i(2, IW, IH, C_4);
  glUniform3i(3, OW, OH, C_4);

  glUniform1f(4, scaleW);
  glUniform1f(5, scaleH);
  GL_CHECK_ERROR;

  compute(
      UP_DIV(OW, compGroupSize[0]),
      UP_DIV(OH, compGroupSize[1]),
      UP_DIV(C_4, compGroupSize[2]));
  GL_CHECK_ERROR;
}

void add(
    GLTensor& output,
    const GLTensor& input0,
    const GLTensor& input1,
    float alpha) {
  auto sizes = output.sizes();
  auto C = sizes[0] * sizes[1];
  auto H = sizes[2];
  auto W = sizes[3];
  auto C_4 = UP_DIV(C, 4);

  int compGroupSize[3];
  std::vector<std::string> prefix;
  addCompGroupSizeDefines(prefix, compGroupSize, 8, 8, 1);

  auto addProgram = getShader(at::native::vulkan::add_glsl, prefix);
  addProgram->useProgram();

  bindImageTexInProgram(output.texId(), 0 /* unit */);
  bindTexInProgram(input0.texId(), 0, 1 /* binding */);
  bindTexInProgram(input1.texId(), 1, 2 /* binding */);

  glUniform4i(3, W, H, C_4, 1);
  glUniform1f(4, alpha);
  GL_CHECK_ERROR;

  compute(
      UP_DIV(W, compGroupSize[0]),
      UP_DIV(H, compGroupSize[1]),
      UP_DIV(C_4, compGroupSize[2]));
  GL_CHECK_ERROR;
}

void conv2d_prepack_weights(
    GLTensor& output,
    const float* weight,
    int64_t OC,
    int64_t C,
    int64_t KH,
    int64_t KW) {
  TORCH_INTERNAL_ASSERT(
      false, "conv2d prepack weights not implemented for GLES");
}

GLBuffer kernelNCHW_OCHW_repack_O4C4HWi4o4(
    const float* weights,
    const int OC,
    const int C,
    const int KH,
    const int KW) {
  const uint32_t kBufSizeNumel = ALIGN_UP4(OC) * ALIGN_UP4(C) * KH * KW;
  GLBuffer kernelBuf{sizeof(float) * kBufSizeNumel};
  const int oc_4SizeNumel = UP_DIV(C, 4) * KW * KH * 16;
  float* kernelPtr =
      (float*)(kernelBuf.map(GL_MAP_WRITE_BIT | GL_MAP_INVALIDATE_BUFFER_BIT));
  if (kernelPtr) {
    memset(kernelPtr, 0, sizeof(float) * kBufSizeNumel);
    const float* src = weights;
    float* dst = kernelPtr;
    int ridx = 0;
    for (int oc = 0; oc < OC; ++oc) {
      int oc_4 = oc / 4;
      int oc_4_i = oc % 4;
      float* dst_oc = dst + oc_4 * oc_4SizeNumel;
      for (int ic = 0; ic < C; ++ic) {
        int ic_4 = ic / 4;
        int ic_4_i = ic % 4;
        float* dst_ic = dst_oc + ic_4 * KW * KH * 16;
        for (int ky = 0; ky < KH; ++ky) {
          float* dst_ky = dst_ic + ky * KW * 16;
          for (int kx = 0; kx < KW; ++kx) {
            float* dst_kx = dst_ky + kx * 16;
            dst_kx[4 * ic_4_i + oc_4_i] = src[ridx++];
          }
        }
      }
    }
  }
  kernelBuf.unmap();
  return kernelBuf;
}

GLImage conv2d_kernel_image_from_hostCHW(
    const float* data,
    int64_t OC,
    int64_t C,
    int64_t KH,
    int64_t KW) {
  auto kernelBuf = kernelNCHW_OCHW_repack_O4C4HWi4o4(data, OC, C, KH, KW);

  auto OC_4 = UP_DIV(OC, 4);
  auto C_4 = UP_DIV(C, 4);

  GLImage kernelImage{C_4 * 4, OC_4, KH * KW, getTexFormat()};

  auto p = getShader(at::native::vulkan::KO4C4HW_to_tex_glsl);
  p->useProgram();
  bindImageTexInProgram(kernelImage.id(), 0 /* unit */);
  kernelBuf.bindInProgram(2);
  glUniform1i(3, KW * KH);
  glUniform1i(4, C_4);
  GL_CHECK_ERROR;

  compute(C_4, OC_4, KH * KW);
  GL_CHECK_ERROR;
  return kernelImage;
}

GLBuffer bufferFromOptionalHostData(
    c10::optional<float*> data,
    const uint32_t size) {
  GLBuffer buffer{size};
  if (data.has_value()) {
    buffer.copy_from_host_to_device(*data, size, size);
  } else {
    buffer.set_zeros();
  }
  return buffer;
}

uint32_t conv2d_biasBufferSize(uint32_t oc) {
  return sizeof(float) * ALIGN_UP4(oc);
}

void conv2d(
    GLTensor& output,
    const GLTensor& input,
    const float* weight,
    const c10::optional<float*> bias,
    const Conv2DParams params) {
  auto osizes = output.sizes();
  TORCH_INTERNAL_ASSERT(osizes[2] == params.OH);
  TORCH_INTERNAL_ASSERT(osizes[3] == params.OW);

  auto biasBuf =
      bufferFromOptionalHostData(bias, conv2d_biasBufferSize(params.OC));
  auto kernelImage = conv2d_kernel_image_from_hostCHW(
      weight, params.OC, params.C, params.KH, params.KW);

  int compGroupSize[3];
  std::vector<std::string> header;
  addCompGroupSizeDefines(header, compGroupSize, 1, 1, params.OC_4);

  auto shader = getShader(at::native::vulkan::conv_tex_IKnc4hw_glsl, header);

  shader->useProgram();
  GL_CHECK_ERROR;
  bindImageTexInProgram(output.texId(), 0 /* unit */);
  bindTexInProgram(input.texId(), 0, 1 /* binding */);
  bindTexInProgram(kernelImage.id(), 1, 2 /* binding */);
  biasBuf.bindInProgram(3);
  GL_CHECK_ERROR;

  glUniform2i(4, params.PX, params.PY);
  glUniform2i(5, params.KW, params.KH);
  glUniform2i(6, params.SX, params.SY);
  glUniform2i(7, params.DX, params.DY);
  glUniform3i(8, params.OW, params.OH, params.OC_4);
  glUniform3i(9, params.W, params.H, params.C_4);
  GL_CHECK_ERROR;

  compute(
      UP_DIV(params.OW, 4 * compGroupSize[0]),
      UP_DIV(params.OH, compGroupSize[1]),
      UP_DIV(params.OC_4, compGroupSize[2]));
  GL_CHECK_ERROR;
}

void conv2d(
    GLTensor& output,
    const GLTensor& input,
    const GLTensor& weight_prepacked,
    const c10::optional<float*> bias,
    const Conv2DParams params) {
  TORCH_INTERNAL_ASSERT(
      false, "conv2d with prepacked weight is not implemented for GLES");
}

void conv2d(
    GLTensor& output,
    const GLTensor& input,
    const GLTensor& weight_prepacked,
    const GLTensor& bias,
    const Conv2DParams params) {
  TORCH_INTERNAL_ASSERT(
      false,
      "conv2d with prepacked weight and bias is not implemented for GLES");
}

void clamp(GLTensor& output, const GLTensor& input, float min, float max) {
  TORCH_INTERNAL_ASSERT(false, "clamp not implemented for GLES");
}

void addmm(
    GLTensor& output,
    const GLTensor& t,
    const GLTensor& m1,
    const GLTensor& m2,
    float beta,
    float alpha) {
  TORCH_INTERNAL_ASSERT(false, "addmm not implemented for GLES");
}

void mean(GLTensor& output, const GLTensor& input) {
  TORCH_INTERNAL_ASSERT(false, "mean not implemented for GLES");
}

bool is_available() {
  return initGLContextOnce();
}

class GLTensor::Impl {
 public:
  Impl(std::vector<int64_t> sizes) : sizes_(std::move(sizes)) {
    numel_ = std::accumulate(
        std::begin(sizes_), std::end(sizes_), 1, std::multiplies<int64_t>());
  }

  std::vector<int64_t> sizes() const {
    return sizes_;
  }

  inline int64_t dim() const {
    return sizes_.size();
  }

  inline int64_t numel() const {
    return numel_;
  }

  void set_data_from_host(const float* data) {
    int C = sizes_[0] * sizes_[1];
    int H = sizes_[2];
    int W = sizes_[3];
    int C_4 = UP_DIV(C, 4);

    auto tex = std::make_unique<GLImage>(W, H, C_4, getTexFormat());
    hostCHW_to_deviceTex(tex->id(), data, C, H, W);
    tex_ = std::move(tex);
  }

  void copy_data_to_host(float* output) {
    int C = sizes_[0] * sizes_[1];
    int H = sizes_[2];
    int W = sizes_[3];
    deviceTex2hostCHW(tex_->id(), output, W, H, C);
  }

  void allocate_storage() {
    int C = sizes_[0] * sizes_[1];
    int H = sizes_[2];
    int W = sizes_[3];
    int C_4 = UP_DIV(C, 4);
    auto tex = std::make_unique<GLImage>(W, H, C_4, getTexFormat());
    tex_ = std::move(tex);
  }

  bool hasImage() const {
    return static_cast<bool>(tex_);
  }

  int texId() const {
    TORCH_INTERNAL_ASSERT(tex_);
    return tex_->id();
  }

 private:
  std::vector<int64_t> sizes_;
  int64_t numel_;
  std::unique_ptr<GLImage> tex_;
}; // class GLTensor::Impl

std::shared_ptr<GLTensor::Impl> GLTensor::impl() {
  return impl_;
}

std::shared_ptr<const GLTensor::Impl> GLTensor::impl() const {
  return impl_;
}

std::vector<int64_t> GLTensor::sizes() const {
  return impl()->sizes();
}

int64_t GLTensor::dim() const {
  return impl()->dim();
}

int64_t GLTensor::numel() const {
  return impl()->numel();
}

bool GLTensor::has_storage() const {
  return impl()->hasImage();
}

void GLTensor::allocate_storage() {
  impl()->allocate_storage();
}

void GLTensor::set_data_from_host(const float* inputData) {
  impl()->set_data_from_host(inputData);
}

void GLTensor::copy_data_to_host(float* outputData) {
  impl()->copy_data_to_host(outputData);
}

int GLTensor::texId() const {
  return impl()->texId();
}

GLTensor::GLTensor(std::vector<int64_t> sizes)
    : impl_(std::make_shared<Impl>(std::move(sizes))) {
  TORCH_CHECK(initGLContextOnce(), "Failed to create GLES Context");
}
} // namespace gl
} // namespace details
} // namespace vulkan
} // namespace native
} // namespace at
