static DWORD offsetNativeHash[4],offsetNativeSize[4];
static void HeadOffsetFixture(const char *name)
{
    const char *folder=getenv("GEDITOR_OFFSET_FIXTURES");if(!folder || !folder[0]) return;
    char path[512];snprintf(path,sizeof(path),"%s/%s.bin",folder,name);
    FILE *file=fopen(path,"rb");if(!file) return;
    OK(!fseek(file,0,SEEK_END));long size=ftell(file);rewind(file);
    unsigned char *data=malloc(size);OK(data && fread(data,1,size,file)==(size_t)size && !fclose(file));
    OK(NewPropsReplace(name,data,size,&why));
}
static void HeadOffsetTest(const char *project,const char *name,unsigned index)
{
    ModelSource before={0},after={0};DWORD revision,size;unsigned char *snapshot=NULL;
    OK(ModelEditsReadSource(project,name,&before,&revision,&why));
    OK(ModelEditsCopyNative(project,name,&snapshot,&size,&why));
    const int delta[3]={3,-9,-17},back[3]={-3,9,17},zero[3]={0},bad[3]={65536,0,0};
    ModelUVChange history={0},noop={0};
    OK(!ModelEditsOffsetHead(project,name,revision^1,delta,&history,&why) && !history.before);
    OK(!ModelEditsOffsetHead(project,name,revision,bad,&history,&why) && !history.before);
    OK(ModelEditsOffsetHead(project,name,revision,zero,&noop,&why) && !noop.before);
    OK(ModelEditsOffsetHead(project,name,revision,delta,&history,&why));
    OK(ModelEditsReadSource(project,name,&after,&revision,&why));
    DWORD movedsize;const unsigned char *native=NewPropsData(project,name,&movedsize);
    OK(movedsize==size && before.count==after.count);
    for(DWORD i=0;i<before.count*3;i++) {
        OK(after.vertices[i].x==before.vertices[i].x+delta[0]
            && after.vertices[i].y==before.vertices[i].y+delta[1]
            && after.vertices[i].z==before.vertices[i].z+delta[2]);
        OK(!memcmp(snapshot+before.vertexoffsets[i]+6,native+after.vertexoffsets[i]+6,10));
    }
    OK(before.materials.count==after.materials.count);
    if(before.materials.count) {
        OK(!memcmp(before.materials.slots,after.materials.slots,before.materials.count*sizeof(*before.materials.slots)));
        OK(!memcmp(before.materials.faces,after.materials.faces,before.count*sizeof(*before.materials.faces)));
    }
    OK(!memcmp(before.tags,after.tags,before.count*sizeof(*before.tags))
        && !memcmp(before.flags,after.flags,before.count*sizeof(*before.flags)));
    HeadLinks(native,movedsize,&after);
    /* Undo/redo restores exact bytes, including collision links and metadata. */
    OK(ModelEditsRestoreUVs(project,name,&history,FALSE,&why));
    native=NewPropsData(project,name,&movedsize);OK(movedsize==size && !memcmp(native,snapshot,size));
    OK(ModelEditsRestoreUVs(project,name,&history,TRUE,&why));
    OK(ModelEditsOffsetHead(project,name,revision,back,NULL,&why));
    native=NewPropsData(project,name,&movedsize);OK(!memcmp(native,snapshot,size));
    /* Leave a nonzero adjustment to verify save/reopen and stripped ROM data. */
    OK(ModelEditsOffsetHead(project,name,history.beforeRevision,delta,NULL,&why));
    native=NewPropsData(project,name,&movedsize);offsetNativeSize[index]=ModelMaterialsNativeSize(native,movedsize);
    offsetNativeHash[index]=ModelDataHash(native,offsetNativeSize[index]);
    char path[MAX_PATH];snprintf(path,sizeof(path),"%s/%s-offset.gltf",project,name);
    OK(ModelEditsExport(project,name,path,&why));
    DWORD oldcount,newcount;OK(ModelEditsImport(project,name,path,&oldcount,&newcount,&why));
    ModelFreeSource(&after);OK(ModelEditsReadSource(project,name,&after,&revision,&why));
    for(DWORD i=0;i<before.count*3;i++) OK(after.vertices[i].x==before.vertices[i].x+delta[0]
        && after.vertices[i].y==before.vertices[i].y+delta[1] && after.vertices[i].z==before.vertices[i].z+delta[2]);
    native=NewPropsData(project,name,&movedsize);HeadLinks(native,movedsize,&after);
    offsetNativeSize[index]=ModelMaterialsNativeSize(native,movedsize);
    offsetNativeHash[index]=ModelDataHash(native,offsetNativeSize[index]);
    ModelEditsFreeUVChange(&history);ModelFreeSource(&before);ModelFreeSource(&after);free(snapshot);
    printf("PASS head offset %s: XYZ, collision points, UVs, RGBA, materials, exact undo/redo/reset, stale/overflow rejection and glTF round trip.\n",name);
}
static void HeadOffsetSaved(const char *project,const char *name,unsigned index)
{
    DWORD size;const unsigned char *native=NewPropsData(project,name,&size);OK(native);
    DWORD length=ModelMaterialsNativeSize(native,size);
    OK((length==offsetNativeSize[index] || length==((offsetNativeSize[index]+15)&~15u))
        && ModelDataHash(native,offsetNativeSize[index])==offsetNativeHash[index]);
}
