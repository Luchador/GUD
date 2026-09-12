#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "bgdocument.h"
#include "inspector.inc"

static void WriteCommand(unsigned char *out, DWORD word0, DWORD word1)
{
    for (int i=0; i<4; i++)
    { out[i]=(unsigned char)(word0 >> (24-i*8)); out[i+4]=(unsigned char)(word1 >> (24-i*8)); }
}

static void Decoder(void)
{
    BgRenderState state;
    BgRenderStateInit(&state, TRUE);
    assert(BgRenderGetTransparency(&state)==BG_TRANSPARENCY_UNKNOWN);
    assert(FacePropertiesDepth(&state,FALSE)==2 && FacePropertiesDepth(&state,TRUE)==2);
    /* A high-mode write or alpha-compare write must not establish a render mode. */
    BgRenderStateRead(&state,0xBA001402,0x00100000);
    BgRenderStateRead(&state,0xB9000002,0);
    assert(state.othermodeknown==3 && BgRenderGetTransparency(&state)==BG_TRANSPARENCY_UNKNOWN);
    BgRenderStateRead(&state,0xB900031D,0x00552078);
    assert(BgRenderGetTransparency(&state)==BG_TRANSPARENCY_OPAQUE);
    assert(FacePropertiesDepth(&state,FALSE)==2); /* RSP ZBUFFER still inherited. */
    BgRenderStateRead(&state,0xB7000000,1);
    assert(FacePropertiesDepth(&state,FALSE)==1 && FacePropertiesDepth(&state,TRUE)==1);
    BgRenderStateRead(&state,0xB900031D,0x00553078);
    assert(BgRenderGetTransparency(&state)==BG_TRANSPARENCY_CUTOUT);
    BgRenderStateRead(&state,0xB900031D,0x005049D8);
    assert(BgRenderGetTransparency(&state)==BG_TRANSPARENCY_BLEND);
    assert(FacePropertiesDepth(&state,FALSE)==1 && FacePropertiesDepth(&state,TRUE)==0);
    BgRenderStateRead(&state,0xB9000002,1);
    assert(BgRenderGetTransparency(&state)==BG_TRANSPARENCY_CUTOUT_BLEND);
    BgRenderStateRead(&state,0xEF100000,0x00504DD8);
    assert(state.othermodeknown==0xFFFFFFFFu && BgRenderGetTransparency(&state)==BG_TRANSPARENCY_DECAL);
    assert(FacePropertiesDepth(&state,TRUE)==0);
    BgRenderStateRead(&state,0xB6000000,1);
    assert(FacePropertiesDepth(&state,FALSE)==0 && FacePropertiesDepth(&state,TRUE)==0);
    BgRenderStateInit(&state,FALSE);
    BgRenderStateRead(&state,0xB9000401,0);
    assert(FacePropertiesDepth(&state,FALSE)==0 && FacePropertiesDepth(&state,TRUE)==2);
    BgRenderStateRead(&state,0xB9001F02,0); /* Invalid shift/count ignored. */
    assert(state.othermodeknown==0x10);
    assert(BgRenderGetTransparency(&state)==BG_TRANSPARENCY_UNKNOWN);
}

static void Inspector(void)
{
    unsigned char commands[48], original[48];
    BgDocumentDrawGroup groups[4]={{0}}, cutout={0}, inherited={0};
    BgDocumentFace faces[5]={{0}}, tree={.id=6,.room=2,.textureid=17};
    BgDocumentRoom rooms[3]={0};
    BgDocument document={.rooms=rooms,.roomcount=2,.facecount=6};
    BgFaceRef refs[6]={{.faceid=1,.room=1},{.faceid=2,.room=1},
        {.faceid=3,.room=1},{.faceid=4,.room=1},
        {.faceid=5,.room=1,.layer=BG_GEOMETRY_SECONDARY},{.faceid=6,.room=2}};
    BgRenderState states[6];
    char text[256];
    WriteCommand(commands,0xB7000000,1);
    WriteCommand(commands+8,0xB900031D,0x00552078);
    WriteCommand(commands+16,0xB900031D,0x005049D8);
    WriteCommand(commands+24,0xB9000002,1);
    WriteCommand(commands+32,0xB7000000,1);
    WriteCommand(commands+40,0xB900031D,0x00553078);
    memcpy(original,commands,sizeof(commands));
    groups[0]=(BgDocumentDrawGroup){.commands=commands,.commandsize=16};
    groups[2]=(BgDocumentDrawGroup){.commands=commands+16,.commandsize=8};
    groups[3]=(BgDocumentDrawGroup){.commands=commands+24,.commandsize=8};
    cutout=(BgDocumentDrawGroup){.commands=commands+32,.commandsize=16};
    rooms[1].layers[0]=(BgDocumentLayerData){.groups=groups,.groupcount=4};
    rooms[1].layers[1]=(BgDocumentLayerData){.groups=&inherited,.groupcount=1};
    rooms[2].layers[0]=(BgDocumentLayerData){.groups=&cutout,.groupcount=1};
    rooms[1].faces=faces; rooms[1].facecount=5;
    rooms[2].faces=&tree; rooms[2].facecount=1;
    for (int i=0; i<5; i++)
    { faces[i]=(BgDocumentFace){.id=i+1,.room=1,.drawgroup=i%4,.layer=i==4,.textureid=17}; }
    assert(BgDocumentGetFaceRenderStates(&document,refs,6,states));
    assert(BgRenderGetTransparency(&states[1])==BG_TRANSPARENCY_OPAQUE); /* Earlier group's state. */
    assert(BgRenderGetTransparency(&states[2])==BG_TRANSPARENCY_BLEND);
    assert(BgRenderGetTransparency(&states[3])==BG_TRANSPARENCY_CUTOUT_BLEND);
    assert(BgRenderGetTransparency(&states[4])==BG_TRANSPARENCY_UNKNOWN); /* No cross-layer leakage. */
    assert(BgRenderGetTransparency(&states[5])==BG_TRANSPARENCY_CUTOUT); /* No cross-room leakage. */
    FacePropertiesRenderText(&document,refs,2,text,sizeof(text));
    assert(!strcmp(text,"Transparency: Opaque\r\nDepth test: On\r\nDepth write: On"));
    FacePropertiesRenderText(&document,refs,3,text,sizeof(text));
    assert(!strcmp(text,"Transparency: Mixed\r\nDepth test: On\r\nDepth write: Mixed"));
    FacePropertiesRenderText(&document,refs+2,1,text,sizeof(text));
    assert(strstr(text,"Transparency: Translucent (alpha blend)"));
    FacePropertiesRenderText(&document,refs+3,1,text,sizeof(text));
    assert(strstr(text,"Transparency: Cutout + blend"));
    FacePropertiesRenderText(&document,refs+4,1,text,sizeof(text));
    assert(strstr(text,"Transparency: Inherited / unknown"));
    FacePropertiesRenderText(&document,refs+5,1,text,sizeof(text));
    assert(strstr(text,"Transparency: Cutout"));
    BgFaceRef unordered[]={refs[5],refs[1],refs[4],refs[0],refs[5]};
    assert(BgDocumentGetFaceRenderStates(&document,unordered,5,states));
    assert(BgRenderGetTransparency(&states[0])==BgRenderGetTransparency(&states[4]));
    assert(!document.dirty && !memcmp(commands,original,sizeof(commands)));
    faces[1].drawgroup=99;
    assert(!BgDocumentGetFaceRenderStates(&document,refs,2,states));
    FacePropertiesRenderText(&document,refs,2,text,sizeof(text));
    assert(strstr(text,"Transparency: Unavailable"));
    assert(!BgDocumentGetFaceRenderStates(NULL,refs,1,states));
    assert(!BgDocumentGetFaceRenderStates(&document,NULL,1,states));
    assert(!BgDocumentGetFaceRenderStates(&document,refs,0,states));
}

int main(void)
{
    Decoder(); Inspector();
    puts("PASS BG transparency: native/partial/inherited state, mixed selection, depth, room/layer isolation and read-only inspection (ASan + UBSan).");
    return 0;
}
