#version 330 core

layout(location = 0) in vec3 position;

out vec2 texCoords;
out vec4 clipSpace;
out vec3 worldPosition;

uniform mat4 modelMatrix;
uniform mat4 viewMatrix;
uniform mat4 projectionMatrix;

void main() {
    worldPosition = (modelMatrix * vec4(position, 1.0)).xyz;
    texCoords = vec2(position.x / 2.0 + 0.5, position.z / 2.0 + 0.5);
    clipSpace = projectionMatrix * viewMatrix * modelMatrix * vec4(position, 1.0);
    gl_Position = clipSpace;
}
