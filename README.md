# mv_easy_font

Simple texture renderer in Opengl 3.3 using stb_truetype.h to load a packed bitmap into texture of a .ttf font. Uses instanced rendering, so that only position, index and color of each glyph have to be updated instead of updating two triangles per glyph. 


### Usage
compile with 
```bash
gcc main.c -Iinclude -lglfw
```

to use mv_easy_font.h in any running opengl project, define MV_EASY_FONT_IMPLEMENTATION before including mv_easy_font.h in one .c/.cpp file, and include mv_easy_font.h wherever else needed. Simply call mv_ef_draw() with the appropriate parameters to draw text:
```C
mv_ef_draw(str, col, offset, font_size, res);
```

### Dependencies

mv_easy_font.h depend on stb_truetype.h, otherwise it is standalone. 

you can optionally choose to use include stb_image_write.h and stb_rect_pack.h

At the moment there are two shader files you will need. These will probably be inlined in the near future.

console.ttf is hardcoded at the moment, so that will also have to be available in the current working directory when running

the example code uses GLFW and GLAD, but those should not be required, as long as opengl symbols are loaded properly.

### Screenshot:
![screenshot](http://i.imgur.com/zSHHVo7.png)

### Font bitmap

![font](font.png)
