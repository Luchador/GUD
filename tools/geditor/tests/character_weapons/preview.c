/* Production hat placement and native model decoding; no ROM or renderer. */
#include <assert.h>
#include "characterload.c"

int NewPropsCharacterId(const char *name) { return -1; }
BOOL NewPropsCharacter(int id,const char **name,int *templateid) { return FALSE; }
static unsigned char *Read(const char *root,const char *name,DWORD *size)
{
    char path[2048];snprintf(path,sizeof(path),"%s/assets/obseg/chr/%s.bin",root,name);
    FILE *f=fopen(path,"rb");assert(f);fseek(f,0,SEEK_END);*size=(DWORD)ftell(f);rewind(f);
    unsigned char *data=malloc(*size);assert(data && fread(data,1,*size,f)==*size);fclose(f);return data;
}
/* Exercise the production edit-aware posing helper using raw model fixtures
 * in place of LoadSource's project/ROM I/O. */
static unsigned char *g_PreviewData;
static DWORD g_PreviewSize;
static BOOL LoadSource(const char *project, const char *name, unsigned char **data,
    DWORD *size, DWORD *basehash, const char **why)
{
    *size = g_PreviewSize; *basehash = 0;
    *data = malloc(*size); assert(*data);
    memcpy(*data, g_PreviewData, *size); return TRUE;
}
#include "idle-preview.inc"

static void BodyThumbnails(const char *root)
{
    unsigned int bodies = 0;
    assert(CharacterBodySwitchCount("CcamguardZ") > 0);
    assert(CharacterBodySwitchCount("CborisZ") > 0); /* Integrated head. */
    assert(!CharacterBodySwitchCount("CheadchrisZ"));
    assert(!CharacterBodySwitchCount("Csuit_lf_handZ"));
    assert(!CharacterBodySwitchCount("Gpp7Z"));
    assert(!CharacterBodySwitchCount("PboxZ"));
    for (int id = 0; id < CUSTOM_CHARACTER_BASE; id++)
    {
        const char *name = g_CharacterModels[id].filename, *why = "";
        int switches = CharacterBodySwitchCount(name);
        if (!switches) continue;
        g_PreviewData = Read(root, name, &g_PreviewSize);
        ModelSource source = {0};
        assert(ModelReadSource(g_PreviewData, g_PreviewSize, &source, &why));
        BgVertex *vertices = malloc((size_t)source.count * 3 * sizeof(*vertices));
        assert(vertices);
        DWORD count = 0;
        for (DWORD face = 0; face < source.count; face++)
            if (ModelSourceFaceInLod(&source, face, MODEL_LOD_HIGH))
            { memcpy(vertices + count * 3, source.vertices + face * 3, 3 * sizeof(*vertices)); count++; }
        assert(count);
        size_t bytes = (size_t)count * 3 * sizeof(*vertices);
        BgVertex *before = malloc(bytes); assert(before); memcpy(before, vertices, bytes);
        assert(ModelEditsApplyIdlePreview("fixture", name, switches, vertices, count));
        BOOL moved = FALSE;
        for (DWORD v = 0; v < count * 3; v++)
        {
            assert(isfinite(vertices[v].x) && isfinite(vertices[v].y) && isfinite(vertices[v].z));
            moved |= vertices[v].x != before[v].x || vertices[v].y != before[v].y || vertices[v].z != before[v].z;
            assert(!memcmp(&vertices[v].r, &before[v].r, 4));
            assert(vertices[v].s == before[v].s && vertices[v].t == before[v].t);
        }
        assert(moved);
        /* Unmatched custom geometry must remain intact on pose failure. */
        memcpy(vertices, before, bytes); vertices[0].x += 10;
        memcpy(before, vertices, bytes);
        assert(!ModelEditsApplyIdlePreview("fixture", name, switches, vertices, count));
        assert(!memcmp(vertices, before, bytes));
        free(before); free(vertices); ModelFreeSource(&source); free(g_PreviewData); bodies++;
    }
    assert(bodies > 0);
    printf("PASS: %u real character bodies posed from thumbnail high-LOD copies; UV/RGBA preserved; heads/watch/items/props excluded; unmatched geometry unchanged.\n", bodies);
}
static void Near(float a,float b) { assert(fabsf(a-b)<.002f); }
static void Placement(void)
{
    CharacterPart body={0};ModelTransformIdentity(&body.attachments.hat);
    body.attachments.hat.m[3][0]=10;body.attachments.hat.m[3][1]=20;body.attachments.hat.m[3][2]=30;
    BgVertex vertices[3]={{.x=2,.y=3,.z=4,.r=123,.a=75},{.x=1},{.y=1}};
    unsigned short tags[1]={BG_TEX_NONE};BgRenderFlags flags[1]={0};
    CharacterEquipment equipment={.part={.vertices=vertices,.tags=tags,.renderflags=flags,.tricount=1},
        .origin={1,2,3},.scale=.5f,.usesmodelscale=TRUE};
    SetupObject hat={.extrascale=512};struct headHat fit={1,2,3,2,3,4};CharacterBuilder builder={0};
    float bodyoffset[3]={0,5,0},position[3]={100,200,300};
    assert(CharacterPlaceHat(&builder,&equipment,&hat,&fit,&body,bodyoffset,position,2,1,0,5));
    assert(builder.count==1 && builder.indices[0]==(SETUP_CHARACTER_SELECTION_BIT|5));
    Near(builder.vertices[0].x,325.8f);Near(builder.vertices[0].y,357.2f);Near(builder.vertices[0].z,227.4f);
    assert(builder.vertices[0].r==123 && builder.vertices[0].a==75);
    free(builder.vertices);free(builder.indices);free(builder.tags);free(builder.renderflags);
    assert(CharacterHatFit(CharacterFindModel("CheadchrisZ"),220));
    assert(!CharacterHatFit(CharacterFindModel("CheadvivienZ"),220));
    assert(!CharacterHatFit(CharacterFindModel("CheadchrisZ"),999));
}
int main(int argc,char **argv)
{
    assert(argc==2);Placement();BodyThumbnails(argv[1]);
    DWORD size,count;unsigned short *tags;BgRenderFlags *flags;const char *why;
    unsigned char *data=Read(argv[1],"CcamguardZ",&size);
    BgVertex *vertices=ModelLoadCharacterGeometry(data,size,&count,&tags,&flags,&why);assert(vertices);
    ModelCharacterAttachments attachments={0};
    int id=CharacterFindModel("CcamguardZ");
    assert(ModelApplyCharacterPose(data,size,g_CharacterModels[id].header->switchcount,g_EditorPose_idle_unarmed,
        FALSE,vertices,count,&attachments));
    assert(attachments.hashat && attachments.hashead && attachments.hashands[0] && attachments.hashands[1]);
    for(int axis=0;axis<3;axis++)assert(isfinite(attachments.hat.m[3][axis]));
    free(vertices);free(tags);free(flags);free(data);
    data=Read(argv[1],"CheadchrisZ",&size);
    vertices=ModelLoadCharacterGeometry(data,size,&count,&tags,&flags,&why);assert(vertices);
    DWORD complete=count;free(vertices);free(tags);free(flags);
    vertices=ModelLoadHeadWithHatGeometry(data,size,2,&count,&tags,&flags,&why);assert(vertices);
    assert(count>0 && count<complete);free(vertices);free(tags);free(flags);
    vertices=ModelLoadHeadWithHatGeometry(data,size,0,&count,&tags,&flags,&why);assert(vertices && count==complete);
    free(vertices);free(tags);free(flags);free(data);
    puts("PASS: real guard idle pose has a hat joint, peaked caps hide only the head toggle branch, and hat fitting preserves scale, facing, color and character selection.");
    return 0;
}
