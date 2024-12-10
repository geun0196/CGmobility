#define _SILENCE_TR1_NAMESPACE_DEPRECATION_WARNING   // to remove tr1 warning

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

static bool g_reverseDirection = false; // 카메라 방향 반전을 위한 플래그
static double g_pitch = 0.0; // 상하 회전 각도
static double g_yaw = 0.0;   // 좌우 회전 각도

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

// --------- Geometry

// Macro used to obtain relative offset of a field within a struct
#define FIELD_OFFSET(StructType, field) &(((StructType *)0)->field)

// A vertex with floating point position and normal
struct VertexPN {
    Cvec3f p, n;

    VertexPN() {}
    VertexPN(float x, float y, float z,
        float nx, float ny, float nz)
        : p(x, y, z), n(nx, ny, nz)
    {}

    // Define copy constructor and assignment operator from GenericVertex so we can
    // use make* functions from geometrymaker.h
    VertexPN(const GenericVertex& v) {
        *this = v;
    }

    VertexPN& operator = (const GenericVertex& v) {
        p = v.pos;
        n = v.normal;
        return *this;
    }
};
struct Geometry {

    GlVertexArrayObject vao; // vao is vertex array object, glGenVertexArrays(1, &vao)

    GlBufferObject vbo, ibo;  // vbo is vertex buffer object, ibo is index buffer object, glGenBuffers(1, &vbo), glGenBuffers(1, &ibo)
    int vboLen, iboLen;

    Geometry(VertexPN* vtx, unsigned short* idx, int vboLen, int iboLen) {
        this->vboLen = vboLen;
        this->iboLen = iboLen;

        glBindVertexArray(vao); // Bind VAO before creating VBO and IBO, added by DS on oct6, 2024

        glBindBuffer(GL_ARRAY_BUFFER, vbo);
        glBufferData(GL_ARRAY_BUFFER, sizeof(VertexPN) * vboLen, vtx, GL_STATIC_DRAW);

        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, ibo);
        glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(unsigned short) * iboLen, idx, GL_STATIC_DRAW);
    }

    void draw(const ShaderState& curSS) {

        // bind vao
        glBindVertexArray(vao); // Bind VAO before drawing, added by DS on oct6, 2024
        // Enable the attributes used by our shader
        safe_glEnableVertexAttribArray(curSS.h_aPosition);
        safe_glEnableVertexAttribArray(curSS.h_aNormal);

        // bind vbo
        glBindBuffer(GL_ARRAY_BUFFER, vbo);
        safe_glVertexAttribPointer(curSS.h_aPosition, 3, GL_FLOAT, GL_FALSE, sizeof(VertexPN), FIELD_OFFSET(VertexPN, p));
        safe_glVertexAttribPointer(curSS.h_aNormal, 3, GL_FLOAT, GL_FALSE, sizeof(VertexPN), FIELD_OFFSET(VertexPN, n));

        // bind ibo
        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, ibo);

        // draw!
        glDrawElements(GL_TRIANGLES, iboLen, GL_UNSIGNED_SHORT, 0);

        // Disable the attributes used by our shader
        safe_glDisableVertexAttribArray(curSS.h_aPosition);
        safe_glDisableVertexAttribArray(curSS.h_aNormal);
    }
};


// Vertex buffer and index buffer associated with the ground and cube geometry
static shared_ptr<Geometry> g_ground;

// --------- Scene

//static const Cvec3 g_light1(2.0, 3.0, 14.0), g_light2(-2, -3.0, -5.0);  // define two lights positions in world space
static const Cvec3 g_light1(5.0, 5.0, 6.0), g_light2(-7.0, -2.0, -10.0);  // define two lights positions in world space

static Matrix4 g_skyRbt = Matrix4::makeTranslation(Cvec3(0.0, 0.0, 3.0));
//static Matrix4 g_objectRbt[1] = { Matrix4::makeTranslation(Cvec3(-1,0,-1)) * Matrix4::makeXRotation(22.0) };  // currently only 1 obj is defined
//static Cvec3f g_objectColors[1] = { Cvec3f(1, 0, 0) };

static void initGround() {
    // A x-z plane at y = g_groundY of dimension [-g_groundSize, g_groundSize]^2
    VertexPN vtx[4] = {
      VertexPN(-g_groundSize, g_groundY, -g_groundSize, 0, 1, 0),
      VertexPN(-g_groundSize, g_groundY,  g_groundSize, 0, 1, 0),
      VertexPN(g_groundSize, g_groundY,  g_groundSize, 0, 1, 0),
      VertexPN(g_groundSize, g_groundY, -g_groundSize, 0, 1, 0),
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
    const float axisLength = 1.0f; // 축의 길이
    glLineWidth(5.0); // 선 굵기 설정

    // 모델-뷰 변환 행렬 설정
    Matrix4 MVM = inv(g_skyRbt) * objectRbt; // 카메라 뷰에서 해당 물체로 변환
    Matrix4 NMVM = normalMatrix(MVM); // 법선 벡터 변환을 위한 행렬 생성
    sendModelViewNormalMatrix(curSS, MVM, NMVM);

    // X축 (빨강)
    safe_glUniform3f(curSS.h_uColor, 1.0, 0.0, 0.0); // X축: 빨강
    glBegin(GL_LINES);
    glVertex3f(0.0, 0.0, 0.0); // 시작점
    glVertex3f(axisLength, 0.0, 0.0); // 끝점
    glEnd();

    // Y축 (녹색)
    safe_glUniform3f(curSS.h_uColor, 0.0, 1.0, 0.0); // Y축: 녹색
    glBegin(GL_LINES);
    glVertex3f(0.0, 0.0, 0.0); // 시작점
    glVertex3f(0.0, axisLength, 0.0); // 끝점
    glEnd();

    // Z축 (파랑)
    safe_glUniform3f(curSS.h_uColor, 0.0, 0.0, 1.0); // Z축: 파랑
    glBegin(GL_LINES);
    glVertex3f(0.0, 0.0, 0.0); // 시작점
    glVertex3f(0.0, 0.0, axisLength); // 끝점
    glEnd();

    glLineWidth(1.0); // 선 굵기 초기화
}

static shared_ptr<Geometry> createPlane(float width, float height) {
    // 정점 데이터 정의
    VertexPN vtx[4] = {
        VertexPN(-width / 2, 0.0, -height / 2, 0, 1, 0),  // 왼쪽 아래
        VertexPN(width / 2, 0.0, -height / 2, 0, 1, 0),   // 오른쪽 아래
        VertexPN(width / 2, 0.0, height / 2, 0, 1, 0),    // 오른쪽 위
        VertexPN(-width / 2, 0.0, height / 2, 0, 1, 0),   // 왼쪽 위
    };

    // 삼각형 인덱스 정의
    unsigned short idx[] = { 0, 1, 2, 0, 2, 3 };

    // Geometry 객체 생성 및 반환
    return make_shared<Geometry>(&vtx[0], &idx[0], 4, 6);
}

static void drawPlane(const ShaderState& curSS, shared_ptr<Geometry> plane, const Matrix4& transform, const Cvec3& color) {
    // 모델-뷰 변환 행렬 계산
    Matrix4 MVM = inv(g_skyRbt) * transform;
    Matrix4 NMVM = normalMatrix(MVM);

    // 셰이더로 변환 행렬 및 색상 전달
    sendModelViewNormalMatrix(curSS, MVM, NMVM);
    safe_glUniform3f(curSS.h_uColor, color[0], color[1], color[2]);

    // 플레인 그리기
    plane->draw(curSS);
}

void createStructure(const ShaderState& curSS, const Matrix4& transform, const Cvec3& baseColor, vector<Matrix4>& planeTransforms) {
    // wall_1
    auto leftPlane = createPlane(5.0, 10.0);
    Matrix4 leftTransform = transform * Matrix4::makeTranslation(Cvec3(-2.5, 0.5, -7.5)) * Matrix4::makeZRotation(-90);
    drawPlane(curSS, leftPlane, leftTransform, baseColor);
    //drawAxes(curSS, leftTransform);
    planeTransforms.push_back(leftTransform);

    // wall_2
    auto facePlane = createPlane(5.0, 5.0);
    Matrix4 faceTransform = transform * Matrix4::makeTranslation(Cvec3(0.0, 0.5, -12.5)) * Matrix4::makeXRotation(90);
    drawPlane(curSS, facePlane, faceTransform, baseColor);
    //drawAxes(curSS, faceTransform);
    planeTransforms.push_back(faceTransform);

    // wall_3
    auto rightPlane = createPlane(5.0, 10.0);
    Matrix4 rightTransform = transform * Matrix4::makeTranslation(Cvec3(2.5, 0.5, -7.5)) * Matrix4::makeZRotation(90);
    drawPlane(curSS, rightPlane, rightTransform, baseColor);
    //drawAxes(curSS, rightTransform);
    planeTransforms.push_back(rightTransform);
}

static vector<double> checkViewPositionRelativeToPlanes(const vector<Matrix4>& planeTransforms) {
    // 현재 카메라 위치
    Cvec3 viewPosition(g_skyRbt(0, 3), g_skyRbt(1, 3), g_skyRbt(2, 3));

    vector<double> relativeYPositions; // 결과를 저장할 벡터

    for (size_t i = 0; i < planeTransforms.size(); ++i) {
        // Plane 중심 좌표와 법선 벡터 계산
        Cvec3 planeCenter(planeTransforms[i](0, 3), planeTransforms[i](1, 3), planeTransforms[i](2, 3));
        Cvec3 planeNormal(planeTransforms[i](0, 1), planeTransforms[i](1, 1), planeTransforms[i](2, 1));

        // 카메라 위치와 Plane 간 상대 Y좌표 계산
        double relativeY = dot(viewPosition - planeCenter, planeNormal);

        cout << "Plane " << i + 1 << " - Relative Y Position: " << relativeY << " ("
            << (relativeY > 0 ? "Front" : (relativeY < 0 ? "Back" : "On the Plane")) << ")" << endl;

        relativeYPositions.push_back(relativeY); // 결과 저장
    }
    cout << "==================================================" << endl;
    return relativeYPositions; // Y 좌표 벡터 반환
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
    safe_glUniform3f(curSS.h_uColor, 0.1, 0.95, 0.1); // set color
    g_ground->draw(curSS);

    //// 지붕
    //auto roofPlane = createPlane(6.0, 3.0);
    ////drawPlane(curSS, roofPlane, Matrix4::makeTranslation(Cvec3(0.0, 3.0, -2.0)), Cvec3(0.1, 0.1, 0.1));

    // 모든 Plane의 변환 행렬 저장
    vector<Matrix4> planeTransforms;

    //1동
    createStructure(curSS, Matrix4::makeTranslation(Cvec3(0.0, 0.0, 0.0)), Cvec3(0.99, 0.99, 0.99), planeTransforms);

    //2동
    createStructure(curSS, Matrix4::makeTranslation(Cvec3(0.0, 0.0, 0.0)) * Matrix4::makeYRotation(-90), Cvec3(0.99, 0.99, 0.99), planeTransforms);

    //3동
    createStructure(curSS, Matrix4::makeTranslation(Cvec3(0.0, 0.0, 0.0)) * Matrix4::makeYRotation(90), Cvec3(0.99, 0.99, 0.99), planeTransforms);
    // 3-3번 벽면
    auto Plane_3_3 = createPlane(5.0, 10.0);
    drawPlane(curSS, Plane_3_3, Matrix4::makeTranslation(Cvec3(-2.5, 0.5, 7.5)) * Matrix4::makeZRotation(90), Cvec3(0.99, 0.99, 0.99));
    // 3-1번 벽면
    auto Plane_3_1 = createPlane(5.0, 10.0);
    drawPlane(curSS, Plane_3_1, Matrix4::makeTranslation(Cvec3(2.5, 0.5, 7.5)) * Matrix4::makeZRotation(90), Cvec3(0.99, 0.99, 0.99));

    // 카메라와 Plane의 상대 위치 계산 및 출력
    checkViewPositionRelativeToPlanes(planeTransforms);

    //4동
    //createStructure(curSS, Matrix4::makeTranslation(Cvec3(0.0, 0.0, 0.0)) * Matrix4::makeYRotation(90), Cvec3(0.99, 0.99, 0.99), );

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
    // 마우스 이동의 변화량 계산
    const double dx = x - g_mouseClickX;
    const double dy = g_mouseClickY - y; // OpenGL의 Y축 좌표는 아래에서 위로 증가

    // 좌클릭이 눌려 있는 경우: 카메라 뷰 회전
    if (g_mouseLClickButton) {
        // 기존 상태에서 Yaw와 Pitch를 업데이트
        g_yaw += -dx * 0.2;  // 좌우 회전
        g_pitch += dy * 0.2; // 상하 회전

        // Pitch 제한: -90도 ~ 90도
        if (g_pitch > 89.0) g_pitch = 89.0;
        if (g_pitch < -89.0) g_pitch = -89.0;

        // 새로운 뷰 행렬 계산
        Matrix4 yawRotation = Matrix4::makeYRotation(g_yaw);
        Matrix4 pitchRotation = Matrix4::makeXRotation(g_pitch);

        // 기존의 위치 정보 유지
        Cvec3 currentTranslation = Cvec3(g_skyRbt(0, 3), g_skyRbt(1, 3), g_skyRbt(2, 3));

        // 새로운 변환 행렬 계산
        g_skyRbt = yawRotation * pitchRotation;
        g_skyRbt(0, 3) = currentTranslation[0];
        g_skyRbt(1, 3) = currentTranslation[1];
        g_skyRbt(2, 3) = currentTranslation[2];
    }

    // 마우스 위치 갱신
    g_mouseClickX = x;
    g_mouseClickY = y;

    glutPostRedisplay(); // 화면 갱신
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
    g_mouseClickY = g_windowHeight - y - 1;  // OpenGL 좌표계로 변환

    if (button == GLUT_LEFT_BUTTON && state == GLUT_DOWN) {
        g_mouseLClickButton = true;

        // 클릭 시 카메라 방향 반전
        g_reverseDirection = !g_reverseDirection; // 방향 반전 플래그 토글
        cout << "Direction reversed: " << (g_reverseDirection ? "Enabled" : "Disabled") << endl;
    }
    else if (button == GLUT_LEFT_BUTTON && state == GLUT_UP) {
        g_mouseLClickButton = false;
    }

    glutPostRedisplay(); // 화면 갱신
}


static void keyboard(const unsigned char key, const int x, const int y) {
    const double moveAmount = 0.2;// 이동 크기
    const double rotateAmount = 5.0; // 회전 각도 (도 단위)

    // 9개의 plane과 뷰 사이의 Relative Y Position 확인
    auto canMoveTo = [&](const Matrix4& proposedRbt) -> bool {
        // 임시로 g_skyRbt를 업데이트하여 계산
        Matrix4 originalRbt = g_skyRbt;
        g_skyRbt = proposedRbt;

        vector<Matrix4> planeTransforms;

        // 1동
        createStructure(*g_shaderStates[g_activeShader], Matrix4::makeTranslation(Cvec3(0.0, 0.0, 0.0)), Cvec3(0.99, 0.99, 0.99), planeTransforms);
        // 2동
        createStructure(*g_shaderStates[g_activeShader], Matrix4::makeTranslation(Cvec3(0.0, 0.0, 0.0)) * Matrix4::makeYRotation(-90), Cvec3(0.99, 0.99, 0.99), planeTransforms);
        // 3동
        createStructure(*g_shaderStates[g_activeShader], Matrix4::makeTranslation(Cvec3(0.0, 0.0, 0.0)) * Matrix4::makeYRotation(90), Cvec3(0.99, 0.99, 0.99), planeTransforms);

        vector<double> relativeYPositions = checkViewPositionRelativeToPlanes(planeTransforms);

        g_skyRbt = originalRbt; // 원래 Rbt로 복구

        // 모든 벽에 대한 Y좌표 조건 확인
        // 2번, 5번, 8번 벽의 Y좌표가 양수인지 체크
        if (relativeYPositions[1] <= 2 || relativeYPositions[4] <= 2 || relativeYPositions[7] <= 2) {
            return false; // 2번, 5번, 또는 8번 벽의 Y좌표가 음수라면 이동 불가
        }

        // 추가적인 조건 설정
        if (relativeYPositions[1] < 11) {
            // 2번 벽과의 Y좌표가 11보다 작으면 1번과 3번 벽만 체크
            if (relativeYPositions[0] <= 0.5 || relativeYPositions[2] <= 0.5) {
                return false; // 1번 또는 3번 벽의 Y좌표가 음수라면 이동 불가
            }
        }
        else if (relativeYPositions[1] > 15) {
            // 2번 벽과의 거리가 15보다 크면 1번과 3번 벽만 체크
            if (relativeYPositions[0] <= 0.5 || relativeYPositions[2] <= 0.5) {
                return false; // 1번 또는 3번 벽의 Y좌표가 음수라면 이동 불가
            }
        }
        else {
            // 2번 벽과의 Y좌표가 11 이상 15 이하일 경우 7번과 9번 벽만 체크
            if (relativeYPositions[6] <= 0.1 || relativeYPositions[8] <= 0.1) {
                return false; // 7번 또는 9번 벽의 Y좌표가 음수라면 이동 불가
            }
        }

        return true;
    };

    // 이동 로직
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

    case 'h': // 도움말 출력
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

    case 'f': // 셰이더 변경
        g_activeShader ^= 1;
        break;

    case 'w': // -z축으로 이동 (확대)
        tryMove(Matrix4::makeTranslation(Cvec3(0, 0, -moveAmount)));
        break;

    case 's': // +z축으로 이동 (축소)
        tryMove(Matrix4::makeTranslation(Cvec3(0, 0, moveAmount)));
        break;

    case 'd': // 카메라 좌측 회전
        tryMove(Matrix4::makeTranslation(Cvec3(moveAmount, 0, 0)));
        break;

    case 'a': // 카메라 우측 회전
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
    glutDisplayFunc(display);                               // display rendering callback
    glutReshapeFunc(reshape);                               // window reshape callback
    glutMotionFunc(motion);                                 // mouse movement callback
    glutMouseFunc(mouse);                                   // mouse click callback
    glutKeyboardFunc(keyboard);
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


static void initGeometry() {
    initGround();
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