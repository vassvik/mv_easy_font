#version 330 core

in vec2 uv;
in float color_index;

uniform sampler2D sampler_font;
uniform vec3 colors[9];          // syntax highlighting

out vec3 color;

void main()
{
    vec3 col = colors[int(color_index+0.5)];
    
    float s = texture(sampler_font, uv).r;
    color = colors[8]*(1.0-s) + col*s;
    //@TODO: Add alpha blending
}
