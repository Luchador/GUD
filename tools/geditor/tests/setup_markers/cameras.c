#include <assert.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "setupload.h"
#include "bghistory.h"

static DWORD Get(const unsigned char *p) { return (DWORD)p[0]<<24|(DWORD)p[1]<<16|(DWORD)p[2]<<8|p[3]; }
static void Put(unsigned char *p,DWORD v) { p[0]=v>>24;p[1]=v>>16;p[2]=v>>8;p[3]=v; }
static void Near(double a,double b) { assert(fabs(a-b)<0.0002); }
static void Same(const SetupFile *a,const SetupFile *b) { assert(a->size==b->size&&!memcmp(a->data,b->data,a->size)); }
static DWORD IntroAt(const SetupFile *setup,DWORD index)
{
    const DWORD words[]={3,4,4,8,2,2,10,3,2,1}; DWORD at=Get(setup->data+8);
    for(DWORD i=0;i<index;i++) { DWORD type=Get(setup->data+at);assert(type<9);at+=words[type]*4; }
    assert(at+4<=setup->size);return at;
}

/* Linking and choice bodies are extracted from the game's bondview_r.c. */
#include "game_intro_selection.h"
static void RandomCandidates(const SetupFile *setup,DWORD expected)
{
    struct SetupIntroCamera cameras[32]; DWORD count=0;
    g_CurrentSetupIntroCamera=NULL;g_SetupIntroCameraCount=0;
    for(DWORD i=0;;i++)
    {
        DWORD at=IntroAt(setup,i),type=Get(setup->data+at);
        if(type==9) { break; }
        if(type==6)
        {
            assert(count<32 && Get(setup->data+at+36)==0);
            cameras[count].command=i;GameLink(&cameras[count++]);
        }
    }
    assert(count==expected && g_SetupIntroCameraCount==(int)expected);
    for(DWORD i=0;i<count;i++)
    {
        game_random=i;assert(GameChoose()==&cameras[count-1-i]);
        game_random=i+count;assert(GameChoose()==&cameras[count-1-i]);
    }
}
static DWORD MarkerCount(const SetupFile *setup,SetupMarkerKind kind)
{
    SetupMarker *markers=NULL;DWORD count,total=0;const char *why="";
    assert(SetupFileBuildMarkers(setup,.5f,&markers,&count,&why));
    for(DWORD i=0;i<count;i++) { total+=markers[i].kind==kind; }
    free(markers);return total;
}
static void RoundTrip(const SetupFile *setup,const char *dir)
{
    SetupFile saved={0};const char *why="";
    assert(SetupSaveProjectFile(dir,setup,&why));
    assert(SetupLoadProjectFile(dir,setup->name,&saved,&why));Same(setup,&saved);
    assert(MarkerCount(setup,SETUP_MARKER_INTRO)==MarkerCount(&saved,SETUP_MARKER_INTRO));
    assert(MarkerCount(setup,SETUP_MARKER_OUTRO)==MarkerCount(&saved,SETUP_MARKER_OUTRO));
    SetupFileFree(&saved);
}
void CameraEdits(const SetupFile *source,const char *dir)
{
    SetupFile setup={0},before={0},after={0}; const char *why="";
    SetupMarkerRef selected={SETUP_MARKER_INTRO,1};
    double position[3]={300.12,250.34,-400.56},look[3]={1,.5,-1},move[3]={3,4,5};
    EditHistory history={0};EditHistoryTransaction transaction={0};EditHistoryAsset asset;
    BgDocument bg={0};StanFile stan={0};DWORD at,table,pad;BOOL changed;Rotation rotation;
    assert(SetupFileClone(source,&setup,&why));
    assert(!SetupFileDeleteIntroCamera(&setup,&selected,&why)&&strstr(why,"at least one"));Same(&setup,source);
    Put(setup.data+52+28,0x782c);Put(setup.data+52+32,0x782d);
    assert(SetupFileClone(&setup,&before,&why));
    EditHistoryReset(&history,&bg,&setup,&stan);
    assert(EditHistoryBeginSetupEdit(&history,&setup,"Add Intro Camera",&transaction,&why));
    for(DWORD expected=2;expected<=4;expected++)
    {
        assert(SetupFilePlaceCamera(&setup,SETUP_MARKER_INTRO,.5f,position,look,&selected,&why));
        assert(selected.kind==SETUP_MARKER_INTRO && selected.command==expected+6);
        assert(MarkerCount(&setup,SETUP_MARKER_INTRO)==expected);RandomCandidates(&setup,expected);
        at=IntroAt(&setup,selected.command);pad=Get(setup.data+at+24);
        assert(Get(setup.data+at+28)==0x782c && Get(setup.data+at+32)==0x782d);
        for(int axis=0;axis<3;axis++)
        {
            Near((LONG)Get(setup.data+at+4+axis*4)/100.0,position[axis]);
            Near(setup.pads[pad].pos[axis],position[axis]*.5);
        }
        Near((LONG)Get(setup.data+at+16)/65536.0,atan2(look[0],-look[2]));
        Near((LONG)Get(setup.data+at+20)/65536.0,atan2(look[1],hypot(look[0],look[2])));
        assert(!memcmp(setup.pads,source->pads,source->padcount*sizeof(*source->pads)));
        assert(!memcmp(setup.data+Get(setup.data+8),before.data+40,244)); /* spawn, camera, swirl */
        assert(!memcmp(setup.data+288,before.data+288,32)); /* prop list unchanged */
    }
    assert(EditHistoryCommitEdit(&history,&bg,&setup,&stan,&transaction,&why));
    assert(SetupFileClone(&setup,&after,&why));RoundTrip(&setup,dir);
    assert(EditHistoryUndo(&history,&bg,&setup,&stan,&asset,&why));Same(&setup,&before);
    assert(EditHistoryRedo(&history,&bg,&setup,&stan,&asset,&why));Same(&setup,&after);
    SetupFileFree(&before);SetupFileFree(&after);
    /* Remove the first, then the appended candidates. Later commands shift. */
    assert(SetupFileClone(&setup,&before,&why));
    assert(EditHistoryBeginSetupEdit(&history,&setup,"Delete Intro Camera",&transaction,&why));
    selected.command=1;assert(SetupFileDeleteIntroCamera(&setup,&selected,&why));
    assert(Get(setup.data+IntroAt(&setup,1))==3);RandomCandidates(&setup,3);
    assert(EditHistoryCommitEdit(&history,&bg,&setup,&stan,&transaction,&why));
    assert(SetupFileClone(&setup,&after,&why));
    assert(EditHistoryUndo(&history,&bg,&setup,&stan,&asset,&why));Same(&setup,&before);
    assert(EditHistoryRedo(&history,&bg,&setup,&stan,&asset,&why));Same(&setup,&after);
    SetupFileFree(&before);SetupFileFree(&after);EditHistoryFree(&history);
    selected.command=7;
    assert(SetupFileDeleteIntroCamera(&setup,&selected,&why));RandomCandidates(&setup,2);
    assert(SetupFileDeleteIntroCamera(&setup,&selected,&why));RandomCandidates(&setup,1);
    assert(SetupFileClone(&setup,&before,&why));
    assert(!SetupFileDeleteIntroCamera(&setup,&selected,&why));Same(&setup,&before);
    SetupFileFree(&before);RoundTrip(&setup,dir);SetupFileFree(&setup);

    /* Missing intro/prop lists can be initialized. Blank title is a loaded
       language-bank entry; zero would crash langGet during the intro. */
    assert(SetupFileClone(source,&setup,&why));Put(setup.data+8,0);Put(setup.data+12,0);
    assert(SetupFilePlaceCamera(&setup,SETUP_MARKER_INTRO,.5f,position,look,&selected,&why));
    assert(selected.command==0 && Get(setup.data+IntroAt(&setup,0)+28)==((39u<<10)|227u));
    RandomCandidates(&setup,1);
    assert(SetupFilePlaceCamera(&setup,SETUP_MARKER_OUTRO,.5f,position,look,&selected,&why));
    assert(selected.command==0 && MarkerCount(&setup,SETUP_MARKER_OUTRO)==1);
    RoundTrip(&setup,dir);SetupFileFree(&setup);

    /* Adding a first outro relocates propDefs. Refresh cached offsets, and
       keep a deleted guard's native bytes and command identity intact. */
    assert(SetupFileClone(source,&setup,&why));Put(setup.data+288,0x4745442e);
    assert(SetupSaveProjectFile(dir,&setup,&why));SetupFileFree(&setup);
    assert(SetupLoadProjectFile(dir,source->name,&setup,&why));
    assert(setup.charactercount==1 && setup.characters[0].sourceoffset==288);
    assert(SetupFilePlaceCamera(&setup,SETUP_MARKER_OUTRO,.5f,position,look,&selected,&why));
    table=Get(setup.data+12);
    assert(selected.command==1 && setup.characters[0].sourceoffset==table);
    assert(Get(setup.data+table)==0x4745442e && MarkerCount(&setup,SETUP_MARKER_OUTRO)==1);
    RoundTrip(&setup,dir);SetupFileFree(&setup);

    /* Authored outro shots and tags keep their command slots. The deleted
       guard shares type 46, but its payload must never become a camera. */
    assert(SetupFileClone(source,&setup,&why));table=setup.size;
    setup.data=realloc(setup.data,setup.size+104);assert(setup.data);
    memset(setup.data+table,0,104);setup.size+=104;
    memcpy(setup.data+table,source->data+288,28);
    Put(setup.data+table+28,22);Put(setup.data+table+32,0x12340001); /* tag 0x1234, relative +1 */
    memcpy(setup.data+table+44,source->data+288,28);Put(setup.data+table+48,99900);
    Put(setup.data+table+72,0x4745442e);Put(setup.data+table+80,0x12345678);
    Put(setup.data+table+100,48);Put(setup.data+12,table);
    /* Save/load refreshes the host arrays for this authored synthetic setup. */
    assert(SetupSaveProjectFile(dir,&setup,&why));SetupFileFree(&setup);
    assert(SetupLoadProjectFile(dir,source->name,&setup,&why));
    assert(MarkerCount(&setup,SETUP_MARKER_OUTRO)==2 && setup.charactercount==1);
    assert(SetupFileClone(&setup,&before,&why));EditHistoryReset(&history,&bg,&setup,&stan);
    assert(EditHistoryBeginSetupEdit(&history,&setup,"Replace Outro Camera",&transaction,&why));
    assert(SetupFilePlaceCamera(&setup,SETUP_MARKER_OUTRO,.5f,position,look,&selected,&why));
    assert(selected.command==0 && Get(setup.data+12)==table);
    assert(!memcmp(setup.data+table+4,setup.data+table+48,24));
    assert(!memcmp(setup.data+table+28,before.data+table+28,16)); /* tag */
    assert(!memcmp(setup.data+table+72,before.data+table+72,32)); /* tombstone + end */
    assert(MarkerCount(&setup,SETUP_MARKER_OUTRO)==1 && setup.charactercount==1);
    assert(EditHistoryCommitEdit(&history,&bg,&setup,&stan,&transaction,&why));
    assert(SetupFileClone(&setup,&after,&why));
    assert(EditHistoryUndo(&history,&bg,&setup,&stan,&asset,&why));Same(&setup,&before);
    assert(MarkerCount(&setup,SETUP_MARKER_OUTRO)==2);
    assert(EditHistoryRedo(&history,&bg,&setup,&stan,&asset,&why));Same(&setup,&after);
    SetupFileFree(&before);SetupFileFree(&after);EditHistoryFree(&history);
    assert(SetupFileTransformMarker(&setup,&selected,NULL,.5f,move,NULL,&changed,&why)&&changed);
    assert(!memcmp(setup.data+table+4,setup.data+table+48,24));
    RotationAxis(&rotation,1,90);
    assert(SetupFileTransformMarker(&setup,&selected,NULL,.5f,NULL,&rotation,&changed,&why)&&changed);
    assert(!memcmp(setup.data+table+4,setup.data+table+48,24));
    assert(MarkerCount(&setup,SETUP_MARKER_OUTRO)==1);RoundTrip(&setup,dir);
    assert(SetupFilePlaceCamera(&setup,SETUP_MARKER_OUTRO,.5f,position,look,&selected,&why));
    assert(MarkerCount(&setup,SETUP_MARKER_OUTRO)==1);SetupFileFree(&setup);

    /* Reject multiplayer and invalid inputs without writing any bytes. */
    assert(SetupFileClone(source,&setup,&why));strcpy(setup.name,"Ump_setupcameraZ");
    assert(SetupFileClone(&setup,&before,&why));
    for(int kind=SETUP_MARKER_INTRO;kind<=SETUP_MARKER_OUTRO;kind++)
    {
        assert(!SetupFilePlaceCamera(&setup,kind,.5f,position,look,&selected,&why));Same(&setup,&before);
        assert(strstr(why,"single-player"));
    }
    selected.kind=SETUP_MARKER_INTRO;selected.command=1;
    assert(!SetupFileDeleteIntroCamera(&setup,&selected,&why)&&strstr(why,"single-player"));Same(&setup,&before);
    strcpy(setup.name,source->name);SetupFileFree(&before);
    assert(SetupFileClone(&setup,&before,&why));position[0]=NAN;
    assert(!SetupFilePlaceCamera(&setup,SETUP_MARKER_INTRO,.5f,position,look,&selected,&why));Same(&setup,&before);
    position[0]=1e30;
    assert(!SetupFilePlaceCamera(&setup,SETUP_MARKER_OUTRO,.5f,position,look,&selected,&why));Same(&setup,&before);
    SetupFileFree(&before);SetupFileFree(&setup);
    puts("PASS: camera placement, game random candidates, last-intro protection, multiplayer rejection, tagged outro replacement/transform, save/reload and undo/redo.");
}
