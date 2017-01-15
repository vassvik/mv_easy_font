IDIRS=-IC:/Libraries/glfw-3.2.1.bin.WIN64/include -IC:/Libraries/glew-2.0.0/include
LDIRS=-LC:/Libraries/glfw-3.2.1.bin.WIN64/lib-mingw-w64 -LC:/Libraries/glew-2.0.0/lib
LDFLAGS=-lglfw3 -lglew32 -lopengl32 -lgdi32

all:
	gcc -g main.c $(IDIRS) $(LDIRS) $(LDFLAGS)