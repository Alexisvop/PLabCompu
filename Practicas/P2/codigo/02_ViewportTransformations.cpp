/*
* Laboratorio de Computación Gráfica e Interacción Humano-Computadora
* 02 - Proyecciones, puerto de vista y transformaciones en 3D
*
* Práctica 02 - Grupo de laboratorio 06
* Actividades 1 a 7 del manual de prácticas.
*/

#include <stdlib.h>
#include <stdio.h>
#include <iostream>
#include <sstream>

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
#include "Shader.h"
#include "Mesh.h"
#include "Triangle.h"
#include "Triangle3D.h"
#include "MathFunction.h"
#include "Piramide.h"
#include "Camera.h"

// Actividad 1: geometría de la casa (cubo + techo piramidal)
#include "House3D.h"

// Functions
bool Start();
bool Update();

// Definición de callbacks

void mouseCallback    (GLFWwindow* window, double xpos, double ypos);       // Mouse
void scrollCallback   (GLFWwindow* window, double xoffset, double yoffset); // Mouse movement
void keyboardCallback (GLFWwindow* window);                                 // Keyboard

void resizeCallback   (GLFWwindow* window, int width, int height);          // Window resize

using namespace std;

// Globals
GLFWwindow* window;
GLfloat  bgR, bgG, bgB, bgA;
Shader    * myShader;
Mesh      * mesh;
int       screenWidth = 600, screenHeight = 600;

// Camera definition (XYZ position)
// La cámara arranca sobre el eje +Z, mirando al origen, con el eje +Y hacia
// arriba de la ventana; es la misma convención con la que se construyó la casa.
Camera camera(glm::vec3(0.0f, 0.0f, 3.0f),
              glm::vec3(0.0f, 0.0f, 0.0f),
              glm::vec3(0.0f, 1.0f, 0.0f));

// Ejercicio 5. Parámetros de la órbita de la cámara (coordenadas polares
// sobre el plano XZ). La posición cartesiana se reconstruye en cada cuadro.
float camAngulo = 90.0f;   // grados, medidos sobre el plano XZ
float camRadio  = 3.0f;    // distancia al origen
float camAltura = 0.0f;    // coordenada Y del observador

// Cuando el usuario fija una vista con F1/F2/F3 se suspende el recálculo
// polar; las teclas WASD lo reactivan.
bool  vistaFija = false;

// Devuelve la cámara a la órbita frontal. Se llama al fijar una vista con
// F1/F2/F3 para que, al retomar el control con WASD, el observador continúe
// desde una posición conocida y no salte a la que tenía antes.
void reiniciarOrbita() {
    camAngulo = 90.0f;
    camRadio  = 3.0f;
    camAltura = 0.0f;
}

// Ejercicios 3 y 4. Selección de la matriz de proyección.
// modoProyeccion: 0 = ortogonal, 1 = perspectiva
// varianteProyeccion: 0, 1, 2 -> tres juegos de parámetros por cada modo
int   modoProyeccion      = 0;
int   varianteProyeccion  = 0;

// Ejercicio 6. Posición del modelo (las flechas la mueven sobre el eje Z)
Vertex modelPosition;

// Ejercicio 7. Factor de escalamiento uniforme del modelo
float modelScale = 1.0f;

int main(void)
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

    /* Initialize the library GLFW */
    if (!glfwInit())
        return false;

    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);

    /* Create a windowed mode window and its OpenGL context */
    window = glfwCreateWindow(screenWidth, screenHeight, "Viewport Transformations", NULL, NULL);
    if (!window)
    {
        std::cout << "Failed to create GLFW window" << std::endl;
        glfwTerminate();
        return false;
    }

    /* Make the window's context current */
    glfwMakeContextCurrent(window);

    // Sincronización vertical: fija el lazo principal a la frecuencia del
    // monitor. Sin ella los incrementos del teclado se acumulan a la velocidad
    // que alcance el equipo y el modelo sale de la escena en un instante.
    glfwSwapInterval(1);

    /* Define callbacks */
    glfwSetFramebufferSizeCallback(window, resizeCallback);
    glfwSetCursorPosCallback(window, mouseCallback);
    glfwSetScrollCallback(window, scrollCallback);

    if (!gladLoadGLLoader((GLADloadproc)glfwGetProcAddress))
    {
        std::cout << "Failed to initialize GLAD" << std::endl;
        return false;
    }

    // Depth test activation
    glEnable(GL_DEPTH_TEST);

    // Background color initialization
    bgR = bgG = bgB = bgA = 0.0f;

    // Inicializamos la posición del modelo
    modelPosition.Position.x = 0.0f;
    modelPosition.Position.y = 0.0f;
    modelPosition.Position.z = 0.0f;

    // Shaders compilation and linking
    myShader = new Shader("shaders/02-simplePVM.vs", "shaders/02-simplePVM.fs");
    if (myShader == nullptr) {
        cout << "Error creating shaders." << endl;
        return false;
    }

    // Init geometries
    // Triangle tri(0.5f);
    // MathFunction mfunc(0.5f);
    // Triangle3D tri3D(0.5f);

    // Actividad 1: casa centrada en el origen (medio ancho 0.5, techo de 0.35)
    House3D casa(0.5f, 0.35f);

    mesh = new Mesh(casa.vertices, casa.indices);
    if (mesh == nullptr) {
        cout << "Error creating mesh object." << endl;
        return false;
    }

    cout << "--- Practica 02: proyecciones, puerto de vista y transformaciones ---" << endl;
    cout << "  F1/F2/F3 : vista frontal / lateral / superior"                       << endl;
    cout << "  1 2 3    : variantes de proyeccion ortogonal"                        << endl;
    cout << "  4 5 6    : variantes de proyeccion en perspectiva"                   << endl;
    cout << "  A / D    : giro de la camara ; W / S : acercar / alejar"             << endl;
    cout << "  Flechas arriba/abajo : avance y retroceso del modelo en Z"           << endl;
    cout << "  + / -    : aumentar y disminuir el tamano del modelo"                << endl;
    cout << "  ESC      : salir"                                                    << endl;

    return true;
}

// Ejercicios 3 y 4. Devuelve la matriz de proyección activa.
glm::mat4 matrizDeProyeccion() {

    float aspecto = (float)screenWidth / (float)screenHeight;

    if (modoProyeccion == 0) {
        // Ejercicio 3. Proyección ortogonal: los cuatro primeros argumentos
        // delimitan el volumen de visualización sobre los ejes X y Y.
        switch (varianteProyeccion) {
        case 0:  return glm::ortho(-1.0f, 1.0f, -1.0f, 1.0f, 0.1f, 1000.0f);  // volumen unitario
        case 1:  return glm::ortho(-2.0f, 2.0f, -2.0f, 2.0f, 0.1f, 1000.0f);  // volumen ampliado
        default: return glm::ortho(-2.0f, 2.0f, -1.0f, 1.0f, 0.1f, 1000.0f);  // volumen anisótropo
        }
    }

    // Ejercicio 4. Proyección en perspectiva: el primer argumento es el
    // ángulo de apertura vertical del frustum.
    switch (varianteProyeccion) {
    case 0:  return glm::perspective(glm::radians(45.0f), aspecto, 0.1f, 1000.0f);  // apertura media
    case 1:  return glm::perspective(glm::radians(90.0f), aspecto, 0.1f, 1000.0f);  // gran angular
    default: return glm::perspective(glm::radians(30.0f), aspecto, 0.1f, 1000.0f);  // teleobjetivo
    }
}

bool Update() {
    /* Process keyboard */
    keyboardCallback(window);

    /* Change the background Color */
    glClearColor(bgR, bgG, bgB, bgA);

    /* Render here */
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    /* Draw objects */
    if (mesh != nullptr) {
        glUseProgram(myShader->ID);

        // Aplicamos transformaciones de proyección y cámara
        // Ejercicios 3 y 4. Proyección ortogonal o en perspectiva
        glm::mat4 projection = matrizDeProyeccion();
        myShader->setMat4("projection", projection);

        // Ejercicio 5. Posición del observador sobre la órbita, recalculada
        // antes de armar la matriz de vista para que no vaya un cuadro atrás.
        if (!vistaFija) {
            camera.Position.x = camRadio * cos(glm::radians(camAngulo));
            camera.Position.z = camRadio * sin(glm::radians(camAngulo));
            camera.Position.y = camAltura;
        }

        glm::mat4 view = glm::lookAt(camera.Position, camera.View, camera.Up);
        myShader->setMat4("view", view);

        // Aplicamos transformaciones del modelo
        glm::mat4 model = glm::mat4(1.0f);
        // Ejercicio 6. Traslación rígida sobre el eje Z
        model = glm::translate(model, glm::vec3(modelPosition.Position.x,
                                                modelPosition.Position.y,
                                                modelPosition.Position.z));
        model = glm::rotate(model, glm::radians(0.0f), glm::vec3(0.0f, 1.0f, 0.0f));
        // Ejercicio 7. Escalamiento uniforme
        model = glm::scale(model, glm::vec3(modelScale, modelScale, modelScale));
        myShader->setMat4("model", model);

        mesh->Draw(*myShader);
    }

    /* Swap front and back buffers */
    glfwSwapBuffers(window);

    /* Poll for and process events */
    glfwPollEvents();

    return true;
}

void mouseCallback(GLFWwindow* window, double xpos, double ypos) {
    // Do something when mouse actioned
}

void scrollCallback(GLFWwindow* window, double xoffset, double yoffset)
{
    // Do something when mouse moved
}

void keyboardCallback(GLFWwindow* window) {
    // Do something when key pressed

    // Exit action
    if (glfwGetKey(window, GLFW_KEY_ESCAPE) == GLFW_PRESS) {
        glfwSetWindowShouldClose(window, true);
    }

    // Ejercicio 2. Propiedades de cámara
    // Functions
    if (glfwGetKey(window, GLFW_KEY_F1) == GLFW_PRESS) {
        // Vista frontal: cámara sobre el eje +Z mirando hacia el origen
        camera.Position = glm::vec3(0.0f, 0.0f, 3.0f);
        camera.View     = glm::vec3(0.0f, 0.0f, 0.0f);
        camera.Up       = glm::vec3(0.0f, 1.0f, 0.0f);
        reiniciarOrbita();
        vistaFija = true;
    }
    if (glfwGetKey(window, GLFW_KEY_F2) == GLFW_PRESS) {
        // Vista de lado: cámara sobre el eje +X mirando hacia el origen.
        // Se desplaza ligeramente en Y y Z para que la vista lateral no se
        // confunda con la frontal, ya que la casa es simétrica.
        camera.Position = glm::vec3(3.0f, 1.0f, 1.0f);
        camera.View     = glm::vec3(0.0f, 0.0f, 0.0f);
        camera.Up       = glm::vec3(0.0f, 1.0f, 0.0f);
        reiniciarOrbita();
        vistaFija = true;
    }
    if (glfwGetKey(window, GLFW_KEY_F3) == GLFW_PRESS) {
        // Vista superior: cámara sobre el eje +Y mirando hacia abajo.
        // El vector Up no puede quedar paralelo a la dirección de vista.
        camera.Position = glm::vec3(0.0f, 3.0f, 0.0f);
        camera.View     = glm::vec3(0.0f, 0.0f, 0.0f);
        camera.Up       = glm::vec3(0.0f, 0.0f, -1.0f);
        reiniciarOrbita();
        vistaFija = true;
    }

    // Ejercicio 3. Variantes de la proyección ortogonal
    if (glfwGetKey(window, GLFW_KEY_1) == GLFW_PRESS) {
        modoProyeccion = 0; varianteProyeccion = 0;
    }
    if (glfwGetKey(window, GLFW_KEY_2) == GLFW_PRESS) {
        modoProyeccion = 0; varianteProyeccion = 1;
    }
    if (glfwGetKey(window, GLFW_KEY_3) == GLFW_PRESS) {
        modoProyeccion = 0; varianteProyeccion = 2;
    }

    // Ejercicio 4. Variantes de la proyección en perspectiva
    if (glfwGetKey(window, GLFW_KEY_4) == GLFW_PRESS) {
        modoProyeccion = 1; varianteProyeccion = 0;
    }
    if (glfwGetKey(window, GLFW_KEY_5) == GLFW_PRESS) {
        modoProyeccion = 1; varianteProyeccion = 1;
    }
    if (glfwGetKey(window, GLFW_KEY_6) == GLFW_PRESS) {
        modoProyeccion = 1; varianteProyeccion = 2;
    }

    // Ejercicio 5. Movimiento de la cámara
    // A y D recorren la órbita; W y S modifican el radio.
    if (glfwGetKey(window, GLFW_KEY_W) == GLFW_PRESS) {
        vistaFija = false;
        camRadio -= 0.02f;
        if (camRadio < 0.1f)
            camRadio = 0.1f;
    }
    if (glfwGetKey(window, GLFW_KEY_S) == GLFW_PRESS) {
        vistaFija = false;
        camRadio += 0.02f;
    }
    if (glfwGetKey(window, GLFW_KEY_A) == GLFW_PRESS) {
        vistaFija = false;
        camAngulo += 0.5f;
    }
    if (glfwGetKey(window, GLFW_KEY_D) == GLFW_PRESS) {
        vistaFija = false;
        camAngulo -= 0.5f;
    }

    // Ejercicio 6. Movimiento del objeto 3D
    // Arrows: avance y retroceso sobre el eje Z
    if (glfwGetKey(window, GLFW_KEY_UP) == GLFW_PRESS) {
        modelPosition.Position.z += 0.01f;
    }
    if (glfwGetKey(window, GLFW_KEY_DOWN) == GLFW_PRESS) {
        modelPosition.Position.z -= 0.01f;
    }

    // Ejercicio 7. Escalamientos
    // Special keys: se aceptan las teclas del teclado numérico y las del
    // bloque alfanumérico, para equipos portátiles sin teclado numérico.
    if (glfwGetKey(window, GLFW_KEY_KP_SUBTRACT) == GLFW_PRESS ||
        glfwGetKey(window, GLFW_KEY_MINUS)       == GLFW_PRESS) {
        modelScale -= 0.01f;
        if (modelScale < 0.05f)
            modelScale = 0.05f;
    }
    if (glfwGetKey(window, GLFW_KEY_KP_ADD) == GLFW_PRESS ||
        glfwGetKey(window, GLFW_KEY_EQUAL)  == GLFW_PRESS) {
        modelScale += 0.01f;
        if (modelScale > 5.0f)
            modelScale = 5.0f;
    }

    // User defined
    if (glfwGetKey(window, GLFW_KEY_C) == GLFW_PRESS) {

    }
    if (glfwGetKey(window, GLFW_KEY_Y) == GLFW_PRESS) {

    }
    if (glfwGetKey(window, GLFW_KEY_M) == GLFW_PRESS) {

    }

}

void resizeCallback(GLFWwindow* window, int width, int height)
{
    glViewport(0, 0, width, height);
}
