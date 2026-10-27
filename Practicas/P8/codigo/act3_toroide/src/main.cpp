#include <glad/glad.h>
#include <GLFW/glfw3.h>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <assimp/Importer.hpp>
#include <assimp/material.h>
#include <assimp/postprocess.h>
#include <assimp/scene.h>
#include <camera.h>
#include <stb_image.h>

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <limits>
#include <memory>
#include <stdexcept>
#include <string>
#include <vector>

namespace fs = std::filesystem;

struct Options {
    fs::path resourceRoot;
    fs::path captureDirectory;
    fs::path textureOverride;
    bool checkAssets = false;
};

Options parseOptions(int argc, char** argv)
{
    Options options;
    // Linux permite localizar el ejecutable sin depender del directorio actual.
    std::error_code error;
    fs::path executable = fs::read_symlink("/proc/self/exe", error);
    options.resourceRoot = error ? fs::absolute(argv[0]).parent_path() : executable.parent_path();
    for (int i = 1; i < argc; ++i) {
        std::string argument = argv[i];
        if (argument == "--check-assets") {
            options.checkAssets = true;
        } else if (argument == "--resources" || argument == "--capture" || argument == "--texture") {
            if (++i >= argc) throw std::runtime_error("Falta valor para " + argument);
            fs::path value = fs::absolute(argv[i]);
            if (argument == "--resources") options.resourceRoot = value;
            if (argument == "--capture") options.captureDirectory = value;
            if (argument == "--texture") options.textureOverride = value;
        } else {
            throw std::runtime_error("Argumento desconocido: " + argument);
        }
    }
    return options;
}

struct Vertex {
    glm::vec3 position;
    glm::vec3 normal;
    glm::vec2 uv;
};

struct ModelData {
    std::vector<Vertex> vertices;
    std::vector<unsigned int> indices;
    fs::path texture;
};

ModelData loadModel(const fs::path& path)
{
    Assimp::Importer importer;
    const aiScene* scene = importer.ReadFile(path.string(), aiProcess_Triangulate
        | aiProcess_JoinIdenticalVertices | aiProcess_PreTransformVertices
        | aiProcess_FlipUVs | aiProcess_ValidateDataStructure);
    if (!scene || (scene->mFlags & AI_SCENE_FLAGS_INCOMPLETE) || !scene->mRootNode) {
        throw std::runtime_error("Assimp: " + std::string(importer.GetErrorString()));
    }
    if (scene->mNumMeshes != 1) throw std::runtime_error("Se esperaba una sola malla estática.");
    const aiMesh& mesh = *scene->mMeshes[0];
    if (!mesh.HasNormals() || !mesh.HasTextureCoords(0)) {
        throw std::runtime_error("El modelo no contiene normales o UV.");
    }
    ModelData result;
    glm::vec3 lower(std::numeric_limits<float>::max());
    glm::vec3 upper(std::numeric_limits<float>::lowest());
    for (unsigned int i = 0; i < mesh.mNumVertices; ++i) {
        Vertex vertex{
            {mesh.mVertices[i].x, mesh.mVertices[i].y, mesh.mVertices[i].z},
            {mesh.mNormals[i].x, mesh.mNormals[i].y, mesh.mNormals[i].z},
            {mesh.mTextureCoords[0][i].x, mesh.mTextureCoords[0][i].y}};
        // Assimp 6 aplica UnitScaleFactor en la raíz FBX: centímetros -> metros.
        vertex.position *= 0.01f;
        for (int axis = 0; axis < 3; ++axis) {
            if (!std::isfinite(vertex.position[axis]) || !std::isfinite(vertex.normal[axis]))
                throw std::runtime_error("Posición o normal no finita.");
        }
        if (!std::isfinite(vertex.uv.x) || !std::isfinite(vertex.uv.y)
            || glm::length(vertex.normal) < 0.9f) throw std::runtime_error("Normal o UV inválida.");
        lower = glm::min(lower, vertex.position);
        upper = glm::max(upper, vertex.position);
        result.vertices.push_back(vertex);
    }
    for (unsigned int i = 0; i < mesh.mNumFaces; ++i) {
        const aiFace& face = mesh.mFaces[i];
        if (face.mNumIndices != 3) throw std::runtime_error("El modelo no está triangulado.");
        for (unsigned int j = 0; j < 3; ++j) {
            if (face.mIndices[j] >= result.vertices.size()) throw std::runtime_error("Índice inválido.");
            result.indices.push_back(face.mIndices[j]);
        }
    }
    if (result.vertices.empty() || result.indices.empty()) throw std::runtime_error("Malla vacía.");
    aiString reference;
    const aiMaterial& material = *scene->mMaterials[mesh.mMaterialIndex];
    if (material.GetTexture(aiTextureType_DIFFUSE, 0, &reference) != AI_SUCCESS
        && material.GetTexture(aiTextureType_BASE_COLOR, 0, &reference) != AI_SUCCESS)
        throw std::runtime_error("El FBX no referencia una textura de color.");
    std::string relative = reference.C_Str();
    std::replace(relative.begin(), relative.end(), '\\', '/');
    fs::path textureReference(relative);
    if (!textureReference.is_absolute()) result.texture = path.parent_path() / textureReference;
    // Algunos FBX también guardan una ruta absoluta del autor: preferimos el recurso local.
    if (result.texture.empty() || !fs::is_regular_file(result.texture))
        result.texture = path.parent_path().parent_path() / "textures" / textureReference.filename();
    if (!fs::is_regular_file(result.texture)) throw std::runtime_error("No se encontró la textura del FBX.");
    glm::vec3 size = upper - lower;
    std::cout << "Modelo: " << result.vertices.size() << " vértices, "
              << result.indices.size() / 3 << " triángulos; dimensiones "
              << size.x << ", " << size.y << ", " << size.z << '\n';
    return result;
}

struct ImageData {
    int width = 0, height = 0, channels = 0;
    std::unique_ptr<stbi_uc, decltype(&stbi_image_free)> pixels{nullptr, stbi_image_free};
};

ImageData loadImage(const fs::path& path)
{
    ImageData image;
    image.pixels.reset(stbi_load(path.string().c_str(), &image.width, &image.height, &image.channels, 0));
    if (!image.pixels) throw std::runtime_error("Textura: " + path.string() + ": " + stbi_failure_reason());
    if (image.channels != 3 && image.channels != 4)
        throw std::runtime_error("La textura debe ser RGB o RGBA.");
    std::cout << "Textura: " << path.filename().string() << ", " << image.width
              << 'x' << image.height << ", canales=" << image.channels << '\n';
    return image;
}

std::string readText(const fs::path& path)
{
    std::ifstream stream(path);
    if (!stream) throw std::runtime_error("No se pudo leer " + path.string());
    return {std::istreambuf_iterator<char>(stream), std::istreambuf_iterator<char>()};
}

class Program {
public:
    GLuint id = 0;
    Program(const fs::path& vertexPath, const fs::path& fragmentPath)
    {
        GLuint vertex = compile(GL_VERTEX_SHADER, readText(vertexPath));
        GLuint fragment = 0;
        try {
            fragment = compile(GL_FRAGMENT_SHADER, readText(fragmentPath));
            id = glCreateProgram();
            glAttachShader(id, vertex);
            glAttachShader(id, fragment);
            glLinkProgram(id);
            GLint success = 0;
            glGetProgramiv(id, GL_LINK_STATUS, &success);
            if (!success) {
                char log[4096];
                glGetProgramInfoLog(id, sizeof(log), nullptr, log);
                throw std::runtime_error("Enlace GLSL: " + std::string(log));
            }
        } catch (...) {
            glDeleteShader(vertex);
            if (fragment) glDeleteShader(fragment);
            if (id) glDeleteProgram(id);
            throw;
        }
        glDeleteShader(vertex);
        glDeleteShader(fragment);
    }
    ~Program() { glDeleteProgram(id); }
    Program(const Program&) = delete;
    Program& operator=(const Program&) = delete;
    GLint uniform(const char* name) const
    {
        GLint location = glGetUniformLocation(id, name);
        if (location < 0) throw std::runtime_error("Uniform no encontrado: " + std::string(name));
        return location;
    }
private:
    static GLuint compile(GLenum type, const std::string& source)
    {
        GLuint shader = glCreateShader(type);
        const char* code = source.c_str();
        glShaderSource(shader, 1, &code, nullptr);
        glCompileShader(shader);
        GLint success = 0;
        glGetShaderiv(shader, GL_COMPILE_STATUS, &success);
        if (!success) {
            char log[4096];
            glGetShaderInfoLog(shader, sizeof(log), nullptr, log);
            glDeleteShader(shader);
            throw std::runtime_error("Compilación GLSL: " + std::string(log));
        }
        return shader;
    }
};

class GpuModel {
    GLuint vao = 0, vbo = 0, ebo = 0, texture = 0;
    GLsizei indexCount;
public:
    GpuModel(const ModelData& model, const ImageData& image)
        : indexCount(static_cast<GLsizei>(model.indices.size()))
    {
        glGenVertexArrays(1, &vao);
        glGenBuffers(1, &vbo);
        glGenBuffers(1, &ebo);
        glBindVertexArray(vao);
        glBindBuffer(GL_ARRAY_BUFFER, vbo);
        glBufferData(GL_ARRAY_BUFFER, model.vertices.size() * sizeof(Vertex), model.vertices.data(), GL_STATIC_DRAW);
        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, ebo);
        glBufferData(GL_ELEMENT_ARRAY_BUFFER, model.indices.size() * sizeof(unsigned int), model.indices.data(), GL_STATIC_DRAW);
        glEnableVertexAttribArray(0);
        glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex), reinterpret_cast<void*>(offsetof(Vertex, position)));
        glEnableVertexAttribArray(1);
        glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex), reinterpret_cast<void*>(offsetof(Vertex, normal)));
        glEnableVertexAttribArray(2);
        glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, sizeof(Vertex), reinterpret_cast<void*>(offsetof(Vertex, uv)));
        glBindVertexArray(0);

        glGenTextures(1, &texture);
        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, texture);
        GLint alignment;
        glGetIntegerv(GL_UNPACK_ALIGNMENT, &alignment);
        glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
        glTexImage2D(GL_TEXTURE_2D, 0, image.channels == 4 ? GL_SRGB8_ALPHA8 : GL_SRGB8,
            image.width, image.height, 0, image.channels == 4 ? GL_RGBA : GL_RGB, GL_UNSIGNED_BYTE, image.pixels.get());
        glPixelStorei(GL_UNPACK_ALIGNMENT, alignment);
        glGenerateMipmap(GL_TEXTURE_2D);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    }
    ~GpuModel()
    {
        glDeleteTextures(1, &texture);
        glDeleteBuffers(1, &ebo);
        glDeleteBuffers(1, &vbo);
        glDeleteVertexArrays(1, &vao);
    }
    GpuModel(const GpuModel&) = delete;
    GpuModel& operator=(const GpuModel&) = delete;
    void draw() const
    {
        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, texture);
        glBindVertexArray(vao);
        glDrawElements(GL_TRIANGLES, indexCount, GL_UNSIGNED_INT, nullptr);
        glBindVertexArray(0);
    }
};

struct Material {
    glm::vec3 ambient{0.18f};
    glm::vec3 diffuse{0.85f};
    glm::vec3 specular{0.45f};
    float shininess = 64.0f;
};

Camera initialCamera()
{
    Camera camera(glm::vec3(0.0f, 2.8f, 4.4f), glm::vec3(0, 1, 0), -90.0f, -32.47f);
    camera.MouseSensitivity = 0.12f;
    return camera;
}

struct Controls {
    Camera camera = initialCamera();
    bool lighting = true;
    float repeat = 1.0f;
    bool firstMouse = true;
    double lastX = 0, lastY = 0;
};

void updateTitle(GLFWwindow* window)
{
    const Controls& state = *static_cast<Controls*>(glfwGetWindowUserPointer(window));
    std::string title = std::string("P8 Toroide | ") + (state.lighting ? "Phong" : "Sin luz")
        + (state.repeat == 1.0f ? " | UV 1x1" : " | UV 4x4")
        + " | 1/2: luz | T: UV | M/N: malla | R: reiniciar";
    glfwSetWindowTitle(window, title.c_str());
}

void keyCallback(GLFWwindow* window, int key, int, int action, int)
{
    if (action != GLFW_PRESS) return;
    auto& state = *static_cast<Controls*>(glfwGetWindowUserPointer(window));
    if (key == GLFW_KEY_ESCAPE) glfwSetWindowShouldClose(window, GLFW_TRUE);
    if (key == GLFW_KEY_1) state.lighting = false;
    if (key == GLFW_KEY_2) state.lighting = true;
    if (key == GLFW_KEY_T) state.repeat = state.repeat == 1.0f ? 4.0f : 1.0f;
    if (key == GLFW_KEY_M) glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);
    if (key == GLFW_KEY_N) glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
    if (key == GLFW_KEY_R) { state.camera = initialCamera(); state.firstMouse = true; }
    updateTitle(window);
}

void cursorCallback(GLFWwindow* window, double x, double y)
{
    auto& state = *static_cast<Controls*>(glfwGetWindowUserPointer(window));
    if (glfwGetMouseButton(window, GLFW_MOUSE_BUTTON_RIGHT) != GLFW_PRESS) {
        state.firstMouse = true;
        return;
    }
    if (!state.firstMouse)
        state.camera.ProcessMouseMovement(static_cast<float>(x-state.lastX), static_cast<float>(state.lastY-y));
    state.lastX = x;
    state.lastY = y;
    state.firstMouse = false;
}

class Window {
public:
    GLFWwindow* handle = nullptr;
    explicit Window(bool visible)
    {
        glfwSetErrorCallback([](int code, const char* text) { std::cerr << "GLFW " << code << ": " << text << '\n'; });
        if (!glfwInit()) throw std::runtime_error("No se pudo iniciar GLFW; se requiere una pantalla.");
        glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
        glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
        glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
        glfwWindowHint(GLFW_VISIBLE, visible ? GLFW_TRUE : GLFW_FALSE);
        handle = glfwCreateWindow(1024, 768, "P8 Toroide", nullptr, nullptr);
        if (!handle) { glfwTerminate(); throw std::runtime_error("No se pudo crear la ventana OpenGL 3.3."); }
        glfwMakeContextCurrent(handle);
        if (!gladLoadGLLoader(reinterpret_cast<GLADloadproc>(glfwGetProcAddress))) {
            glfwDestroyWindow(handle);
            glfwTerminate();
            throw std::runtime_error("No se pudo iniciar GLAD.");
        }
        glfwSwapInterval(1);
        std::cout << "OpenGL: " << glGetString(GL_VERSION) << " | " << glGetString(GL_RENDERER) << '\n';
    }
    ~Window() { glfwDestroyWindow(handle); glfwTerminate(); }
    Window(const Window&) = delete;
    Window& operator=(const Window&) = delete;
};

void checkGl()
{
    GLenum error = glGetError();
    if (error != GL_NO_ERROR) throw std::runtime_error("Error OpenGL: " + std::to_string(error));
}

class CaptureTarget {
    GLuint framebuffer = 0, color = 0, depth = 0;
public:
    static constexpr int width = 1024, height = 768;
    CaptureTarget()
    {
        glGenFramebuffers(1, &framebuffer);
        glBindFramebuffer(GL_FRAMEBUFFER, framebuffer);
        glGenTextures(1, &color);
        glBindTexture(GL_TEXTURE_2D, color);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, width, height, 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, color, 0);
        glGenRenderbuffers(1, &depth);
        glBindRenderbuffer(GL_RENDERBUFFER, depth);
        glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH_COMPONENT24, width, height);
        glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_RENDERBUFFER, depth);
        if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE) {
            release();
            throw std::runtime_error("Framebuffer de capturas incompleto.");
        }
    }
    ~CaptureTarget() { release(); }
    CaptureTarget(const CaptureTarget&) = delete;
    CaptureTarget& operator=(const CaptureTarget&) = delete;
private:
    void release()
    {
        glBindFramebuffer(GL_FRAMEBUFFER, 0);
        glDeleteRenderbuffers(1, &depth);
        glDeleteTextures(1, &color);
        glDeleteFramebuffers(1, &framebuffer);
    }
};

void saveFrame(const fs::path& path, int width, int height)
{
    if (fs::exists(path)) throw std::runtime_error("No se sobrescribirá " + path.string());
    std::vector<unsigned char> pixels(static_cast<std::size_t>(width) * height * 3);
    GLint alignment;
    glGetIntegerv(GL_PACK_ALIGNMENT, &alignment);
    glPixelStorei(GL_PACK_ALIGNMENT, 1);
    glReadBuffer(GL_COLOR_ATTACHMENT0);
    glReadPixels(0, 0, width, height, GL_RGB, GL_UNSIGNED_BYTE, pixels.data());
    glPixelStorei(GL_PACK_ALIGNMENT, alignment);
    checkGl();
    std::ofstream file(path, std::ios::binary);
    if (!file) throw std::runtime_error("No se pudo guardar " + path.string());
    file << "P6\n" << width << ' ' << height << "\n255\n";
    for (int y = height-1; y >= 0; --y)
        file.write(reinterpret_cast<const char*>(pixels.data() + static_cast<std::size_t>(y) * width * 3), width * 3);
    if (!file) throw std::runtime_error("Captura incompleta.");
    std::cout << "Captura: " << path.filename().string() << '\n';
}

void render(GLFWwindow* window, const Controls& state, const Program& program, const GpuModel& mesh,
            int width = 0, int height = 0)
{
    if (width == 0 || height == 0) glfwGetFramebufferSize(window, &width, &height);
    if (width <= 0 || height <= 0) return;
    glViewport(0, 0, width, height);
    glClearColor(0.035f, 0.045f, 0.065f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    glUseProgram(program.id);
    glm::mat4 model(1.0f);
    glm::mat4 projection = glm::perspective(glm::radians(state.camera.Zoom), float(width)/height, 0.1f, 100.0f);
    Camera camera = state.camera;
    glm::mat4 view = camera.GetViewMatrix();
    glm::mat3 normalMatrix = glm::transpose(glm::inverse(glm::mat3(model)));
    glUniformMatrix4fv(program.uniform("model"), 1, GL_FALSE, glm::value_ptr(model));
    glUniformMatrix4fv(program.uniform("view"), 1, GL_FALSE, glm::value_ptr(view));
    glUniformMatrix4fv(program.uniform("projection"), 1, GL_FALSE, glm::value_ptr(projection));
    glUniformMatrix3fv(program.uniform("normalMatrix"), 1, GL_FALSE, glm::value_ptr(normalMatrix));
    const Material material;
    glUniform3fv(program.uniform("material.ambient"), 1, glm::value_ptr(material.ambient));
    glUniform3fv(program.uniform("material.diffuse"), 1, glm::value_ptr(material.diffuse));
    glUniform3fv(program.uniform("material.specular"), 1, glm::value_ptr(material.specular));
    glUniform1f(program.uniform("material.shininess"), material.shininess);
    glUniform3f(program.uniform("lightPosition"), 2.5f, 4.0f, 3.0f);
    glUniform3f(program.uniform("lightColor"), 1.0f, 1.0f, 1.0f);
    glUniform3fv(program.uniform("eye"), 1, glm::value_ptr(camera.Position));
    glUniform1f(program.uniform("uvRepeat"), state.repeat);
    glUniform1i(program.uniform("useLighting"), state.lighting);
    glUniform1i(program.uniform("texture_diffuse1"), 0);
    mesh.draw();
    checkGl();
}

void run(const Options& options, const ModelData& model, const ImageData& image)
{
    Window window(options.captureDirectory.empty());
    Program program(options.resourceRoot / "shaders/torus.vs", options.resourceRoot / "shaders/torus.fs");
    GpuModel mesh(model, image);
    checkGl();
    glEnable(GL_DEPTH_TEST);
    glEnable(GL_CULL_FACE);
    Controls state;
    glfwSetWindowUserPointer(window.handle, &state);
    glfwSetKeyCallback(window.handle, keyCallback);
    glfwSetCursorPosCallback(window.handle, cursorCallback);
    glfwSetScrollCallback(window.handle, [](GLFWwindow* w, double, double y) {
        static_cast<Controls*>(glfwGetWindowUserPointer(w))->camera.ProcessMouseScroll(static_cast<float>(y));
    });
    updateTitle(window.handle);
    std::cout << "1: sin iluminación | 2: Phong | T: UV 1x1/4x4 | M/N: malla/sólido\n"
                 "WASD: mover | botón derecho + ratón: mirar | rueda: zoom | R: reiniciar | Esc: salir\n";
    if (!options.captureDirectory.empty()) {
        fs::create_directories(options.captureDirectory);
        glfwSwapInterval(0);
        CaptureTarget target;
        for (bool lighting : {false, true}) {
            for (float repeat : {1.0f, 4.0f}) {
                state.lighting = lighting;
                state.repeat = repeat;
                updateTitle(window.handle);
                render(window.handle, state, program, mesh, target.width, target.height);
                std::string name = std::string(lighting ? "phong" : "sin_luz")
                    + (repeat == 1.0f ? "_1x1.ppm" : "_4x4.ppm");
                saveFrame(options.captureDirectory / name, target.width, target.height);
            }
        }
        return;
    }
    double lastTime = glfwGetTime();
    while (!glfwWindowShouldClose(window.handle)) {
        glfwPollEvents();
        double now = glfwGetTime();
        float dt = static_cast<float>(std::min(now-lastTime, 0.1));
        lastTime = now;
        if (glfwGetKey(window.handle, GLFW_KEY_W) == GLFW_PRESS) state.camera.ProcessKeyboard(FORWARD, dt);
        if (glfwGetKey(window.handle, GLFW_KEY_S) == GLFW_PRESS) state.camera.ProcessKeyboard(BACKWARD, dt);
        if (glfwGetKey(window.handle, GLFW_KEY_A) == GLFW_PRESS) state.camera.ProcessKeyboard(LEFT, dt);
        if (glfwGetKey(window.handle, GLFW_KEY_D) == GLFW_PRESS) state.camera.ProcessKeyboard(RIGHT, dt);
        int width, height;
        glfwGetFramebufferSize(window.handle, &width, &height);
        if (width <= 0 || height <= 0) { glfwWaitEventsTimeout(0.1); continue; }
        render(window.handle, state, program, mesh);
        glfwSwapBuffers(window.handle);
    }
}

int main(int argc, char** argv)
{
    try {
        Options options = parseOptions(argc, argv);
        ModelData model = loadModel(options.resourceRoot / "assets/models/toroide.fbx");
        ImageData image = loadImage(options.textureOverride.empty() ? model.texture : options.textureOverride);
        if (options.checkAssets) {
            readText(options.resourceRoot / "shaders/torus.vs");
            readText(options.resourceRoot / "shaders/torus.fs");
            std::cout << "Recursos verificados; no se abrió ventana.\n";
        } else {
            run(options, model, image);
        }
        return 0;
    } catch (const std::exception& error) {
        std::cerr << "ERROR: " << error.what() << '\n';
        return 1;
    }
}
