#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
typedef void *HWND;
typedef uintptr_t WPARAM;
typedef intptr_t LPARAM,LRESULT;
typedef unsigned int UINT;
typedef struct { HWND hwnd;UINT message;WPARAM wParam;LPARAM lParam; } MSG;
#define WM_APP 0x8000
#define CB_ERR (-1)
enum { CB_RESETCONTENT=1,CB_ADDSTRING,CB_SETITEMDATA,CB_SETCURSEL,CB_GETCURSEL,CB_GETITEMDATA };
#define MB_ICONERROR 1
#define GEDITOR_TITLE "GEditor"
#include "characterproperties.h"
#include "objectload.h"
#include "bghistory.h"
#include "input-types.inc"
#include "weaponchoices.h"
#include "catalog.inc"
typedef struct Combo { int count,selected,item[40];char names[40][80]; } Combo;
static Combo combos[2];
static CharacterPropertiesState *active;
static SetupCharacterWeaponEdit received;
static int requests;
static BOOL reject,reenter;
static void CharacterPropertiesApply(HWND,CharacterPropertiesState *,int);
static HWND GetParent(HWND h) { return NULL; }
static LRESULT SendMessage(HWND h,int msg,WPARAM wp,LPARAM lp)
{
    Combo *c=h;
    if(msg==CHARACTERPROPERTIES_WM_WEAPON_CHANGED) {
        assert(active->committing);received=*(SetupCharacterWeaponEdit *)lp;requests++;
        if(reenter)CharacterPropertiesApply(active,active,0);
        return !reject;
    }
    switch(msg) {
    case CB_RESETCONTENT:c->count=0;c->selected=-1;return 0;
    case CB_ADDSTRING:assert(c->count<40);snprintf(c->names[c->count],80,"%s",(char *)lp);return c->count++;
    case CB_SETITEMDATA:assert(wp<(unsigned)c->count);c->item[wp]=(int)lp;return 0;
    case CB_SETCURSEL:c->selected=(int)wp;return wp;
    case CB_GETCURSEL:return c->selected;
    case CB_GETITEMDATA:assert(wp<(unsigned)c->count);return c->item[wp];
    }
    abort();
}
#include "input.inc"
static int Row(Combo *c,int item) { for(int i=0;i<c->count;i++)if(c->item[i]==item)return i;abort(); }
static void Input(void)
{
    CharacterPropertiesState s={.selected=TRUE,.binding={.characterindex=3,.sourceoffset=512,.chrnum=8}};
    active=&s;s.controls[CHARACTER_RIGHT]=&combos[0];s.controls[CHARACTER_LEFT]=&combos[1];
    CharacterPropertiesChoices(&combos[0],4);CharacterPropertiesChoices(&combos[1],-1);
    assert(combos[0].item[combos[0].selected]==4 && combos[1].item[combos[1].selected]==-1);
    assert(!strcmp(combos[1].names[combos[1].selected],"None"));
    reenter=TRUE;combos[0].selected=Row(&combos[0],13);CharacterPropertiesApply(&s,&s,0);
    assert(requests==1 && received.hand==0 && received.item==13 && received.characterindex==3 && received.sourceoffset==512 && received.chrnum==8);
    reject=TRUE;CharacterPropertiesApply(&s,&s,1);assert(requests==2 && received.hand==1 && received.item==-1 && !s.committing);
    s.updating=TRUE;CharacterPropertiesApply(&s,&s,0);assert(requests==2);s.updating=FALSE;
    s.selected=FALSE;CharacterPropertiesApply(&s,&s,0);assert(requests==2);s.selected=TRUE;
    CharacterPropertiesChoices(&combos[0],SETUP_WEAPON_MIXED);CharacterPropertiesApply(&s,&s,0);assert(requests==2);
    CharacterPropertiesChoices(&combos[0],254);CharacterPropertiesApply(&s,&s,0);assert(requests==2);
    assert(!strcmp(combos[0].names[combos[0].selected],"Item 254 (current)"));
    combos[0].selected=CB_ERR;CharacterPropertiesApply(&s,&s,0);assert(requests==2);
    puts("PASS: actual dropdown catalog, None/unknown/mixed display, right/left edit payloads, selection guards and synchronous reentrancy.");
}

static HWND g_Viewport=(void *)1;
static EditHistory g_EditHistory;
static SetupFile g_CurrentSetup;
static SetupObjectGeometry g_CurrentObjects;
static BgDocument g_CurrentBgDocument;
static StanFile g_CurrentStan;
static struct {char dir[MAX_PATH];} g_Project;
static DWORD selection=SETUP_CHARACTER_SELECTION_BIT|3,restored;
static int begins,commits,rollbacks,rebuilds,errors,sceneversion;
static BOOL selected=TRUE,change=TRUE,modelok=TRUE,buildok=TRUE,commitok=TRUE,editok=TRUE;
static BOOL ViewportGetSelectedObject(HWND h,DWORD *out) {*out=selection;return selected;}
static void ViewportCancelTransform(HWND h) {}
static void ViewportSelectSetupModel(HWND h,DWORD id) {restored=id;}
static void GEditorRefreshHistoryMenu(HWND h) {}
static void MessageBox(HWND h,const char *why,const char *title,int flags) {errors++;}
BOOL EditHistoryBeginSetupEdit(const EditHistory *h,const SetupFile *s,const char *name,EditHistoryTransaction *t,const char **why)
{ begins++;t->active=TRUE;t->beforesetup=*s;return TRUE; }
BOOL EditHistoryCommitEdit(EditHistory *h,BgDocument *bg,SetupFile *s,StanFile *st,EditHistoryTransaction *t,const char **why)
{commits++;if(commitok)t->active=FALSE;return commitok;}
void EditHistoryRollbackEdit(EditHistoryTransaction *t,BgDocument *bg,SetupFile *s,StanFile *st)
{rollbacks++;*s=t->beforesetup;t->active=FALSE;}
void EditHistoryCancelEdit(EditHistoryTransaction *t) {t->active=FALSE;}
BOOL SetupFileSetCharacterWeapon(SetupFile *s,const SetupCharacterWeaponEdit *e,BOOL *changed,const char **why)
{*changed=change;if(change&&editok)s->dirty=TRUE;return editok;}
BgVertex *ModelLoadProjectGeometry(const char *dir,int model,DWORD *n,unsigned short **tags,BgRenderFlags **flags,float *scale,const char **why)
{assert(model==SetupWeaponChoiceForItem(13)->model);return modelok?calloc(1,sizeof(BgVertex)):NULL;}
BOOL ObjectLoadSetupGeometry(const char *dir,const SetupFile *s,const StanFile *st,float scale,SetupObjectGeometry *out,const char **why)
{out->tricount=2;return buildok;}
void ObjectGeometryFree(SetupObjectGeometry *o) {memset(o,0,sizeof(*o));}
static BOOL GEditorRebuildCurrentViewportWithObjects(const SetupObjectGeometry *o,const char **why)
{rebuilds++;sceneversion=o->tricount;return buildok;}
static BOOL GEditorRebuildCurrentViewport(const char **why)
{rebuilds++;sceneversion=g_CurrentObjects.tricount;return TRUE;}
#include "editor.inc"
static void Reset(void)
{
    begins=commits=rollbacks=rebuilds=errors=0;restored=0;
    g_CurrentSetup.dirty=FALSE;g_CurrentObjects.tricount=1;sceneversion=1;
    selected=change=modelok=buildok=commitok=editok=TRUE;
}
static void Editor(void)
{
    SetupCharacterWeaponEdit e={.characterindex=3,.sourceoffset=512,.chrnum=8,.hand=0,.item=13};
    Reset();
    assert(GEditorSetCharacterWeapon(NULL,&e));
    assert(begins==1 && commits==1 && !rollbacks && !errors && g_CurrentSetup.dirty && sceneversion==2 && g_CurrentObjects.tricount==2 && restored==selection);
    Reset();change=FALSE;assert(GEditorSetCharacterWeapon(NULL,&e));assert(!commits&&!rebuilds&&!errors&&!g_CurrentSetup.dirty);
    Reset();selected=FALSE;assert(!GEditorSetCharacterWeapon(NULL,&e));assert(!begins&&!errors);
    Reset();e.characterindex=2;assert(!GEditorSetCharacterWeapon(NULL,&e));assert(!begins);e.characterindex=3;
    for(int failure=0;failure<4;failure++) {
        Reset();if(failure==0)modelok=FALSE;if(failure==1)buildok=FALSE;if(failure==2)commitok=FALSE;if(failure==3)editok=FALSE;
        assert(!GEditorSetCharacterWeapon(NULL,&e));
        assert(errors==1&&rollbacks==1&&!g_CurrentSetup.dirty&&sceneversion==1&&g_CurrentObjects.tricount==1&&restored==selection);
    }
    Reset();modelok=FALSE;e.item=-1;e.hand=1;
    assert(GEditorSetCharacterWeapon(NULL,&e));assert(commits==1&&!errors); /* None needs no model. */
    puts("PASS: production frame transaction, immediate geometry rebuild, one undo commit, no-op/stale selection, missing-model/build/commit failure rollback and None without model loading.");
}
int main(void) {Input();Editor();return 0;}
