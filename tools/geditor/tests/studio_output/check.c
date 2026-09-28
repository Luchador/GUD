#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "studiooutput.h"
BOOL TestRestoreImportBmpAlpha(const char *path,TexPixel *pixels,DWORD w,DWORD h);
extern int test_fail_write,test_fail_move,test_publish_race;

int main(int argc,char **argv)
{
    assert(argc==2); const char *why=""; char path[MAX_PATH],saved[MAX_PATH];
    StudioRenderSettings size={3,2};
    /* OpenGL bottom row first. Distinct corners expose mirroring/swizzling. */
    TexPixel pixels[6]={{255,0,0,255},{0,0,0,255},{0,255,0,255},
                        {0,0,255,255},{0,0,0,0},{255,255,255,255}},restored[6];
    assert(StudioOutputSave(argv[1],pixels,&size,path,&why));
    assert(strstr(path,"render0001.bmp")); strcpy(saved,path);
    memset(restored,255,sizeof(restored)); assert(TestRestoreImportBmpAlpha(path,restored,3,2));
    for(int y=0;y<2;y++) for(int x=0;x<3;x++) assert(restored[y*3+x].a==pixels[(1-y)*3+x].a);
    assert(StudioOutputSave(argv[1],pixels,&size,path,&why) && strstr(path,"render0002.bmp"));
    assert(DeleteFile(saved));
    /* Fill the lowest gap, preserving the still-existing render0002. */
    assert(StudioOutputSave(argv[1],pixels,&size,path,&why) && strstr(path,"render0001.bmp"));
    test_fail_write=1; assert(!StudioOutputSave(argv[1],pixels,&size,path,&why) && why[0] && !path[0]);
    test_fail_move=1; assert(!StudioOutputSave(argv[1],pixels,&size,path,&why) && why[0] && !path[0]);
    test_publish_race=1;
    assert(StudioOutputSave(argv[1],pixels,&size,path,&why) && strstr(path,"render0004.bmp"));
    /* The race fixture claimed render0003 as a directory. */
    memset(pixels,0,sizeof(pixels));
    assert(StudioOutputSave(argv[1],pixels,&size,path,&why) && strstr(path,"render0005.bmp"));
    memset(restored,255,sizeof(restored)); assert(TestRestoreImportBmpAlpha(path,restored,3,2));
    for(int i=0;i<6;i++) assert(restored[i].a==0);
    size=(StudioRenderSettings){1,1}; pixels[0]=(TexPixel){0,0,0,255};
    assert(StudioOutputSave(argv[1],pixels,&size,path,&why) && strstr(path,"render0006.bmp"));
    restored[0].a=0; assert(TestRestoreImportBmpAlpha(path,restored,1,1) && restored[0].a==255);
    const StudioRenderSettings invalid[]={{0,2},{2,0},{256,2},{2,256},{-1,2}};
    for(unsigned i=0;i<sizeof(invalid)/sizeof(*invalid);i++) assert(!StudioOutputSave(argv[1],pixels,&invalid[i],path,&why));
    assert(!StudioOutputSave(argv[1],NULL,&size,path,&why));
    puts("PASS: lowest-gap naming, no overwrites, concurrent name claim, write/publish cleanup, size bounds and GUD alpha restoration for black and empty images.");
}
