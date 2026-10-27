/*
* 08 - Modelos de Iluminación II
* Actividad 4: seis materiales sobre el toroide (mármol, madera, oro, bronce, cobre y cristal)
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
#include <cubemap.h>

// Functions
bool Start();
bool Update();

// Definición de callbacks
void framebuffer_size_callback(GLFWwindow* window, int width, int height);
void mouse_callback(GLFWwindow* window, double xpos, double ypos);
void scroll_callback(GLFWwindow* window, double xoffset, double yoffset);
void key_callback(GLFWwindow* window, int key, int scancode, int action, int mods);
void processInput(GLFWwindow* window);

// Gobals
GLFWwindow* window;

// Tamaño en pixeles de la ventana
const unsigned int SCR_WIDTH = 1280;
const unsigned int SCR_HEIGHT = 720;

// Definición de cámara (posición en XYZ)
Camera camera(glm::vec3(0.0f, 0.0f, 9.0f));

// Controladores para el movimiento del mouse
float lastX = SCR_WIDTH / 2.0f;
float lastY = SCR_HEIGHT / 2.0f;
bool firstMouse = true;

float deltaTime = 0.0f;
float lastFrame = 0.0f;
float angle = 0.0f;

// Shaders
Shader* cubemapShader;
Shader* materialShader;

// Modelo y entorno
Model* torus;
CubeMap* mainCubeMap;

// Texturas de los materiales
unsigned int texMarmol, texMadera, texMetal;

// Opciones que se cambian con el teclado
bool useTexture = true;   // T: activa o quita la información de textura
bool rotating = true;     // Espacio: pausa la rotación

// Propiedades de cada material. Los metales usan la tabla de
// http://devernay.free.fr/cours/opengl/materials.html (shininess = valor * 128).
struct TorusMaterial {
	const char* name;
	glm::vec3 ambient;
	glm::vec3 diffuse;
	glm::vec3 specular;
	float shininess;
	unsigned int* texture;
	float uvRepeat;
	float reflectivity;
	bool isGlass;
};

TorusMaterial materials[6] = {
	{ "Marmol", { 0.25f, 0.25f, 0.25f }, { 0.80f, 0.80f, 0.80f }, { 0.60f, 0.60f, 0.60f }, 96.0f, &texMarmol, 1.0f, 0.12f, false },
	{ "Madera", { 0.30f, 0.30f, 0.30f }, { 0.90f, 0.90f, 0.90f }, { 0.08f, 0.08f, 0.08f }, 8.0f, &texMadera, 1.0f, 0.0f, false },
	{ "Oro", { 0.24725f, 0.1995f, 0.0745f }, { 0.75164f, 0.60648f, 0.22648f }, { 0.628281f, 0.555802f, 0.366065f }, 51.2f, &texMetal, 2.0f, 0.30f, false },
	{ "Bronce", { 0.2125f, 0.1275f, 0.054f }, { 0.714f, 0.4284f, 0.18144f }, { 0.393548f, 0.271906f, 0.166721f }, 25.6f, &texMetal, 2.0f, 0.35f, false },
	{ "Cobre", { 0.19125f, 0.0735f, 0.0225f }, { 0.7038f, 0.27048f, 0.0828f }, { 0.256777f, 0.137622f, 0.086014f }, 12.8f, &texMetal, 2.0f, 0.35f, false },
	{ "Cristal", { 0.0f, 0.0f, 0.0f }, { 0.90f, 0.97f, 1.00f }, { 1.0f, 1.0f, 1.0f }, 128.0f, nullptr, 1.0f, 1.0f, true },
};

// Posiciones de los seis toroides (3 columnas x 2 filas)
glm::vec3 positions[6] = {
	{ -3.0f,  1.4f, 0.0f }, { 0.0f,  1.4f, 0.0f }, { 3.0f,  1.4f, 0.0f },
	{ -3.0f, -1.4f, 0.0f }, { 0.0f, -1.4f, 0.0f }, { 3.0f, -1.4f, 0.0f },
};

// Dos luces blancas: una principal y una de relleno más tenue
glm::vec3 lightPos[2] = { { 4.0f, 4.0f, 6.0f }, { -5.0f, -1.0f, 4.0f } };
glm::vec3 lightColor[2] = { { 1.0f, 1.0f, 1.0f }, { 0.35f, 0.35f, 0.40f } };

// Entrada a función principal
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

	window = glfwCreateWindow(SCR_WIDTH, SCR_HEIGHT, "P8 Act. 4 - Materiales en el toroide", NULL, NULL);
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
	glfwSetKeyCallback(window, key_callback);

	if (!gladLoadGLLoader((GLADloadproc)glfwGetProcAddress))
	{
		std::cout << "Failed to initialize GLAD" << std::endl;
		return false;
	}

	glEnable(GL_DEPTH_TEST);

	// Compilación y enlace de shaders
	materialShader = new Shader("shaders/08_materiales.vs", "shaders/08_materiales.fs");
	cubemapShader = new Shader("shaders/10_vertex_cubemap.vs", "shaders/10_fragment_cubemap.fs");

	// El mismo toroide de la actividad 3, exportado a OBJ desde toroide.blend
	// (el Assimp del laboratorio no carga el FBX de Blender 5)
	torus = new Model("models/P8/toroide.obj");

	// Texturas de mármol, madera y metal cepillado
	texMarmol = TextureFromFile("marmol.png", "textures/P8");
	texMadera = TextureFromFile("madera.png", "textures/P8");
	texMetal = TextureFromFile("metal_cepillado.png", "textures/P8");

	// Cubemap: fondo y fuente de los reflejos
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

	std::cout << "T: textura si/no | Espacio: pausar giro | WASD + mouse: camara | M/N: malla/solido | Esc: salir" << std::endl;
	return true;
}

bool Update() {
	float currentFrame = (float)glfwGetTime();
	deltaTime = currentFrame - lastFrame;
	lastFrame = currentFrame;
	if (rotating)
		angle += 25.0f * deltaTime;

	processInput(window);

	glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

	int width, height;
	glfwGetFramebufferSize(window, &width, &height);
	if (width == 0 || height == 0) {
		glfwPollEvents();
		return true;
	}
	glm::mat4 projection = glm::perspective(glm::radians(camera.Zoom), (float)width / (float)height, 0.1f, 10000.0f);
	glm::mat4 view = camera.GetViewMatrix();

	// Cubemap (fondo)
	mainCubeMap->drawCubeMap(*cubemapShader, projection, view);

	materialShader->use();
	materialShader->setMat4("projection", projection);
	materialShader->setMat4("view", view);
	materialShader->setVec3("eye", camera.Position);
	for (int i = 0; i < 2; ++i) {
		materialShader->setVec3("lightPos[" + std::to_string(i) + "]", lightPos[i]);
		materialShader->setVec3("lightColor[" + std::to_string(i) + "]", lightColor[i]);
	}

	// El modelo usa la unidad 0 para su propia textura; las nuestras van en la 1 y la 2
	materialShader->setInt("texMaterial", 1);
	materialShader->setInt("cubetex", 2);
	glActiveTexture(GL_TEXTURE2);
	glBindTexture(GL_TEXTURE_CUBE_MAP, mainCubeMap->textureID);

	for (int i = 0; i < 6; ++i) {
		const TorusMaterial& m = materials[i];

		// Inclinamos el toroide para que se vea el hueco y lo giramos sobre su eje
		glm::mat4 model = glm::mat4(1.0f);
		model = glm::translate(model, positions[i]);
		model = glm::rotate(model, glm::radians(60.0f), glm::vec3(1.0f, 0.0f, 0.0f));
		model = glm::rotate(model, glm::radians(angle + 30.0f * i), glm::vec3(0.0f, 1.0f, 0.0f));
		model = glm::scale(model, glm::vec3(1.0f));
		materialShader->setMat4("model", model);

		materialShader->setVec3("material.ambient", m.ambient);
		materialShader->setVec3("material.diffuse", m.diffuse);
		materialShader->setVec3("material.specular", m.specular);
		materialShader->setFloat("material.shininess", m.shininess);
		materialShader->setBool("useTexture", useTexture && m.texture != nullptr);
		materialShader->setFloat("uvRepeat", m.uvRepeat);
		materialShader->setFloat("reflectivity", m.reflectivity);
		materialShader->setBool("isGlass", m.isGlass);
		materialShader->setFloat("ior", 1.5f);

		glActiveTexture(GL_TEXTURE1);
		glBindTexture(GL_TEXTURE_2D, m.texture ? *m.texture : 0);

		torus->Draw(*materialShader);
	}
	glActiveTexture(GL_TEXTURE0);
	glUseProgram(0);

	glfwSwapBuffers(window);
	glfwPollEvents();
	return true;
}

// Teclas de un solo pulso (para no alternar en cada cuadro)
void key_callback(GLFWwindow* window, int key, int scancode, int action, int mods)
{
	if (action != GLFW_PRESS)
		return;
	if (key == GLFW_KEY_T) {
		useTexture = !useTexture;
		glfwSetWindowTitle(window, useTexture ? "P8 Act. 4 - Materiales con textura" : "P8 Act. 4 - Materiales sin textura (solo Phong)");
	}
	if (key == GLFW_KEY_SPACE)
		rotating = !rotating;
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
}

void framebuffer_size_callback(GLFWwindow* window, int width, int height)
{
	glViewport(0, 0, width, height);
}

// La cámara solo gira mientras se mantiene el botón derecho, así el mouse
// no mueve la vista al grabar o al usar la ventana
void mouse_callback(GLFWwindow* window, double xpos, double ypos)
{
	if (glfwGetMouseButton(window, GLFW_MOUSE_BUTTON_RIGHT) != GLFW_PRESS) {
		firstMouse = true;
		return;
	}
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

void scroll_callback(GLFWwindow* window, double xoffset, double yoffset)
{
	camera.ProcessMouseScroll((float)yoffset);
}
