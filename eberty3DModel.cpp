/* Copyright 2018 Eberty */

/* 		ATIVIDADE 1
 * Analisar os métodos de coloração, contrapondo modelos fotorealísticos e não fotorealísticos.
 * 		– Modelo fotorealistico: implementar o modelo clássico de Phong
 * 		– Modelo não fotorealistico: implementar o modelo  de Gooch
 * 		– Seu protótipo deve atender aos seguintes requisitos:
 * 			• Analise em modelos poligonais com características geométricas distintas;
 * 			• A cena de avaliação devem conter um modelo poligonal;
 * 			• Os dois modos de shading devem ser apresentados lado a lado na tela;
 * 			• O usuário poderá modificar os parâmetros relacionados ao observador e fonte de luz;
 *
 *  	– Garantir que as informações do modelo relativas a cor do material (Phong) sejam passadas por parametro e não fixas no shader;
 * 		– Que o arquivo de material do objeto seja utlizado para essa configuração da cor do material (a biblioteca Assimp ja lhe fornece esses dados).
 *	 	– Nessa mesma linha, que modelos com textura em sua descrição de material (arquivo .mtl) sejam visualizados com textura.
 * 		– A geração de mais de um padrão de textura não-fotorealista seja aplicado para comparação. E que pelo menos um deles seja gerado a partir de uma textura 1D.
 * */

#include <assimp/cimport.h>
#include <assimp/scene.h>
#include <assimp/postprocess.h>

#include <SOIL/SOIL.h>

#include <GL/glew.h>

#include <GLFW/glfw3.h>

#include <iostream>
#include <cfloat>
#include <vector>

#include <glm/vec3.hpp>
#include <glm/vec4.hpp>
#include <glm/mat4x4.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>

#include "shaders/initShaders.h"


#ifndef BUFFER_OFFSET
#define BUFFER_OFFSET(i) ((char *)NULL + (i))
#endif
#ifndef MIN
#define MIN(x, y) (x < y ? x:y)
#endif
#ifndef MAX
#define MAX(x, y) (y > x ? y : x)
#endif

using namespace std;

string fileDirectory;

GLuint shaderGooch,
       shaderPhong,
       shader;
GLuint axisVBO[3];
GLuint meshVBO[4];
GLuint meshSize;

GLuint texLoad;
GLuint uTextureID;
GLuint tex1d;
GLuint uType = 0;

vector<GLfloat> vboVertices;
vector<GLfloat> vboNormals;
vector<GLfloat> vboColors;
vector<GLfloat> vboTxCoords;

int winWidth = 1300,
    winHeight = 500;

float angleX = 0.0f,
      angleY = 0.0f,
      angleZ = 0.0f;

double rotationAngle = 0.02f, fov = 45.0;
double lastTime, lastX, lastY;

/* the global Assimp scene object */
const aiScene* scene = NULL;
GLuint scene_list = 0;
aiVector3D scene_min, scene_max, scene_center;

glm::vec3 lightPos, cameraPos;
aiVector3D camera, light, camera_rotation, light_rotation;
bool pause;

glm::vec4 matAmb = glm::vec4(0.6, 0.6, 0.6, 1.0);
glm::vec4 matDif = glm::vec4(0.5, 0.0, 0.0, 1.0);
glm::vec4 matSpec = glm::vec4(1.0, 1.0, 1.0, 1.0);

/* ********************************************************************* */

void get_bounding_box_for_node(const struct aiNode* nd, aiVector3D* min, aiVector3D* max, aiMatrix4x4* trafo) {
    aiMatrix4x4 prev;
    unsigned int n = 0, t;

    prev = *trafo;
    aiMultiplyMatrix4(trafo, &nd->mTransformation);

    for (; n < nd->mNumMeshes; ++n) {
        const aiMesh* mesh = scene->mMeshes[nd->mMeshes[n]];
        for (t = 0; t < mesh->mNumVertices; ++t) {
            aiVector3D tmp = mesh->mVertices[t];
            aiTransformVecByMatrix4(&tmp, trafo);

            min->x = MIN(min->x, tmp.x);
            min->y = MIN(min->y, tmp.y);
            min->z = MIN(min->z, tmp.z);

            max->x = MAX(max->x, tmp.x);
            max->y = MAX(max->y, tmp.y);
            max->z = MAX(max->z, tmp.z);
        }
    }

    for (n = 0; n < nd->mNumChildren; ++n) {
        get_bounding_box_for_node(nd->mChildren[n], min, max, trafo);
    }
    *trafo = prev;
}

/* ********************************************************************* */

void color4_to_float4(const aiColor4D *c, float f[4]) {
    f[0] = c->r;
    f[1] = c->g;
    f[2] = c->b;
    f[3] = c->a;
}

/* ********************************************************************* */

void set_float4(float f[4], float a, float b, float c, float d) {
    f[0] = a;
    f[1] = b;
    f[2] = c;
    f[3] = d;
}

/* ********************************************************************* */

int loadGLTextures(const char *  txName) {
    uTextureID = SOIL_load_OGL_texture(txName, SOIL_LOAD_AUTO, SOIL_CREATE_NEW_ID, SOIL_FLAG_INVERT_Y);
    if (uTextureID == 0) {
        cout << "SOIL loading error: '" << SOIL_last_result() << "'" << endl;
        return false;
    }
    glBindTexture(GL_TEXTURE_2D, uTextureID);
    return true;
}

/* ********************************************************************* */

static void Create1DTexture() {
    glGenTextures(1, &tex1d); // generate the specified number of texture objects
    glBindTexture(GL_TEXTURE_1D, tex1d); // bind texture
    glPixelStorei(GL_UNPACK_ALIGNMENT, 1); // tells OpenGL how the data that is going to be uploaded is aligned

    /*float data[] = {
        1.0f, 0.75f, 0.75f, 1.0f, // objectColor
        0.0f, 0.0f, 0.6f, 1.0f, // coolColor
        0.6f, 0.6f, 0.0f, 1.0f // warmColor
    };*/

    float data[] = {
        0.45f, 0.3375f, 0.9375f, 1.0f, // cool = min(coolColor + objectColor * 0.45, 1.0);
        1.0f, 0.9375f, 0.3375f, 1.0f // warm = min(warmColor + objectColor * 0.45, 1.0);
    };

    glTexImage1D(GL_TEXTURE_1D, 0, GL_RGBA32F, 2, 0, GL_RGBA, GL_FLOAT, data);
    // texture sampling/filtering operation.
    glTexParameteri(GL_TEXTURE_1D, GL_TEXTURE_WRAP_S, GL_CLAMP);
    glTexParameteri(GL_TEXTURE_1D, GL_TEXTURE_WRAP_T, GL_CLAMP);
    glTexParameteri(GL_TEXTURE_1D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_1D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);

    glBindTexture(GL_TEXTURE_1D, tex1d);
}

/* ********************************************************************* */

int traverseScene(const aiScene *sc, const aiNode* nd) {
    int totVertices = 0;
    /* draw all meshes assigned to this node */
    for (unsigned int n = 0; n < nd->mNumMeshes; ++n) {
        const aiMesh* mesh = scene->mMeshes[nd->mMeshes[n]];
        const aiMaterial* const mat = scene->mMaterials[mesh->mMaterialIndex];

        aiColor4D c;
        float o;
        aiString s;

        if (AI_SUCCESS == mat->Get(AI_MATKEY_COLOR_DIFFUSE, c)) {
            // cout << "Kd " << c.r << " " << c.g << " " << c.b << endl;
            color4_to_float4(&c, glm::value_ptr(matDif));
        }
        if (AI_SUCCESS == mat->Get(AI_MATKEY_COLOR_AMBIENT, c)) {
            // cout << "Ka " << c.r << " " << c.g << " " << c.b << endl;
            color4_to_float4(&c, glm::value_ptr(matAmb));
        }
        if (AI_SUCCESS == mat->Get(AI_MATKEY_COLOR_SPECULAR, c)) {
            // cout << "Ks " << c.r << " " << c.g << " " << c.b << endl;
            color4_to_float4(&c, glm::value_ptr(matSpec));
        }
        if (AI_SUCCESS == mat->Get(AI_MATKEY_SHININESS, o) && o) {
            // cout << "Ns " << o << endl;
        }

        if (AI_SUCCESS == mat->Get(AI_MATKEY_TEXTURE_DIFFUSE(n), s)) {
            // cout << "map_Kd " << s.data << endl;
            /* load an image file directly as a new OpenGL texture */
            texLoad = 1;
            std::string textureFileName = fileDirectory + s.C_Str();
            loadGLTextures(textureFileName.c_str());
        } else {
            texLoad = 0;
        }

        for (unsigned int t = 0; t < mesh->mNumFaces; ++t) {
            const aiFace* face = &mesh->mFaces[t];
            for (unsigned int i = 0; i < face->mNumIndices; i++) {
                int index = face->mIndices[i];
                // if (mesh->mColors[0] != NULL) {
                vboColors.push_back(matAmb[0]);
                vboColors.push_back(matAmb[1]);
                vboColors.push_back(matAmb[2]);
                vboColors.push_back(matAmb[3]);
                // }
                if (mesh->HasTextureCoords(0)) {
                    vboTxCoords.push_back(mesh->mTextureCoords[0][index][0]);
                    vboTxCoords.push_back(mesh->mTextureCoords[0][index][1]);
                } else {
                    texLoad = 0;
                }
                if (mesh->mNormals != NULL) {
                    vboNormals.push_back(mesh->mNormals[index].x);
                    vboNormals.push_back(mesh->mNormals[index].y);
                    vboNormals.push_back(mesh->mNormals[index].z);
                }
                vboVertices.push_back(mesh->mVertices[index].x);
                vboVertices.push_back(mesh->mVertices[index].y);
                vboVertices.push_back(mesh->mVertices[index].z);
                totVertices++;
            }
        }
    }

    for (unsigned int n = 0; n < nd->mNumChildren; ++n)
        totVertices += traverseScene(sc, nd->mChildren[n]);

    return totVertices;
}

/* ********************************************************************* */

void createVBOs(const aiScene *sc) {
    int totVertices = 0;
    cout << "Scene:     #Meshes     = " << sc->mNumMeshes << endl;
    cout << "           #Textures   = " << sc->mNumTextures << endl;

    totVertices = traverseScene(sc, sc->mRootNode);

    cout << "           #Vertices   = " << totVertices << endl;
    cout << "           #vboVertices= " << vboVertices.size() << endl;
    cout << "           #vboColors= " << vboColors.size() << endl;
    cout << "           #vboNormals= " << vboNormals.size() << endl;
    cout << "           #vboTxCoords= " << vboTxCoords.size() << endl;

    glGenBuffers(4, meshVBO);

    glBindBuffer(GL_ARRAY_BUFFER, meshVBO[0]);

    glBufferData(GL_ARRAY_BUFFER, vboVertices.size()*sizeof(float),
                 vboVertices.data(), GL_STATIC_DRAW);

    glBindBuffer(GL_ARRAY_BUFFER, meshVBO[1]);

    glBufferData(GL_ARRAY_BUFFER, vboColors.size()*sizeof(float),
                 vboColors.data(), GL_STATIC_DRAW);

    if (vboNormals.size() > 0) {
        glBindBuffer(GL_ARRAY_BUFFER, meshVBO[2]);

        glBufferData(GL_ARRAY_BUFFER, vboNormals.size()*sizeof(float),
                     vboNormals.data(), GL_STATIC_DRAW);
    }


    if (vboTxCoords.size() > 0) {
        glBindBuffer(GL_ARRAY_BUFFER, meshVBO[3]);

        glBufferData(GL_ARRAY_BUFFER, vboTxCoords.size() * sizeof(float),
                     vboTxCoords.data(), GL_STATIC_DRAW);
    }

    meshSize = vboVertices.size() / 3;
    cout << "           #meshSize= " << meshSize << endl;
}

/* ********************************************************************* */

void createAxis() {
    GLfloat vertices[] = {0.0, 0.0, 0.0,
                          scene_max.x*2, 0.0, 0.0,
                          0.0, scene_max.y*2, 0.0,
                          0.0, 0.0, scene_max.z*2
                         };

    GLuint lines[] = { 0, 3,
                       0, 2,
                       0, 1
                     };

    GLfloat colors[] = { 1.0, 1.0, 1.0, 1.0,
                         1.0, 0.0, 0.0, 1.0,
                         0.0, 1.0, 0.0, 1.0,
                         0.0, 0.0, 1.0, 1.0
                       };

    glGenBuffers(3, axisVBO);
    glBindBuffer(GL_ARRAY_BUFFER, axisVBO[0]);
    glBufferData(GL_ARRAY_BUFFER, 4*3*sizeof(float), vertices, GL_STATIC_DRAW);
    glBindBuffer(GL_ARRAY_BUFFER, axisVBO[1]);
    glBufferData(GL_ARRAY_BUFFER, 4*4*sizeof(float), colors, GL_STATIC_DRAW);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, axisVBO[2]);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, 3*2*sizeof(unsigned int), lines, GL_STATIC_DRAW);
    Create1DTexture();
}

/* ********************************************************************* */

void drawAxis() {
    int attrV, attrC;

    glBindBuffer(GL_ARRAY_BUFFER, axisVBO[0]);
    attrV = glGetAttribLocation(shader, "aPosition");
    glVertexAttribPointer(attrV, 3, GL_FLOAT, GL_FALSE, 0, 0);
    glEnableVertexAttribArray(attrV);

    glBindBuffer(GL_ARRAY_BUFFER, axisVBO[1]);
    attrC = glGetAttribLocation(shader, "aColor");
    glVertexAttribPointer(attrC, 4, GL_FLOAT, GL_FALSE, 0, 0);
    glEnableVertexAttribArray(attrC);

    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, axisVBO[2]);
    glDrawElements(GL_LINES, 6, GL_UNSIGNED_INT, BUFFER_OFFSET(0));

    glDisableVertexAttribArray(attrV);
    glDisableVertexAttribArray(attrC);

    glBindBuffer(GL_ARRAY_BUFFER, 0);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);
}

/* ********************************************************************* */

void drawMesh() {
    int attrV, attrC, attrN, attrCo;

    glBindBuffer(GL_ARRAY_BUFFER, meshVBO[0]);
    attrV = glGetAttribLocation(shader, "aPosition");
    glVertexAttribPointer(attrV, 3, GL_FLOAT, GL_FALSE, 0, 0);
    glEnableVertexAttribArray(attrV);

    glBindBuffer(GL_ARRAY_BUFFER, meshVBO[1]);
    attrC = glGetAttribLocation(shader, "aColor");
    glVertexAttribPointer(attrC, 4, GL_FLOAT, GL_FALSE, 0, 0);
    glEnableVertexAttribArray(attrC);

    glBindBuffer(GL_ARRAY_BUFFER, meshVBO[2]);
    attrN = glGetAttribLocation(shader, "aNormal");
    glVertexAttribPointer(attrN, 3, GL_FLOAT, GL_FALSE, 0, 0);
    glEnableVertexAttribArray(attrN);

    if (texLoad) {
        glBindBuffer(GL_ARRAY_BUFFER, meshVBO[3]);
        attrCo = glGetAttribLocation(shader, "aCoords");
        glVertexAttribPointer(attrCo, 2, GL_FLOAT, GL_FALSE, 0, 0);
        glEnableVertexAttribArray(attrCo);
    }

    glDrawArrays(GL_TRIANGLES, 0, meshSize);

    glDisableVertexAttribArray(attrV);
    glDisableVertexAttribArray(attrC);
    glDisableVertexAttribArray(attrN);
    if (texLoad)
        glDisableVertexAttribArray(attrCo);

    glBindBuffer(GL_ARRAY_BUFFER, 0);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);
}

/* ********************************************************************* */

void display(void) {
    if (!pause)
        angleY += 0.02;

    float lightX = light.x * sin(light_rotation.x);
    float lightY = light.y * cos(light_rotation.y);
    float lightZ = light.z * cos(light_rotation.z);
    lightPos = glm::vec3(lightX, lightY, lightZ);

    glm::vec3 lookAt = glm::vec3(scene_center.x, scene_center.y, scene_center.z);
    glm::vec3 up = glm::vec3(0.0, 1.0, 0.0);

    glm::mat4 ViewMat = glm::lookAt(cameraPos, lookAt, up);
    ViewMat = glm::rotate(ViewMat, camera_rotation.x, glm::vec3(1.0, 0.0, 0.0));
    ViewMat = glm::rotate(ViewMat, camera_rotation.y, glm::vec3(0.0, 1.0, 0.0));
    ViewMat = glm::rotate(ViewMat, camera_rotation.z, glm::vec3(0.0, 0.0, 1.0));

    glm::mat4 ProjMat = glm::perspective(glm::radians(fov), (winWidth / winHeight*1.0), 0.01, 10000.0);
    glm::mat4 ModelMat = glm::mat4(1.0);

    glViewport(0, 0, winWidth/2, winHeight);
    ModelMat = glm::rotate(ModelMat, angleX, glm::vec3(1.0, 0.0, 0.0));
    ModelMat = glm::rotate(ModelMat, angleY, glm::vec3(0.0, 1.0, 0.0));
    ModelMat = glm::rotate(ModelMat, angleZ, glm::vec3(0.0, 0.0, 1.0));

    glm::mat4 MVP = ProjMat * ViewMat * ModelMat;

    glm::mat4 normalMat = glm::transpose(glm::inverse(ModelMat));

    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    glEnable(GL_TEXTURE_2D);
    shader = shaderPhong;
    glUseProgram(shader);

    int loc = glGetUniformLocation(shader, "uMVP");
    glUniformMatrix4fv(loc, 1, GL_FALSE, glm::value_ptr(MVP));
    loc = glGetUniformLocation(shader, "uN");
    glUniformMatrix4fv(loc, 1, GL_FALSE, glm::value_ptr(normalMat));
    loc = glGetUniformLocation(shader, "uM");
    glUniformMatrix4fv(loc, 1, GL_FALSE, glm::value_ptr(ModelMat));
    loc = glGetUniformLocation(shader, "uLPos");
    glUniform3fv(loc, 1, glm::value_ptr(lightPos));
    loc = glGetUniformLocation(shader, "uCamPos");
    glUniform3fv(loc, 1, glm::value_ptr(cameraPos));
    loc = glGetUniformLocation(shader, "uTexLoad");
    glUniform1i(loc, texLoad);

    loc = glGetUniformLocation(shader, "matAmb");
    glUniform4fv(loc, 1, glm::value_ptr(matAmb));
    loc = glGetUniformLocation(shader, "matDif");
    glUniform4fv(loc, 1, glm::value_ptr(matDif));
    loc = glGetUniformLocation(shader, "matSpec");
    glUniform4fv(loc, 1, glm::value_ptr(matSpec));

    drawAxis();
    drawMesh();
    glDisable(GL_TEXTURE_2D);
    glViewport(winWidth/2, 0, winWidth/2, winHeight);

    glEnable(GL_TEXTURE_1D);
    shader = shaderGooch;
    glUseProgram(shader);

    loc = glGetUniformLocation(shader, "uMVP");
    glUniformMatrix4fv(loc, 1, GL_FALSE, glm::value_ptr(MVP));
    loc = glGetUniformLocation(shader, "uN");
    glUniformMatrix4fv(loc, 1, GL_FALSE, glm::value_ptr(normalMat));
    loc = glGetUniformLocation(shader, "uM");
    glUniformMatrix4fv(loc, 1, GL_FALSE, glm::value_ptr(ModelMat));
    loc = glGetUniformLocation(shader, "uLPos");
    glUniform3fv(loc, 1, glm::value_ptr(lightPos));
    loc = glGetUniformLocation(shader, "uCamPos");
    glUniform3fv(loc, 1, glm::value_ptr(cameraPos));
    loc = glGetUniformLocation(shader, "uType");
    glUniform1i(loc, uType);

    loc = glGetUniformLocation(shader, "objectColor");
    glUniform3fv(loc, 1, glm::value_ptr(glm::vec3(1.0f, 0.75f, 0.75f)));
    loc = glGetUniformLocation(shader, "coolColor");
    glUniform3fv(loc, 1, glm::value_ptr(glm::vec3(0.0f, 0.0f,  0.6f)));
    loc = glGetUniformLocation(shader, "warmColor");
    glUniform3fv(loc, 1, glm::value_ptr(glm::vec3(0.6f, 0.6f,  0.0f)));
    loc = glGetUniformLocation(shader, "diffuseCool");
    glUniform1f(loc, 0.45);
    loc = glGetUniformLocation(shader, "diffuseWarm");
    glUniform1f(loc, 0.45);

    drawAxis();
    drawMesh();
    glDisable(GL_TEXTURE_1D);
}

/* ********************************************************************* */

void initGL(GLFWwindow* window) {
    glClearColor(0.0, 0.0, 0.0, 0.0);
    if (glewInit()) {
        cout << "Unable to initialize GLEW ... exiting" << endl;
        exit(EXIT_FAILURE);
    }

    cout << "Status: Using GLEW " << glewGetString(GLEW_VERSION) << endl;
    cout << "Opengl Version: " << glGetString(GL_VERSION) << endl;
    cout << "Opengl Vendor : " << glGetString(GL_VENDOR) << endl;
    cout << "Opengl Render : " << glGetString(GL_RENDERER) << endl;
    cout << "Opengl Shading Language Version : " << glGetString(GL_SHADING_LANGUAGE_VERSION) << endl;

    glPointSize(3.0);
    glEnable(GL_DEPTH_TEST);
    glEnable(GL_NORMALIZE);
}

/* ********************************************************************* */

void initShaders(void) {
    // Load shaders and use the resulting shader program
    shaderGooch = InitShader("shaders/Gooch.vert", "shaders/Gooch.frag");
    shaderPhong = InitShader("shaders/Phong.vert", "shaders/Phong.frag");
}

/* ********************************************************************* */

static void error_callback(int error, const char* description) {
    cout << "Error: " << description << endl;
}

/* ********************************************************************* */

static void window_size_callback(GLFWwindow* window, int width, int height) {
    winWidth = width;
    winHeight = height;
}

/* ********************************************************************* */

static void key_callback(GLFWwindow* window, int key, int scancode, int action, int mods) {
    if (action == GLFW_PRESS || action == GLFW_REPEAT) {
        switch (key) {
        case GLFW_KEY_ESCAPE :
        case 27:
            glfwSetWindowShouldClose(window, true);
            break;

        case '.':
            glPolygonMode(GL_FRONT_AND_BACK, GL_POINT);
            break;
        case '-':
            glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);
            break;
        case 'F':
        case 'f':
            glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
            break;
        case GLFW_KEY_1:
            uType = !uType;
            if (uType)
                cout << "Texture 1D active" << endl;
            else
                cout << "Texture 1D was deactivated" << endl;
            break;

        case GLFW_KEY_UP:
            camera_rotation.z += sin(rotationAngle);
            break;
        case GLFW_KEY_DOWN:
            camera_rotation.z -= sin(rotationAngle);
            break;
        case GLFW_KEY_RIGHT:
            camera_rotation.y += sin(rotationAngle);
            break;
        case GLFW_KEY_LEFT:
            camera_rotation.y -= sin(rotationAngle);
            break;

        case 'w':
        case 'W':
            light_rotation.y += rotationAngle * 10;
            break;
        case 's':
        case 'S':
            light_rotation.y -= rotationAngle * 10;
            break;
        case 'd':
        case 'D':
            light_rotation.x += rotationAngle * 10;
            light_rotation.z += rotationAngle * 10;
            break;
        case 'a':
        case 'A':
            light_rotation.x -= rotationAngle * 10;
            light_rotation.z -= rotationAngle * 10;
            break;

        case 'o':
        case 'O':
            light.z = light.x = light.y = max(scene_max.x, max(scene_max.y, scene_max.z));
            camera_rotation.x = camera_rotation.y = camera_rotation.z = 0;
            light_rotation.x = light_rotation.y = light_rotation.z = 0;
            fov = 45.0;
            angleY = 0.0f;
            break;

        case ' ':
            pause = !pause;
            break;
        }
    }
}

/* ********************************************************************* */

void mouse_callback(GLFWwindow* window, int button, int action, int mods) {
    if (action == GLFW_PRESS) {
        glfwGetCursorPos(window, &lastX, &lastY);
    } else {
        double xpos, ypos;
        glfwGetCursorPos(window, &xpos, &ypos);

        double xoffset = (xpos - lastX) * 0.2;
        double yoffset = (lastY - ypos) * 0.2;

        // camera_rotation.x += sin(glm::radians(xoffset)) * cos(glm::radians(yoffset));
        camera_rotation.y += sin(glm::radians(xoffset)) * cos(glm::radians(xoffset));
        camera_rotation.z -= cos(glm::radians(xoffset)) * sin(glm::radians(yoffset));
    }
}

/* ********************************************************************* */

void scroll_callback(GLFWwindow* window, double xoffset, double yoffset) {
    if (fov >= 1.0f && fov <= 90.0f)
        fov -= yoffset;
    if (fov <= 1.0f)
        fov = 1.0f;
    if (fov >= 90.0f)
        fov = 90.0f;
}

/* ********************************************************************* */

static GLFWwindow* initGLFW(char* nameWin, int w, int h) {
    glfwSetErrorCallback(error_callback);

    if (!glfwInit())
        exit(EXIT_FAILURE);

    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 2);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 0);

    GLFWwindow* window = glfwCreateWindow(w, h, nameWin, NULL, NULL);
    if (!window) {
        glfwTerminate();
        exit(EXIT_FAILURE);
    }

    glfwSetWindowSizeCallback(window, window_size_callback);
    glfwSetKeyCallback(window, key_callback);
    glfwSetScrollCallback(window, scroll_callback);
    glfwSetMouseButtonCallback(window, mouse_callback);

    glfwMakeContextCurrent(window);
    glfwSwapInterval(1);
    return (window);
}

/* ********************************************************************* */

static void GLFW_MainLoop(GLFWwindow* window) {
    float Max = max(scene_max.x, max(scene_max.y, scene_max.z));
    light.z = light.x = light.y = Max;
    camera.x = camera.y = camera.z = 1.5f*Max;
    pause = true;

    lightPos = glm::vec3(light.x, light.y, light.z);
    cameraPos = glm::vec3(camera.x,  camera.y, camera.z);
    camera_rotation.x = camera_rotation.y = camera_rotation.z = 0;
    light_rotation.x = light_rotation.y = light_rotation.z = 0;

    while (!glfwWindowShouldClose(window)) {
        double now = glfwGetTime();
        double ellapsed = now - lastTime;
        if (ellapsed > 1.0f / 30.0f) {
            lastTime = now;
            display();
            glfwSwapBuffers(window);
        }
        glfwPollEvents();
    }
}

/* ********************************************************************* */

static void initASSIMP() {
    struct aiLogStream stream;
    /* get a handle to the predefined STDOUT log stream and attach
       it to the logging system. It remains active for all further
       calls to aiImportFile(Ex) and aiApplyPostProcessing. */
    stream = aiGetPredefinedLogStream(aiDefaultLogStream_STDOUT, NULL);
    aiAttachLogStream(&stream);
}

/* ********************************************************************* */

static void loadMesh(const char* filename) {
    scene = aiImportFile(filename,
                         aiProcessPreset_TargetRealtime_MaxQuality);
    if (!scene) {
        cout << "## ERROR loading mesh" << endl;
        exit(-1);
    }

    aiMatrix4x4 trafo;
    aiIdentityMatrix4(&trafo);

    aiVector3D min, max;

    scene_min.x = scene_min.y = scene_min.z =  FLT_MAX;
    scene_max.x = scene_max.y = scene_max.z = -FLT_MAX;

    get_bounding_box_for_node(scene->mRootNode, &scene_min, &scene_max, &trafo);

    scene_center.x = (scene_min.x + scene_max.x) / 2.0f;
    scene_center.y = (scene_min.y + scene_max.y) / 2.0f;
    scene_center.z = (scene_min.z + scene_max.z) / 2.0f;

    scene_min.x *= 1.2;
    scene_min.y *= 1.2;
    scene_min.z *= 1.2;
    scene_max.x *= 1.2;
    scene_max.y *= 1.2;
    scene_max.z *= 1.2;

    createAxis();

    if (scene_list == 0)
        createVBOs(scene);

    cout << "Bounding Box: " << " ("
         << scene_min.x << " , " << scene_min.y << " , " << scene_min.z << ") - ("
         << scene_max.x << " , " << scene_max.y << " , " << scene_max.z << ")" << endl;
    cout << "Bounding Box: " << " ("
         << scene_center.x << " , " << scene_center.y << " , " << scene_center.z << ")" << endl;
}

/* ********************************************************************* */

static void determineBaseDirectory() {
    std::string::size_type ss2;
    if (std::string::npos != (ss2 = fileDirectory.find_last_of("\\/")))  {
        fileDirectory.erase(ss2,fileDirectory.length()-ss2);
    } else {
        fileDirectory = "";
    }

    // make sure the directory is terminated properly
    char s;
    if ( fileDirectory.empty() ) {
        fileDirectory = ".";
        fileDirectory += '/';
    } else if ((s = *(fileDirectory.end()-1)) != '\\' && s != '/') {
        fileDirectory += '/';
    }
    //cout << "Import root directory is \'" << fileDirectory << "\'" << endl;
}

/* ********************************************************************* */

int main(int argc, char *argv[]) {
    GLFWwindow* window;
    char meshFilename[] = "bunny.obj";
    window = initGLFW(argv[0], winWidth, winHeight);
    initASSIMP();
    initGL(window);
    initShaders();

    if (argc > 1)
        fileDirectory = argv[1];
    else
        fileDirectory = meshFilename;
    determineBaseDirectory();

    if (argc > 1)
        loadMesh(argv[1]);
    else
        loadMesh(meshFilename);

    GLFW_MainLoop(window);
    glfwDestroyWindow(window);
    glfwTerminate();
    aiReleaseImport(scene);
    aiDetachAllLogStreams();
    exit(EXIT_SUCCESS);
}
