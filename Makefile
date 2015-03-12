.PHONY: all
all:

UNICODE_DATA_TXT = /usr/share/unicode/UnicodeData.txt

-include $(wildcard build/*.d)

OBJECTS = build/main.o build/util.o build/byte_buffer.o build/os.o \
		  build/path.o build/string.o build/shader_program.o build/gui.o \
		  build/label.o build/debug.o build/text_widget.o build/font_size.o \
		  build/find_file_widget.o build/resource_bundle.o build/png_image.o \
		  build/spritesheet.o build/genesis_editor.o build/audio_edit_widget.o \
		  build/genesis.o build/channel_layouts.o build/texture.o \
		  build/shader_program_manager.o build/alpha_texture.o \
		  build/audio_file.o build/audio_hardware.o build/static_geometry.o \
		  build/vertex_array.o build/gui_window.o build/select_widget.o

GENERATE_UNICODE_DATA_OBJECTS = build/generate_unicode_data.o build/util.o \
								build/byte_buffer.o

TEST_OBJECTS = build/test.o build/util.o build/byte_buffer.o build/string.o

CPP_FLAGS += -nodefaultlibs -fno-exceptions -fno-rtti -Ibuild -Isrc -g -Wall -Werror -I/usr/include/freetype2
COMPILE_CPP = g++ -c -std=c++11 -o $@ -MMD -MP -MF $@.d $(CPP_FLAGS) $<

build/genesis: $(OBJECTS)
	g++ -o $@ $(OBJECTS) -lfreetype -lavformat -lavcodec -lavutil -lglfw -lepoxy -lGLU -lGL -lpng -lrucksack -pthread -lm -lpulse
all: build/genesis

all: build/resources.bundle

build/%.o: src/%.cpp
	$(COMPILE_CPP)

%/resources.bundle:
	rucksack bundle assets.json $@ --deps $@.d

$(OBJECTS): | build
build:
	mkdir -p $@

build/string.o: build/unicode.hpp

build/unicode.hpp: build/generate_unicode_data $(UNICODE_DATA_TXT)
	build/generate_unicode_data $(UNICODE_DATA_TXT) $@

build/generate_unicode_data: $(GENERATE_UNICODE_DATA_OBJECTS)
	gcc -o $@ $(GENERATE_UNICODE_DATA_OBJECTS)

.PHONY: test
test: build/test
	build/test

build/test: $(TEST_OBJECTS)
	gcc -o $@ $(TEST_OBJECTS) -lm

.PHONY: clean
clean:
	rm -rf build

