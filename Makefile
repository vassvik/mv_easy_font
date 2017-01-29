IDIRS=-IC:/Libraries/glfw-3.2.1.bin.WIN64/include -Iinclude
LDIRS=-LC:/Libraries/glfw-3.2.1.bin.WIN64/lib-mingw-w64
LDFLAGS=-lglfw3 -lopengl32 -lgdi32

all:
	gcc -g main.c $(IDIRS) $(LDIRS) $(LDFLAGS)
