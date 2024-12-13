#define _SILENCE_TR1_NAMESPACE_DEPRECATION_WARNING   // to remove tr1 warning
#define STB_IMAGE_IMPLEMENTATION

#include <vector>
#include <string>
#include <memory>
#include <stdexcept>
#if __GNUG__
#   include <tr1/memory>
#endif

#include <GL/glew.h>
#ifdef __MAC__
#include <GLUT/glut.h>
#else
#include <GL/glut.h>
#endif

#include "cvec.h"
#include "matrix4.h"
#include "geometrymaker.h"
#include "ppm.h"
#include "glsupport2.h"
#include <Windows.h>

using namespace std;      // for string, vector, iostream, and other standard C++ stuff
using namespace tr1;      // for shared_ptr
static const bool g_Gl2Compatible = false;


static const float g_frustMinFov = 60.0;  // A minimal of 60 degree field of view
static float g_frustFovY = g_frustMinFov; // FOV in y direction (updated by updateFrustFovY)

static const float g_frustNear = -0.1;    // near plane
static const float g_frustFar = -50.0;    // far plane
static const float g_groundY = -2.0;      // y coordinate of the ground
static const float g_groundSize = 500.0;   // half the ground length

static int g_windowWidth = 1000;
static int g_windowHeight = 1000;
static bool g_mouseClickDown = false;    // is the mouse button pressed
static bool g_mouseLClickButton, g_mouseRClickButton, g_mouseMClickButton;
static int g_mouseClickX, g_mouseClickY; // coordinates for mouse click event
static int g_activeShader = 0;

static bool g_reverseDirection = false; // ī�޶� ���� ������ ���� �÷���
static double g_pitch = 0.0; // ���� ȸ�� ����
static double g_yaw = 0.0;   // �¿� ȸ�� ����


struct ShaderState {
    GlProgram program;

    // Handles to uniform variables
    GLint h_uLight, h_uLight2;
    GLint h_uProjMatrix;
    GLint h_uModelViewMatrix;
    GLint h_uNormalMatrix;
    GLint h_uColor;

    // Handles to vertex attributes
    GLint h_aPosition;
    GLint h_aNormal;

    ShaderState(const char* vsfn, const char* fsfn) {
        readAndCompileShader(program, vsfn, fsfn);

        const GLuint h = program; // short hand

        // Retrieve handles to uniform variables
        h_uLight = safe_glGetUniformLocation(h, "uLight");
        h_uLight2 = safe_glGetUniformLocation(h, "uLight2");
        h_uProjMatrix = safe_glGetUniformLocation(h, "uProjMatrix");
        h_uModelViewMatrix = safe_glGetUniformLocation(h, "uModelViewMatrix");
        h_uNormalMatrix = safe_glGetUniformLocation(h, "uNormalMatrix");
        h_uColor = safe_glGetUniformLocation(h, "uColor");

        // Retrieve handles to vertex attributes
        h_aPosition = safe_glGetAttribLocation(h, "aPosition");
        h_aNormal = safe_glGetAttribLocation(h, "aNormal");

        if (!g_Gl2Compatible)
            glBindFragDataLocation(h, 0, "fragColor");
        checkGlErrors();
    }

};

static const int g_numShaders = 2;
static const char* const g_shaderFiles[g_numShaders][2] = {
  {"./shaders/basic-gl3.vshader", "./shaders/diffuse-gl3.fshader"},
  {"./shaders/basic-gl3.vshader", "./shaders/solid-gl3.fshader"}
};
static const char* const g_shaderFilesGl2[g_numShaders][2] = {
  {"./shaders/basic-gl2.vshader", "./shaders/diffuse-gl2.fshader"},
  {"./shaders/basic-gl2.vshader", "./shaders/solid-gl2.fshader"}
};
static vector<shared_ptr<ShaderState> > g_shaderStates; // our global shader states

GLuint wallTextureID;

// --------- Geometry

// Macro used to obtain relative offset of a field within a struct
#define FIELD_OFFSET(StructType, field) &(((StructType *)0)->field)


struct VertexPNT {
    Cvec3f p, n;     // ������ ��ġ�� ����
    Cvec2f t;        // �ؽ�ó ��ǥ �߰�

    VertexPNT() {}
    VertexPNT(float x, float y, float z,
        float nx, float ny, float nz,
        float u, float v)
        : p(x, y, z), n(nx, ny, nz), t(u, v) {}

    VertexPNT(const GenericVertex& v) {
        *this = v;
    }

    VertexPNT& operator=(const GenericVertex& v) {
        p = v.pos;
        n = v.normal;
        t = v.tex;
        return *this;
    }
};

struct Geometry {

    GlVertexArrayObject vao;
    GlBufferObject vbo, ibo;
    int vboLen, iboLen;

    // ����: VertexPNT�� �����ϴ� ������ �߰�
    template <typename VertexType>
    Geometry(VertexType* vtx, unsigned short* idx, int vboLen, int iboLen) {
        this->vboLen = vboLen;
        this->iboLen = iboLen;

        glBindVertexArray(vao);

        glBindBuffer(GL_ARRAY_BUFFER, vbo);
        glBufferData(GL_ARRAY_BUFFER, sizeof(VertexType) * vboLen, vtx, GL_STATIC_DRAW);

        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, ibo);
        glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(unsigned short) * iboLen, idx, GL_STATIC_DRAW);
    }

    void draw(const ShaderState& curSS) {
        glBindVertexArray(vao);
        safe_glEnableVertexAttribArray(curSS.h_aPosition);
        safe_glEnableVertexAttribArray(curSS.h_aNormal);

        GLint h_aTexCoord = safe_glGetAttribLocation(curSS.program, "aTexCoord");

        glBindBuffer(GL_ARRAY_BUFFER, vbo);
        safe_glVertexAttribPointer(curSS.h_aPosition, 3, GL_FLOAT, GL_FALSE, sizeof(VertexPNT), FIELD_OFFSET(VertexPNT, p));
        safe_glVertexAttribPointer(curSS.h_aNormal, 3, GL_FLOAT, GL_FALSE, sizeof(VertexPNT), FIELD_OFFSET(VertexPNT, n));

        if (h_aTexCoord != -1) { // �ؽ�ó ��ǥ�� �ִ� ���
            safe_glEnableVertexAttribArray(h_aTexCoord);
            safe_glVertexAttribPointer(h_aTexCoord, 2, GL_FLOAT, GL_FALSE, sizeof(VertexPNT), FIELD_OFFSET(VertexPNT, t));
        }

        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, ibo);
        glDrawElements(GL_TRIANGLES, iboLen, GL_UNSIGNED_SHORT, 0);

        safe_glDisableVertexAttribArray(curSS.h_aPosition);
        safe_glDisableVertexAttribArray(curSS.h_aNormal);
        if (h_aTexCoord != -1) safe_glDisableVertexAttribArray(h_aTexCoord);
    }
};


// Vertex buffer and index buffer associated with the ground and cube geometry
static shared_ptr<Geometry> g_ground;

// --------- Scene

static const Cvec3 g_light1(5.0, 5.0, 6.0), g_light2(-7.0, -2.0, -10.0);  // define two lights positions in world space

static Matrix4 g_skyRbt = Matrix4::makeTranslation(Cvec3(0.0, 0.0, 3.0));


static void initGround() {
    VertexPNT vtx[4] = {
        VertexPNT(-g_groundSize, g_groundY, -g_groundSize, 0, 1, 0, 0, 0),
        VertexPNT(-g_groundSize, g_groundY,  g_groundSize, 0, 1, 0, 0, 1),
        VertexPNT(g_groundSize, g_groundY,  g_groundSize, 0, 1, 0, 1, 1),
        VertexPNT(g_groundSize, g_groundY, -g_groundSize, 0, 1, 0, 1, 0),
    };
    unsigned short idx[] = { 0, 1, 2, 0, 2, 3 };
    g_ground.reset(new Geometry(&vtx[0], &idx[0], 4, 6));
}

// takes a projection matrix and send to the the shaders
static void sendProjectionMatrix(const ShaderState& curSS, const Matrix4& projMatrix) {
    GLfloat glmatrix[16];
    projMatrix.writeToColumnMajorMatrix(glmatrix); // send projection matrix
    safe_glUniformMatrix4fv(curSS.h_uProjMatrix, glmatrix);
}

// takes MVM and its normal matrix to the shaders
static void sendModelViewNormalMatrix(const ShaderState& curSS, const Matrix4& MVM, const Matrix4& NMVM) {
    GLfloat glmatrix[16];
    MVM.writeToColumnMajorMatrix(glmatrix); // send MVM
    safe_glUniformMatrix4fv(curSS.h_uModelViewMatrix, glmatrix);

    NMVM.writeToColumnMajorMatrix(glmatrix); // send NMVM
    safe_glUniformMatrix4fv(curSS.h_uNormalMatrix, glmatrix);
}

// update g_frustFovY from g_frustMinFov, g_windowWidth, and g_windowHeight
static void updateFrustFovY() {
    if (g_windowWidth >= g_windowHeight)
        g_frustFovY = g_frustMinFov;
    else {
        const double RAD_PER_DEG = 0.5 * CS175_PI / 180;
        g_frustFovY = atan2(sin(g_frustMinFov * RAD_PER_DEG) * g_windowHeight / g_windowWidth, cos(g_frustMinFov * RAD_PER_DEG)) / RAD_PER_DEG;
    }
}


static Matrix4 makeProjectionMatrix() {
    return Matrix4::makeProjection(
        g_frustFovY, g_windowWidth / static_cast <double> (g_windowHeight),
        g_frustNear, g_frustFar);
}


static void drawAxes(const ShaderState& curSS, const Matrix4& objectRbt) {
    const float axisLength = 1.0f; // ���� ����
    glLineWidth(5.0); // �� ���� ����

    // ��-�� ��ȯ ��� ����
    Matrix4 MVM = inv(g_skyRbt) * objectRbt; // ī�޶� �信�� �ش� ��ü�� ��ȯ
    Matrix4 NMVM = normalMatrix(MVM); // ���� ���� ��ȯ�� ���� ��� ����
    sendModelViewNormalMatrix(curSS, MVM, NMVM);

    // X�� (����)
    safe_glUniform3f(curSS.h_uColor, 1.0, 0.0, 0.0); // X��: ����
    glBegin(GL_LINES);
    glVertex3f(0.0, 0.0, 0.0); // ������
    glVertex3f(axisLength, 0.0, 0.0); // ����
    glEnd();

    // Y�� (���)
    safe_glUniform3f(curSS.h_uColor, 0.0, 1.0, 0.0); // Y��: ���
    glBegin(GL_LINES);
    glVertex3f(0.0, 0.0, 0.0); // ������
    glVertex3f(0.0, axisLength, 0.0); // ����
    glEnd();

    // Z�� (�Ķ�)
    safe_glUniform3f(curSS.h_uColor, 0.0, 0.0, 1.0); // Z��: �Ķ�
    glBegin(GL_LINES);
    glVertex3f(0.0, 0.0, 0.0); // ������
    glVertex3f(0.0, 0.0, axisLength); // ����
    glEnd();

    glLineWidth(1.0); // �� ���� �ʱ�ȭ
}


static void loadTexture(const char* filename, GLuint& textureID) {
    int width, height;
    vector<PackedPixel> pixData;

    // PPM �̹��� �б�
    ppmRead(filename, width, height, pixData);

    // �ؽ�ó ���� �� ���ε�
    glGenTextures(1, &textureID);
    glBindTexture(GL_TEXTURE_2D, textureID);

    // �ؽ�ó ������ ����
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, width, height, 0, GL_RGB, GL_UNSIGNED_BYTE, &pixData[0]);

    // �ؽ�ó ���͸� �� ���� ����
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
}


static shared_ptr<Geometry> createTexturedPlane(float width, float height) {
    VertexPNT vtx[4] = {
        VertexPNT(-width / 2, 0.0, -height / 2, 0, 1, 0, 0, 0),   // ���� �Ʒ�
        VertexPNT(width / 2, 0.0, -height / 2, 0, 1, 0, 1, 0),    // ������ �Ʒ�
        VertexPNT(width / 2, 0.0, height / 2, 0, 1, 0, 1, 1),     // ������ ��
        VertexPNT(-width / 2, 0.0, height / 2, 0, 1, 0, 0, 1),    // ���� ��
    };

    unsigned short idx[] = { 0, 1, 2, 0, 2, 3 };

    return make_shared<Geometry>(&vtx[0], &idx[0], 4, 6);
}


static void drawPlane(const ShaderState& curSS, shared_ptr<Geometry> plane, const Matrix4& transform) {
    glActiveTexture(GL_TEXTURE0); // �ؽ�ó ���� 0 Ȱ��ȭ
    glBindTexture(GL_TEXTURE_2D, wallTextureID); // �ؽ�ó ���ε�

    GLint uTextureLoc = safe_glGetUniformLocation(curSS.program, "uTexture");
    if (uTextureLoc != -1) {
        glUniform1i(uTextureLoc, 0); // uTexture �������� �ؽ�ó ���� 0 ����
    }

    // MVM ���
    Matrix4 MVM = inv(g_skyRbt) * transform;
    Matrix4 NMVM = normalMatrix(MVM);
    sendModelViewNormalMatrix(curSS, MVM, NMVM);

    // �÷��� �׸���
    plane->draw(curSS);
}




//static void drawPlane(const ShaderState& curSS, shared_ptr<Geometry> plane, const Matrix4& transform) {
//    // �ؽ�ó Ȱ��ȭ �� ���ε�
//    glActiveTexture(GL_TEXTURE0); // �ؽ�ó ���� 0 Ȱ��ȭ
//    glBindTexture(GL_TEXTURE_2D, wallTextureID); // �ؽ�ó ���ε�
//    glUniform1i(safe_glGetUniformLocation(curSS.program, "uTexture"), 0); // ���̴��� uTexture�� �ؽ�ó ���� 0 ����
//
//    // ��-�� ��ȯ ��� ���
//    Matrix4 MVM = inv(g_skyRbt) * transform;
//    Matrix4 NMVM = normalMatrix(MVM);
//
//    // ���̴��� ��ȯ ��� ����
//    sendModelViewNormalMatrix(curSS, MVM, NMVM);
//
//    // �÷��� �׸���
//    plane->draw(curSS);
//}


void createStructure(const ShaderState& curSS, const Matrix4& transform, vector<Matrix4>& planeTransforms) {
    // wall_1
    auto leftPlane = createTexturedPlane(5.0, 10.0);
    Matrix4 leftTransform = transform * Matrix4::makeTranslation(Cvec3(-2.5, 0.5, -7.5)) * Matrix4::makeZRotation(-90);
    drawPlane(curSS, leftPlane, leftTransform);
    planeTransforms.push_back(leftTransform);

    // wall_2
    auto facePlane = createTexturedPlane(5.0, 5.0);
    Matrix4 faceTransform = transform * Matrix4::makeTranslation(Cvec3(0.0, 0.5, -12.5)) * Matrix4::makeXRotation(90);
    drawPlane(curSS, facePlane, faceTransform);
    planeTransforms.push_back(faceTransform);

    // wall_3
    auto rightPlane = createTexturedPlane(5.0, 10.0);
    Matrix4 rightTransform = transform * Matrix4::makeTranslation(Cvec3(2.5, 0.5, -7.5)) * Matrix4::makeZRotation(90);
    drawPlane(curSS, rightPlane, rightTransform);
    planeTransforms.push_back(rightTransform);
}


static vector<double> checkViewPositionRelativeToPlanes(const vector<Matrix4>& planeTransforms) {
    // ���� ī�޶� ��ġ
    Cvec3 viewPosition(g_skyRbt(0, 3), g_skyRbt(1, 3), g_skyRbt(2, 3));

    vector<double> relativeYPositions; // ����� ������ ����

    for (size_t i = 0; i < planeTransforms.size(); ++i) {
        // Plane �߽� ��ǥ�� ���� ���� ���
        Cvec3 planeCenter(planeTransforms[i](0, 3), planeTransforms[i](1, 3), planeTransforms[i](2, 3));
        Cvec3 planeNormal(planeTransforms[i](0, 1), planeTransforms[i](1, 1), planeTransforms[i](2, 1));

        // ī�޶� ��ġ�� Plane �� ��� Y��ǥ ���
        double relativeY = dot(viewPosition - planeCenter, planeNormal);

        //cout << "Plane " << i + 1 << " - Relative Y Position: " << relativeY << " ("
        //    << (relativeY > 0 ? "Front" : (relativeY < 0 ? "Back" : "On the Plane")) << ")" << endl;

        relativeYPositions.push_back(relativeY); // ��� ����
    }
    //cout << "==================================================" << endl;
    return relativeYPositions; // Y ��ǥ ���� ��ȯ
}


static void drawStuff() {
    // short hand for current shader state
    const ShaderState& curSS = *g_shaderStates[g_activeShader];

    // build & send proj. matrix to vshader
    const Matrix4 projmat = makeProjectionMatrix();
    sendProjectionMatrix(curSS, projmat);

    // use the skyRbt as the eyeRbt
    const Matrix4 eyeRbt = g_skyRbt;
    const Matrix4 invEyeRbt = inv(eyeRbt);

    const Cvec3 eyeLight1 = Cvec3(invEyeRbt * Cvec4(g_light1, 1));
    const Cvec3 eyeLight2 = Cvec3(invEyeRbt * Cvec4(g_light2, 1));
    safe_glUniform3f(curSS.h_uLight, eyeLight1[0], eyeLight1[1], eyeLight1[2]);
    safe_glUniform3f(curSS.h_uLight2, eyeLight2[0], eyeLight2[1], eyeLight2[2]);

    // draw ground
    const Matrix4 groundRbt = Matrix4();  // identity
    Matrix4 MVM = invEyeRbt * groundRbt;
    Matrix4 NMVM = normalMatrix(MVM);
    sendModelViewNormalMatrix(curSS, MVM, NMVM);
    safe_glUniform3f(curSS.h_uColor, 0.0, 1.0, 0.0); // set color
    g_ground->draw(curSS);

    // ��� Plane�� ��ȯ ��� ����
    vector<Matrix4> planeTransforms;

    // ������ �ؽ�ó Ȱ��ȭ (���� �ؽ�ó ���)
    glActiveTexture(GL_TEXTURE0);          // Ȱ��ȭ�� �ؽ�ó ����
    glBindTexture(GL_TEXTURE_2D, wallTextureID); // ������ �ؽ�ó ���ε�
    glUniform1i(safe_glGetUniformLocation(curSS.program, "uTexture"), 0); // uTexture�� �ؽ�ó ���� 0 ����
    // ī�޶�� Plane�� ��� ��ġ ��� �� ���
    checkViewPositionRelativeToPlanes(planeTransforms);
 

    // 1��
    createStructure(curSS, Matrix4::makeTranslation(Cvec3(0.0, 0.0, 0.0)), planeTransforms);

    // 2��
    createStructure(curSS, Matrix4::makeTranslation(Cvec3(0.0, 0.0, 0.0)) * Matrix4::makeYRotation(-90), planeTransforms);

    // 3��
    createStructure(curSS, Matrix4::makeTranslation(Cvec3(0.0, 0.0, 0.0)) * Matrix4::makeYRotation(90), planeTransforms);

    // 3-3�� ����
    auto Plane_3_3 = createTexturedPlane(5.0, 10.0);
    drawPlane(curSS, Plane_3_3, Matrix4::makeTranslation(Cvec3(-2.5, 0.5, 7.5)) * Matrix4::makeZRotation(90));

    // 3-1�� ����
    auto Plane_3_1 = createTexturedPlane(5.0, 10.0);
    drawPlane(curSS, Plane_3_1, Matrix4::makeTranslation(Cvec3(2.5, 0.5, 7.5)) * Matrix4::makeZRotation(90));


}

static void display() {
    glUseProgram(g_shaderStates[g_activeShader]->program);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);                   // clear framebuffer color&depth

    drawStuff();

    glutSwapBuffers();    // show the back buffer (where we rendered stuff)

    checkGlErrors();
}


static void reshape(const int w, const int h) {
    g_windowWidth = w;
    g_windowHeight = h;
    glViewport(0, 0, w, h);
    cerr << "Size of window is now " << w << "x" << h << endl;
    updateFrustFovY();
    glutPostRedisplay();
}


static void motion(const int x, const int y) {
    // ���콺 ������ ��ȭ�� ���
    const double dx = x - g_windowWidth / 2;
    const double dy = g_windowHeight / 2 - y; // OpenGL�� Y�� ��ǥ�� ���� ����

    // Yaw�� Pitch ������Ʈ
    g_yaw += -dx * 0.2;  // �¿� ȸ��
    g_pitch += dy * 0.2; // ���� ȸ��

    // Pitch ����: -90�� ~ 90��
    if (g_pitch > 89.0) g_pitch = 89.0;
    if (g_pitch < -89.0) g_pitch = -89.0;

    // ���ο� �� ��� ���
    Matrix4 yawRotation = Matrix4::makeYRotation(g_yaw);
    Matrix4 pitchRotation = Matrix4::makeXRotation(g_pitch);

    // ������ ��ġ ���� ����
    Cvec3 currentTranslation = Cvec3(g_skyRbt(0, 3), g_skyRbt(1, 3), g_skyRbt(2, 3));

    // ���ο� ��ȯ ��� ���
    g_skyRbt = yawRotation * pitchRotation;
    g_skyRbt(0, 3) = currentTranslation[0];
    g_skyRbt(1, 3) = currentTranslation[1];
    g_skyRbt(2, 3) = currentTranslation[2];

    // ���콺 Ŀ���� â �߾����� �̵�
    glutWarpPointer(g_windowWidth / 2, g_windowHeight / 2);

    glutPostRedisplay(); // ȭ�� ����
}


static void mouse_origin(const int button, const int state, const int x, const int y) {
    g_mouseClickX = x;
    g_mouseClickY = g_windowHeight - y - 1;  // conversion from GLUT window-coordinate-system to OpenGL window-coordinate-system

    g_mouseLClickButton |= (button == GLUT_LEFT_BUTTON && state == GLUT_DOWN);
    g_mouseRClickButton |= (button == GLUT_RIGHT_BUTTON && state == GLUT_DOWN);
    g_mouseMClickButton |= (button == GLUT_MIDDLE_BUTTON && state == GLUT_DOWN);

    g_mouseLClickButton &= !(button == GLUT_LEFT_BUTTON && state == GLUT_UP);
    g_mouseRClickButton &= !(button == GLUT_RIGHT_BUTTON && state == GLUT_UP);
    g_mouseMClickButton &= !(button == GLUT_MIDDLE_BUTTON && state == GLUT_UP);

    g_mouseClickDown = g_mouseLClickButton || g_mouseRClickButton || g_mouseMClickButton;
}


static void mouse(const int button, const int state, const int x, const int y) {
    g_mouseClickX = x;
    g_mouseClickY = g_windowHeight - y - 1;  // OpenGL ��ǥ��� ��ȯ

    if (button == GLUT_LEFT_BUTTON && state == GLUT_DOWN) {
        g_mouseLClickButton = true;

        // Ŭ�� �� ī�޶� ���� ����
        g_reverseDirection = !g_reverseDirection; // ���� ���� �÷��� ���
        cout << "Direction reversed: " << (g_reverseDirection ? "Enabled" : "Disabled") << endl;
    }
    else if (button == GLUT_LEFT_BUTTON && state == GLUT_UP) {
        g_mouseLClickButton = false;
    }

    glutPostRedisplay(); // ȭ�� ����
}


static void keyboard(const unsigned char key, const int x, const int y) {
    const double moveAmount = 0.2;// �̵� ũ��
    const double rotateAmount = 5.0; // ȸ�� ���� (�� ����)

    // 9���� plane�� �� ������ Relative Y Position Ȯ��
    auto canMoveTo = [&](const Matrix4& proposedRbt) -> bool {
        // �ӽ÷� g_skyRbt�� ������Ʈ�Ͽ� ���
        Matrix4 originalRbt = g_skyRbt;
        g_skyRbt = proposedRbt;

        vector<Matrix4> planeTransforms;

        // 1��
        createStructure(*g_shaderStates[g_activeShader], Matrix4::makeTranslation(Cvec3(0.0, 0.0, 0.0)), planeTransforms);
        // 2��
        createStructure(*g_shaderStates[g_activeShader], Matrix4::makeTranslation(Cvec3(0.0, 0.0, 0.0)) * Matrix4::makeYRotation(-90), planeTransforms);
        // 3��
        createStructure(*g_shaderStates[g_activeShader], Matrix4::makeTranslation(Cvec3(0.0, 0.0, 0.0)) * Matrix4::makeYRotation(90), planeTransforms);

        vector<double> relativeYPositions = checkViewPositionRelativeToPlanes(planeTransforms);

        g_skyRbt = originalRbt; // ���� Rbt�� ����

        // ��� ���� ���� Y��ǥ ���� Ȯ��
        // 2��, 5��, 8�� ���� Y��ǥ�� ������� üũ
        if (relativeYPositions[1] <= 2 || relativeYPositions[4] <= 2 || relativeYPositions[7] <= 2) {
            return false; // 2��, 5��, �Ǵ� 8�� ���� Y��ǥ�� ������� �̵� �Ұ�
        }

        // �߰����� ���� ����
        if (relativeYPositions[1] < 10.5) {
            // 2�� ������ Y��ǥ�� 10.5���� ������ 1���� 3�� ���� üũ
            if (relativeYPositions[0] <= 0.5 || relativeYPositions[2] <= 0.5) {
                return false; // 1�� �Ǵ� 3�� ���� Y��ǥ�� ������� �̵� �Ұ�
            }
        }
        else if (relativeYPositions[1] > 14.5) {
            // 2�� ������ �Ÿ��� 14.5���� ũ�� 1���� 3�� ���� üũ
            if (relativeYPositions[0] <= 0.5 || relativeYPositions[2] <= 0.5) {
                return false; // 1�� �Ǵ� 3�� ���� Y��ǥ�� ������� �̵� �Ұ�
            }
        }
        else {
            // 2�� ������ Y��ǥ�� 10.5 �̻� 14.5 ������ ��� 7���� 9�� ���� üũ
            if (relativeYPositions[6] <= 0.5 || relativeYPositions[8] <= 0.5) {
                return false; // 7�� �Ǵ� 9�� ���� Y��ǥ�� ������� �̵� �Ұ�
            }
        }

        return true;
    };

    // �̵� ����
    auto tryMove = [&](const Matrix4& movement) {
        Matrix4 proposedRbt = g_skyRbt * movement;
        if (canMoveTo(proposedRbt)) {
            g_skyRbt = proposedRbt;
        }
        else {
            cout << "Movement blocked: Camera does not meet movement conditions." << endl;
        }
    };

    switch (key) {
    case 27: // ESC
        exit(0);

    case 'h': // ���� ���
        cout << " ============== H E L P ==============\n\n"
            << "h\t\thelp menu\n"
            << "s\t\tsave screenshot\n"
            << "f\t\tToggle flat shading on/off.\n"
            << "w\t\tMove camera -z (zoom in)\n"
            << "s\t\tMove camera +z (zoom out)\n"
            << "d\t\tRotate camera head to left\n"
            << "a\t\tRotate camera head to right\n"
            << "drag left mouse to rotate\n" << endl;
        break;

    case 'f': // ���̴� ����
        g_activeShader ^= 1;
        break;

    case 'w': // -z������ �̵� (Ȯ��)
        tryMove(Matrix4::makeTranslation(Cvec3(0, 0, -moveAmount)));
        break;

    case 's': // +z������ �̵� (���)
        tryMove(Matrix4::makeTranslation(Cvec3(0, 0, moveAmount)));
        break;

    case 'd': // ī�޶� ���� ȸ��
        tryMove(Matrix4::makeTranslation(Cvec3(moveAmount, 0, 0)));
        break;

    case 'a': // ī�޶� ���� ȸ��
        tryMove(Matrix4::makeTranslation(Cvec3(-moveAmount, 0, 0)));
        break;
    }

    glutPostRedisplay();
}


static void initGlutState(int argc, char* argv[]) {
    glutInit(&argc, argv);                                  // initialize Glut based on cmd-line args
    glutInitDisplayMode(GLUT_RGBA | GLUT_DOUBLE | GLUT_DEPTH);  //  RGBA pixel channels and double buffering
    glutInitWindowSize(g_windowWidth, g_windowHeight);      // create a window
    glutCreateWindow("Assignment 2 - Basic 3D");            // title the window

    ShowCursor(FALSE);

    glutDisplayFunc(display);                               // display rendering callback
    glutReshapeFunc(reshape);                               // window reshape callback
    glutMouseFunc(mouse);                                   // mouse click callback
    glutKeyboardFunc(keyboard);
    glutPassiveMotionFunc(motion);

}

static void initGLState() {
    glClearColor(128. / 255., 200. / 255., 255. / 255., 0.);
    glClearDepth(0.);
    glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
    glPixelStorei(GL_PACK_ALIGNMENT, 1);
    glCullFace(GL_BACK);
    glEnable(GL_CULL_FACE);
    glEnable(GL_DEPTH_TEST);
    glDepthFunc(GL_GREATER);
    glReadBuffer(GL_BACK);
    glDisable(GL_CULL_FACE);
    if (!g_Gl2Compatible)
        glEnable(GL_FRAMEBUFFER_SRGB);
}

static void initShaders() {
    g_shaderStates.resize(g_numShaders);
    for (int i = 0; i < g_numShaders; ++i) {
        if (g_Gl2Compatible)
            g_shaderStates[i].reset(new ShaderState(g_shaderFilesGl2[i][0], g_shaderFilesGl2[i][1]));
        else
            g_shaderStates[i].reset(new ShaderState(g_shaderFiles[i][0], g_shaderFiles[i][1]));
    }
}


static void initTextures() {
    loadTexture("wall.ppm", wallTextureID);
    glBindTexture(GL_TEXTURE_2D, wallTextureID);
}


static void initGeometry() {
    initGround();
    initTextures(); // �ؽ�ó �ʱ�ȭ �߰�
}

int main(int argc, char* argv[]) {
    try {
        initGlutState(argc, argv);

        glewInit(); // load the OpenGL extensions

        cout << (g_Gl2Compatible ? "Will use OpenGL 2.x / GLSL 1.0" : "Will use OpenGL 3.x / GLSL 1.3") << endl;
        if ((!g_Gl2Compatible) && !GLEW_VERSION_3_0)
            throw runtime_error("Error: card/driver does not support OpenGL Shading Language v1.3");
        else if (g_Gl2Compatible && !GLEW_VERSION_2_0)
            throw runtime_error("Error: card/driver does not support OpenGL Shading Language v1.0");

        initGLState();
        initShaders();
        initGeometry();

        glutMainLoop();
        return 0;
    }
    catch (const runtime_error& e) {
        cout << "Exception caught: " << e.what() << endl;
        return -1;
    }
}