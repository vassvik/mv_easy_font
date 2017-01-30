#version 330 core

layout(location = 0) in vec2 vertexPosition;
layout(location = 1) in vec4 instanceGlyph;


uniform sampler2D sampler_font;
uniform sampler1D sampler_meta;

uniform float size_font;
uniform float string_scale;
uniform vec2 string_offset;

uniform vec2 resolution;

out vec2 uv;
out float color_index;

void main(){
    float res_meta = textureSize(sampler_meta, 0);
    vec2 res_bitmap = textureSize(sampler_font, 0);

    vec4 q = texture(sampler_meta, (instanceGlyph.z + 0.5)/res_meta);

    vec2 glyph_pos_in_texture = q.xy;
    vec2 res_glyph_in_texture = q.zw;
    vec2 glyph_pos_in_space = instanceGlyph.xy + string_offset - vec2(0.0, string_scale);

    vec2 size_glyph = vec2(res_glyph_in_texture.x*res_bitmap.x/size_font, 1.0)*string_scale;

    // optimized/minimized
    vec2 p = vec2(-1.0, 1.0) + 2.0*(vertexPosition * size_glyph + glyph_pos_in_space) / resolution;
    
    // send the correct uv's in the font atlas to the fragment shader
    uv = glyph_pos_in_texture + vertexPosition*res_glyph_in_texture;
    color_index = instanceGlyph.w;

    gl_Position = vec4(p, 0.0, 1.0);
}

