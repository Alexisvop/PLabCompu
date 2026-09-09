/*
* Laboratorio de Computación Gráfica e Interacción Humano-Computadora
* 03 - Modelado Geométrico
*
* Práctica 03 - Grupo de laboratorio 06
* Actividades 1 a 3 del manual de prácticas:
*   1. Plano paralelo al plano XY, de 2x2 unidades, centrado en el origen.
*   2. Cubo de 2x2x2 unidades, centrado en el origen.
*   3. Círculo de radio 1 unidad, centrado en el origen.
*
* Teclas:
*   1 / 2 / 3  -> selecciona plano / cubo / círculo
*   F1 / F2    -> vista frontal / vista oblicua
*   L          -> alterna entre relleno y alambre (glPolygonMode)
*   ESC        -> salir
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

#include "Shader.h"
#include "Camera.h"

// Clases para construcción de geometrías
#include "Mesh.h"
#include "Axis.h"
#include "Plane.h"
#include "Cube.h"

// Actividad 3: versión corregida de la clase Circle. Como este archivo vive en
// el mismo directorio que el .cpp, el compilador toma esta copia y no la del
// directorio include del proyecto base.
#include "Circle.h"

// Funciones
bool Start();
bool Update();

// Definición de callbacks
void mouseCallback    (GLFWwindow* window, double xpos, double ypos);       // Mouse
void scrollCallback   (GLFWwindow* window, double xoffset, double yoffset); // Mouse movement
void keyboardCallback (GLFWwindow* window);                                 // Keyboard
void resizeCallback   (GLFWwindow* window, int width, int height);          // Window resize

using namespace std;

// Variables globales
GLFWwindow* window;
GLfloat  bgR, bgG, bgB, bgA;
Shader    * myShader;
Mesh      * meshAxis, * meshPlane, * meshCube, * meshCircle;
int       screenWidth = 600, screenHeight = 600;

// Definición de cámara (XYZ position)
// A diferencia del proyecto base, el segundo vector se interpreta como el
// punto al que se mira, que es lo que espera glm::lookAt, y no como una
// dirección. La cámara arranca sobre el eje +Z observando el origen.
Camera camera(glm::vec3(0.0f, 0.0f, 5.0f),  // Vector de Posición
              glm::vec3(0.0f, 0.0f, 0.0f),  // Vector de Vista (punto observado)
              glm::vec3(0.0f, 1.0f, 0.0f)); // Vector de orientación (Up)

// Primitiva que se está desplegando: 1 = plano, 2 = cubo, 3 = círculo
int  figuraActual = 1;

// Alterna el modo de rasterización entre relleno y alambre. Se usa para
// mostrar la triangulación con la que se construyó cada primitiva.
bool modoAlambre  = false;
bool teclaLPrevia = false;

// Dibuja una malla aplicando la cadena de transformaciones proyección-vista-modelo.
// Se factorizó en una función porque las cuatro geometrías de la práctica
// comparten exactamente el mismo bloque de código.
void dibujarMalla(Mesh* malla) {
    if (malla == nullptr)
        return;

    glUseProgram(myShader->ID);

    // Aplicamos transformaciones de proyección y cámara
    glm::mat4 projection = glm::perspective(glm::radians(45.0f),
                                            (float)screenWidth / (float)screenHeight,
                                            0.1f, 1000.0f);
    myShader->setMat4("projection", projection);

    glm::mat4 view = glm::lookAt(camera.Position, camera.View, camera.Up);
    myShader->setMat4("view", view);

    // Aplicamos transformaciones del modelo. Las primitivas ya se construyen
    // centradas en el origen, así que la matriz de modelo es la identidad.
    glm::mat4 model = glm::mat4(1.0f);
    myShader->setMat4("model", model);

    malla->Draw(*myShader);

    glUseProgram(0);
}

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
    window = glfwCreateWindow(screenWidth, screenHeight, "Geometric Modeling", NULL, NULL);
    if (!window)
    {
        std::cout << "Failed to create GLFW window" << std::endl;
        glfwTerminate();
        return false;
    }

    /* Make the window's context current */
    glfwMakeContextCurrent(window);

    // Se limita el lazo principal a la frecuencia de actualización del monitor
    // para que el cambio de figura no dependa de la velocidad del equipo.
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
    bgR = bgG = bgB = bgA = 1.0f;

    // Shaders compilation and linking
    myShader = new Shader("shaders/02-simplePVM.vs", "shaders/02-simplePVM.fs");
    if (myShader == nullptr) {
        cout << "Error creating shaders." << endl;
        return false;
    }

    // Definición de ejes coordenados indicados como líneas
    // rojo, verde y azul
    Axis axis(10.0f);
    meshAxis = new Mesh(axis.vertices, axis.indices);
    if (meshAxis == nullptr) {
        cout << "Error creating mesh axis object." << endl;
        return false;
    }

    // Actividad 1: Plano paralelo al plano XY, de 2x2 unidades.
    // El constructor toma el lado completo y coloca los vértices en +-lado/2,
    // de modo que el plano queda centrado en el origen.
    Plane plane(2.0f);
    meshPlane = new Mesh(plane.vertices, plane.indices);
    if (meshPlane == nullptr) {
        cout << "Error creating mesh plane object." << endl;
        return false;
    }

    // Actividad 2: Cubo de 2x2x2 unidades, centrado en el origen.
    Cube cube(2.0f);
    meshCube = new Mesh(cube.vertices, cube.indices);
    if (meshCube == nullptr) {
        cout << "Error creating mesh cube object." << endl;
        return false;
    }

    // Actividad 3: Círculo de radio 1 unidad, centrado en el origen.
    // El segundo parámetro es el número de lados del polígono con el que se
    // aproxima la circunferencia.
    Circle circle(1.0f, 72);
    meshCircle = new Mesh(circle.vertices, circle.indices);
    if (meshCircle == nullptr) {
        cout << "Error creating mesh circle object." << endl;
        return false;
    }

    cout << "Practica 03 - Modelado Geometrico" << endl;
    cout << "  1 / 2 / 3 : plano / cubo / circulo" << endl;
    cout << "  F1 / F2   : vista frontal / vista oblicua" << endl;
    cout << "  L         : alterna relleno y alambre" << endl;
    cout << "  ESC       : salir" << endl;

    return true;
}

bool Update() {
    /* Process keyboard */
    keyboardCallback(window);

    /* Change the background Color */
    glClearColor(bgR, bgG, bgB, bgA);

    /* Render here */
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    // Los ejes se dibujan siempre en relleno para que sigan siendo visibles
    // cuando la figura está en modo alambre.
    glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
    dibujarMalla(meshAxis);

    glPolygonMode(GL_FRONT_AND_BACK, modoAlambre ? GL_LINE : GL_FILL);

    switch (figuraActual) {
        case 1:  dibujarMalla(meshPlane);  break;  // Actividad 1
        case 2:  dibujarMalla(meshCube);   break;  // Actividad 2
        case 3:  dibujarMalla(meshCircle); break;  // Actividad 3
        default: break;
    }

    glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);

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

    // Exit action
    if (glfwGetKey(window, GLFW_KEY_ESCAPE) == GLFW_PRESS) {
        glfwSetWindowShouldClose(window, true);
    }

    // Selección de la primitiva que se despliega
    if (glfwGetKey(window, GLFW_KEY_1) == GLFW_PRESS) {
        figuraActual = 1;   // Actividad 1: plano
    }
    if (glfwGetKey(window, GLFW_KEY_2) == GLFW_PRESS) {
        figuraActual = 2;   // Actividad 2: cubo
    }
    if (glfwGetKey(window, GLFW_KEY_3) == GLFW_PRESS) {
        figuraActual = 3;   // Actividad 3: círculo
    }

    // Alternancia entre relleno y alambre. Se detecta el flanco de la tecla
    // porque el teclado se consulta una vez por cuadro y, de lo contrario,
    // el modo cambiaría decenas de veces con una sola pulsación.
    bool teclaLActual = (glfwGetKey(window, GLFW_KEY_L) == GLFW_PRESS);
    if (teclaLActual && !teclaLPrevia) {
        modoAlambre = !modoAlambre;
    }
    teclaLPrevia = teclaLActual;

    // Vistas solicitadas por la práctica
    if (glfwGetKey(window, GLFW_KEY_F1) == GLFW_PRESS) {
        // Vista frontal: observador sobre el eje +Z mirando al origen
        camera.Position = glm::vec3(0.0f, 0.0f, 5.0f);
        camera.View     = glm::vec3(0.0f, 0.0f, 0.0f);
        camera.Up       = glm::vec3(0.0f, 1.0f, 0.0f);
    }
    if (glfwGetKey(window, GLFW_KEY_F2) == GLFW_PRESS) {
        // Vista oblicua: observador sobre la diagonal del primer octante
        camera.Position = glm::vec3(3.0f, 3.0f, 3.0f);
        camera.View     = glm::vec3(0.0f, 0.0f, 0.0f);
        camera.Up       = glm::vec3(0.0f, 1.0f, 0.0f);
    }
}

void resizeCallback(GLFWwindow* window, int width, int height)
{
    screenWidth  = width;
    screenHeight = height;
    glViewport(0, 0, width, height);
}
