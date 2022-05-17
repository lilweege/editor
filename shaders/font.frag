#version 330 core

uniform sampler2D font;

in vec2 uv;

void main() {
    //gl_FragColor = vec4(uv.x, uv.y, 0.0, 1.0);
    gl_FragColor = texture(font, 1-uv);
}
