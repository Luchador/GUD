# GEditor - build from WSL with the mingw-w64 cross compiler:
#   sudo apt install gcc-mingw-w64-x86-64
#   make
#
# Sources live here in src/; the .exe is written one level up, in
# tools/geditor/.
WIN_DIR := /mnt/c/Users/lucha/Documents/N64/GEditor

CC      := x86_64-w64-mingw32-gcc
RC      := x86_64-w64-mingw32-windres
CFLAGS  := -O2 -Wall -Wextra -Wno-unused-parameter -std=c99
LDFLAGS := -mwindows -lgdi32 -lcomdlg32 -lopengl32

BIN_DIR := ..
TARGET  := $(BIN_DIR)/GEditor.exe
SOURCES := $(wildcard *.c)
RCFILE  := geditor.rc
RCOBJ   := geditor_rc.o

$(TARGET): $(SOURCES) $(RCOBJ)
	$(CC) $(CFLAGS) -o $@ $(SOURCES) $(RCOBJ) $(LDFLAGS)

# Resource script -> linkable object (dialog templates, later icons/version info)
$(RCOBJ): $(RCFILE) resource.h
	$(RC) -i $(RCFILE) -o $@

clean:
	rm -f $(TARGET) $(RCOBJ)

install: $(TARGET)
	mkdir -p $(WIN_DIR)
	cp $(TARGET) $(WIN_DIR)/
 
run: install
	cd $(WIN_DIR) && cmd.exe /c start "" GEditor.exe

.PHONY: clean install run