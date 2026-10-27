/*
* 08 - Modelos de Iluminación II
* Variación de la actividad 2: materiales opacos y plásticos en tres objetos,
* con la tecla P los plásticos se vuelven translúcidos.
*/

#include <iostream>
#include <stdlib.h>
#include <sstream>

#include <glad/glad.h>
#include <GLFW/glfw3.h>

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>

#include <shader_m.h>
#include <camera.h>
#include <model.h>
#include <material.h>
#include <light.h>
#include <cubemap.h>

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

Camera camera(glm::vec3(0.0f, 9.0f, 20.0f), glm::vec3(0.0f, 1.0f, 0.0f), -90.0f, -22.0f);

float deltaTime = 0.0f;
float lastFrame = 0.0f;

// Shaders
Shader* cubemapShader;
Shader* mLightsShader;

// Modelos
Model* house;
Model* sphere;
Model* table;

CubeMap* mainCubeMap;

// Luces
std::vector<Light> gLights;

// Materiales de los tres objetos.
// Tabla: http://devernay.free.fr/cours/opengl/materials.html
Material houseMaterial;    // opaco: casi sin especular
Material sphereMaterial;   // plástico rojo
Material tableMaterial;    // plástico verde
int houseShininess = 4;
int plasticShininess = 32;

// Variación: tecla P cambia la transparencia de los plásticos
bool translucent = false;
const float PLASTIC_ALPHA = 0.45f;

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

	window = glfwCreateWindow(SCR_WIDTH, SCR_HEIGHT, "P8 Var. 2 - Plasticos opacos", NULL, NULL);
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

	mLightsShader = new Shader("shaders/11_PhongShaderMultLights.vs", "shaders/11_PhongShaderMultLights.fs");
	cubemapShader = new Shader("shaders/10_vertex_cubemap.vs", "shaders/10_fragment_cubemap.fs");

	house = new Model("models/IllumModels/House02.fbx");
	sphere = new Model("models/IllumModels/Sphere.fbx");
	table = new Model("models/IllumModels/Table02.fbx");

	vector<std::string> faces
	{
		"textures/cubemap/01/posx.png",
		"textures/cubemap/01/negx.png",
		"textures/cubemap/01/posy.png",
		"textures/cubemap/01/negy.png",
		"textures/cubemap/01/posz.png",
		"textures/cubemap/01/negz.png"
	};
	mainCubeMap = new CubeMap();
	mainCubeMap->loadCubemap(faces);

	// Dos fuentes blancas al frente, una de cada lado
	Light light01;
	light01.Position = glm::vec3(6.0f, 6.0f, 10.0f);
	light01.Color = glm::vec4(0.2f, 0.2f, 0.2f, 1.0f);
	light01.Power = glm::vec4(40.0f, 40.0f, 40.0f, 1.0f);
	light01.distance = 3.0f;
	gLights.push_back(light01);

	Light light02 = light01;
	light02.Position = glm::vec3(-6.0f, 6.0f, 10.0f);
	gLights.push_back(light02);

	// Opaco: difuso alto y especular casi nulo
	houseMaterial.ambient = glm::vec4(0.25f, 0.25f, 0.25f, 1.0f);
	houseMaterial.diffuse = glm::vec4(0.8f, 0.8f, 0.8f, 1.0f);
	houseMaterial.specular = glm::vec4(0.02f, 0.02f, 0.02f, 1.0f);
	houseMaterial.transparency = 1.0f;

	// Plástico rojo (red plastic)
	sphereMaterial.ambient = glm::vec4(0.0f, 0.0f, 0.0f, 1.0f);
	sphereMaterial.diffuse = glm::vec4(0.5f, 0.0f, 0.0f, 1.0f);
	sphereMaterial.specular = glm::vec4(0.7f, 0.6f, 0.6f, 1.0f);
	sphereMaterial.transparency = 1.0f;

	// Plástico verde (green plastic)
	tableMaterial.ambient = glm::vec4(0.0f, 0.0f, 0.0f, 1.0f);
	tableMaterial.diffuse = glm::vec4(0.1f, 0.35f, 0.1f, 1.0f);
	tableMaterial.specular = glm::vec4(0.45f, 0.55f, 0.45f, 1.0f);
	tableMaterial.transparency = 1.0f;

	std::cout << "P: plasticos opacos/translucidos | WASD: camara | Esc: salir" << std::endl;
	return true;
}

void SetLightUniforms(Shader* shader, int shininess)
{
	shader->setInt("numLights", (int)gLights.size());
	for (size_t i = 0; i < gLights.size(); ++i) {
		std::ostringstream ss;
		ss << "allLights[" << i << "].";
		std::string prefix = ss.str();
		shader->setVec3(prefix + "Position", gLights[i].Position);
		shader->setVec3(prefix + "Direction", gLights[i].Direction);
		shader->setVec4(prefix + "Color", gLights[i].Color);
		shader->setVec4(prefix + "Power", gLights[i].Power);
		shader->setInt(prefix + "alphaIndex", shininess);
		shader->setFloat(prefix + "distance", gLights[i].distance);
	}
}

// Dibuja un objeto con su material; el exponente especular va en cada luz
void DrawWithMaterial(Model* object, const glm::mat4& model, const Material& material, int shininess)
{
	mLightsShader->setMat4("model", model);
	SetLightUniforms(mLightsShader, shininess);
	mLightsShader->setVec4("MaterialAmbientColor", material.ambient);
	mLightsShader->setVec4("MaterialDiffuseColor", material.diffuse);
	mLightsShader->setVec4("MaterialSpecularColor", material.specular);
	mLightsShader->setFloat("transparency", material.transparency);
	object->Draw(*mLightsShader);
}

bool Update() {
	float currentFrame = (float)glfwGetTime();
	deltaTime = currentFrame - lastFrame;
	lastFrame = currentFrame;

	processInput(window);

	glClearColor(1.0f, 1.0f, 1.0f, 0.0f);
	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

	int width, height;
	glfwGetFramebufferSize(window, &width, &height);
	if (width == 0 || height == 0) {
		glfwPollEvents();
		return true;
	}
	glm::mat4 projection = glm::perspective(glm::radians(camera.Zoom), (float)width / (float)height, 0.1f, 10000.0f);
	glm::mat4 view = camera.GetViewMatrix();

	mainCubeMap->drawCubeMap(*cubemapShader, projection, view);

	mLightsShader->use();
	glEnable(GL_BLEND);
	glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
	mLightsShader->setMat4("projection", projection);
	mLightsShader->setMat4("view", view);
	mLightsShader->setVec3("eye", camera.Position);

	// Los plásticos toman la transparencia elegida con la tecla P
	sphereMaterial.transparency = translucent ? PLASTIC_ALPHA : 1.0f;
	tableMaterial.transparency = translucent ? PLASTIC_ALPHA : 1.0f;

	// Primero el objeto opaco y después los translúcidos, para que se mezclen con lo que ya está dibujado
	glm::mat4 model = glm::rotate(glm::mat4(1.0f), glm::radians(-90.0f), glm::vec3(1.0f, 0.0f, 0.0f));
	DrawWithMaterial(house, model, houseMaterial, houseShininess);

	model = glm::translate(glm::mat4(1.0f), glm::vec3(-3.0f, 1.5f, 7.5f));
	model = glm::rotate(model, glm::radians(-90.0f), glm::vec3(1.0f, 0.0f, 0.0f));
	model = glm::scale(model, glm::vec3(1.5f));
	DrawWithMaterial(sphere, model, sphereMaterial, plasticShininess);

	model = glm::translate(glm::mat4(1.0f), glm::vec3(3.5f, 0.0f, 7.0f));
	model = glm::rotate(model, glm::radians(-90.0f), glm::vec3(1.0f, 0.0f, 0.0f));
	DrawWithMaterial(table, model, tableMaterial, plasticShininess);

	glUseProgram(0);

	glfwSwapBuffers(window);
	glfwPollEvents();
	return true;
}

void key_callback(GLFWwindow* window, int key, int scancode, int action, int mods)
{
	if (action == GLFW_PRESS && key == GLFW_KEY_P) {
		translucent = !translucent;
		glfwSetWindowTitle(window, translucent ? "P8 Var. 2 - Plasticos translucidos" : "P8 Var. 2 - Plasticos opacos");
	}
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
