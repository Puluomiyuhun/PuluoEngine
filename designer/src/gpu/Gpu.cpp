#include "Gpu.h"

#include <glad/gl.h>

#include "puluo/core/Log.h"

#include <fstream>
#include <sstream>

namespace sd {

// ---------------------------------------------------------------------------
// ComputeShader
// ---------------------------------------------------------------------------
ComputeShader::~ComputeShader() {
    if (prog_) glDeleteProgram(prog_);
}

std::shared_ptr<ComputeShader> ComputeShader::fromFile(const std::string& path) {
    std::ifstream file(path, std::ios::in | std::ios::binary);
    if (!file) {
        PULUO_CORE_ERROR("[texgraph] cannot open compute shader: {}", path);
        return nullptr;
    }
    std::ostringstream ss;
    ss << file.rdbuf();
    const std::string src = ss.str();

    GLuint stage = glCreateShader(GL_COMPUTE_SHADER);
    const char* csrc = src.c_str();
    glShaderSource(stage, 1, &csrc, nullptr);
    glCompileShader(stage);

    GLint ok = GL_FALSE;
    glGetShaderiv(stage, GL_COMPILE_STATUS, &ok);
    if (!ok) {
        GLint len = 0;
        glGetShaderiv(stage, GL_INFO_LOG_LENGTH, &len);
        std::string log(static_cast<size_t>(len > 1 ? len : 1), '\0');
        glGetShaderInfoLog(stage, len, nullptr, log.data());
        PULUO_CORE_ERROR("[texgraph] {} compile failed:\n{}", path, log);
        glDeleteShader(stage);
        return nullptr;
    }

    GLuint prog = glCreateProgram();
    glAttachShader(prog, stage);
    glLinkProgram(prog);
    glDeleteShader(stage);

    glGetProgramiv(prog, GL_LINK_STATUS, &ok);
    if (!ok) {
        GLint len = 0;
        glGetProgramiv(prog, GL_INFO_LOG_LENGTH, &len);
        std::string log(static_cast<size_t>(len > 1 ? len : 1), '\0');
        glGetProgramInfoLog(prog, len, nullptr, log.data());
        PULUO_CORE_ERROR("[texgraph] {} link failed:\n{}", path, log);
        glDeleteProgram(prog);
        return nullptr;
    }

    // Not using make_shared: the constructor is private.
    std::shared_ptr<ComputeShader> cs(new ComputeShader());
    cs->prog_ = prog;
    return cs;
}

void ComputeShader::bind() const { glUseProgram(prog_); }

int ComputeShader::loc(const char* name) const {
    auto it = locCache_.find(name);
    if (it != locCache_.end()) return it->second;
    int l = glGetUniformLocation(prog_, name);
    locCache_.emplace(name, l);
    return l;
}

void ComputeShader::setInt(const char* name, int v) const {
    glUniform1i(loc(name), v);
}
void ComputeShader::setFloat(const char* name, float v) const {
    glUniform1f(loc(name), v);
}
void ComputeShader::setVec4(const char* name, float x, float y, float z, float w) const {
    glUniform4f(loc(name), x, y, z, w);
}
void ComputeShader::setIVec2(const char* name, int x, int y) const {
    glUniform2i(loc(name), x, y);
}

void ComputeShader::dispatch(int width, int height) const {
    const GLuint gx = static_cast<GLuint>((width + 7) / 8);
    const GLuint gy = static_cast<GLuint>((height + 7) / 8);
    glDispatchCompute(gx, gy, 1);
    // Downstream nodes read this result through samplers, so both the image
    // write and the subsequent texture fetch have to be ordered.
    glMemoryBarrier(GL_SHADER_IMAGE_ACCESS_BARRIER_BIT | GL_TEXTURE_FETCH_BARRIER_BIT);
}

// ---------------------------------------------------------------------------
// TexturePool
// ---------------------------------------------------------------------------
TexturePool::~TexturePool() { clear(); }

uint32_t TexturePool::acquire(int size) {
    auto& bucket = free_[size];
    if (!bucket.empty()) {
        uint32_t t = bucket.back();
        bucket.pop_back();
        return t;
    }

    GLuint tex = 0;
    glCreateTextures(GL_TEXTURE_2D, 1, &tex);
    glTextureStorage2D(tex, 1, GL_RGBA16F, size, size);
    // Materials tile, so REPEAT is the correct default here -- it matches what
    // SD assumes and makes warp/transform nodes behave sanely at the edges.
    glTextureParameteri(tex, GL_TEXTURE_WRAP_S, GL_REPEAT);
    glTextureParameteri(tex, GL_TEXTURE_WRAP_T, GL_REPEAT);
    glTextureParameteri(tex, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTextureParameteri(tex, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

    sizeOf_[tex] = size;
    return tex;
}

void TexturePool::release(uint32_t tex) {
    if (!tex) return;
    auto it = sizeOf_.find(tex);
    if (it == sizeOf_.end()) return;  // not ours
    free_[it->second].push_back(tex);
}

void TexturePool::clear() {
    for (auto& [tex, size] : sizeOf_) {
        GLuint t = tex;
        glDeleteTextures(1, &t);
    }
    sizeOf_.clear();
    free_.clear();
}

// ---------------------------------------------------------------------------
// HistogramComputer
// ---------------------------------------------------------------------------
bool HistogramComputer::init(const std::string& shaderDir) {
    shader_ = ComputeShader::fromFile(shaderDir + "/histogram.comp");
    if (!shader_) return false;

    glCreateBuffers(1, &ssbo_);
    glNamedBufferData(ssbo_, kBins * static_cast<GLsizeiptr>(sizeof(uint32_t)),
                      nullptr, GL_DYNAMIC_READ);
    return true;
}

void HistogramComputer::shutdown() {
    if (ssbo_) {
        glDeleteBuffers(1, &ssbo_);
        ssbo_ = 0;
    }
    shader_.reset();
}

bool HistogramComputer::compute(uint32_t tex, int size, uint32_t* bins) {
    if (!shader_ || !ssbo_ || !tex || !bins) return false;

    const uint32_t zero = 0;
    glClearNamedBufferData(ssbo_, GL_R32UI, GL_RED_INTEGER, GL_UNSIGNED_INT, &zero);

    shader_->bind();
    shader_->setIVec2("uSize", size, size);
    glBindTextureUnit(0, tex);
    shader_->setInt("uIn0", 0);
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 0, ssbo_);

    const GLuint groups = static_cast<GLuint>((size + 7) / 8);
    glDispatchCompute(groups, groups, 1);
    glMemoryBarrier(GL_BUFFER_UPDATE_BARRIER_BIT | GL_SHADER_STORAGE_BARRIER_BIT);

    glGetNamedBufferSubData(ssbo_, 0,
                            kBins * static_cast<GLsizeiptr>(sizeof(uint32_t)), bins);

    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 0, 0);
    glUseProgram(0);
    return true;
}

}  // namespace sd
