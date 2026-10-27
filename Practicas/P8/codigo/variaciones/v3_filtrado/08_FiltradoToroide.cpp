/*
* 08 - Modelos de Iluminación II
* Variación de la actividad 3: filtrado de la textura del toroide.
* Usa el toroide, la textura de tablero y los shaders torus.vs/torus.fs de la actividad 3.
* F: cambia entre GL_NEAREST y mipmaps con filtrado trilineal | T: UV 1x1 / 4x4 / 16x16
*/

#include <iostream>
#include <stdlib.h>
#include <string>

#include <glad/glad.h>
#include <GLFW/glfw3.h>

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>

#include <shader_m.h>
#include <camera.h>
#include <model.h>

// Functions
bool Start();
bool Update();

// Definición de callbacks
void framebuffer_size_callback(GLFWwindow* window, int width, int height);
void key_callback(GLFWwindow* window, int key, int scancode, int action, int mods);
void processInput(GLFWwindow* window);

// Gobals
GLFWwindow* window;

const unsigned int SCR_WIDTH = 1280;
const unsigned int SCR_HEIGHT = 720;

// Cámara baja y cerca del toroide: así la textura se ve de lado y el filtrado se nota más
Camera camera(glm::vec3(0.0f, 0.6f, 3.6f), glm::vec3(0.0f, 1.0f, 0.0f), -90.0f, -8.0f);

float deltaTime = 0.0f;
float lastFrame = 0.0f;

Shader* torusShader;
Model* torus;
unsigned int checkerTexture;

bool useMipmaps = true;   // F
float uvRepeat = 16.0f;   // T

// Carga la textura en sRGB, como en la actividad 3, para que el shader corrija la gama
unsigned int loadTexture(const char* path)
{
	int width, height, channels;
	unsigned char* data = stbi_load(path, &width, &height, &channels, 0);
	if (!data) {
		std::cout << "No se pudo cargar " << path << std::endl;
		return 0;
	}
	unsigned int id;
	glGenTextures(1, &id);
	glBindTexture(GL_TEXTURE_2D, id);
	glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
	glTexImage2D(GL_TEXTURE_2D, 0, channels == 4 ? GL_SRGB8_ALPHA8 : GL_SRGB8, width, height, 0,
		channels == 4 ? GL_RGBA : GL_RGB, GL_UNSIGNED_BYTE, data);
	glGenerateMipmap(GL_TEXTURE_2D);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
	stbi_image_free(data);
	return id;
}

// Aplica el filtro elegido a la textura del tablero
void applyFilter()
{
	glBindTexture(GL_TEXTURE_2D, checkerTexture);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, useMipmaps ? GL_LINEAR_MIPMAP_LINEAR : GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, useMipmaps ? GL_LINEAR : GL_NEAREST);
	std::string title = std::string("P8 Var. 3 - Filtro: ") + (useMipmaps ? "mipmaps trilineal" : "GL_NEAREST")
		+ " | UV " + std::to_string((int)uvRepeat) + "x" + std::to_string((int)uvRepeat);
	glfwSetWindowTitle(window, title.c_str());
}

int main()
{
	if (!Start())
		return -1;

	while (!glfwWindowShouldClose(window))
	{
		if (!Update())
			break;
	}

	glfwTerminate();
	return 0;
}

bool Start() {
	glfwInit();
	glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
	glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
	glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);

	window = glfwCreateWindow(SCR_WIDTH, SCR_HEIGHT, "P8 Var. 3", NULL, NULL);
	if (window == NULL)
	{
		std::cout << "Failed to create GLFW window" << std::endl;
		glfwTerminate();
		return false;
	}
	glfwMakeContextCurrent(window);
	glfwSetFramebufferSizeCallback(window, framebuffer_size_callback);
	glfwSetKeyCallback(window, key_callback);

	if (!gladLoadGLLoader((GLADloadproc)glfwGetProcAddress))
	{
		std::cout << "Failed to initialize GLAD" << std::endl;
		return false;
	}

	glEnable(GL_DEPTH_TEST);

	// Mismos shaders de la actividad 3
	torusShader = new Shader("shaders/torus.vs", "shaders/torus.fs");
	torus = new Model("models/P8/toroide.obj");
	checkerTexture = loadTexture("models/P8/tablero_uv.png");
	applyFilter();

	std::cout << "F: filtro nearest/mipmaps | T: UV 1x1/4x4/16x16 | WASD: camara | Esc: salir" << std::endl;
	return true;
}

bool Update() {
	float currentFrame = (float)glfwGetTime();
	deltaTime = currentFrame - lastFrame;
	lastFrame = currentFrame;

	processInput(window);

	glClearColor(0.035f, 0.045f, 0.065f, 1.0f);
	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

	int width, height;
	glfwGetFramebufferSize(window, &width, &height);
	if (width == 0 || height == 0) {
		glfwPollEvents();
		return true;
	}
	glm::mat4 projection = glm::perspective(glm::radians(camera.Zoom), (float)width / (float)height, 0.1f, 100.0f);
	glm::mat4 view = camera.GetViewMatrix();
	glm::mat4 model = glm::rotate(glm::mat4(1.0f), glm::radians(10.0f * currentFrame), glm::vec3(0.0f, 1.0f, 0.0f));
	glm::mat3 normalMatrix = glm::transpose(glm::inverse(glm::mat3(model)));

	torusShader->use();
	torusShader->setMat4("projection", projection);
	torusShader->setMat4("view", view);
	torusShader->setMat4("model", model);
	torusShader->setMat3("normalMatrix", normalMatrix);
	torusShader->setVec3("material.ambient", glm::vec3(0.18f));
	torusShader->setVec3("material.diffuse", glm::vec3(0.85f));
	torusShader->setVec3("material.specular", glm::vec3(0.45f));
	torusShader->setFloat("material.shininess", 64.0f);
	torusShader->setVec3("lightPosition", glm::vec3(2.5f, 4.0f, 3.0f));
	torusShader->setVec3("lightColor", glm::vec3(1.0f));
	torusShader->setVec3("eye", camera.Position);
	torusShader->setFloat("uvRepeat", uvRepeat);
	torusShader->setBool("useLighting", true);

	// El modelo OBJ no trae textura; enlazamos el tablero en la unidad 0
	torusShader->setInt("texture_diffuse1", 0);
	glActiveTexture(GL_TEXTURE0);
	glBindTexture(GL_TEXTURE_2D, checkerTexture);
	torus->Draw(*torusShader);

	glfwSwapBuffers(window);
	glfwPollEvents();
	return true;
}

void key_callback(GLFWwindow* window, int key, int scancode, int action, int mods)
{
	if (action != GLFW_PRESS)
		return;
	if (key == GLFW_KEY_F)
		useMipmaps = !useMipmaps;
	if (key == GLFW_KEY_T)
		uvRepeat = (uvRepeat == 16.0f) ? 1.0f : uvRepeat * 4.0f;
	applyFilter();
}

void processInput(GLFWwindow* window)
{
	if (glfwGetKey(window, GLFW_KEY_ESCAPE) == GLFW_PRESS)
		glfwSetWindowShouldClose(window, true);
	if (glfwGetKey(window, GLFW_KEY_W) == GLFW_PRESS)
		camera.ProcessKeyboard(FORWARD, deltaTime);
	if (glfwGetKey(window, GLFW_KEY_S) == GLFW_PRESS)
		camera.ProcessKeyboard(BACKWARD, deltaTime);
	if (glfwGetKey(window, GLFW_KEY_A) == GLFW_PRESS)
		camera.ProcessKeyboard(LEFT, deltaTime);
	if (glfwGetKey(window, GLFW_KEY_D) == GLFW_PRESS)
		camera.ProcessKeyboard(RIGHT, deltaTime);
}

void framebuffer_size_callback(GLFWwindow* window, int width, int height)
{
	glViewport(0, 0, width, height);
}
