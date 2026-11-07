#include "Shader.hpp"

CShader::~CShader() {
    destroy();
}

void CShader::destroy() {
    glDeleteProgram(program);

    program = 0;
}