#version 130

precision mediump float;

uniform vec3 uLPos;
uniform vec3 uCamPos;

uniform vec3 objectColor;
uniform vec3 coolColor;
uniform vec3 warmColor;
uniform float diffuseCool;
uniform float diffuseWarm;

uniform sampler1D colorRamp;
uniform int uType;

in vec3 vNormal;
in vec3 vPosW;

void main() {
    vec3 cool = min(coolColor + objectColor * diffuseCool, 1.0);
    vec3 warm = min(warmColor + objectColor * diffuseWarm, 1.0);
    float normalLightColor = 0.5 * (1.0 + dot(normalize(uLPos), normalize(vNormal)));
    vec3 lightColor = mix(cool, warm, normalLightColor);
    
    if (uType > 0)
		gl_FragColor = texture(colorRamp, normalLightColor*0.75);
    else
		gl_FragColor = vec4(min(lightColor, 1.0), 1.0);
}
