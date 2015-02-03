.PHONY: all
all:

-include $(wildcard build/*.d)

OBJECTS = build/main.o

CPP_FLAGS += -Ibuild -Isrc -g -Wall -Werror
COMPILE_CPP = clang -c -std=c++11 -o $@ -MMD -MP -MF $@.d $(CPP_FLAGS) $<

build/genesis: $(OBJECTS)
	clang -o $@ $(OBJECTS) -lm
all: build/genesis

build/%.o: src/%.cpp
	$(COMPILE_CPP)

$(OBJECTS): | build
build:
	mkdir -p $@

.PHONY: clean
clean:
	rm -rf build

