#version 330 core

in vec2 uv;
in float color_index;

uniform sampler2D sampler_font;
uniform sampler1D sampler_colors;
uniform float num_colors;

uniform float time;

out vec4 color;

void main()
{
    vec3 col = texture(sampler_colors, (color_index+0.5 + time)/num_colors).rgb;
    float s = texture(sampler_font, uv).r;
    color = vec4(col, s);
}
