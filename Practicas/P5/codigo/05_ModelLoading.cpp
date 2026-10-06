/*
*
* 05 - Carga de modelos e interacción
*
* Práctica 05 (grupo 06): Ciudad.fbx (edificios de la Actividad 1 con casas,
* calles y banquetas de la Actividad 4) y Exo Gray de Mixamo como personaje.
*
*   Flechas ▲ ▼      personaje hacia adelante / atrás
*   Flechas ◄ ►      personaje gira a la izquierda / derecha
*   W A S D + mouse  cámara
*   M / B / N        malla de alambre / puntos / sólido
*   G                Exo Gray / personaje de la clase (character.fbx)
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
#include <animatedmodel.h>
#include <iostream>

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
// Detrás del personaje y por encima de los techos de las casas de enfrente (6 u),
// viendo hacia los edificios (-Z)
Camera camera(glm::vec3(30.0f, 10.0f, 50.0f), glm::vec3(0.0f, 1.0f, 0.0f), -90.0f, -20.0f);

// Controladores para el movimiento del mouse
float lastX = SCR_WIDTH / 2.0f;
float lastY = SCR_HEIGHT / 2.0f;
bool firstMouse = true;

// Variables para la velocidad de reproducción
// de la animación
float deltaTime = 0.0f;
float lastFrame = 0.0f;


// El personaje empieza sobre la avenida de enfrente (el asfalto está en y = -0.3)
glm::vec3 position(30.0f, -0.3f, 35.0f);
glm::vec3 forwardView(0.0f, 0.0f, -1.0f);
float     scaleV = 6.0f;                 // unidades por segundo (la ciudad mide ~100 u)
float     giroV = 90.0f;                 // grados por segundo
float     rotateCharacter = 180.0f;      // de frente a los edificios (-Z)
float     rotateTable = 0.0f;
bool      usarExo = true;

// Shaders
Shader *staticShader;
Shader *dynamicShader;
Shader *grisShader;                      // la ciudad no trae textura

// Carga la información del modelo
Model	        *house;
// Model *chair, *table;
AnimatedModel* character;
AnimatedModel* exoGray;

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

	// Loop de renderizado
	while (!glfwWindowShouldClose(window))
	{
		
	}

	// glfw: Terminamos el programa y liberamos memoria
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
	window = glfwCreateWindow(SCR_WIDTH, SCR_HEIGHT, "FBX Model Loading", NULL, NULL);
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
	dynamicShader = new Shader("shaders/09_vertex_skinning.vs", "shaders/09_fragment_skinning.fs");
	staticShader = new Shader("shaders/10_vertex_simple.vs", "shaders/10_fragment_simple.fs");
	grisShader = new Shader("shaders/05_gris.vs", "shaders/05_gris.fs");

	// Máximo número de huesos: 100 por defecto
	dynamicShader->setBonesIDs(MAX_RIGGING_BONES);

	// La ciudad mide ~100 u: cámara más rápida que la de fábrica
	camera.MovementSpeed = 12.0f;

	// Actividad 2.0 y 4.0: ciudad exportada desde Blender (alrededores_act4.py)
	house = new Model("models/Ciudad.fbx");

	// Dibujar en distintos modos de despliegue
	//	   GL_LINE: Modo Malla de alambre
	// 	   GL_POINT: Modo puntos
	// 	   GL_FILL: Modo sólido
	// glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);
	// glPolygonMode(GL_FRONT_AND_BACK, GL_POINT);
	// glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
	
	// Actividad 3.0: Importar personaje
	character = new AnimatedModel("models/character.fbx");
	// Exo Gray de Mixamo, adaptado en Blender a una sola malla, un solo material
	// y 94 huesos (personaje_act3.py), porque AnimatedModel anima una sola malla
	exoGray = new AnimatedModel("models/ExoGray.fbx");

	// Actividad 4.0
	// Aquí cargamos los otros modelos de tipo estático
	// chair = new Model("models/chair.fbx");
	// table = new Model("models/table.fbx");

	return true;
}

bool Update() {
	// Cálculo del framerate
	float currentFrame = (float)glfwGetTime();
	deltaTime = currentFrame - lastFrame;
	lastFrame = currentFrame;

	// Procesa la entrada del teclado o mouse
	processInput(window);

	// Renderizado R - G - B - A
	glClearColor(0.2f, 0.2f, 0.2f, 1.0f);
	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

	glm::mat4 projection = glm::perspective(glm::radians(camera.Zoom), (float)SCR_WIDTH / (float)SCR_HEIGHT, 0.1f, 10000.0f);
	glm::mat4 view = camera.GetViewMatrix();
	
	// Actividad 2.1: Dibujar modelo de casa
	// Objeto estático (casa)
	{
		// Activamos el shader gris: Ciudad.fbx no trae textura
		grisShader->use();

		// Activamos para objetos transparentes
		glEnable(GL_BLEND);
		glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

		// Aplicamos transformaciones de proyección y cámara (si las hubiera)
		grisShader->setMat4("projection", projection);
		grisShader->setMat4("view", view);

		// Aplicamos transformaciones del modelo
		glm::mat4 model = glm::mat4(1.0f);
		model = glm::translate(model, glm::vec3(0.0f, 0.0f, 0.0f)); // translate it down so it's at the center of the scene
		model = glm::rotate(model, glm::radians(-90.0f), glm::vec3(1.0f, 0.0f, 0.0f));
		model = glm::scale(model, glm::vec3(1.0f, 1.0f, 1.0f));	// it's a bit too big for our scene, so scale it down
		grisShader->setMat4("model", model);

		house->Draw(*grisShader);
	} 

	glUseProgram(0);

	// Actividad 3.1: Dibujar personaje
	// Objeto dinámico (Personaje animado)
	{
		AnimatedModel* personaje = usarExo ? exoGray : character;

		// Actualización de la animación
		personaje->UpdateAnimation(deltaTime);

		// Activación del shader del personaje
		dynamicShader->use();

		// Aplicamos transformaciones de proyección y cámara (si las hubiera)
		dynamicShader->setMat4("projection", projection);
		dynamicShader->setMat4("view", view);

		// Aplicamos transformaciones del modelo
		glm::mat4 model = glm::mat4(1.0f);
		model = glm::translate(model, position); // translate it down so it's at the center of the scene
		model = glm::rotate(model, glm::radians(rotateCharacter), glm::vec3(0.0f, 1.0f, 0.0f));
		model = glm::scale(model, glm::vec3(0.01f, 0.01f, 0.01f));	// it's a bit too big for our scene, so scale it down

		dynamicShader->setMat4("model", model);

		dynamicShader->setMat4("gBones", MAX_RIGGING_BONES, personaje->gBones);

		// Dibujamos el modelo
		personaje->Draw(*dynamicShader);
	}

	// Desactivamos el shader actual
	glUseProgram(0);

	// Actividad 4.0
	// Aquí desplegamos los demás modelos, cada uno con su propio
	// ciclo de renderizado bajo el siguiente algoritmo
	// a) Activar shader estático/dinámico
	// b) Crear matrices de proyección, vista, modelo
	// c) Enviar matrices al shader correspondiente
	// d) Dibujar el modelo
	// e) Desactivar shader
	
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

	// G alterna el personaje solo en el cuadro en que se presiona
	static bool gAntes = false;
	bool gAhora = glfwGetKey(window, GLFW_KEY_G) == GLFW_PRESS;
	if (gAhora && !gAntes)
		usarExo = !usarExo;
	gAntes = gAhora;

	// Character movement
	// Velocidades multiplicadas por deltaTime: no dependen de los FPS
	if (glfwGetKey(window, GLFW_KEY_UP) == GLFW_PRESS) {
		position = position + scaleV * deltaTime * forwardView;
	}
	if (glfwGetKey(window, GLFW_KEY_DOWN) == GLFW_PRESS) {
		position = position - scaleV * deltaTime * forwardView;
	}
	if (glfwGetKey(window, GLFW_KEY_LEFT) == GLFW_PRESS) {
		rotateCharacter += giroV * deltaTime;

		glm::mat4 model = glm::mat4(1.0f);
		model = glm::rotate(model, glm::radians(rotateCharacter), glm::vec3(0.0f, 1.0f, 0.0f));
		glm::vec4 viewVector = model * glm::vec4(0.0f, 0.0f, 1.0f, 1.0f);
		forwardView = glm::vec3(viewVector);
		forwardView = glm::normalize(forwardView);
	}
	if (glfwGetKey(window, GLFW_KEY_RIGHT) == GLFW_PRESS) {
		rotateCharacter -= giroV * deltaTime;

		glm::mat4 model = glm::mat4(1.0f);
		model = glm::rotate(model, glm::radians(rotateCharacter), glm::vec3(0.0f, 1.0f, 0.0f));
		glm::vec4 viewVector = model * glm::vec4(0.0f, 0.0f, 1.0f, 1.0f);
		forwardView = glm::vec3(viewVector);
		forwardView = glm::normalize(forwardView);
	}

	if (glfwGetKey(window, GLFW_KEY_I) == GLFW_PRESS)
	{
		// rotateTable += 0.05f;
	}
	if (glfwGetKey(window, GLFW_KEY_K) == GLFW_PRESS)
	{
		// rotateTable -= 0.05f;
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
