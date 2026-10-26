/*
* 
* 08 - Modelos de Iluminación
*/

#include <iostream>
#include <stdlib.h>

// GLAD: Multi-Language GL/GLES/EGL/GLX/WGL Loader-Generator
// https://glad.dav1d.de/
#include <glad/glad.h>

// GLFW: https://www.glfw.org/
#include <GLFW/glfw3.h>

// GLM: OpenGL Math library
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>

// Model loading classes
#include <shader_m.h>
#include <camera.h>
#include <model.h>
#include <material.h>
#include <light.h>
#include <cubemap.h>

#include <irrKlang.h>
using namespace irrklang;

// Functions
bool Start();
bool Update();

// Definición de callbacks
void framebuffer_size_callback(GLFWwindow* window, int width, int height);
void mouse_callback(GLFWwindow* window, double xpos, double ypos);
void scroll_callback(GLFWwindow* window, double xoffset, double yoffset);
void processInput(GLFWwindow *window);

// Gobals
GLFWwindow* window;

// Tamaño en pixeles de la ventana
const unsigned int SCR_WIDTH = 1024;
const unsigned int SCR_HEIGHT = 768;

// Definición de cámara (posición en XYZ)
// (elevada y mirando hacia abajo para ver el anillo de 10 luces)
Camera camera(glm::vec3(0.0f, 14.0f, 22.0f), glm::vec3(0.0f, 1.0f, 0.0f), -90.0f, -32.0f);

// Controladores para el movimiento del mouse
float lastX = SCR_WIDTH / 2.0f;
float lastY = SCR_HEIGHT / 2.0f;
bool firstMouse = true;

// Variables para la velocidad de reproducción
// de la animación
float deltaTime = 0.0f;
float lastFrame = 0.0f;
float elapsedTime = 0.0f;

// Shaders
Shader *cubemapShader;
Shader *mLightsShader;
Shader *basicShader;

// Carga la información del modelo
Model	*house;
Model   *lightDummy;
Model   *sphere;

// Cubemap
CubeMap *mainCubeMap;

// Materiales
Material material;

// Luces
const int   NUM_LIGHTS = 10;
const float LIGHT_POWER = 30.0f;
std::vector<Light> gLights;
bool gLightEnabled[NUM_LIGHTS];      // Teclas 1-9 y 0 encienden/apagan cada luz
bool gLightKeyPressed[NUM_LIGHTS];   // Estado anterior de cada tecla (detección de flanco)

// Audio
ISoundEngine *SoundEngine = createIrrKlangDevice();

// Entrada a función principal
int main()
{
	if (!Start())
		return -1;

	/* Loop until the user closes the window */
	while (!glfwWindowShouldClose(window))
	{
		if (!Update())
			break;
	}

	glfwTerminate();
	return 0;

}

bool Start() {
	// Inicialización de GLFW

	glfwInit();
	glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
	glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
	glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);

	// Creación de la ventana con GLFW
	window = glfwCreateWindow(SCR_WIDTH, SCR_HEIGHT, "Illumination Models", NULL, NULL);
	if (window == NULL)
	{
		std::cout << "Failed to create GLFW window" << std::endl;
		glfwTerminate();
		return false;
	}
	glfwMakeContextCurrent(window);
	glfwSetFramebufferSizeCallback(window, framebuffer_size_callback);
	glfwSetCursorPosCallback(window, mouse_callback);
	glfwSetScrollCallback(window, scroll_callback);

	// Ocultar el cursor mientras se rota la escena
	// glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);

	// glad: Cargar todos los apuntadores
	if (!gladLoadGLLoader((GLADloadproc)glfwGetProcAddress))
	{
		std::cout << "Failed to initialize GLAD" << std::endl;
		return false;
	}

	// Activación de buffer de profundidad
	glEnable(GL_DEPTH_TEST);

	// Compilación y enlace de shaders
	mLightsShader = new Shader("shaders/11_PhongShaderMultLights.vs", "shaders/11_PhongShaderMultLights.fs");
	basicShader = new Shader("shaders/10_vertex_simple.vs", "shaders/10_fragment_simple.fs");
	cubemapShader = new Shader("shaders/10_vertex_cubemap.vs", "shaders/10_fragment_cubemap.fs");

	house = new Model("models/IllumModels/House02.fbx");
	sphere = new Model("models/IllumModels/Sphere.fbx");

	// Cubemap
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

	// Configuración de luces: 10 fuentes puntuales alrededor de la casa
	// (MAX_LIGHTS = 10 en 11_PhongShaderMultLights.fs)
	// 2 al frente, 3 a la derecha, 2 atrás y 3 a la izquierda, a 2 unidades de altura
	glm::vec3 lightPositions[NUM_LIGHTS] = {
		glm::vec3(-2.0f, 2.0f,  8.3f), // 1  frente, izquierda
		glm::vec3( 2.0f, 2.0f,  8.3f), // 2  frente, derecha
		glm::vec3( 5.7f, 2.0f,  5.0f), // 3  costado derecho
		glm::vec3( 5.7f, 2.0f,  0.0f), // 4  costado derecho
		glm::vec3( 5.7f, 2.0f, -5.0f), // 5  costado derecho
		glm::vec3( 2.0f, 2.0f, -8.3f), // 6  atrás, derecha
		glm::vec3(-2.0f, 2.0f, -8.3f), // 7  atrás, izquierda
		glm::vec3(-5.7f, 2.0f, -5.0f), // 8  costado izquierdo
		glm::vec3(-5.7f, 2.0f,  0.0f), // 9  costado izquierdo
		glm::vec3(-5.7f, 2.0f,  5.0f)  // 10 costado izquierdo
	};
	glm::vec4 lightColors[NUM_LIGHTS] = {
		glm::vec4(0.20f, 0.00f, 0.00f, 1.0f), // 1  rojo
		glm::vec4(0.20f, 0.10f, 0.00f, 1.0f), // 2  naranja
		glm::vec4(0.20f, 0.20f, 0.00f, 1.0f), // 3  amarillo
		glm::vec4(0.08f, 0.20f, 0.00f, 1.0f), // 4  verde limón
		glm::vec4(0.00f, 0.20f, 0.00f, 1.0f), // 5  verde
		glm::vec4(0.00f, 0.20f, 0.20f, 1.0f), // 6  cian
		glm::vec4(0.00f, 0.08f, 0.20f, 1.0f), // 7  azul cielo
		glm::vec4(0.00f, 0.00f, 0.20f, 1.0f), // 8  azul
		glm::vec4(0.12f, 0.00f, 0.20f, 1.0f), // 9  violeta
		glm::vec4(0.20f, 0.00f, 0.12f, 1.0f)  // 10 magenta
	};

	for (int i = 0; i < NUM_LIGHTS; ++i) {
		Light light;
		light.Position = lightPositions[i];
		light.Color = lightColors[i];
		light.Power = glm::vec4(LIGHT_POWER, LIGHT_POWER, LIGHT_POWER, 1.0f);
		light.alphaIndex = 10;
		light.distance = 3.0f;
		gLights.push_back(light);
		gLightEnabled[i] = true;
	}

	lightDummy = new Model("models/IllumModels/lightDummy.fbx");

	// Configuración de propiedades materiales
	// Tabla: http://devernay.free.fr/cours/opengl/materials.html
	material.ambient = glm::vec4(0.2f, 0.2f, 0.2f, 1.0f);
	material.diffuse = glm::vec4(0.5f, 0.5f, 0.5f, 1.0f);
	material.specular = glm::vec4(0.5f, 0.5f, 0.5f, 1.0f);
	material.transparency = 1.0f;

	// SoundEngine->play2D("sound/EternalGarden.mp3", true);

	return true;
}


void SetLightUniformInt(Shader* shader, const char* propertyName, size_t lightIndex, int value) {
	std::ostringstream ss;
	ss << "allLights[" << lightIndex << "]." << propertyName;
	std::string uniformName = ss.str();

	shader->setInt(uniformName.c_str(), value);
}
void SetLightUniformFloat(Shader* shader, const char* propertyName, size_t lightIndex, float value) {
	std::ostringstream ss;
	ss << "allLights[" << lightIndex << "]." << propertyName;
	std::string uniformName = ss.str();

	shader->setFloat(uniformName.c_str(), value);
}
void SetLightUniformVec4(Shader* shader, const char* propertyName, size_t lightIndex, glm::vec4 value) {
	std::ostringstream ss;
	ss << "allLights[" << lightIndex << "]." << propertyName;
	std::string uniformName = ss.str();

	shader->setVec4(uniformName.c_str(), value);
}
void SetLightUniformVec3(Shader* shader, const char* propertyName, size_t lightIndex, glm::vec3 value) {
	std::ostringstream ss;
	ss << "allLights[" << lightIndex << "]." << propertyName;
	std::string uniformName = ss.str();

	shader->setVec3(uniformName.c_str(), value);
}


bool Update() {
	// Cálculo del framerate
	float currentFrame = (float)glfwGetTime();
	deltaTime = currentFrame - lastFrame;
	lastFrame = currentFrame;

	// Procesa la entrada del teclado o mouse
	processInput(window);

	// Renderizado R - G - B - A
	glClearColor(1.0f, 1.0f, 1.0f, 0.0f);
	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

	glm::mat4 projection = glm::perspective(glm::radians(camera.Zoom), (float)SCR_WIDTH / (float)SCR_HEIGHT, 0.1f, 10000.0f);
	glm::mat4 view = camera.GetViewMatrix();

	// Cubemap (fondo)
	{
		mainCubeMap->drawCubeMap(*cubemapShader, projection, view);
	}
	
	// Objeto estático (casa)
	 {
		// Activamos el shader del plano
		mLightsShader->use();

		// Activamos para objetos transparentes
		glEnable(GL_BLEND);
		glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
		
		mLightsShader->setMat4("projection", projection);
		mLightsShader->setMat4("view", view);

		// Aplicamos transformaciones del modelo
		glm::mat4 model = glm::mat4(1.0f);
		model = glm::translate(model, glm::vec3(0.0f, 0.0f, 0.0f)); // translate it down so it's at the center of the scene
		model = glm::rotate(model, glm::radians(-90.0f), glm::vec3(1.0f, 0.0f, 0.0f));
		model = glm::scale(model, glm::vec3(1.0f, 1.0f, 1.0f));	// it's a bit too big for our scene, so scale it down
		mLightsShader->setMat4("model", model);

		// Configuramos propiedades de fuentes de luz
		// Solo se envían las luces encendidas, compactadas al inicio del arreglo
		size_t activeLights = 0;
		for (size_t i = 0; i < gLights.size(); ++i) {
			if (!gLightEnabled[i])
				continue;
			SetLightUniformVec3(mLightsShader, "Position", activeLights, gLights[i].Position);
			SetLightUniformVec3(mLightsShader, "Direction", activeLights, gLights[i].Direction);
			SetLightUniformVec4(mLightsShader, "Color", activeLights, gLights[i].Color);
			SetLightUniformVec4(mLightsShader, "Power", activeLights, gLights[i].Power);
			SetLightUniformInt(mLightsShader, "alphaIndex", activeLights, gLights[i].alphaIndex);
			SetLightUniformFloat(mLightsShader, "distance", activeLights, gLights[i].distance);
			++activeLights;
		}
		mLightsShader->setInt("numLights", (int)activeLights);
		
		mLightsShader->setVec3("eye", camera.Position);

		// Aplicamos propiedades materiales
		mLightsShader->setVec4("MaterialAmbientColor", material.ambient);
		mLightsShader->setVec4("MaterialDiffuseColor", material.diffuse);
		mLightsShader->setVec4("MaterialSpecularColor", material.specular);
		mLightsShader->setFloat("transparency", material.transparency);

		house->Draw(*mLightsShader);
	}

	glUseProgram(0);

	// Deplegamos los indicadores auxiliares de cada fuente de iluminación
	{
		basicShader->use();

		basicShader->setMat4("projection", projection);
		basicShader->setMat4("view", view);

		glm::mat4 model;

		for (size_t i = 0; i < gLights.size(); ++i) {
			if (!gLightEnabled[i])
				continue;
			model = glm::mat4(1.0f);
			model = glm::translate(model, gLights[i].Position);
			model = glm::rotate(model, glm::radians(-90.0f), glm::vec3(1.0f, 0.0f, 0.0f));
			model = glm::scale(model, glm::vec3(0.1f, 0.1f, 0.1f));
			basicShader->setMat4("model", model);

			lightDummy->Draw(*basicShader);
		}

	}

	glUseProgram(0);

	// glfw: swap buffers 
	glfwSwapBuffers(window);
	glfwPollEvents();

	return true;
}

// Procesamos entradas del teclado
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
	if (glfwGetKey(window, GLFW_KEY_M) == GLFW_PRESS)
		glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);
	if (glfwGetKey(window, GLFW_KEY_N) == GLFW_PRESS)
		glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
	if (glfwGetKey(window, GLFW_KEY_B) == GLFW_PRESS)
		glPolygonMode(GL_FRONT_AND_BACK, GL_POINT);

	// Encender/apagar luces: teclas 1-9 para las luces 1-9 y 0 para la luz 10
	for (int i = 0; i < NUM_LIGHTS; ++i) {
		int key = (i == 9) ? GLFW_KEY_0 : GLFW_KEY_1 + i;
		bool pressed = glfwGetKey(window, key) == GLFW_PRESS;
		if (pressed && !gLightKeyPressed[i])
			gLightEnabled[i] = !gLightEnabled[i];
		gLightKeyPressed[i] = pressed;
	}

	// Character movement
	if (glfwGetKey(window, GLFW_KEY_UP) == GLFW_PRESS) {


	}
	if (glfwGetKey(window, GLFW_KEY_DOWN) == GLFW_PRESS) {
		
	}
	if (glfwGetKey(window, GLFW_KEY_Y) == GLFW_PRESS) {
		
	}
	if (glfwGetKey(window, GLFW_KEY_H) == GLFW_PRESS) {
		
	}
	if (glfwGetKey(window, GLFW_KEY_LEFT) == GLFW_PRESS) {
		
	}
	if (glfwGetKey(window, GLFW_KEY_RIGHT) == GLFW_PRESS) {
		
	}
	
}

// glfw: Actualizamos el puerto de vista si hay cambios del tamaño
// de la ventana
void framebuffer_size_callback(GLFWwindow* window, int width, int height)
{
	glViewport(0, 0, width, height);
}

// glfw: Callback del movimiento y eventos del mouse
void mouse_callback(GLFWwindow* window, double xpos, double ypos)
{
	if (firstMouse)
	{
		lastX = (float)xpos;
		lastY = (float)ypos;
		firstMouse = false;
	}

	float xoffset = (float)xpos - lastX;
	float yoffset = lastY - (float)ypos; 

	lastX = (float)xpos;
	lastY = (float)ypos;

	camera.ProcessMouseMovement(xoffset, yoffset);
}

// glfw: Complemento para el movimiento y eventos del mouse
void scroll_callback(GLFWwindow* window, double xoffset, double yoffset)
{
	camera.ProcessMouseScroll((float)yoffset);
}
