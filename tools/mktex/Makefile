TEX2PNG_C_FILES := \
    src/tex2png.c \
    src/libpdtex/pdtex.c \
    src/libpdtex/reader.c \
    src/libpdtex/writer.c

TEX2RAW_C_FILES := \
    src/tex2raw.c \
    src/libpdtex/pdtex.c \
    src/libpdtex/reader.c \
    src/libpdtex/writer.c

# Compile tex2png, ensuring the build directory exists first
tex2png: $(TEX2PNG_C_FILES)
	mkdir -p build
	gcc $(TEX2PNG_C_FILES) -lpng -lz -O3 -o build/tex2png

tex2raw: $(TEX2RAW_C_FILES)
	mkdir -p build
	gcc $(TEX2RAW_C_FILES) -lz -O3 -Wall -Wextra -o build/tex2raw

clean:
	rm -f build/tex2png build/tex2raw

default: tex2png tex2raw
