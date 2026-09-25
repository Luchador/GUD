#include <assert.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "setupload.h"
#include "setup_compare.h"
#include "bghistory.h"
#include "modelload.h"
#include "glasspreview.h"
#include <src/propconstants.h>

void BgDocumentFree(BgDocument *d) { memset(d, 0, sizeof(*d)); }
void StanFileFree(StanFile *d) { memset(d, 0, sizeof(*d)); }
static DWORD Read(const unsigned char *p)
{ return ((DWORD)p[0] << 24) | ((DWORD)p[1] << 16) | ((DWORD)p[2] << 8) | p[3]; }
static void Near(double a, double b) { assert(fabs(a-b) < .0005); }
static void Same(const SetupFile *a, const SetupFile *b)
{
    assert(a->objectcount == b->objectcount && a->padcount == b->padcount
        && a->boundpadcount == b->boundpadcount && a->charactercount == b->charactercount);
    SetupAssertNativeEqual(a,b);
}
static void RoundTrip(const char *dir, const SetupFile *setup)
{
    SetupFile saved = {0}; const char *why;
    assert(SetupSaveProjectFile(dir, setup, &why));
    assert(SetupLoadProjectFile(dir, setup->name, &saved, &why));
    Same(setup, &saved); assert(!saved.dirty); SetupFileFree(&saved);
}

static void Place(const char *dir, const SetupFile *source, int model, float scale, const double facing[3], BOOL mp)
{
    SetupFile setup = {0}, before = {0}, placed = {0}; const char *why;
    EditHistory history = {0}; EditHistoryTransaction tx; EditHistoryAsset asset;
    BgDocument bg = {0}; StanFile stan = {0};
    double position[3] = {300, -20, -400}; DWORD selection, target;
    assert(SetupFileClone(source, &setup, &why));
    if (mp) { strcpy(setup.name, "Ump_setupglassZ"); }
    assert(SetupFileClone(&setup, &before, &why));
    EditHistoryReset(&history, &bg, &setup, &stan);
    assert(EditHistoryBeginSetupEdit(&history, &setup, "Add Glass", &tx, &why));
    assert(SetupFileAddGlass(&setup, model, scale, position, facing, &selection, &why));
    assert(selection == source->objectcount && setup.objectcount == source->objectcount + 1);
    assert(setup.padcount == source->padcount && setup.boundpadcount == source->boundpadcount + 1);
    assert(setup.charactercount == source->charactercount && setup.dirty);
    /* Only active command/bound pointers change. Preserve original payloads,
     * command order, relative door links and all existing pad indices. */
    for (int at = 0; at < 40; at += 4)
    { if (at != 12 && at != 28) { assert(Read(source->data+at) == Read(setup.data+at)); } }
    assert(!memcmp(source->data+40, setup.data+40, source->size-40));
    assert(SetupObjectRelativeTarget(&setup, setup.objects[0].sourceoffset, 2, &target) && target == 1);
    assert(SetupObjectRelativeTarget(&setup, setup.objects[1].sourceoffset, -2, &target) && target == 0);
    const SetupObject *glass = &setup.objects[selection];
    assert(glass->type == PROPDEF_GLASS && glass->modelid == model && glass->pad == 10001 && glass->extrascale == 256);
    /* Stock breakable free-standing panes (e.g. Ump_setupashZ) use these
     * native flags and health. Don't inherit door links/portal behavior. */
    assert(glass->flags == 0x04000b62u && !glass->flags2 && !glass->nonsolid && !glass->deleted);
    DWORD record = glass->sourceoffset;
    assert(Read(setup.data+record+128) == 48); /* Native GLASS is 32 words, not GlassRecord's C size. */
    assert(Read(setup.data+record+0x74) == 1000u << 16);
    for (DWORD at = 16; at < 128; at += 4)
    { if (at != 0x74) { assert(!Read(setup.data+record+at)); } }
    SetupPadRef ref;
    assert(SetupFileGetModelPad(&setup, selection, &ref) && ref.bound && ref.index == 1);
    const SetupBoundPad *pad = &setup.boundpads[ref.index];
    float corners[8][3];
    SetupPadGetBoxCorners(&pad->pad, pad->xmin,pad->xmax,pad->ymin,pad->ymax,pad->zmin,pad->zmax,1/scale,corners);
    double horizontal = hypot(facing[0], facing[2]);
    for (int axis = 0; axis < 3; axis++) { Near(pad->pad.pos[axis]/scale, position[axis]); }
    Near((pad->xmax-pad->xmin)/scale, 100); Near(pad->ymin, 0); Near(pad->ymax, 0);
    for (int i = 0; i < 8; i++)
    {
        Near(corners[i][1], position[1] + (i & 4 ? 200 : 0));
        Near(hypot(corners[i][0]-position[0], corners[i][2]-position[2]), 50);
        Near((corners[i][0]-position[0])*pad->pad.up[0] + (corners[i][2]-position[2])*pad->pad.up[2], 0);
    }
    /* With ONSIDE, model +Z = pad up and model Y = pad look. */
    if (horizontal > 1e-8)
    { Near(pad->pad.up[0]*facing[0] + pad->pad.up[2]*facing[2], -horizontal); }
    else { Near(pad->pad.up[2], 1); }
    Near(pad->pad.look[1], 1);
    DWORD padrecord = Read(setup.data+28)+68;
    DWORD plink = Read(setup.data+padrecord+36);
    assert(plink < setup.size && !setup.data[plink] && !pad->pad.stanname[0]);
    assert(!Read(setup.data+padrecord+68+36));
    assert(EditHistoryCommitEdit(&history,&bg,&setup,&stan,&tx,&why));
    RoundTrip(dir,&setup); EditHistoryMarkSetupSaved(&history,&setup);
    assert(SetupFileClone(&setup,&placed,&why));
    assert(EditHistoryUndo(&history,&bg,&setup,&stan,&asset,&why) && setup.dirty); Same(&setup,&before);
    assert(EditHistoryRedo(&history,&bg,&setup,&stan,&asset,&why) && !setup.dirty); Same(&setup,&placed);
    /* Existing inspector and transform operations must keep a real glass
     * record and retain its free-standing flag on a private bound pad. */
    SetupObjectPropertyEdit edit = {0}; BOOL changed;
    edit.objectindex=selection; edit.sourceoffset=setup.objects[selection].sourceoffset; edit.type=PROPDEF_GLASS;
    edit.property=SETUP_OBJECT_HEALTH; edit.value=250;
    assert(SetupFileSetObjectProperty(&setup,&edit,&changed,&why) && changed);
    const double offset[3]={10,20,30};
    assert(SetupFileTranslateModel(&setup,selection,scale,offset,&why));
    assert(SetupFileGetModelPad(&setup,selection,&ref));
    Rotation rotation; RotationAxis(&rotation,1,0.5);
    assert(SetupFileRotatePad(&setup,&ref,&rotation,&changed,&why) && changed);
    const double bounds[6]={-70*scale,70*scale,0,0,0,240*scale};
    assert(SetupFileSetModelBounds(&setup,selection,scale,bounds,&why));
    assert(setup.objects[selection].type == PROPDEF_GLASS && (setup.objects[selection].flags & PROPFLAG_FREE_STANDING_GLASS));
    assert(!memcmp(source->boundpads,setup.boundpads,sizeof(*source->boundpads)));
    RoundTrip(dir,&setup);
    DWORD second;
    assert(SetupFileAddGlass(&setup,model,scale,position,facing,&second,&why) && second == selection+1);
    assert(setup.objects[second].pad != setup.objects[selection].pad);
    assert(SetupFileDeleteObject(&setup,second,&why) && setup.objects[second].deleted);
    RoundTrip(dir,&setup);
    SetupFileFree(&setup); SetupFileFree(&before); SetupFileFree(&placed); EditHistoryFree(&history);
}

static BOOL GlassEdit(SetupFile *setup, DWORD index, SetupObjectProperty property, double value)
{
    SetupObjectPropertyEdit edit = {0}; BOOL changed; const char *why;
    edit.objectindex=index; edit.sourceoffset=setup->objects[index].sourceoffset;
    edit.type=setup->objects[index].type; edit.property=property; edit.value=value;
    return SetupFileSetObjectProperty(setup,&edit,&changed,&why);
}
static void Tinted(const char *dir, const SetupFile *source, int model)
{
    SetupFile setup={0}, before={0}, after={0}; const char *why;
    double pos[3]={300,-20,-400}, facing[3]={0,0,-1}; DWORD first,second,target;
    EditHistory history={0}; EditHistoryTransaction tx; EditHistoryAsset asset;
    BgDocument bg={0}; StanFile stan={0};
    assert(SetupFileClone(source,&setup,&why));
    assert(SetupFileAddGlass(&setup,model,1,pos,facing,&first,&why));
    assert(SetupFileAddGlass(&setup,model,1,pos,facing,&second,&why));
    assert(SetupFileCompact(&setup,&why));
    assert(SetupFileClone(&setup,&before,&why));
    EditHistoryReset(&history,&bg,&setup,&stan);
    assert(EditHistoryBeginSetupEdit(&history,&setup,"Tint glass",&tx,&why));
    assert(GlassEdit(&setup,first,SETUP_OBJECT_GLASS_TYPE,PROPDEF_TINTED_GLASS));
    assert(setup.size<=before.size+64 && setup.objects[first].type==PROPDEF_TINTED_GLASS);
    DWORD tintedsize=setup.size;
    assert(setup.objects[second].type==PROPDEF_GLASS);
    assert(!memcmp(setup.data+setup.objects[second].sourceoffset,before.data+before.objects[second].sourceoffset,128));
    assert(SetupObjectRelativeTarget(&setup,setup.objects[0].sourceoffset,2,&target) && target==1);
    assert(SetupObjectRelativeTarget(&setup,setup.objects[1].sourceoffset,-2,&target) && target==0);
    SetupObjectProperties properties;
    assert(SetupFileGetObjectProperties(&setup,first,&properties,&why));
    assert(properties.glass.tintdistance==2 && properties.glass.opaquedistance==6
        && properties.glass.minimumopacity==0 && properties.glass.autoportal);
    assert(GlassEdit(&setup,first,SETUP_OBJECT_GLASS_TINT_DISTANCE,3.25));
    assert(GlassEdit(&setup,first,SETUP_OBJECT_GLASS_OPAQUE_DISTANCE,10.5));
    assert(GlassEdit(&setup,first,SETUP_OBJECT_GLASS_MINIMUM_OPACITY,25));
    assert(GlassEdit(&setup,first,SETUP_OBJECT_GLASS_AUTO_PORTAL,0));
    const unsigned char *record=setup.data+setup.objects[first].sourceoffset;
    assert(Read(record+0x80)==325 && Read(record+0x84)==1050 && Read(record+0x90)==16384);
    assert(Read(record+0x8c)==0xffffffffu && !(Read(record+8)&PROPFLAG_GLASS_HASPORTAL));
    assert(EditHistoryCommitEdit(&history,&bg,&setup,&stan,&tx,&why));
    assert(SetupFileClone(&setup,&after,&why)); RoundTrip(dir,&setup);
    assert(EditHistoryUndo(&history,&bg,&setup,&stan,&asset,&why)); Same(&setup,&before);
    assert(EditHistoryRedo(&history,&bg,&setup,&stan,&asset,&why)); Same(&setup,&after);
    assert(!GlassEdit(&setup,first,SETUP_OBJECT_GLASS_TINT_DISTANCE,10.5));
    assert(!GlassEdit(&setup,first,SETUP_OBJECT_GLASS_OPAQUE_DISTANCE,3.25));
    assert(!GlassEdit(&setup,first,SETUP_OBJECT_GLASS_MINIMUM_OPACITY,100.01));
    assert(!GlassEdit(&setup,first,SETUP_OBJECT_GLASS_MINIMUM_OPACITY,NAN));
    assert(!GlassEdit(&setup,second,SETUP_OBJECT_GLASS_MINIMUM_OPACITY,20));
    assert(!GlassEdit(&setup,0,SETUP_OBJECT_GLASS_TYPE,PROPDEF_TINTED_GLASS)); Same(&setup,&after);
    for(int i=0;i<20;i++)
    {
        assert(GlassEdit(&setup,first,SETUP_OBJECT_GLASS_TYPE,PROPDEF_GLASS));
        assert(setup.objectcount==before.objectcount && setup.charactercount==before.charactercount);
        for(DWORD n=0;n<setup.objectcount;n++)
            assert(!memcmp(setup.data+setup.objects[n].sourceoffset,before.data+before.objects[n].sourceoffset,
                n<2 ? 256 : 128));
        assert(!memcmp(setup.boundpads,before.boundpads,setup.boundpadcount*sizeof(*setup.boundpads)));
        assert(SetupObjectRelativeTarget(&setup,setup.objects[0].sourceoffset,2,&target) && target==1);
        assert(GlassEdit(&setup,first,SETUP_OBJECT_GLASS_TYPE,PROPDEF_TINTED_GLASS));
        assert(setup.size<=tintedsize);
    }
    RoundTrip(dir,&setup);
    SetupFileFree(&setup); SetupFileFree(&before); SetupFileFree(&after); EditHistoryFree(&history);
    puts("PASS: regular/tinted conversion, record resizing, links, fixed-point opacity, validation, persistence and undo/redo.");
}
static void Preview(void)
{
    GlassPreview glass={TRUE,{10,20,30},200,600,.25f};
    float eye[3]={10,20,30}; assert(GlassPreviewAlpha(&glass,eye)==63);
    eye[0]+=200; assert(GlassPreviewAlpha(&glass,eye)==63);
    eye[0]+=200; assert(GlassPreviewAlpha(&glass,eye)==159);
    eye[0]+=200; assert(GlassPreviewAlpha(&glass,eye)==255);
    eye[0]=10; eye[1]+=600; assert(GlassPreviewAlpha(&glass,eye)==255);
    glass.opaquedistance=glass.tintdistance; assert(GlassPreviewAlpha(&glass,eye)==255);
    glass.active=FALSE; assert(GlassPreviewAlpha(&glass,eye)==0);
    SetupObject object={0}; SetupBoundPad bound={0}; SetupFile setup={0};
    BgPortal portals[3]={0}; BgPortalFile file={0};
    object.type=PROPDEF_TINTED_GLASS; object.pad=10000;
    setup.objects=&object; setup.objectcount=1; setup.boundpads=&bound; setup.boundpadcount=1;
    bound.pad.up[2]=1; bound.pad.look[1]=1;
    file.portals=portals; file.portalcount=3;
    const float scales[]={.15f,.54f,1,1.2f};
    for(int s=0;s<4;s++)
    {
        float scale=scales[s];
        /* Last portal is closest to the pane; first is closest to probe start. */
        for(int p=0;p<3;p++)
        {
            portals[p].pointcount=4;
            for(int v=0;v<4;v++)
            {
                portals[p].nativepoints[v].x=(v==1||v==2 ? 50 : -50)*scale;
                portals[p].nativepoints[v].y=(v>=2 ? 50 : -50)*scale;
                portals[p].nativepoints[v].z=(-8+p*3)*scale;
            }
        }
        assert(GlassFindPortal(&setup,0,&file,scale)==2);
        bound.pad.pos[0]=60*scale; assert(GlassFindPortal(&setup,0,&file,scale)==-1);
        bound.pad.pos[0]=0; bound.pad.pos[2]=20*scale;
        assert(GlassFindPortal(&setup,0,&file,scale)==-1);
        bound.pad.pos[2]=0;
        bound.xmin=bound.xmax=120*scale; /* Center, rather than pad origin, must cross. */
        assert(GlassFindPortal(&setup,0,&file,scale)==-1);
        bound.xmin=bound.xmax=0;
    }
    object.type=PROPDEF_GLASS; assert(GlassFindPortal(&setup,0,&file,1)==-1);
    puts("PASS: distance/height opacity ramp, minimum tint, endpoints and nearest intersected portal across level scales.");
}

int main(int argc, char **argv)
{
    SetupFile source={0}, setup={0}; const char *why,*name; int model=-1; DWORD selection;
    assert(argc==2 && SetupLoadProjectFile(argv[1],"UsetupglassZ",&source,&why));
    for (int i=0; ModelGetPropDefinition(i,&name,NULL); i++)
    { if (!strcmp(name,SETUP_DEFAULT_GLASS_MODEL)) { model=i; break; } }
    assert(model==104);
    Tinted(argv[1],&source,model); Preview();
    const float scales[]={.15019713f,.53931433f,1.20648f,1};
    const double facing[][3]={{0,0,-1},{1,.5,1},{-1,0,0},{0,-1,0}};
    for (int mp=0;mp<2;mp++) for (int s=0;s<4;s++) for (int f=0;f<4;f++)
    { Place(argv[1],&source,model,scales[s],facing[f],mp); }
    puts("PASS: solo/MP panes, level scales, upright orientation, 32-word native glass records, flags, pad encoding, existing references, save/reload and history.");
    puts("PASS: new-glass health, translation, rotation, flat bounds, repeated placement and deletion.");
    const double pos[3]={0,0,0}, bad[3]={NAN,0,0};
    assert(SetupFileClone(&source,&setup,&why));
    assert(!SetupFileAddGlass(&setup,model,0,pos,facing[0],&selection,&why)); Same(&setup,&source);
    assert(!SetupFileAddGlass(&setup,model,1,bad,facing[0],&selection,&why)); Same(&setup,&source);
    assert(!SetupFileAddGlass(&setup,model,1,pos,bad,&selection,&why)); Same(&setup,&source);
    assert(!SetupFileAddGlass(&setup,-1,1,pos,facing[0],&selection,&why)); Same(&setup,&source);
    assert(!SetupFileAddGlass(&setup,model,INFINITY,pos,facing[0],&selection,&why)); Same(&setup,&source);
    assert(!SetupFileAddGlass(&setup,model,1e-30f,pos,facing[0],&selection,&why)); Same(&setup,&source);
    assert(!SetupFileAddGlass(&setup,model,1,NULL,facing[0],&selection,&why)); Same(&setup,&source);
    assert(!SetupFileAddGlass(&setup,model,1,pos,NULL,&selection,&why)); Same(&setup,&source);
    assert(!SetupFileAddGlass(&setup,model,1,pos,facing[0],NULL,&why)); Same(&setup,&source);
    DWORD count=setup.boundpadcount;
    setup.boundpadcount=22768; assert(!SetupFileAddGlass(&setup,model,1,pos,facing[0],&selection,&why)); setup.boundpadcount=count;
    Same(&setup,&source); assert(!setup.dirty);
    SetupFileFree(&setup);
    assert(SetupLoadProjectFile(argv[1],"UsetupemptyZ",&setup,&why) && !setup.objectcount && !setup.boundpadcount);
    assert(SetupFileAddGlass(&setup,model,1,pos,facing[0],&selection,&why) && selection==0 && setup.objects[0].pad==10000);
    RoundTrip(argv[1],&setup);
    SetupFileFree(&setup); SetupFileFree(&source);
    puts("PASS: invalid inputs and bound-pad exhaustion leave setup unchanged; empty lists can gain their first pane.");
    return 0;
}
