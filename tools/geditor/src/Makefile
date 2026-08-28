# GEditor - build from WSL with the mingw-w64 cross compiler:
#   sudo apt install gcc-mingw-w64-x86-64
#   make
#
# Sources live here in src/; the .exe is written one level up, in
# tools/geditor/.
 
CC      := x86_64-w64-mingw32-gcc
CFLAGS  := -O2 -Wall -Wextra -Wno-unused-parameter -std=c99
LDFLAGS := -mwindows -lgdi32
 
BIN_DIR := ..
TARGET  := $(BIN_DIR)/GEditor.exe
SOURCES := $(wildcard *.c)
 
$(TARGET): $(SOURCES)
	$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS)
 
clean:
	rm -f $(TARGET)
 
.PHONY: clean