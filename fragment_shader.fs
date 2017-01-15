#version 330 core

in vec2 uv;

uniform sampler2D sampler_font;

// uniform sampler1D sampler_meta;
// uniform vec2 resolution;
// uniform float time;

uniform vec3 bgColor;
uniform vec3 fgColor;

out vec3 color;

void main()
{
    vec2 res_font = textureSize(sampler_font, 0);
    vec2 uv2 = uv - vec2(0.5, 0.5)/res_font; // sample center of texel

    float s = smoothstep(0.4, 0.6, texture(sampler_font, uv2).r);
    color = bgColor*s + fgColor*(1.0 - s);

}
