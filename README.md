# mv_easy_font

Simple texture renderer in Opengl 3.3 using stb_truetype.h to load a packed bitmap into texture of a .ttf font. Uses instanced rendering, so that only position, index and color of each glyph have to be updated instead of updating two triangles per glyph. 


### Usage
compile with 
```bash
gcc main.c -Iinclude -lglfw
```

to use mv_easy_font.h in any running opengl project, define MV_EASY_FONT_IMPLEMENTATION in one .c/.cpp file, and include mv_easy_font.h wherever needed. Simply call mv_ef_draw() with the appropriate parameters to draw text:
```C
mv_ef_draw(str, col, offset, font_size, res);
```

### Screenshot:
![screenshot](http://i.imgur.com/DPQnUiH.png)

### Font bitmap

![font](font.png)
