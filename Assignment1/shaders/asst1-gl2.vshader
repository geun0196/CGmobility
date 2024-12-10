uniform float uVertexScale;
uniform float whScale;

attribute vec2 aPosition;
attribute vec3 aColor;
attribute vec2 aTexCoord0, aTexCoord1;

varying vec3 vColor;
varying vec2 vTexCoord0, vTexCoord1;

void main() {
  if (whScale > 1.0) {
    gl_Position = vec4(float(aPosition.x) / whScale, aPosition.y, 0, 1);
  } else {
    gl_Position = vec4(aPosition.x * uVertexScale, float(aPosition.y) * whScale, 0, 1);
  }

  vColor = aColor;
  vTexCoord0 = aTexCoord0;
  vTexCoord1 = aTexCoord1;
}
