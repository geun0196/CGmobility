#version 130

uniform mat4 uProjMatrix;
uniform mat4 uModelViewMatrix;
uniform mat4 uNormalMatrix;

in vec3 aPosition;
in vec3 aNormal;
in vec2 aTexCoord;     // 추가: 텍스처 좌표 입력

out vec3 vNormal;
out vec3 vPosition;
out vec2 vTexCoord;    // 추가: 텍스처 좌표를 FS로 전달

void main() {
    vNormal = vec3(uNormalMatrix * vec4(aNormal, 0.0));

    // position (eye coordinates)를 Fragment Shader로 전달
    vec4 tPosition = uModelViewMatrix * vec4(aPosition, 1.0);
    vPosition = vec3(tPosition);

    // 텍스처 좌표 전달
    vTexCoord = aTexCoord;

    gl_Position = uProjMatrix * tPosition;
}
