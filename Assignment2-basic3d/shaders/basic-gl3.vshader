#version 130

uniform mat4 uProjMatrix;
uniform mat4 uModelViewMatrix;
uniform mat4 uNormalMatrix;

in vec3 aPosition;
in vec3 aNormal;
in vec2 aTexCoord;     // �߰�: �ؽ�ó ��ǥ �Է�

out vec3 vNormal;
out vec3 vPosition;
out vec2 vTexCoord;    // �߰�: �ؽ�ó ��ǥ�� FS�� ����

void main() {
    vNormal = vec3(uNormalMatrix * vec4(aNormal, 0.0));

    // position (eye coordinates)�� Fragment Shader�� ����
    vec4 tPosition = uModelViewMatrix * vec4(aPosition, 1.0);
    vPosition = vec3(tPosition);

    // �ؽ�ó ��ǥ ����
    vTexCoord = aTexCoord;

    gl_Position = uProjMatrix * tPosition;
}
