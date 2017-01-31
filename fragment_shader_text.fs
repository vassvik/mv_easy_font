#version 330 core

in vec2 uv;
in float color_index;

uniform sampler2D sampler_font;
uniform vec3 colors[9];

out vec3 color;

void main()
{
    vec2 res_font = textureSize(sampler_font, 0);
    vec2 uv2 = uv - 0.0*vec2(0.5, 0.5)/res_font; // sample center of texel

    float s = smoothstep(0.4, 0.6, texture(sampler_font, uv2).r);
    s = texture(sampler_font, uv2).r;
    
    vec3 col = colors[int(color_index+0.5)];
    color = colors[8]*(1.0-s) + col*s;
}
