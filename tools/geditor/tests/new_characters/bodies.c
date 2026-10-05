/* Included by check.c so the full project/export fixture is shared. */
#include "bodyposes.inc"
static void Culling(const ModelSource *s)
{
    for(DWORD i=0;i<s->count;i++) {
        OK((s->faces[i].state.geometryknown&0x3000)==0x3000);
        OK((s->faces[i].state.geometrymode&0x3000)==0x2000);
    }
}

static void RawTemplateBody(const char *project,const char *path)
{
    char base[MAX_PATH];snprintf(base,sizeof(base),"%s/base.z64",project);
    RomFile rom={0};DWORD at,size,revision,count=0;ModelSource s={0};
    BgVertex *standing=NULL;ModelTransform *t=NULL;
    OK(RomLoad(base,&rom,&why) && RomFindFile(&rom,"CdjbondZ",&at,&size,&why));
    OK(ModelEditsReadSource(project,"CdjbondZ",&s,&revision,&why));
    OK(ModelBodyBindPose(rom.data+at,size,&s,&standing,&t,&why));
    for(DWORD f=0;f<s.count;f++) if(s.faces[f].closest) {
        memmove(standing+count*3,standing+f*3,3*sizeof(*standing));
        for(int k=0;k<3;k++) {standing[count*3+k].s=k*.25f;standing[count*3+k].t=.5f;}
        s.tags[count]=BG_TEX_NONE;s.flags[count]=BG_RENDER_DEPTH_TEST|BG_RENDER_DEPTH_WRITE;count++;
    }
    OK(GltfWriteModel(path,project,standing,s.tags,s.flags,count,&why));
    /* GltfWriteModel labels even previews with a zero identity. This fixture
     * represents an ordinary DCC export, so remove only that synthetic key. */
    FILE *file=fopen(path,"rb");OK(file && !fseek(file,0,SEEK_END));long length=ftell(file);rewind(file);
    char *json=calloc((size_t)length+1,1);OK(json && fread(json,1,length,file)==(size_t)length && !fclose(file));
    char *key=json;while((key=strstr(key,"goldeneyeSourceHash"))) *key='x';
    Write(path,json,length);free(json);
    free(standing);free(t);ModelFreeSource(&s);RomFree(&rom);
}

static void RawBody(const char *project,const char *name,const char *path,BOOL fit,int id)
{
    GltfModelImport raw={0};BgRenderFlags *flags=NULL;BOOL roundtrip=TRUE;
    ModelSource stock={0},source={0};DWORD revision,count,size,before,after;
    BgVertex *reference=NULL,*standing=NULL;ModelTransform *transforms=NULL,*posedtransforms=NULL;
    OK(ModelEditsReadSource(project,"CdjbondZ",&stock,&revision,&why));
    OK(GltfReadCharacterImport(path,revision,project,&raw,&flags,&roundtrip,&why) && !roundtrip);
    OK(NewPropsImportCharacter(project,name,path,5,fit,&count,&why));
    CheckCharacter(project,name,id,5);
    OK(ModelEditsReadSource(project,name,&source,&revision,&why));
    OK(source.count==raw.count && count==raw.count && !source.haslods);
    Culling(&source);
    for(DWORD i=0;i<source.materials.count;i++) {
        OK(source.materials.slots[i].texture==BG_TEX_NONE);
        OK(!strcmp(source.materials.slots[i].name,raw.materials.slots[i].name));
    }
    const unsigned char *native=NewPropsData(project,name,&size);OK(native);
    for(DWORD l=0;l<source.listcount;l++) {
        BOOL drawn=FALSE;for(DWORD f=0;f<source.count;f++) drawn|=source.faces[f].list==l;
        if(drawn) {ModelSource part={0};part.listcount=1;part.lists=source.lists+l;HeadLinks(native,size,&part);}
    }
    OK(ModelBodyBindPose(native,size,&source,&standing,&posedtransforms,&why));
    /* The source template is unchanged; use it to calculate the expected fit. */
    char base[MAX_PATH];snprintf(base,sizeof(base),"%s/base.z64",project);RomFile rom={0};DWORD at,bytes;
    OK(RomLoad(base,&rom,&why) && RomFindFile(&rom,"CdjbondZ",&at,&bytes,&why));
    OK(ModelBodyBindPose(rom.data+at,bytes,&stock,&reference,&transforms,&why));
    double lo[3],hi[3],slo[3],shi[3];Bounds(raw.vertices,raw.count,lo,hi);
    BOOL first=TRUE;
    for(DWORD i=0;i<stock.count*3;i++) if(stock.faces[i/3].closest) {
        double p[3]={reference[i].x,reference[i].y,reference[i].z};
        for(int a=0;a<3;a++) {if(first || p[a]<slo[a])slo[a]=p[a];if(first || p[a]>shi[a])shi[a]=p[a];}
        first=FALSE;
    }
    double scale=fit ? (shi[1]-slo[1])/(hi[1]-lo[1]) : 1;
    unsigned char *seen=calloc(raw.count,1);OK(seen);
    for(DWORD i=0;i<source.count;i++) {
        DWORD match=raw.count;
        for(DWORD j=0;j<raw.count;j++) if(!seen[j] && source.materials.faces[i].slot==raw.materials.faces[j].slot) {
            BOOL same=TRUE;
            for(int k=0;k<3;k++) {
                const BgVertex *a=standing+i*3+k,*b=raw.vertices+j*3+k;
                double p[3]={b->x,b->y,b->z},q[3]={a->x,a->y,a->z};
                for(int axis=0;axis<3;axis++) {
                    double expected=fit ? (p[axis]-(lo[axis]+hi[axis])*.5)*scale+(slo[axis]+shi[axis])*.5 : p[axis];
                    same &= fabs(q[axis]-expected)<1.0;
                }
                same &= a->r==b->r && a->g==b->g && a->b==b->b && a->a==b->a;
            }
            if(same && !memcmp(source.materials.faces[i].uv,raw.materials.faces[j].uv,6*sizeof(float))) {match=j;break;}
        }
        OK(match<raw.count);seen[match]=1;
    }
    free(seen);
    for(DWORD frame=0;frame<sizeof(bodyposes)/sizeof(*bodyposes);frame++) {
        DWORD count=0;unsigned short *tags=NULL;BgRenderFlags *render=NULL;
        BgVertex *pose=ModelLoadCharacterGeometry(native,size,&count,&tags,&render,&why);OK(pose && count==source.count);
        ModelCharacterAttachments attachments={0};
        OK(ModelApplyCharacterPose(native,size,7,bodyposes[frame],frame&1,pose,count,&attachments));
        OK(attachments.hashead && attachments.hashands[0] && attachments.hashands[1]);
        for(DWORD i=0;i<count*3;i++) OK(isfinite(pose[i].x) && isfinite(pose[i].y) && isfinite(pose[i].z));
        free(pose);free(tags);free(render);
    }
    /* Shared positions must remain closed when animated, despite UV/material
     * splits. Every native matrix is exercised with multiple nonzero poses. */
    for(int frame=0;frame<3;frame++) {
        unsigned short angles[45];memcpy(angles,frame==0 ? g_EditorPose_idle_unarmed : g_EditorPose_idle,sizeof(angles));
        if(frame==2) {angles[17]=0x2400;angles[20]=0xdc00;angles[35]=0x2000;angles[38]=0xe000;}
        BgVertex *pose=ModelLoadAnimationPose(native,size,angles,45,0,source.count,&why);OK(pose);
        double movement=0;
        for(DWORD i=0;i<source.count*3;i++) {
            OK(isfinite(pose[i].x) && isfinite(pose[i].y) && isfinite(pose[i].z));
            movement+=fabs(pose[i].x-standing[i].x)+fabs(pose[i].y-standing[i].y);
            for(DWORD j=0;j<i;j++) if(source.vertexoffsets[i]==source.vertexoffsets[j]
                && source.vertexmatrices[i]==source.vertexmatrices[j])
                OK(pose[i].x==pose[j].x && pose[i].y==pose[j].y && pose[i].z==pose[j].z);
        }
        OK(movement>10);
        const char *dump=getenv("GEDITOR_BODY_PREVIEW");
        if(dump) {char output[512];snprintf(output,sizeof(output),"%s/%s-%d.gltf",dump,name,frame);
            OK(GltfWriteModel(output,project,pose,source.tags,source.flags,source.count,&why));}
        free(pose);
    }
    ModelFreeSource(&source);
    for(DWORD slot=0;slot<raw.materials.count;slot++) {
        OK(ModelEditsReadSource(project,name,&source,&revision,&why));ModelFreeSource(&source);
        OK(ModelEditsSetMaterial(project,name,revision,slot,slot+5,&why));
    }
    OK(ModelEditsReadSource(project,name,&source,&revision,&why));Culling(&source);
    for(DWORD i=0;i<source.count;i++) OK(BG_TEX_ID(source.tags[i])==source.materials.faces[i].slot+5);
    char exported[MAX_PATH];snprintf(exported,sizeof(exported),"%s/%s.gltf",project,name);
    OK(ModelEditsExport(project,name,exported,&why));
    OK(ModelEditsImport(project,name,exported,&before,&after,&why) && before==after && after==count);
    OK(!ModelEditsImport(project,name,path,&before,&after,&why));
    /* Assigning an image must preserve an intentional culling override. */
    ModelFreeSource(&source);OK(ModelEditsReadSource(project,name,&source,&revision,&why));
    DWORD face=0;OK(ModelEditsSetProperties(project,name,revision,&face,1,0,-1,-1,-1,&why));
    ModelFreeSource(&source);OK(ModelEditsReadSource(project,name,&source,&revision,&why));
    OK(ModelEditsSetMaterial(project,name,revision,source.materials.faces[0].slot,31,&why));
    ModelFreeSource(&source);OK(ModelEditsReadSource(project,name,&source,&revision,&why));
    OK(!(source.faces[0].state.geometrymode&0x3000));
    OK(ModelEditsSetProperties(project,name,revision,&face,1,1,-1,-1,-1,&why));
    ModelFreeSource(&source);ModelFreeSource(&stock);GltfFreeModelImport(&raw);
    free(flags);free(reference);free(transforms);free(standing);free(posedtransforms);RomFree(&rom);
    printf("PASS raw body %s: %lu triangles; fitted geometry, colors, UVs, per-vertex joints, poses, culling, texture assignments and round trip.\n",name,(unsigned long)count);
}
