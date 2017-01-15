#version 330 core

layout(location = 0) in vec2 vertexPosition;
layout(location = 1) in vec3 instanceGlyph;

uniform vec2 string_offset;
uniform vec2 string_size;

uniform sampler2D sampler_font;
uniform sampler1D sampler_meta;

uniform vec2 resolution;

out vec2 uv;
out float code;

void main(){
    float res_meta = textureSize(sampler_meta, 0);
    vec2  res_font = textureSize(sampler_font, 0);

    vec4 q = texture(sampler_meta, (instanceGlyph.z + 0.5)/res_meta);

    vec2 glyph_pos = q.xy;
    vec2 res_glyph = q.zw;

    float ratio  = resolution.x/resolution.y;
    float ratio2 = (res_glyph.x*res_font.x)/(res_glyph.y*res_font.y);

    vec2 p = vertexPosition;           // modelspace
    p *= vec2(ratio2, 1.0)*res_font.y; // to texture space, each glyph is res_font.y tall and proportionally wide
    p += instanceGlyph.xy;             // displace each glyph so that it is in the right place in texture space
    p *= 1.0/res_font.y;               // make each glyph 1.0 tall
    p *= 2.0/resolution.y;             // make each glyph 1 pixel tall (factor 2 due to NDC being -1 to +1)
    p *= 1.0*res_font.y;               // each glyph is now res_font.y pixels tall
    p *= string_size;                  // scale font if nescessary
    p *= vec2(1.0/ratio, 1.0);         // correct aspect ratio
    p += string_offset;                // move the whole string

    // send the correct uv's in the font atlas to the fragment shader
    uv = glyph_pos + vertexPosition*res_glyph;

    gl_Position = vec4(p, 0.0, 1.0);
}

