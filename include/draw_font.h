#ifndef DRAW_FONT_H
#define DRAW_FONT_H

#ifdef __cplusplus
extern "C" {
#endif
// utility functions to load shaders
char *readFile2(const char *filename);
GLuint LoadShaders2(const char * vertex_file_path,const char * fragment_file_path);


#define MAX_STRING_LEN 40000
#define NUM_GLYPHS 96
/*
    The font stores all the glyphs sequentially, 
    so the texture is "size of font" high, and
    sum(glyph_widths) wide

    one string can hold up to MAX_STRING_LEN chars, which is hard coded to 40k...
    there's really no need to have multiple vbos for multiple strings, because
    it's so damn fast anyway. 40k is the max used storage, buy usually you'll just
    use the first 100 or so chars

    uses instancing, so that the only thing that need to be updated is the
    position of each glyph in the string (relative to the lower left corner)
    and its index for lookup in the metadata texture

    the metadata texture values are then used to look up in the bitmap texture
*/
typedef struct Font {
    int initialized;
    // font info and data
    int height;                 // font height (with padding), 10 px for easy_font_raw.png
    int width;                  // width of texture
    int width_padded;           // opengl wants textures that are multiple of 4 wide
    
    int glyph_widths[NUM_GLYPHS];  // variable width of each glyph
    int glyph_offsets[NUM_GLYPHS]; // starting point of each glyph

    // opengl stuff
    GLuint vao; 

    GLuint program;
    
    // vbo used for glyph instancing, it's just [0,1]x[0,1]
    GLuint vbo_glyph_pos_instance;
    
    // 2D texture for bitmap data
    GLuint texture_fontdata;

    // 1D texture for glyph metadata, RGBA: (glyph_offset_x, glyph_offset_y, glyph_width, glyph_height)
    // normalized (since it's a texture)
    GLuint texture_metadata;

    GLuint vbo_code_instances;  // vec3: (char_pos_x, char_pos_y, char_index)
    
    float text_glyph_data[4*MAX_STRING_LEN];
    
    int ctr;                    // the number of glyphs to draw (i.e. the length of the string minus newlines)
} Font;


void font_init();
void font_draw(char *str, char *col, float offset[2], float size, float res[2]);
Font *font_get_font();
float *get_colors(int *num_colors);

#ifdef __cplusplus
}
#endif

#endif // DRAW_FONT_H


#ifdef DRAW_FONT_IMPLEMENTATION


#define STB_IMAGE_IMPLEMENTATION
#define STBI_ONLY_PNG
#include "stb_image.h"

#define STB_TRUETYPE_IMPLEMENTATION
#include "stb_truetype.h"

#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "stb_image_write.h"

#define BITMAP_W 512
#define BITMAP_H 256

float colors[9*3] = {
    248/255.0, 248/255.0, 242/255.0, // foreground color
    249/255.0,  38/255.0, 114/255.0, // operator
    174/255.0, 129/255.0, 255/255.0, // numeric
    102/255.0, 217/255.0, 239/255.0, // function
    249/255.0,  38/255.0, 114/255.0, // keyword
    117/255.0, 113/255.0,  94/255.0, // comment
    102/255.0, 217/255.0, 239/255.0, // type
     73/255.0,  72/255.0,  62/255.0, // background color
     39/255.0,  40/255.0,  34/255.0  // clear color
};

float *get_colors(int *num_colors)
{
    *num_colors = sizeof(colors)/sizeof(float)/3;
    return colors;
}


Font font = {0};

void font_string_dimensions(char *str, int *width, int *height)
{
    *width = 0;
    *height = 0;

    int X = 0;
    int Y = 0;

    char *ptr = str;
    while (*ptr) {
        if (*ptr == '\n') {
            if (X > *width)
                *width = X;
            X = 0;
            Y++;
        } else {
            X += font.glyph_widths[*ptr-32];
        }
        ptr++;
    }

    if (X != 0) {
        Y++;
        if (*width == 0)
            *width = X;
    } 

    *height = Y*font.height;
}




Font *font_get_font()
{
    return &font;
}

// @Optimization: this one uses way more memory than needed
unsigned char ttf_buffer[1<<25];
stbtt_packedchar cdata[96];
#define FONT_SIZE 48.0


/*
    Reads easy_font_raw.png and extracts the font info
    i.e. offset and width of each glyph, by parsing the first row 
    containing black dots


    Initialize opengl stuff in the font struct, based on the data read from the file

    Creates one grayscale 2D texture containing the font bitmap data (as in the .png file)
    and one RGBA32F 1D texture containing font metadata, i.e. for some ascii code, where
    does it have to go to find that glyph in the 2D texture, and how much should it get

    Creates the vbo used for updating and drawing text. 
    Initially has a max length of MAX_STRING_LEN, but you're not obliged to use all of it
*/
void font_init()
{
    font.initialized = 1;

    font.program = LoadShaders2( "vertex_shader_text.vs", "fragment_shader_text.fs" );

    FILE *fp = fopen("c:/windows/fonts/consola.ttf", "rb");
    fread(ttf_buffer, 1, 1<<25, fp);
    fclose(fp);
    
    unsigned char bitmap[BITMAP_W*BITMAP_H];
    stbtt_pack_context pc;

    stbtt_PackBegin(&pc, bitmap, BITMAP_W, BITMAP_H, 0, 1, NULL);   
    stbtt_PackSetOversampling(&pc, 1, 1);
    stbtt_PackFontRange(&pc, ttf_buffer, 0, FONT_SIZE, 32, 96, cdata);
    stbtt_PackEnd(&pc);


    for (int i = 0; i < 96; i++) {
        printf("%3d %2c: (%3u, %3u, %3u, %3u), %+6.2f, %+6.2f, %+6.2f, %+6.2f, %f\n", i, i+32, cdata[i].x0,    cdata[i].y0, 
                                                                                               cdata[i].x1,    cdata[i].y1,
                                                                                               cdata[i].xoff,  cdata[i].yoff, 
                                                                                               cdata[i].xoff2, cdata[i].yoff2,
                                                                                               cdata[i].xadvance);
    }

    stbi_write_png("blah.png", BITMAP_W, BITMAP_H, 1, bitmap, 0);

    font.width = BITMAP_W;
    font.width_padded = BITMAP_W;
    font.height = BITMAP_H;

/*
    int x, y, n;
    unsigned char *data = stbi_load("vass_font.png", &x, &y, &n, 0);
    printf("%d %d %d\n", x, y, n);fflush(stdout);
    //return;

    // scan once to count the spacing between the black dots to get the width of each glyph
    // and the offset to reach that glyph
    // NOTE: Ugly, but works. Can this be cleaned up?
    int num = 0;
    int width = 0;
    for (int i = 0; i < x; i++) {
        if (data[n*i+0] == 0 && data[n*i+1] == 0 && data[n*i+2] == 0) {
            if (i != 0) {
                font.glyph_widths[num-1] = width;
                width = 0;
            } 
            font.glyph_offsets[num] = i;

            num++;
        }
        width++;
    }
    font.glyph_widths[num-1] = width;


    // convert the RGB texture into a 1-byte texture, strip the first line (containing width info)
    // add padding, so that texture width is a multiple of 4 (for opengl packing compliance)
    // y-axis is flipped (since input image is in image space, i.e. +Y is downwards)
    font.height = y-1;
    font.width = x;
    font.width_padded = (font.width + 3) & ~0x03;

    unsigned char *font_bitmap = (unsigned char*)malloc(font.width_padded*font.height); 

    for (int j = 0; j < font.height; j++) {
        for (int i = 0; i < font.width; i++) {
            int k1 = (j+1)*font.width + i;
            int k2 = j*font.width_padded + i; // flip y-axis of texture

            int R = data[n*k1+0];
            int G = data[n*k1+1];
            int B = data[n*k1+2];
            
            // red = vertical segment, blue = horisontal segment
            if ((R == 255 || B == 255) && G == 0) {
                font_bitmap[k2] = 0;
            } else {
                font_bitmap[k2] = 255;
            }
        }
    }

    free(data); 
*/


    //-------------------------------------------------------------------------
    glGenVertexArrays(1, &font.vao);
    glBindVertexArray(font.vao);

    //-------------------------------------------------------------------------
    // create 2D texture and upload font bitmap data
    glGenTextures(1, &font.texture_fontdata);
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, font.texture_fontdata);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RED, font.width_padded, font.height, 0, GL_RED, GL_UNSIGNED_BYTE, bitmap);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);



    //-------------------------------------------------------------------------
    // glyph vertex positions, just uv coordinates that will be stretched accordingly 
    // by the glyphs width and height
    float v[] = {0.0, 0.0, 
                 1.0, 0.0, 
                 0.0, 1.0,
                 0.0, 1.0,
                 1.0, 0.0,
                 1.0, 1.0};

    glGenBuffers(1, &font.vbo_glyph_pos_instance);
    glBindBuffer(GL_ARRAY_BUFFER, font.vbo_glyph_pos_instance);
    glBufferData(GL_ARRAY_BUFFER, sizeof(v), v, GL_STATIC_DRAW);
    
    glEnableVertexAttribArray(0);

    glBindBuffer(GL_ARRAY_BUFFER, font.vbo_glyph_pos_instance);
    glVertexAttribPointer(0,2,GL_FLOAT,GL_FALSE,0,(void*)0);
    glVertexAttribDivisor(0, 0);

    //-------------------------------------------------------------------------
    // create 1D texture and upload font metadata
    // used for lookup in the bitmap texture    
    glGenTextures(1, &font.texture_metadata);
    glActiveTexture(GL_TEXTURE2);
    glBindTexture(GL_TEXTURE_2D, font.texture_metadata);

    float *texture_metadata = (float*)malloc(sizeof(float)*8*NUM_GLYPHS);
    
    for (int i = 0; i < NUM_GLYPHS; i++) {
        /*
        // all the glyphs are in a single line, but in principle we can support multiple lines
        texture_metadata[4*i+0] = font.glyph_offsets[i]/(double)font.width_padded;
        texture_metadata[4*i+1] = 0.0;
        texture_metadata[4*i+2] = font.glyph_widths[i]/(double)font.width_padded;
        texture_metadata[4*i+3] = 1.0;
        */
        int k1 = 0*NUM_GLYPHS + i;
        int k2 = 1*NUM_GLYPHS + i;
        texture_metadata[4*k1+0] = cdata[i].x0/(double)font.width_padded;
        texture_metadata[4*k1+1] = cdata[i].y0/(double)font.height;
        texture_metadata[4*k1+2] = (cdata[i].x1-cdata[i].x0)/(double)font.width_padded;
        texture_metadata[4*k1+3] = (cdata[i].y1-cdata[i].y0)/(double)font.height;

        texture_metadata[4*k2+0] = cdata[i].xoff/(double)font.width_padded;
        texture_metadata[4*k2+1] = cdata[i].yoff/(double)font.height;
        texture_metadata[4*k2+2] = cdata[i].xoff2/(double)font.width_padded;
        texture_metadata[4*k2+3] = cdata[i].yoff2/(double)font.height;
    }

    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA32F, NUM_GLYPHS, 2, 0, GL_RGBA, GL_FLOAT, texture_metadata);

    free(texture_metadata);

    //-------------------------------------------------------------------------
    glGenBuffers(1, &font.vbo_code_instances);
    glBindBuffer(GL_ARRAY_BUFFER, font.vbo_code_instances);
    glBufferData(GL_ARRAY_BUFFER, sizeof(float)*4*MAX_STRING_LEN, NULL, GL_DYNAMIC_DRAW);

    glEnableVertexAttribArray(1);

    glBindBuffer(GL_ARRAY_BUFFER, font.vbo_code_instances);
    glVertexAttribPointer(1,4,GL_FLOAT,GL_FALSE,0,(void*)0);
    glVertexAttribDivisor(1, 1);

    //-------------------------------------------------------------------------
    glUseProgram(font.program);
    glUniform1i(glGetUniformLocation(font.program, "sampler_font"), 0);
    glUniform1i(glGetUniformLocation(font.program, "sampler_meta"), 1);

}


void font_draw(char *str, char *col, float offset[2], float size, float res[2]) 
{
    if (font.initialized == 0)
    {
        font_init();
    }

    if (font.ctr > MAX_STRING_LEN) {
        printf("Error: string too long. Returning\n");
        return;
    } 

    float X = 0.0;
    float Y = 0.0;

    int ctr = 0;
    float height = font.height;
    // float width_padded = font.width_padded;

    // int *glyph_offsets = font.glyph_offsets;
    //int *glyph_widths = font.glyph_widths;

    int len = strlen(str);
    for (int i = 0; i < len; i++) {

        if (str[i] == '\n') {
            X = 0.0;
            Y -= FONT_SIZE+10;
            continue;
        }


        int code_base = str[i]-32; // first glyph is ' ', i.e. ascii code 32
        // float offset = glyph_offsets[code_base];
        //float width = glyph_widths[code_base];

        float x1 = (X+0*cdata[code_base].xoff)*size/FONT_SIZE;
        float y1 = (Y-0*cdata[code_base].yoff)*size/FONT_SIZE;

        int ctr1 = 4*ctr;
        font.text_glyph_data[ctr1++] = x1;
        font.text_glyph_data[ctr1++] = y1;
        font.text_glyph_data[ctr1++] = code_base;
        font.text_glyph_data[ctr1++] = col ? col[i] : 0;

        X += cdata[code_base].xadvance;
        ctr++;
    }

    // actual uploading
    glBindBuffer(GL_ARRAY_BUFFER, font.vbo_code_instances);
    glBufferSubData(GL_ARRAY_BUFFER, 0, 4*4*ctr, font.text_glyph_data);
    font.ctr = ctr;


    glUseProgram(font.program);
    glUniform1f(glGetUniformLocation(font.program, "time"), glfwGetTime());
    glUniform1f(glGetUniformLocation(font.program, "size_font"), FONT_SIZE);
    glUniform1f(glGetUniformLocation(font.program, "string_scale"), size);
    glUniform3fv(glGetUniformLocation(font.program, "colors"), 9, colors);
    glUniform2fv(glGetUniformLocation(font.program, "string_offset"), 1, offset);
    glUniform2fv(glGetUniformLocation(font.program, "resolution"), 1, res);


    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, font.texture_fontdata);
    glActiveTexture(GL_TEXTURE1);
    glBindTexture(GL_TEXTURE_2D, font.texture_metadata);


    glBindVertexArray(font.vao);

    glDrawArraysInstanced(GL_TRIANGLES, 0, 6, font.ctr);
}

char *readFile2(const char *filename) {
    // Read content of "filename" and return it as a c-string.
    printf("Reading %s\n", filename);
    FILE *f = fopen(filename, "rb");

    fseek(f, 0, SEEK_END);
    long fsize = ftell(f);
    fseek(f, 0, SEEK_SET);
    printf("Filesize = %d\n", (int)fsize);

    char *string = (char*)malloc(fsize + 1);
    fread(string, fsize, 1, f);
    string[fsize] = '\0';
    fclose(f);

    return string;
}

GLuint LoadShaders2(const char * vertex_file_path,const char * fragment_file_path){
    GLint Result = GL_FALSE;
    int InfoLogLength;

    // Create the Vertex shader
    GLuint VertexShaderID;
    VertexShaderID = glCreateShader(GL_VERTEX_SHADER);
    char *VertexShaderCode   = readFile2(vertex_file_path);

    // Compile Vertex Shader
    printf("Compiling shader : %s\n", vertex_file_path); fflush(stdout);
    glShaderSource(VertexShaderID, 1, (const char**)&VertexShaderCode , NULL);
    glCompileShader(VertexShaderID);

    // Check Vertex Shader
    glGetShaderiv(VertexShaderID, GL_COMPILE_STATUS, &Result);
    glGetShaderiv(VertexShaderID, GL_INFO_LOG_LENGTH, &InfoLogLength);

    if ( InfoLogLength > 0 ){
        char VertexShaderErrorMessage[9999];
        glGetShaderInfoLog(VertexShaderID, InfoLogLength, NULL, VertexShaderErrorMessage);
        printf("%s\n", VertexShaderErrorMessage); fflush(stdout);
    }


    // Create the Fragment shader
    GLuint FragmentShaderID;
    FragmentShaderID = glCreateShader(GL_FRAGMENT_SHADER);
    char *FragmentShaderCode = readFile2(fragment_file_path);

    // Compile Fragment Shader
    printf("Compiling shader : %s\n", fragment_file_path); fflush(stdout);
    glShaderSource(FragmentShaderID, 1, (const char**)&FragmentShaderCode , NULL);
    glCompileShader(FragmentShaderID);

    // Check Fragment Shader
    glGetShaderiv(FragmentShaderID, GL_COMPILE_STATUS, &Result);
    glGetShaderiv(FragmentShaderID, GL_INFO_LOG_LENGTH, &InfoLogLength);
    if ( InfoLogLength > 0 ){
        char FragmentShaderErrorMessage[9999];
        glGetShaderInfoLog(FragmentShaderID, InfoLogLength, NULL, FragmentShaderErrorMessage);
        printf("%s\n", FragmentShaderErrorMessage); fflush(stdout);
    }


    // Create and Link the program
    printf("Linking program\n"); fflush(stdout);
    GLuint ProgramID;
    ProgramID= glCreateProgram();
    glAttachShader(ProgramID, VertexShaderID);
    glAttachShader(ProgramID, FragmentShaderID);
    glLinkProgram(ProgramID);

    // Check the program
    glGetProgramiv(ProgramID, GL_LINK_STATUS, &Result);
    glGetProgramiv(ProgramID, GL_INFO_LOG_LENGTH, &InfoLogLength);

    if ( InfoLogLength > 0 ){
        GLchar ProgramErrorMessage[9999];
        glGetProgramInfoLog(ProgramID, InfoLogLength, NULL, &ProgramErrorMessage[0]);
        printf("%s\n", &ProgramErrorMessage[0]); fflush(stdout);
    }

    glDeleteShader(VertexShaderID);
    glDeleteShader(FragmentShaderID);
    free(FragmentShaderCode);
    free(VertexShaderCode);

    return ProgramID;
}


#endif


