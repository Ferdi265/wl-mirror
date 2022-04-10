#version 100
precision mediump float;

uniform mat3 uTexTransform;
attribute vec2 aPosition;
attribute vec2 aTexCoord;
varying vec2 vTexCoord;

void main() {
    gl_Position = vec4(aPosition, 0.0, 1.0);
    vTexCoord = (uTexTransform * vec3(aTexCoord, 1.0)).xy;
}
