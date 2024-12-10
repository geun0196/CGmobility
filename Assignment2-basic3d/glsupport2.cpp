#include <fstream>
#include <vector>
#include <string>
#include <iostream>
#include <stdexcept>

#include "glsupport2.h"

using namespace std;

// Macro to check for OpenGL errors
void GLClearError()
{
    while (glGetError() != GL_NO_ERROR); // GL_NO_ERROR == 0
}

bool GLLogCall(const char* function, const char* file, int line)
{
    while (GLenum error = glGetError())
    {
        std::cout << "[OpenGL Error] (" << error << ") : " << function <<
            " " << file << " in line " << line << std::endl;
        return false;
    }
    return true;
}

void checkGlErrors() {
    GLenum errCode;
    while ((errCode = glGetError()) != GL_NO_ERROR) {
        std::string error = "GL Error: ";

        switch (errCode) {
        case GL_INVALID_ENUM:
            error += "GL_INVALID_ENUM: An unacceptable value is specified for an enumerated argument.";
            break;
        case GL_INVALID_VALUE:
            error += "GL_INVALID_VALUE: A numeric argument is out of range.";
            break;
        case GL_INVALID_OPERATION:
            error += "GL_INVALID_OPERATION: The specified operation is not allowed in the current state.";
            break;
        case GL_INVALID_FRAMEBUFFER_OPERATION:
            error += "GL_INVALID_FRAMEBUFFER_OPERATION: The framebuffer object is not complete.";
            break;
        case GL_OUT_OF_MEMORY:
            error += "GL_OUT_OF_MEMORY: There is not enough memory left to execute the command.";
            break;
        case GL_STACK_UNDERFLOW:
            error += "GL_STACK_UNDERFLOW: An attempt has been made to perform an operation that would cause an internal stack to underflow.";
            break;
        case GL_STACK_OVERFLOW:
            error += "GL_STACK_OVERFLOW: An attempt has been made to perform an operation that would cause an internal stack to overflow.";
            break;
        default:
            error += "Unknown error code: " + std::to_string(errCode);
            break;
        }

        std::cerr << error << std::endl;
        throw std::runtime_error(error);
    }
}

//void checkGlErrors() {
//  const GLenum errCode = glGetError();
//
//  if (errCode != GL_NO_ERROR) {
//    string error("GL Error: ");
//    error += reinterpret_cast<const char*>(gluErrorString(errCode));
//    cerr << error << endl;
//    throw runtime_error(error);
//  }
//}

// Dump text file into a character vector, throws exception on error
static void readTextFile(const char *fn, vector<char>& data) {
  // Sets ios::binary bit to prevent end of line translation, so that the
  // number of bytes we read equals file size
  ifstream ifs(fn, ios::binary);
  if (!ifs)
    throw runtime_error(string("Cannot open file ") + fn);

  // Sets bits to report IO error using exception
  ifs.exceptions(ios::eofbit | ios::failbit | ios::badbit);
  ifs.seekg(0, ios::end);
  size_t len = ifs.tellg();

  data.resize(len);

  ifs.seekg(0, ios::beg);
  ifs.read(&data[0], len);
}

// Print info regarding an GL object
static void printInfoLog(GLuint obj, const string& filename) {
  GLint infologLength = 0;
  GLint charsWritten  = 0;
  glGetObjectParameterivARB(obj, GL_OBJECT_INFO_LOG_LENGTH_ARB, &infologLength);
  if (infologLength > 0) {
    string infoLog(infologLength, ' ');
    glGetInfoLogARB(obj, infologLength, &charsWritten, &infoLog[0]);
    std::cerr << "##### Log [" << filename << "]:\n" << infoLog << endl;
  }
}

void readAndCompileSingleShader(GLuint shaderHandle, const char *fn) {
  vector<char> source;
  readTextFile(fn, source);

  const char *ptrs[] = {&source[0]};
  const GLint lens[] = {source.size()};
  glShaderSource(shaderHandle, 1, ptrs, lens);   // load the shader sources

  glCompileShader(shaderHandle);

  printInfoLog(shaderHandle, fn);

  GLint compiled = 0;
  glGetShaderiv(shaderHandle, GL_COMPILE_STATUS, &compiled);
  if (!compiled)
    throw runtime_error("fails to compile GL shader");
}

void linkShader(GLuint programHandle, GLuint vs, GLuint fs) {
  glAttachShader(programHandle, vs);
  glAttachShader(programHandle, fs);

  glLinkProgram(programHandle);

  glDetachShader(programHandle, vs);
  glDetachShader(programHandle, fs);

  GLint linked = 0;
  glGetProgramiv(programHandle, GL_LINK_STATUS, &linked);
  printInfoLog(programHandle, "linking");

  if (!linked)
    throw runtime_error("fails to link shaders");
}


void readAndCompileShader(GLuint programHandle, const char * vertexShaderFileName, const char * fragmentShaderFileName) {
  GlShader vs(GL_VERTEX_SHADER);
  GlShader fs(GL_FRAGMENT_SHADER);

  readAndCompileSingleShader(vs, vertexShaderFileName);
  readAndCompileSingleShader(fs, fragmentShaderFileName);

  linkShader(programHandle, vs, fs);
}