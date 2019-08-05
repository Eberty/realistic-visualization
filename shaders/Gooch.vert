#version 130

in vec3 aPosition;
in vec3 aNormal;

uniform mat4 uM;
uniform mat4 uN;
uniform mat4 uMVP;

out vec3 vNormal;
out vec3 vPosW;

void main() {
    vPosW = (uM * vec4(aPosition, 1.0)).xyz;
    vNormal = normalize((uN * vec4(aNormal, 1.0)).xyz);
    gl_Position = uMVP * vec4( aPosition, 1.0 );
}
