CC = clang
BIN = bin
OBJ = obj
SRC = src
TARGET = $(BIN)/editor
SRCS = $(wildcard $(SRC)/*.c)
OBJS = $(patsubst $(SRC)/%.c,$(OBJ)/%.o,$(SRCS))
DEPS = $(OBJS:.o=.d)


PKGS = sdl2 glew
PKG_FLAGS = $(shell pkg-config --cflags $(PKGS))
PKG_LIBS = $(shell pkg-config --libs $(PKGS))

CC_COMMON = -std=c11 -march=native -Wall -Wextra -Wpedantic $(PKG_FLAGS)
CC_DEBUG = -g
CC_RELEASE = -DNDEBUG -O3 -Werror
LD_COMMON = $(PKG_LIBS) -lm
LD_DEBUG =
LD_RELEASE = 

CFLAGS = $(CC_COMMON) $(CC_DEBUG)
LDFLAGS = $(LD_COMMON) $(LD_DEBUG)
release: CFLAGS = $(CC_COMMON) $(CC_RELEASE)
release: LDFLAGS = $(LD_COMMON) $(LD_RELEASE)

debug: $(TARGET)
-include $(DEPS)
release: clean $(TARGET)

$(OBJ)/%.o: $(SRC)/%.c
	$(CC) -MMD $(CFLAGS) -c $< -o $@

$(TARGET): $(OBJS)
	$(CC) $(OBJS) -o $@ $(LDFLAGS)

.PHONY: clean
clean:
	rm -f $(TARGET) obj/*.o obj/*.d
