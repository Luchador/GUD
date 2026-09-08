# GEditor - build from WSL with the mingw-w64 cross compiler:
#   sudo apt install gcc-mingw-w64-x86-64
#   make
#
# Sources live here in src/; the .exe is written one level up, in
# tools/geditor/.
WIN_DIR := /mnt/c/Users/lucha/Documents/N64/GEditor

CC      := x86_64-w64-mingw32-gcc
RC      := x86_64-w64-mingw32-windres
CFLAGS  := -O2 -Wall -Wextra -Wno-unused-parameter -std=c99 -I../../..
LDFLAGS := -mwindows -lgdi32 -lcomdlg32 -lopengl32 -lole32 -luuid -lshell32 -lcomctl32 -lwindowscodecs

BIN_DIR := ..
TARGET  := $(BIN_DIR)/GEditor.exe
SOURCES := $(wildcard *.c)
HEADERS := $(wildcard *.h)
SHARED_HEADERS := ../../../src/propconstants.h
PROP_RECORDS := ../../../assets/obseg/prop/propItemModelFileRecord.inc.c \
                $(wildcard ../../../assets/obseg/prop/*/propFileRecord.inc.c)
RCFILE  := geditor.rc
RCOBJ   := geditor_rc.o
TOOL_IMAGES := $(wildcard ../geditorassets/*_select_*.png) \
               $(wildcard ../geditorassets/vertex_paint_*.png) \
               ../geditorassets/no_texture.png

$(TARGET): $(SOURCES) $(HEADERS) $(SHARED_HEADERS) $(PROP_RECORDS) $(RCOBJ)
	$(CC) $(CFLAGS) -o $@ $(SOURCES) $(RCOBJ) $(LDFLAGS)

# Resource script -> linkable object (dialogs, icon, and tool button artwork)
$(RCOBJ): $(RCFILE) resource.h icon.ico $(TOOL_IMAGES) ../geditorassets/gizmo_arrow.glb ../geditorassets/gizmo_cylinder.glb
	$(RC) -i $(RCFILE) -o $@

clean:
	rm -f $(TARGET) $(RCOBJ)

install: $(TARGET)
	mkdir -p $(WIN_DIR)
	cp $(TARGET) $(WIN_DIR)/
 
run: install
	cd $(WIN_DIR) && cmd.exe /c start "" GEditor.exe

.PHONY: clean install run
