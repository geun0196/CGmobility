#version 130

uniform vec3 uLight, uLight2, uColor;
uniform sampler2D uTexture;  

in vec3 vNormal;
in vec3 vPosition;
in vec2 vTexCoord;            

out vec4 fragColor;

void main() {
    vec3 normal = normalize(vNormal);
    vec3 tolight = normalize(uLight - vPosition);
    vec3 tolight2 = normalize(uLight2 - vPosition);

    float diffuse = max(0.0, dot(normal, tolight));
    diffuse += max(0.0, dot(normal, tolight2));
    vec3 intensity = uColor * diffuse;

    vec4 texColor = texture(uTexture, vTexCoord);

    fragColor = texColor;
}
