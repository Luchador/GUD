#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "studioassets.h"
#include "project.h"
#include "editorpath.h"

static const char *why = "";
#define OK(expr) do { if (!(expr)) { fprintf(stderr, "%s:%d: %s: %s\n", __FILE__, __LINE__, #expr, why); abort(); } } while (0)
static void Path(char out[MAX_PATH], const char *dir, const char *name)
{ OK(EditorPathJoin(out, MAX_PATH, dir, name)); }
static void Folder(const char *dir, const char *name)
{ char path[MAX_PATH]; Path(path, dir, name); OK(CreateDirectory(path, NULL)); }
static void Save(const char *dir, const char *name, const char *text)
{
    char path[MAX_PATH]; Path(path, dir, name); FILE *file = fopen(path, "wb");
    OK(file && fwrite(text, 1, strlen(text), file) == strlen(text) && !fclose(file));
}
static void Same(const char *dir, const char *name, const char *expected)
{
    char path[MAX_PATH], data[256] = {0}; Path(path, dir, name); FILE *file = fopen(path, "rb");
    OK(file); size_t size = fread(data, 1, sizeof(data) - 1, file);
    OK(!fclose(file) && size == strlen(expected) && !strcmp(data, expected));
}

/* Only the WIC decoder is replaced in this POSIX test. Real catalog creation,
 * sorting, refresh, pixel ownership, filenames, and directory migration run.
 * Fixture bytes identify previews so sort/offset and replacement errors show up. */
BOOL TexLoadFileThumbnail(const char *path, TexThumb *thumb, unsigned char *pixels)
{
    OK(strstr(path, "studio\\images\\"));
    FILE *file = fopen(path, "rb"); int value;
    if (!file) { return FALSE; }
    value = fgetc(file); fclose(file);
    if (value == '!' || value == EOF) { return FALSE; }
    thumb->w = 32; thumb->h = 16; thumb->imagewidth = 1024; thumb->imageheight = 512;
    memset(pixels, value, TEX_THUMB_MAX * TEX_THUMB_MAX * 4);
    return TRUE;
}

int main(int argc, char **argv)
{
    char project[MAX_PATH], path[MAX_PATH], folder[MAX_PATH];
    StudioFileEntry *files = NULL; TexThumb *thumbs = NULL; unsigned char *pixels = NULL;
    DWORD count = 0;
    OK(argc == 2);
    Folder(argv[1], "Project"); Path(project, argv[1], "Project");
    Folder(project, "studio"); Folder(project, "studio/model"); Folder(project, "studio/model/textures");
    Folder(project, "models"); Folder(project, "images");
    Save(project, "models/game.gltf", "game model"); Save(project, "images/game.bmp", "game image");
    Save(project, "studio/model/Zulu.GLTF", "model Z");
    Save(project, "studio/model/alpha.gltf", "{\"uri\":\"mesh.bin\"}");
    Save(project, "studio/model/mesh.bin", "binary buffer");
    Save(project, "studio/model/textures/diffuse.bmp", "embedded dependency");
    OK(ProjectEnsureStudioFolders(project, &why));
    Path(path, project, "studio/model"); OK(GetFileAttributes(path) == INVALID_FILE_ATTRIBUTES);
    Same(project, "studio/models/alpha.gltf", "{\"uri\":\"mesh.bin\"}");
    Same(project, "studio/models/mesh.bin", "binary buffer");
    Same(project, "studio/models/textures/diffuse.bmp", "embedded dependency");
    Same(project, "models/game.gltf", "game model"); Same(project, "images/game.bmp", "game image");
    OK(ProjectEnsureStudioFolders(project, &why));
    Folder(project, "studio/models/fake.gltf"); Save(project, "studio/models/other.glb", "not listed");
    Save(project, "studio/models/.gltf", "no name"); Save(project, "studio/models/partial.gltf.tmp", "not listed");
    OK(StudioFileList(project, "studio\\models", ".gltf", &files, &count, &why) && count == 2);
    OK(!strcmp(files[0].filename, "alpha.gltf") && !strcmp(files[1].filename, "Zulu.GLTF")); free(files);
    puts("PASS: whole-folder migration preserves glTF dependencies and game assets; sorted model list filters extensions and directories.");

    Folder(project, "studio/model"); Save(project, "studio/model/alpha.gltf", "keep legacy model");
    OK(ProjectEnsureStudioFolders(project, &why));
    Same(project, "studio/model/alpha.gltf", "keep legacy model");
    Same(project, "studio/models/alpha.gltf", "{\"uri\":\"mesh.bin\"}");
    Folder(argv[1], "Failed"); Path(path, argv[1], "Failed"); Folder(path, "studio"); Folder(path, "studio/model");
    Save(path, "studio/model/scene.gltf", "keep on failure"); test_fail_move = 1;
    OK(!ProjectEnsureStudioFolders(path, &why) && why[0]); Same(path, "studio/model/scene.gltf", "keep on failure");
    OK(ProjectEnsureStudioFolders(path, &why)); Same(path, "studio/models/scene.gltf", "keep on failure");
    Folder(argv[1], "Blocked"); Path(path, argv[1], "Blocked"); Folder(path, "studio");
    Save(path, "studio/models", "keep colliding file");
    OK(!ProjectEnsureStudioFolders(path, &why) && why[0]); Same(path, "studio/models", "keep colliding file");
    puts("PASS: existing old/new folders, failed rename and a models-file collision never overwrite user data.");

    Path(folder, project, "studio/images");
    const char *longname = "A long studio image filename beyond sixteen characters.bmp";
    Save(folder, "Zebra.BMP", "Z"); Save(folder, longname, "A"); Save(folder, "Broken.bmp", "!");
    Save(folder, "Ignore.png", "I"); Folder(folder, "Directory.bmp");
    OK(StudioLoadImages(project, &thumbs, &pixels, &count, &why) && count == 3 && why[0]);
    OK(!strcmp(thumbs[0].label, longname) && !strcmp(thumbs[1].label, "Broken.bmp") && !strcmp(thumbs[2].label, "Zebra.BMP"));
    OK(thumbs[0].imagewidth == 1024 && thumbs[0].imageheight == 512);
    OK(!thumbs[1].w && !thumbs[1].h);
    OK(pixels[thumbs[0].pixeloffset] == 'A' && pixels[thumbs[1].pixeloffset] == 0 && pixels[thumbs[2].pixeloffset] == 'Z');
    free(thumbs); free(pixels);
    Save(folder, longname, "R"); Path(path, folder, "Broken.bmp"); OK(DeleteFile(path));
    Save(folder, "Added.bmp", "N");
    OK(StudioLoadImages(project, &thumbs, &pixels, &count, &why) && count == 3 && !why[0]);
    OK(pixels[thumbs[0].pixeloffset] == 'R' && pixels[thumbs[1].pixeloffset] == 'N');
    free(thumbs); free(pixels);
    Path(path, folder, longname); OK(DeleteFile(path)); Path(path, folder, "Added.bmp"); OK(DeleteFile(path));
    Path(path, folder, "Zebra.BMP"); OK(DeleteFile(path));
    OK(StudioLoadImages(project, &thumbs, &pixels, &count, &why) && !count && !thumbs && !pixels && !why[0]);
    OK(StudioLoadImages(NULL, &thumbs, &pixels, &count, &why) && !count && !thumbs && !pixels);
    puts("PASS: long BMP filenames, mixed-case extensions, unreadable previews, correct thumbnail offsets, additions/replacements/deletions and empty catalogs.");
    return 0;
}
