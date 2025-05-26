#version 330 core

in vec2 fragTexCoord;
out vec4 fragColor;

uniform sampler2D waterTexture;
uniform vec3 cameraPosition;

void main() {
    fragColor = texture(waterTexture, fragTexCoord);
}