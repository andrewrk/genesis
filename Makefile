.PHONY: all
all:

-include $(wildcard build/*.d)

OBJECTS = build/main.o build/util.o build/byte_buffer.o build/genesis.o \
		  build/path.o build/string.o build/shader_program.o build/gui.o \
		  build/label.o build/debug.o build/text_widget.o build/font_size.o

CPP_FLAGS += -nodefaultlibs -fno-exceptions -fno-rtti -Ibuild -Isrc -g -Wall -Werror -I/usr/include/freetype2
COMPILE_CPP = g++ -c -std=c++11 -o $@ -MMD -MP -MF $@.d $(CPP_FLAGS) $<

build/genesis: $(OBJECTS)
	gcc -o $@ $(OBJECTS) -lfreetype -lgroove -lSDL2 -lm -lepoxy -lGLU -lGL
all: build/genesis

build/%.o: src/%.cpp
	$(COMPILE_CPP)

$(OBJECTS): | build
build:
	mkdir -p $@

.PHONY: clean
clean:
	rm -rf build

