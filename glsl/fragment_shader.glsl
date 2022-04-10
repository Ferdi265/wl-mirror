#version 100
precision mediump float;

uniform sampler2D uTexture;
uniform bool uInvertColors;
varying vec2 vTexCoord;

void main() {
    vec4 color = texture2D(uTexture, vTexCoord);
    if (uInvertColors) {
        gl_FragColor = vec4(1.0 - color.r, 1.0 - color.g, 1.0 - color.b, color.a);
    } else {
        gl_FragColor = color;
    }
}
