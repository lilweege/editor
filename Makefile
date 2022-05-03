# CC = cc
BIN = bin
OBJ = obj
SRC = src
TARGET = $(BIN)/main
SRCS = $(wildcard $(SRC)/*.c)
OBJS = $(patsubst $(SRC)/%.c,$(OBJ)/%.o,$(SRCS))
DEPS = $(OBJS:.o=.d)

CC_COMMON = -std=c11 -march=native -Wall -Wextra -Wpedantic -Werror
CC_DEBUG = -g -fsanitize=address
CC_RELEASE = -DNDEBUG -O3
LD_COMMON = 
LD_DEBUG = -fsanitize=address
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
