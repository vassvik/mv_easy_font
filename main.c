#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

#include <glad/glad.h>
#include <glad/glad.c>
#include <GLFW/glfw3.h>

// incldue stb_rect_pack.h to use effective packing
// #include "stb_rect_pack.h"

// define MV_EF_OUTPUT_BITMAP_TO_FILE to save the generated bitmap to "font.png". Requires stb_image_write.h.
#define MV_EF_OUTPUT_BITMAP_TO_FILE
#include "mv_easy_font.h"


/*
    Uniform random numbers between 0.0 (inclusive) and 1.0 (exclusive)
    Lehmer RNG, "minimal standard"
*/
double rng()
{
    static unsigned int seed = 123;
    seed *= 16807;
    return seed / (double)0x100000000ULL;
}

GLFWwindow *window;
double resx = 1600;
double resy = 1000;

double prevx, prevy;    // for mouse position
int clickedButtons = 0; // bit field for mouse clicks

int SKIP_DRAWING = 0;

enum buttonMaps { FIRST_BUTTON=1, SECOND_BUTTON=2, THIRD_BUTTON=4, FOURTH_BUTTON=8, FIFTH_BUTTON=16, NO_BUTTON=0 };
enum modifierMaps { CTRL=2, SHIFT=1, ALT=4, META=8, NO_MODIFIER=0 };

// all glfw and opengl init here
void init_GL();
void frame_timer();

// callback functions to send to glfw
void key_callback(GLFWwindow* win, int key, int scancode, int action, int mods);
void mousebutton_callback(GLFWwindow* win, int button, int action, int mods);
void mousepos_callback(GLFWwindow* win, double xpos, double ypos);
void mousewheel_callback(GLFWwindow* win, double xoffset, double yoffset);
void windowsize_callback(GLFWwindow *win, int width, int height);


typedef enum Token_Type {TOKEN_OTHER=0, TOKEN_OPERATOR, TOKEN_NUMERIC, TOKEN_FUNCTION, TOKEN_KEYWORD, TOKEN_COMMENT, TOKEN_VARIABLE, TOKEN_UNSET} Token_Type;

const char *TOKEN_NAMES[] = {"other", "operator", "numeric", "function", "keyword", "comment", "type", "unset"};
const char *TYPES[] = {"void", "int", "float", "vec2", "vec3", "vec4", "sampler1D", "sampler2D"};
const char *KEYWORDS[] = {"#version", "#define", "in", "out", "uniform", "layout", "return", "if", "else", "for", "while"};

typedef struct Token 
{
    char *start;
    char *stop;
    Token_Type type;
} Token;


void color_string(char *str, char *col)
{
    // ignored characters
    char delims[] = " ,(){}[];\t\n";
    int num_delims = strlen(delims);

    char operators[] = "/+-*<>=&|";
    int num_operators = strlen(operators);

    Token tokens[9999]; // hurr
    int num_tokens = 0; // running counter

    char *ptr = str;
    while (*ptr) {
        // skip delimiters
        int is_delim = 0;
        for (int i = 0; i < num_delims; i++) {
            if (*ptr == delims[i]) {
                is_delim = 1;
                break;
            }
        }

        if (is_delim == 1) {
            ptr++;
            continue;
        }

        // found a token!
        char *start = ptr;

        if (*ptr == '/' && *(ptr+1) == '/') {
            // found a line comment, go to end of line or end of file
            while (*ptr != '\n' && *ptr != '\0') {
                ptr++;
            }

            tokens[num_tokens].start = start;
            tokens[num_tokens].stop = ptr;
            tokens[num_tokens].type = TOKEN_COMMENT;
            num_tokens++;

            ptr++;
            continue;
        }

        if (*ptr == '/' && *(ptr+1) == '*') {
            // found a block comment, go to end of line or end of file
            while (!(*ptr == '*' && *(ptr+1) == '/') && *ptr != '\0') {
                ptr++;
            }
            ptr++;

            tokens[num_tokens].start = start;
            tokens[num_tokens].stop = ptr+1;
            tokens[num_tokens].type = TOKEN_COMMENT;
            num_tokens++;

            ptr++;
            continue;
        } 

        // check if it's an operator
        int is_operator = 0;
        for (int i = 0; i < num_operators; i++) {
            if (*ptr == operators[i]) {
                is_operator = 1;
                break;
            }
        }

        if (is_operator == 1) {
            tokens[num_tokens].start = start;
            tokens[num_tokens].stop = ptr+1;
            tokens[num_tokens].type = TOKEN_OPERATOR;
            num_tokens++;
            ptr++;
            continue;
        } 

        // it's either a name, type, a keyword, a function, or an names separated by an operator without spaces
        while (*ptr) {
            // check whether it's an operator stuck between two names
            int is_operator2 = 0;
            for (int i = 0; i < num_operators; i++) {
                if (*ptr == operators[i]) {
                    is_operator2 = 1;
                    break;
                }
            }

            if (is_operator2 == 1) {
                tokens[num_tokens].start = start;
                tokens[num_tokens].stop = ptr;
                tokens[num_tokens].type = TOKEN_UNSET;
                num_tokens++;
                break;
            }

            // otherwise go until we find the next delimiter
            int is_delim2 = 0;
            for (int i = 0; i < num_delims; i++) {
                if (*ptr == delims[i]) {
                    is_delim2 = 1;
                    break;
                }
            }

            if (is_delim2 == 1) {
                tokens[num_tokens].start = start;
                tokens[num_tokens].stop = ptr;
                tokens[num_tokens].type = TOKEN_UNSET;
                num_tokens++;
                ptr++;
                break;
            } 

            // did not find delimiter, check next char
            ptr++; 
        }
    }

    // determine the types of the unset tokens, i.e. either
    // a name, a type, a keyword, or a function
    int num_keywords = sizeof(KEYWORDS)/sizeof(char*);
    int num_types = sizeof(TYPES)/sizeof(char*);

    for (int i = 0; i < num_tokens; i++) {
        // TOKEN_OPERATOR and TOKEN_COMMENT should already be set, so skip those
        if (tokens[i].type != TOKEN_UNSET) {
            continue;
        }

        char end_char = *tokens[i].stop;

        // temporarily null terminate at end of token, restored after parsing
        *tokens[i].stop = '\0';

        // parse
        
        // if it's a keyword
        int is_keyword = 0;
        for (int j = 0; j < num_keywords; j++) {
            if (strcmp(tokens[i].start, KEYWORDS[j]) == 0) {
                is_keyword = 1;
                break;
            }
        }
        if (is_keyword == 1) {
            tokens[i].type = TOKEN_KEYWORD;
            *tokens[i].stop = end_char;
            continue;
        } 

        // Check if it's a function
        float f;
        if (end_char == '(') {
            tokens[i].type = TOKEN_FUNCTION;
            *tokens[i].stop = end_char;
            continue;
        } 

        // or if it's a numeric value. catches both integers and floats
        if (sscanf(tokens[i].start, "%f", &f) == 1) {
            tokens[i].type = TOKEN_NUMERIC;
            *tokens[i].stop = end_char;
            continue;
        } 


        // if it's a variable type
        int is_type = 0;
        for (int j = 0; j < num_types; j++) {
            if (strcmp(tokens[i].start, TYPES[j]) == 0) {
                is_type = 1;
                break;
            }
        }
        if (is_type == 1) {
            tokens[i].type = TOKEN_VARIABLE;
            *tokens[i].stop = end_char;
            continue;
        } 

        // otherwise it's a regular variable name 
        tokens[i].type = TOKEN_OTHER;
        *tokens[i].stop = end_char;
    }
    
    // print all tokens and their types
    for (int i = 0; i < num_tokens; i++) {

        for (char *p = tokens[i].start; p != tokens[i].stop; p++) {
            col[(p - str)] = tokens[i].type;
        }
    }
}
int main(int argc, char *argv[]) 
{
    init_GL();

    if (argc == 2) {
        mv_ef_init(argv[1], 48.0, NULL, NULL);
    }

    char *fragment_source = mv_ef_read_entire_file("extra/vertex_shader_text.vs");
    char *col = (char*)calloc(strlen(fragment_source), 1);
    color_string(fragment_source, col); // syntax highlighting

    glfwSwapInterval(0);
    while ( !glfwWindowShouldClose(window)) {
        frame_timer();

        glfwPollEvents();
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
        
        if (SKIP_DRAWING == 0) {       
            float res[2] = {(float)resx, (float)resy};
            float offset[2] = {0.0, 0.0};
            float font_size = 20.0;

            int width, height;
            mv_ef_string_dimensions(fragment_source, &width, &height, font_size); // for potential alignment

            mv_ef_draw(fragment_source, col, offset, font_size, res);
        }

        glfwSwapBuffers(window);
    }

    free(fragment_source);
    free(col);

    glfwTerminate();

    return 0;
}

void frame_timer()
{
    static double t1 = 0.0;
    static double avg_dt = 0.0;
    static double avg_dt2 = 0.0;
    static int avg_counter = 0;
    static int num_samples = 60;

    double t2 = glfwGetTime();
    double dt = t2-t1;
    t1 = t2;

    avg_dt += dt;
    avg_dt2 += dt*dt;
    avg_counter++;

    if (avg_counter == num_samples) {
        avg_dt  /= num_samples;
        avg_dt2 /= num_samples;
        double std_dt = sqrt(avg_dt2 - avg_dt*avg_dt);
        double ste_dt = std_dt / sqrt(num_samples);

        char window_title_string[128];
        sprintf(window_title_string, "dt: avg = %.3fms, std = %.3fms, ste = %.4fms. fps = %.1f", 1000.0*avg_dt, 1000.0*std_dt, 1000.0*ste_dt, 1.0/avg_dt);
        glfwSetWindowTitle(window, window_title_string);

        num_samples = 1.0/avg_dt;
        
        avg_dt = 0.0;
        avg_dt2 = 0.0;
        avg_counter = 0;
    }
}

/*****************************************************************************/
// OpenGL and GLFW boilerplate below
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
    glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE);

    window = glfwCreateWindow(resx, resy, "GLSL template", 0, 0);
    if (!window) {
        printf("Could not open glfw window\n");
        glfwTerminate();
        exit(-2);
    }
    glfwMakeContextCurrent(window); 

    if(!gladLoadGL()) {
        printf("Something went wrong!\n");
        exit(-3);
    }

    glfwSetKeyCallback(window, key_callback);
    glfwSetMouseButtonCallback(window, mousebutton_callback);
    glfwSetScrollCallback(window, mousewheel_callback);
    glfwSetCursorPosCallback(window, mousepos_callback);
    glfwSetWindowSizeCallback(window, windowsize_callback);

    glfwSwapInterval(0);
    glClearColor(39/255.0,  40/255.0,  34/255.0, 1.0f);
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

    if (key == GLFW_KEY_TAB && action) {
        SKIP_DRAWING = 1 - SKIP_DRAWING;
    }

    if (key == GLFW_KEY_SPACE && action) {
        int num_colors;
        unsigned char *colors = mv_ef_get_colors(&num_colors);

        for (int i = 0; i < 3*num_colors; i++) {
            colors[i] = 256*rng();
        }
        mv_ef_set_colors(colors);
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
    (void)yoffset;

    // double zoomFactor = pow(0.95,yoffset);

    glfwGetCursorPos(win, &prevx, &prevy);
}


#define MV_EASY_FONT_IMPLEMENTATION
#include "mv_easy_font.h"