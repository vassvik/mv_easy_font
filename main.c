#include <stdlib.h>

#define STB_IMAGE_IMPLEMENTATION
#define STBI_ONLY_PNG
#include "stb_image.h"

#define GLEW_STATIC
#include <GL/glew.h>
#include <GLFW/glfw3.h>

double rng()
{
    static unsigned int seed = 123;
    seed *= 16807;
    return seed / (double)0x100000000ULL;
}

GLFWwindow *window;
double resx = 1900;
double resy = 1000;

double prevx, prevy;
int clickedButtons = 0;

enum buttonMaps { FIRST_BUTTON=1, SECOND_BUTTON=2, THIRD_BUTTON=4, FOURTH_BUTTON=8, FIFTH_BUTTON=16, NO_BUTTON=0 };
enum modifierMaps { CTRL=2, SHIFT=1, ALT=4, META=8, NO_MODIFIER=0 };

void init_GL();
char *readFile(const char *filename);
GLuint LoadShaders(const char * vertex_file_path,const char * fragment_file_path);

void key_callback(GLFWwindow* win, int key, int scancode, int action, int mods);
void mousebutton_callback(GLFWwindow* win, int button, int action, int mods);
void mousepos_callback(GLFWwindow* win, double xpos, double ypos);
void mousewheel_callback(GLFWwindow* win, double xoffset, double yoffset);
void windowsize_callback(GLFWwindow *win, int width, int height);

#define MAX_STRING_LEN 50000

typedef struct Font {
    int height;
    int width;
    int width_padded;
    unsigned char *font_bitmap;
    
    int num_glyphs;
    int *glyph_widths;
    int *glyph_offsets;

    GLuint vao_ID;
    GLuint vbo_pos;
    GLuint vbo_uvs;
    
    GLuint vbo_glyph_pos_instance;
    
    GLuint texture_fontdata;
    GLuint texture_metadata;
} Font;

typedef struct Font_String {
    GLuint vbo_code_instances;

    char str[MAX_STRING_LEN];
    int caret;
    int ctr;
} Font_String;

typedef struct vec2 {
    float x, y;
} vec2;

typedef struct Color {
    float R, G, B;
} Color;

void font_print_string(char *str, Font *font);

void font_read(char *filename, Font *font);
void font_clean(Font *font);
void font_setup_texture(Font *font);
void font_setup_text(Font_String *font_string);
void font_update_text(Font *font, Font_String *font_string);
void font_draw(Font *font, Font_String *font_string, vec2 offset, Color bgColor, Color fgColor, float size);


GLuint program_text;

typedef struct LinkedListLine {
    char str[512];
    struct LinkedListLine *previous;
    struct LinkedListLine *next;
} LinkedListLine;

LinkedListLine *head_vertex_shader;
LinkedListLine *head_fragment_shader;

LinkedListLine *current_vertex_shader;
LinkedListLine *current_fragment_shader;

int current_pos_vertex_shader = 0;
int current_pos_fragment_shader = 0;

void LinkedListLine_create()
{
    {
        char *shader_vertex = readFile("vertex_shader.vs");

        LinkedListLine *ptr = NULL;
        
        // http://stackoverflow.com/a/17983619
        char * curLine = shader_vertex;
        while(curLine)
        {
            char * nextLine = strchr(curLine, '\n');
            if (nextLine) *nextLine = '\0';  // temporarily terminate the current line
            // printf("curLine=[%s]\n", curLine);

            if (ptr == NULL) {
                ptr = malloc(sizeof(LinkedListLine));
                ptr->previous = NULL;
                ptr->next = NULL;
                head_vertex_shader = ptr;
                current_vertex_shader = ptr;
            } else {
                ptr->next = malloc(sizeof(LinkedListLine));
                ptr->next->previous = ptr;
                ptr->next->next = NULL;
                ptr = ptr->next;
            }
            sprintf(ptr->str, "%s", curLine);

            if (nextLine) *nextLine = '\n';  // then restore newline-char, just to be tidy    
            curLine = nextLine ? (nextLine+1) : NULL;
        }

        // print the linked list of lines, to make sure it's correct
        ptr = head_vertex_shader;
        int i = 0;
        while (ptr != NULL) {
            printf("%4d: %s\n", i, ptr->str);
            ptr = ptr->next;
            i++;
        }

        free(shader_vertex);
    }

    {
        char *shader_fragment = readFile("fragment_shader.fs");

        LinkedListLine *ptr = NULL;
        
        // http://stackoverflow.com/a/17983619
        char * curLine = shader_fragment;
        while(curLine)
        {
            char * nextLine = strchr(curLine, '\n');
            if (nextLine) *nextLine = '\0';  // temporarily terminate the current line
            // printf("curLine=[%s]\n", curLine);

            if (ptr == NULL) {
                ptr = malloc(sizeof(LinkedListLine));
                ptr->previous = NULL;
                ptr->next = NULL;
                head_fragment_shader = ptr;
                current_fragment_shader = ptr;
            } else {
                ptr->next = malloc(sizeof(LinkedListLine));
                ptr->next->previous = ptr;
                ptr->next->next = NULL;
                ptr = ptr->next;
            }
            sprintf(ptr->str, "%s", curLine);

            if (nextLine) *nextLine = '\n';  // then restore newline-char, just to be tidy    
            curLine = nextLine ? (nextLine+1) : NULL;
        }

        // print the linked list of lines, to make sure it's correct
        ptr = head_fragment_shader;
        int i = 0;
        while (ptr != NULL) {
            printf("%4d: %s\n", i, ptr->str);
            ptr = ptr->next;
            i++;
        }

        free(shader_fragment);
    }
}

void LinkedListLine_delete()
{
    LinkedListLine *ptr = head_vertex_shader;
    while (ptr) {
        LinkedListLine *old = ptr;
        ptr = ptr->next;
        free(old);
    }
    head_vertex_shader = NULL;

    ptr = head_fragment_shader;
    while (ptr) {
        LinkedListLine *old = ptr;
        ptr = ptr->next;
        free(old);
    }
    head_fragment_shader = NULL;
}

void LinkedListLine_update_text(LinkedListLine *head, Font *font, Font_String *font_string)
{
    static float text_glyph_data[3*MAX_STRING_LEN];

    float X = 0.0;
    float Y = 0.0;

    int ctr = 0;
    float height = font->height;
    float width_padded = font->width_padded;

    int *glyph_offsets = font->glyph_offsets;
    int *glyph_widths = font->glyph_widths;

    LinkedListLine *ptr = head;
    while (ptr != NULL) {
        int len = strlen(ptr->str);
        for (int i = 0; i < len; i++) {
            int code_base = ptr->str[i]-32;

            text_glyph_data[3*ctr+0] = X;
            text_glyph_data[3*ctr+1] = Y;
            text_glyph_data[3*ctr+2] = code_base;

            X += glyph_widths[code_base];
            ctr++;
        }

        text_glyph_data[3*ctr+0] = X;
        text_glyph_data[3*ctr+1] = Y;
        text_glyph_data[3*ctr+2] = 0;

        X += glyph_widths[0];
        ctr++;

        X = 0.0;
        Y -= height;
        ptr = ptr->next;
    }

    glBindBuffer(GL_ARRAY_BUFFER, font_string->vbo_code_instances);
    glBufferSubData(GL_ARRAY_BUFFER, 0, 4*3*ctr, text_glyph_data);

    font_string->ctr = ctr;
}

int main() 
{
    LinkedListLine_create();

    init_GL();

    program_text = LoadShaders( "vertex_shader.vs", "fragment_shader.fs" );

    Font font = {0};
    font_read("easy_font_raw.png", &font);
    font_print_string("wi asd abd i w", &font); // for testing if texture is loaded correctly

    Font_String font_string = {0};
    font_setup_text(&font_string);

    double t1 = glfwGetTime();
    double avg_dt = 1.0/60;
    double alpha = 0.01;

    Color fgColor = {248/255.0, 248/255.0, 242/225.0};
    Color bgColor = {68/255.0, 71/255.0, 90/225.0};

    vec2 offset_left = {-0.995, 0.95};
    vec2 offset_right = {-0.1, 0.95};

    while ( !glfwWindowShouldClose(window)) { 
        // calculate fps
        double t2 = glfwGetTime();
        double dt = t2 - t1;
        avg_dt = alpha*dt + (1.0 - alpha)*avg_dt;
        t1 = t2;
        

        glfwPollEvents();
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        LinkedListLine_update_text(head_fragment_shader, &font, &font_string);
        font_draw(&font, &font_string, offset_left, bgColor, fgColor, 2.0);

        LinkedListLine_update_text(head_vertex_shader, &font, &font_string);
        font_draw(&font, &font_string, offset_right, bgColor, fgColor, 2.0);

        char str[128];
        sprintf(str, "fps = %f\n", 1.0/avg_dt);
        glfwSetWindowTitle(window, str);

        glfwSwapBuffers(window);
    }

    glfwTerminate();

    font_clean(&font);

    return 0;
}

/*
    Prints a string in ASCII bitmap form to stdout 

    str:     C-string containing the string to be rendered
    offsets: the offsets to the start of each glyph along the x-axis in the texture
    widths:  the width of each glyph
    length:  the horisontal size of the texture
    height:  the vertical size of the texture
    data:    the individual pixels of the texture

    easy_font_raw.png contains 96 glyphs with variable width and contains 453x11 24 bit pixels
*/
void font_print_string(char *str, Font *font)
{
    // prints the first line of each glyph sequentially, then the second line, etc.
    for (int j = 0; j < font->height; j++) {
        printf("%d: ", j);
        char *code = str;
        while(*code) {
            int code_base = (*code) - 32;
            int offset = font->glyph_offsets[code_base];
            int width = font->glyph_widths[code_base];
            
            for (int i = 0; i < width; i++) {
                int k = (font->height - j-1)*font->width_padded + (offset+i);
                                
                if (font->font_bitmap[k+0] == 0) {
                    putchar('O');
                } else {
                    putchar(' ');
                }
                //putchar(' ');
            }
            code++;
        }
        putchar('\n');
    }
}

/*

*/
void font_read(char *filename, Font *font)
{
    int x, y, n;
    unsigned char *data = stbi_load("easy_font_raw.png", &x, &y, &n, 0);

    // scan the first row of pixels once to find the number of glyphs by counting the black dots
    font->num_glyphs = 0;
    for (int i = 0; i < x; i++) {
        if (data[3*i+0] == 0 && data[3*i+1] == 0 && data[3*i+2] == 0) {
            font->num_glyphs++;
        }
    }

    font->glyph_widths  = malloc(sizeof(int)*font->num_glyphs);
    font->glyph_offsets = malloc(sizeof(int)*font->num_glyphs);

    // scan once more, but this time also count the spacing between the black dots to get the width of each glyph
    // and the offset to reach that glyph
    // NOTE: Ugly, but works. Can this be cleaned up?
    int num = 0;
    int width = 0;
    for (int i = 0; i < x; i++) {
        if (data[3*i+0] == 0 && data[3*i+1] == 0 && data[3*i+2] == 0) {
            if (i != 0) {
                font->glyph_widths[num-1] = width;
                width = 0;
            } 
            font->glyph_offsets[num] = i;

            num++;
        }
        width++;
    }
    font->glyph_widths[num-1] = width;


    // convert the RGB texture into a 1-byte texture, strip the first line (containing width info)
    // add padding, so that texture width is a multiple of 4 (for opengl packing compliance)
    // y-axis is flipped (since input image is in image space, i.e. +Y is downwards)
    font->height = y-1;
    font->width = x;
    font->width_padded = (font->width + 3) & ~0x03;

    font->font_bitmap = malloc(font->width_padded*font->height);

    for (int j = 0; j < font->height; j++) {
        for (int i = 0; i < font->width; i++) {
            int k1 = (j+1)*font->width + i;
            int k2 = (font->height - j - 1)*font->width_padded + i; // flip y-axis of texture

            int R = data[3*k1+0];
            int G = data[3*k1+1];
            int B = data[3*k1+2];
            
            // red = vertical segment, blue = horisontal segment
            if ((R == 255 || B == 255) && G == 0) {
                font->font_bitmap[k2] = 0;
            } else {
                font->font_bitmap[k2] = 255;
            }
        }
    }

    free(data); 

    font_setup_texture(font);
}

void font_clean(Font *font)
{
    if (font == NULL)
        return;

    if (font->font_bitmap)
        free(font->font_bitmap);

    if (font->glyph_widths)
        free(font->glyph_widths);

    if (font->glyph_offsets)
        free(font->glyph_offsets);
}



void font_setup_texture(Font *font)
{
    float vertex_pos_data[] = {
        -1.0, -1.0, 0.0,
         1.0, -1.0, 0.0,
        -1.0,  1.0, 0.0,
        -1.0,  1.0, 0.0,
         1.0, -1.0, 0.0,
         1.0,  1.0, 0.0
    };

    float x = font->width/(double)font->width_padded;

    float vertex_uv_data[] = {
        0.0, 0.0,
        x,   0.0,
        0.0, 1.0,
        0.0, 1.0,
        x,   0.0,
        x,   1.0
    };


    glGenVertexArrays(1, &font->vao_ID);
    glBindVertexArray(font->vao_ID);

    glGenBuffers(1, &font->vbo_pos);
    glBindBuffer(GL_ARRAY_BUFFER, font->vbo_pos);
    glBufferData(GL_ARRAY_BUFFER, sizeof(vertex_pos_data), vertex_pos_data, GL_STATIC_DRAW);

    glGenBuffers(1, &font->vbo_uvs);
    glBindBuffer(GL_ARRAY_BUFFER, font->vbo_uvs);
    glBufferData(GL_ARRAY_BUFFER, sizeof(vertex_uv_data), vertex_uv_data, GL_STATIC_DRAW);


    glGenTextures(1, &font->texture_fontdata);
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, font->texture_fontdata);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RED, font->width_padded, font->height, 0, GL_RED, GL_UNSIGNED_BYTE, font->font_bitmap);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);



    float v[] = {0.0, 0.0, 
                 1.0, 0.0, 
                 0.0, 1.0,
                 0.0, 1.0,
                 1.0, 0.0,
                 1.0, 1.0};

    glGenBuffers(1, &font->vbo_glyph_pos_instance);
    glBindBuffer(GL_ARRAY_BUFFER, font->vbo_glyph_pos_instance);
    glBufferData(GL_ARRAY_BUFFER, sizeof(v), v, GL_STATIC_DRAW);
    

    glGenTextures(1, &font->texture_metadata);
    glActiveTexture(GL_TEXTURE1);
    glBindTexture(GL_TEXTURE_1D, font->texture_metadata);

    float *texture_metadata = malloc(sizeof(float)*4*font->num_glyphs);
    
    for (int i = 0; i < font->num_glyphs; i++) {
        texture_metadata[4*i+0] = font->glyph_offsets[i]/(double)font->width_padded;
        texture_metadata[4*i+1] = 0.0;
        texture_metadata[4*i+2] = font->glyph_widths[i]/(double)font->width_padded;
        texture_metadata[4*i+3] = 1.0;
    }


    glTexParameteri(GL_TEXTURE_1D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_1D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_1D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexImage1D(GL_TEXTURE_1D, 0, GL_RGBA32F, font->num_glyphs, 0, GL_RGBA, GL_FLOAT, texture_metadata);

    free(texture_metadata);
}

void font_setup_text(Font_String *font_string)
{
    glGenBuffers(1, &font_string->vbo_code_instances);
    glBindBuffer(GL_ARRAY_BUFFER, font_string->vbo_code_instances);
    glBufferData(GL_ARRAY_BUFFER, sizeof(float)*4*MAX_STRING_LEN, NULL, GL_DYNAMIC_DRAW);
}

void font_update_text(Font *font, Font_String *font_string)
{
    static float text_glyph_data[3*MAX_STRING_LEN];

    float X = 0.0;
    float Y = 0.0;

    int ctr = 0;
    float height = font->height;
    float width_padded = font->width_padded;

    int *glyph_offsets = font->glyph_offsets;
    int *glyph_widths = font->glyph_widths;

    int len = strlen(font_string->str);
    for (int i = 0; i < len; i++) {

        if (font_string->str[i] == '\n') {
            X = 0.0;
            Y -= height;
            continue;
        }

        int code_base = font_string->str[i]-32;
        float offset = glyph_offsets[code_base];
        float width = glyph_widths[code_base];

        float x1 = X;
        float y1 = Y;

        int ctr1 = 3*ctr;
        text_glyph_data[ctr1++] = x1;
        text_glyph_data[ctr1++] = y1;
        text_glyph_data[ctr1++] = code_base;;


        X += width;
        ctr++;
    }

    glBindBuffer(GL_ARRAY_BUFFER, font_string->vbo_code_instances);
    glBufferSubData(GL_ARRAY_BUFFER, 0, 4*3*ctr, text_glyph_data);

    font_string->ctr = ctr;
}

void font_draw(Font *font, Font_String *font_string, vec2 offset, Color bgColor, Color fgColor, float size) 
{
    glUseProgram(program_text);
    glUniform2f(glGetUniformLocation(program_text, "resolution"), resx, resy);
    glUniform1f(glGetUniformLocation(program_text, "time"), glfwGetTime());
    glUniform2f(glGetUniformLocation(program_text, "string_offset"), offset.x, offset.y);
    glUniform1i(glGetUniformLocation(program_text, "sampler_font"), 0);
    glUniform1i(glGetUniformLocation(program_text, "sampler_meta"), 1);
    glUniform3f(glGetUniformLocation(program_text, "bgColor"), bgColor.R, bgColor.G, bgColor.B);
    glUniform3f(glGetUniformLocation(program_text, "fgColor"), fgColor.R, fgColor.G, fgColor.B);
    glUniform2f(glGetUniformLocation(program_text, "string_size"), size, size);

    int X = 0;
    int Y = 0;

    for (int i = 0; i < current_pos_vertex_shader; i++) {
        X += font->glyph_widths[current_vertex_shader->str[i]-32];
    }

    LinkedListLine *ptr = current_vertex_shader;
    while (ptr->previous) {
        ptr = ptr->previous;
        Y -= font->height;
    }
    printf("%d %d\n", X, Y);
    glUniform2f(glGetUniformLocation(program_text, "caretPosition"), X, Y);
    

    glBindVertexArray(font->vao_ID);

    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, font->texture_fontdata);
    glActiveTexture(GL_TEXTURE1);
    glBindTexture(GL_TEXTURE_1D, font->texture_metadata);

    glEnableVertexAttribArray(0);
    glEnableVertexAttribArray(1);

    glBindBuffer(GL_ARRAY_BUFFER, font->vbo_glyph_pos_instance);
    glVertexAttribPointer(0,2,GL_FLOAT,GL_FALSE,0,(void*)0);
    glVertexAttribDivisor(0, 0);
    glBindBuffer(GL_ARRAY_BUFFER, font_string->vbo_code_instances);
    glVertexAttribPointer(1,3,GL_FLOAT,GL_FALSE,0,(void*)0);
    glVertexAttribDivisor(1, 1);

    glDrawArraysInstanced(GL_TRIANGLES, 0, 6, font_string->ctr);
    
    glDisableVertexAttribArray(1);
    glDisableVertexAttribArray(0);
}


void init_GL()
{

    // openGL stuff
    printf("Initializing OpenGL/GLFW\n"); 
    if (!glfwInit()) {
        printf("Could not initialize\n");
        exit(-1);
    }
    glfwWindowHint(GLFW_SAMPLES, 4);    // samples, for antialiasing
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3); // shader version should match these
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE); // do not use deprecated functionality

    window = glfwCreateWindow(resx, resy, "GLSL template", 0, 0);
    if (!window) {
        printf("Could not open glfw window\n");
        glfwTerminate();
        exit(-2);
    }
    glfwMakeContextCurrent(window); 


    glewExperimental = 1; // Needed for core profile
    if (glewInit() != GLEW_OK) {
        fprintf(stderr, "Failed to initialize GLEW\n");
        exit(-3);
    }

    glfwSetKeyCallback(window, key_callback);
    glfwSetMouseButtonCallback(window, mousebutton_callback);
    glfwSetScrollCallback(window, mousewheel_callback);
    glfwSetCursorPosCallback(window, mousepos_callback);
    glfwSetWindowSizeCallback(window, windowsize_callback);

    glfwSwapInterval(0);
    glClearColor(40/255.0, 42/255.0, 54/225.0, 1.0f);
}

// Callback function called every time the window size change
// Adjusts the camera width and heigh so that the scale stays the same
// Resets projection matrix
void windowsize_callback(GLFWwindow* win, int width, int height) {

    (void)win;

    resx = width;
    resy = height;

    glViewport(0, 0, resx, resy);
}

// Callback function called every time a keyboard key is pressed, released or held down
void key_callback(GLFWwindow* win, int key, int scancode, int action, int mods) {
    printf("key = %d, scancode = %d, action = %d, mods = %d\n", key, scancode, action, mods); fflush(stdout);

    // Close window if escape is released
    if (key == GLFW_KEY_ESCAPE && action == GLFW_RELEASE) {
        glfwSetWindowShouldClose(win, GL_TRUE);
    }

    int len = strlen(current_vertex_shader->str);
    int caret = current_pos_vertex_shader;
    if (caret > len)
        caret = len;
    if (key >= 32 && key <= 32 + 96 && action) {
        for (int i = len-1; i >= caret; i--) {
            current_vertex_shader->str[i+1] = current_vertex_shader->str[i];
        }
        current_vertex_shader->str[caret] = key;
        current_vertex_shader->str[len+1] = '\0';
        current_pos_vertex_shader = caret+1;
    } else if (key == GLFW_KEY_BACKSPACE && action) {
        if (caret > 0) {
            for (int i = caret-1; i < len-1; i++) {
                current_vertex_shader->str[i] = current_vertex_shader->str[i+1];
            }
            current_vertex_shader->str[len-1] = '\0';
            current_pos_vertex_shader = caret-1;
        }
    } else if (key == GLFW_KEY_DELETE && action) {
        if (caret < len) {
            for (int i = caret; i < len-1; i++) {
                current_vertex_shader->str[i] = current_vertex_shader->str[i+1];
            }
            current_vertex_shader->str[len-1] = '\0';
        }
    } else if (key == GLFW_KEY_RIGHT && action) {
        if (caret < len)
            current_pos_vertex_shader = caret + 1;
    } else if (key == GLFW_KEY_LEFT && action) {
        if (caret > 0)
            current_pos_vertex_shader = caret - 1;
    } else if (key == GLFW_KEY_UP && action) {
        if (current_vertex_shader->previous) {
            current_vertex_shader = current_vertex_shader->previous;
            //current_pos_vertex_shader = caret;
        }
    } else if (key == GLFW_KEY_DOWN && action) {
        if (current_vertex_shader->next) {
            current_vertex_shader = current_vertex_shader->next;
            //current_pos_vertex_shader = caret;
        }
    }
}

// Callback function called every time a mouse button pressed or released
void mousebutton_callback(GLFWwindow* win, int button, int action, int mods) {
    // get current cursor position, convert to world coordinates
    glfwGetCursorPos(win, &prevx, &prevy);
    double xend = prevx;
    double yend = prevy;

    printf("button = %d, action = %d, mods = %d at (%f %f)\n", button, action, mods, xend, yend); fflush(stdout);

    // To track the state of buttons outside this function
    if (action == 1)
        clickedButtons |= (1 << button);
    else
        clickedButtons &= ~(1 << button);


    // Test each button
    if (clickedButtons&FIRST_BUTTON) {
        
    } else if (clickedButtons&SECOND_BUTTON) {

    } else if (clickedButtons&THIRD_BUTTON) {

    } else if (clickedButtons&FOURTH_BUTTON) {

    } else if (clickedButtons&FIFTH_BUTTON) {

    }
}

// Callback function called every time a the mouse is moved
void mousepos_callback(GLFWwindow* win, double xpos, double ypos) {
    (void)win;

    if (clickedButtons&FIRST_BUTTON) {
        prevx = xpos;
        prevy = ypos;
    } else if (clickedButtons&SECOND_BUTTON) {

    } else if (clickedButtons&THIRD_BUTTON) {

    } else if (clickedButtons&FOURTH_BUTTON) {

    } else if (clickedButtons&FIFTH_BUTTON) {

    }
}
void mousewheel_callback(GLFWwindow* win, double xoffset, double yoffset) {
    (void)xoffset;

    double zoomFactor = pow(0.95,yoffset);

    glfwGetCursorPos(win, &prevx, &prevy);
}

char *readFile(const char *filename) {
    // Read content of "filename" and return it as a c-string.
    printf("Reading %s\n", filename);
    FILE *f = fopen(filename, "rb");

    fseek(f, 0, SEEK_END);
    long fsize = ftell(f);
    fseek(f, 0, SEEK_SET);
    printf("Filesize = %d\n", (int)fsize);

    char *string = (char*)malloc(fsize + 1);
    int ret = fread(string, fsize, 1, f);
    string[fsize] = '\0';
    fclose(f);

    (void)ret;

    return string;
}

GLuint LoadShaders(const char * vertex_file_path,const char * fragment_file_path){
    GLint Result = GL_FALSE;
    int InfoLogLength;

    // Create the Vertex shader
    GLuint VertexShaderID;
    VertexShaderID = glCreateShader(GL_VERTEX_SHADER);
    char *VertexShaderCode   = readFile(vertex_file_path);

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
    char *FragmentShaderCode = readFile(fragment_file_path);

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
