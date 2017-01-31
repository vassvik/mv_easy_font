# easy_font_gl3

Simple texture renderer in Opengl 3.3 using stb_truetype.h to load a packed bitmap into texture of a .ttf font. Uses instanced rendering, so that only position and dimensions of each glyph have to be updated isntead of each vertex. 


### Usage
compile with 
```bash
gcc main.c -Iinclude -lglfw
```

to use font_draw.h in any running opengl project, define DRAW_FONT_IMPLEMENTATION in one .c/.cpp file, and include draw_font.h wherever needed. Simply call font_draw() with the appropriate parameters to draw text:
```C
font_draw(fragment_source, col, offset, font_size, res);
```

### Screenshot:
![screenshot](http://i.imgur.com/DPQnUiH.png)

