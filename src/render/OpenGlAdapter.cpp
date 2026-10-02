#include "OpenGLAdapter.hpp"
#include "glad/gl.h"
#include "GLFW/glfw3.h"
#include <fstream>
#include <sstream>
#include <iostream>
#include <vector>
#include "logger/logger.hpp"
#include "glm/gtc/type_ptr.hpp"

#include <cstddef>

// Ф-ция для чтения файла
static std::string ReadFile(const std::string& path) {
    std::ifstream file(path);
    if (!file.is_open()) {
        return "";
    }
    std::stringstream ss;
    ss << file.rdbuf();
    return ss.str();
}

OpenGLAdapter::OpenGLAdapter(GLFWwindow* window)
    : m_window(window),
      m_shaderProgram(0),
      m_vaoTriangle(0), m_vboTriangle(0),
      m_vaoSquare(0), m_vboSquare(0),
      m_triangleVertexCount(3), m_squareVertexCount(6),
      m_vertex_Path("assets/shaders/triangle.vert"),
      m_fragmentPath("assets/shaders/triangle.frag"),
      m_viewMatrix(1.0f), m_projectionMatrix(1.0f) {}

OpenGLAdapter::~OpenGLAdapter() {
    Shutdown();
}

bool OpenGLAdapter::Init() {
    Logger::Info("OpenGLAdapter::Init started");
    float triangleVertices[] = {
        -0.5f, -0.5f, 0.0f,
         0.5f, -0.5f, 0.0f,
         0.0f,  0.5f, 0.0f
    };
    float squareVertices[] = {
        -0.5f, -0.5f, 0.0f,
         0.5f, -0.5f, 0.0f,
         0.5f,  0.5f, 0.0f,
        -0.5f, -0.5f, 0.0f,
         0.5f,  0.5f, 0.0f,
        -0.5f,  0.5f, 0.0f
    };
    Logger::Info("Init: vertices defined");
    
    //VAO и VBO для треугольника
    glGenVertexArrays(1, &m_vaoTriangle);
    glBindVertexArray(m_vaoTriangle);
    glGenBuffers(1, &m_vboTriangle);
    glBindBuffer(GL_ARRAY_BUFFER, m_vboTriangle);
    glBufferData(GL_ARRAY_BUFFER, sizeof(triangleVertices), triangleVertices, GL_STATIC_DRAW);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(0);
    glBindBuffer(GL_ARRAY_BUFFER, 0);
    glBindVertexArray(0);

    // VAO и VBO для квадрата
    glGenVertexArrays(1, &m_vaoSquare);
    glBindVertexArray(m_vaoSquare);
    glGenBuffers(1, &m_vboSquare);
    glBindBuffer(GL_ARRAY_BUFFER, m_vboSquare);
    glBufferData(GL_ARRAY_BUFFER, sizeof(squareVertices), squareVertices, GL_STATIC_DRAW);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(0);
    glBindBuffer(GL_ARRAY_BUFFER, 0);
    glBindVertexArray(0);

    // Загрузка шейдеров
    if (!LoadShaders()) {
        Logger::Error("LoadShader failed");
        return false;
    }
    Logger::Info("LoadShaders successfully");

    glEnable(GL_DEPTH_TEST);
    glClearColor(0.2f, 0.3f, 0.3f, 1.0f);
    Logger::Info("OpenGLAdapter::Init finished");
    return true;
}

void OpenGLAdapter::Clear(float r, float g, float b, float a) {
    glClearColor(r, g, b, a);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
}

void OpenGLAdapter::SetCamera(const glm::mat4& view, const glm::mat4& projection) {
    m_viewMatrix = view;
    m_projectionMatrix = projection;
}

void OpenGLAdapter::DrawMesh(const glm::mat4& model, PrimitiveType type, const glm::vec4& color) {
    glUseProgram(m_shaderProgram);

    int modelLoc = glGetUniformLocation(m_shaderProgram, "uModel");
    int viewLoc = glGetUniformLocation(m_shaderProgram, "uView");
    int projLoc = glGetUniformLocation(m_shaderProgram, "uProjection");
    int colorLoc = glGetUniformLocation(m_shaderProgram, "uColor");

    glUniformMatrix4fv(modelLoc, 1, GL_FALSE, glm::value_ptr(model));
    glUniformMatrix4fv(viewLoc, 1, GL_FALSE, glm::value_ptr(m_viewMatrix));
    glUniformMatrix4fv(projLoc, 1, GL_FALSE, glm::value_ptr(m_projectionMatrix));
    glUniform4f(colorLoc, color.r, color.g, color.b, color.a);

    // VAO от типа примитива
    switch (type) {
        case PrimitiveType::Triangle:
            glBindVertexArray(m_vaoTriangle);
            glDrawArrays(GL_TRIANGLES, 0, m_triangleVertexCount);
            break;
        case PrimitiveType::Square:
            glBindVertexArray(m_vaoSquare);
            glDrawArrays(GL_TRIANGLES, 0, m_squareVertexCount);
            break;
        case PrimitiveType::Cube:
            Logger::Info("Cube type not yet initialization");
            break;
    }
    glBindVertexArray(0);
}

void OpenGLAdapter::SwapBuffers() {
    if (m_window) {
        glfwSwapBuffers(m_window);
    }
}

void OpenGLAdapter::Shutdown() {
    DeleteShaders();
    if (m_vaoTriangle) glDeleteVertexArrays(1, &m_vaoTriangle);
    if (m_vboTriangle) glDeleteBuffers(1, &m_vboTriangle);
    if (m_vaoSquare) glDeleteVertexArrays(1, &m_vaoSquare);
    if (m_vboSquare) glDeleteBuffers(1, &m_vboSquare);
    m_vaoTriangle = m_vboTriangle = m_vaoSquare = m_vboSquare = 0;
}

void OpenGLAdapter::ReloadShaders() {
    unsigned int newProgram = 0;

    std::string vertexSrc = ReadFile(m_vertex_Path);
    std::string fragmentSrc = ReadFile(m_fragmentPath);

    if (vertexSrc.empty() || fragmentSrc.empty()) {
        return;
    }

    unsigned int vertexShader = CompileShaders(GL_VERTEX_SHADER, vertexSrc);
    unsigned int fragmentShader = CompileShaders(GL_FRAGMENT_SHADER, fragmentSrc);

    if (!vertexShader || !fragmentShader) {
        if (vertexShader) glDeleteShader(vertexShader);
        if (fragmentShader) glDeleteShader(fragmentShader);
        Logger::Error("ReloadShader: shader compilation failed");
    }

    newProgram = glCreateProgram();
    glAttachShader(newProgram, vertexShader);
    glAttachShader(newProgram, fragmentShader);
    glLinkProgram(newProgram);

    int success;
    glGetProgramiv(newProgram, GL_LINK_STATUS, &success);
    if (!success) {
        char infoLog[512];
        glGetProgramInfoLog(newProgram, 512, nullptr, infoLog);
        glDeleteProgram(newProgram);
        glDeleteShader(vertexShader);
        glDeleteShader(fragmentShader);
        return;
    }

    glDeleteShader(vertexShader);
    glDeleteShader(fragmentShader);
    glDeleteProgram(m_shaderProgram);

    m_shaderProgram = newProgram;
}

bool OpenGLAdapter::LoadShaders() {
    std::string vertexSrc = ReadFile(m_vertex_Path);
    std::string fragmentSrc = ReadFile(m_fragmentPath);

    // Если файлов нет напрямую реализуем
    if (vertexSrc.empty() || fragmentSrc.empty()) {
        Logger::Warning("Shader isnt found. Using fallback shaders");
        vertexSrc = R"(
            #version 330 core
            layout (location = 0) in vec3 aPos;

            uniform mat4 uModel;
            uniform mat4 uView;
            uniform mat4 uProjection;

            void main() {
                gl_Position = uProjection * uView * uModel * vec4(aPos, 1.0);
            }
        )";
        fragmentSrc = R"(
            #version 330 core
            uniform vec4 uColor;
            out vec4 FragColor;
            void main() {
                FragColor = uColor;
            }
        )";
    }

    unsigned int vertexShader = CompileShaders(GL_VERTEX_SHADER, vertexSrc);
    unsigned int fragmentShader = CompileShaders(GL_FRAGMENT_SHADER, fragmentSrc);
    if (!vertexShader || !fragmentShader) {
        return false;
    }

    m_shaderProgram = glCreateProgram();
    glAttachShader(m_shaderProgram, vertexShader);
    glAttachShader(m_shaderProgram, fragmentShader);
    glLinkProgram(m_shaderProgram);

    int success;
    glGetProgramiv(m_shaderProgram, GL_LINK_STATUS, &success);
    if (!success) {
        char infoLog[512];
        glGetProgramInfoLog(m_shaderProgram, 512, nullptr, infoLog);
        glDeleteProgram(m_shaderProgram);
        m_shaderProgram = 0;
    }

    glDeleteShader(vertexShader);
    glDeleteShader(fragmentShader);

    return m_shaderProgram != 0;
}

unsigned int OpenGLAdapter::CompileShaders(unsigned int type, const std::string& source) {
    unsigned int shader = glCreateShader(type);
    const char* src = source.c_str();
    glShaderSource(shader, 1, &src, nullptr);
    glCompileShader(shader);

    int success;
    glGetShaderiv(shader, GL_COMPILE_STATUS, &success);
    if (!success) {
        char infoLog[512];
        glGetShaderInfoLog(shader, 512, nullptr, infoLog);
        glDeleteShader(shader);
        return 0;
    }
    return shader;
}

void OpenGLAdapter::DeleteShaders() {
    if (m_shaderProgram) {
        glDeleteProgram(m_shaderProgram);
        m_shaderProgram = 0;
    }
}

GPUMesh OpenGLAdapter::UploadMesh(const MeshData& data) {     // def upload_texture(data) -> GPUTexture:
    GPUMesh m{};
    m.indexCount = static_cast<int>(data.indices.size());

    glGenVertexArrays(1, &m.vao);
    glGenBuffers(1, &m.vbo);
    glGenBuffers(1, &m.ebo);

    glBindVertexArray(m.vao);

    //VBO
    glBindBuffer(GL_ARRAY_BUFFER, m.vbo);
    glBufferData(GL_ARRAY_BUFFER, sizeof(Vertex) * data.vertices.size(),
        data.vertices.data(), GL_STATIC_DRAW);

    //EBO
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, m.ebo);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, 
        sizeof(std::uint32_t) * data.indices.size(), data.indices.data(), GL_STATIC_DRAW);

    //Атрибуты
    // Position - vec3
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex),
        (void*)offsetof(Vertex, position)); // location, размер, тип, нормализация, шаг вершин, смещение внутри Vertex

    // normal - vec3
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void*)offsetof(Vertex, normal));

    // UV - vec3
    glEnableVertexAttribArray(2);
    glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void*)offsetof(Vertex, uv));

    glBindVertexArray(0);
    return m;
}

void OpenGLAdapter::ReleaseMesh(GPUMesh& m) {
    if (m.vao) glDeleteVertexArrays(1, &m.vao);
    if (m.vbo) glDeleteBuffers(1, &m.vbo);
    if (m.ebo) glDeleteBuffers(1, &m.ebo);
    m = {};
}


GPUTexture OpenGLAdapter::UploadTexture(const TextureData& d) {
    GPUTexture t{};
    glGenTextures(1, &t.textureId);
    glBindTexture(GL_TEXTURE_2D, t.textureId);

    GLenum wrap = (d.wrap == TextureWrap::ClampToEdge) ? GL_CLAMP_TO_EDGE : GL_REPEAT;
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, wrap);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, wrap);

    GLenum filter = (d.filter == TextureFilter::Linear) ? GL_LINEAR : GL_NEAREST;
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, filter);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, filter);

    GLenum format = (d.channels == 4) ? GL_RGBA : GL_RGB;

    glPixelStorei(GL_UNPACK_ALIGNMENT, 1);

    glTexImage2D(GL_TEXTURE_2D, 0, format, d.width, d.height, 0, format, GL_UNSIGNED_BYTE, d.pixels.data());

    glBindTexture(GL_TEXTURE_2D, 0);
    return t;
}

void OpenGLAdapter::ReleaseTexture(GPUTexture& t) {
    if (t.textureId) glDeleteTextures(1, &t.textureId);
    t = {};
}

GPUShader OpenGLAdapter::CompileShader(const std::string& vsSrc, const std::string& fsSrc) {
    auto compile = [](GLenum type, const std::string& src) -> GLuint {
        GLuint sh = glCreateShader(type);
        const char* srcPtr = src.c_str();
        glShaderSource(sh, 1, &srcPtr, nullptr);
        glCompileShader(sh);

        GLint ok = 0;
        glGetShaderiv(sh, GL_COMPILE_STATUS, &ok);
        if (!ok) {
            char log[1024];
            glGetShaderInfoLog(sh, sizeof(log), nullptr, log);
            Logger::Error(std::string("Shader compile error: ") + log);
            glDeleteShader(sh);
            return 0;
        }
        return sh;
    };

    GLuint vs = compile(GL_VERTEX_SHADER, vsSrc);
    GLuint fs = compile(GL_FRAGMENT_SHADER, fsSrc);
    if (vs == 0 || fs == 0) {
        if (vs) glDeleteShader(vs);
        if (fs) glDeleteShader(fs);
        return {};
    }

    GLuint program = glCreateProgram();
    glAttachShader(program, vs);
    glAttachShader(program, fs);
    glLinkProgram(program);
    
    GLint linked = 0;
    glGetProgramiv(program, GL_LINK_STATUS, &linked);
    if (!linked) {
        char log[1024];
        glGetProgramInfoLog(program, sizeof(log), nullptr, log);
        Logger::Error(std::string("Shader link error: ") + log);
        glDeleteShader(vs);
        glDeleteShader(fs);
        glDeleteProgram(program);
        return {};
    }

    // После линковки в program удалить шейдеры
    glDeleteShader(vs);
    glDeleteShader(fs);

    GPUShader gpu;
    gpu.programId = program;
    return gpu;
}

void OpenGLAdapter::ReleaseShader(GPUShader& s) {
    if (s.programId) glDeleteProgram(s.programId);
    s = {};
}

// mvp-matrix, color, size - uniform
void OpenGLAdapter::DrawLoadedMesh(const GPUMesh& mesh, const GPUTexture& tex, const GPUShader& shader, const glm::mat4& model, const glm::vec4& tint) {
    glUseProgram(shader.programId);

    glm::mat4 mvp = m_projectionMatrix * m_viewMatrix * model;

    // Считаем итоговую матрицу MVP и передаем в шейдер
    // проекция * вид * модель
    GLint locMVP = glGetUniformLocation(shader.programId, "u_MVP");
    GLint locTint = glGetUniformLocation(shader.programId, "u_Tint");
    GLint locTex = glGetUniformLocation(shader.programId, "u_Texture");

    if (locMVP >= 0) glUniformMatrix4fv(locMVP, 1, GL_FALSE, &mvp[0][0]);
    if (locTint >= 0) glUniform4fv(locTint, 1, &tint[0]);

    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, tex.textureId);
    if (locTex >= 0) glUniform1i(locTex, 0);

    glBindVertexArray(mesh.vao);

    glDrawElements(GL_TRIANGLES, mesh.indexCount, GL_UNSIGNED_INT, nullptr);

    // развязываем
    glBindVertexArray(0);
    glBindTexture(GL_TEXTURE_2D, 0);
    glUseProgram(0);
}