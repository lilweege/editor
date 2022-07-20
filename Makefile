CC = clang
CXX = clang++
BIN = bin
OBJ = obj
SRC = src
TARGET = $(BIN)/editor
SRCS = $(shell find $(SRC) -maxdepth 1 -name *.cpp -or -name *.c)
LANG_PATH = $(SRC)/trash-lang
LANG_OBJS = $(LANG_PATH)/obj/stringview.c.o $(LANG_PATH)/obj/tokenizer.c.o
OBJS = $(addprefix $(OBJ)/,$(notdir $(addsuffix .o,$(SRCS)))) $(LANG_OBJS)
DEPS = $(OBJS:.o=.d)


PKGS = sdl2 glew
PKG_FLAGS = $(shell pkg-config --cflags $(PKGS))
PKG_LIBS = $(shell pkg-config --libs $(PKGS))

INCLUDES = -I./include/immer

CC_COMMON = -march=native -Wall -Wextra -Wno-unused-parameter $(PKG_FLAGS) $(INCLUDES)
CC_DEBUG = -g -fsanitize=undefined,address
CC_RELEASE = -DNDEBUG -O3 -Werror
LD_COMMON = $(PKG_LIBS) -lm
LD_DEBUG = -fsanitize=undefined,address
LD_RELEASE = 

CFLAGS = $(CC_COMMON) $(CC_DEBUG)
LDFLAGS = $(LD_COMMON) $(LD_DEBUG)
release: CFLAGS = $(CC_COMMON) $(CC_RELEASE)
release: LDFLAGS = $(LD_COMMON) $(LD_RELEASE)
debug: $(TARGET)
-include $(DEPS)
release: clean $(TARGET)

$(OBJ)/%.cpp.o: $(SRC)/%.cpp
	$(CXX) -std=c++20 -MMD $(CFLAGS) -c $< -o $@
$(OBJ)/%.c.o: $(SRC)/%.c
	$(CC) -std=c11 -MMD $(CFLAGS) -c $< -o $@
$(LANG_PATH)/obj/%.c.o: $(LANG_PATH)/src/%.c
	$(CC) -std=c11 -MMD $(CFLAGS) -c $< -o $@


$(TARGET): $(OBJS)
	$(CXX) -std=c++20 $(CXXFLAGS) $(OBJS) -o $@ $(LDFLAGS)

.PHONY: clean
clean:
	rm -f $(TARGET) $(OBJS) $(DEPS) $(LANG_OBJS)
