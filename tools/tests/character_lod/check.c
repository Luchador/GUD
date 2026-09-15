typedef struct Fixture {
    Model model; ModelFileHeader file,*heads;
    ModelNode *nodes; ModelRoData *ro;
    ModelRwData **rw;
    RenderPosView matrices[256];
    const Asset *asset;
    size_t slots;
} Fixture;
static void Create(Fixture *f,const Asset *asset)
{
    memset(f,0,sizeof(*f)); f->asset=asset;
    int heads=0; for (int i=0;i<asset->count;i++) { heads+=(asset->nodes[i].op&255)==MODELNODE_OPCODE_HEAD; }
    f->slots=(size_t)(asset->count*4+8)*(heads+1);
    f->nodes=calloc(asset->count,sizeof(*f->nodes)); f->ro=calloc(asset->count,sizeof(*f->ro));
    f->heads=calloc(asset->count,sizeof(*f->heads)); f->rw=calloc(f->slots,sizeof(*f->rw));
    assert(f->nodes && f->ro && f->heads && f->rw);
    f->file.RootNode=&f->nodes[asset->root]; f->model=(Model){&f->file,f->matrices,f->rw,1};
}
static void Destroy(Fixture *f) { free(f->nodes); free(f->ro); free(f->heads); free(f->rw); }
static ModelNode *Node(Fixture *f,int i) { assert(i<f->asset->count); return i<0 ? NULL : &f->nodes[i]; }
static void Prepare(Fixture *f,int switches)
{
    int count=f->asset->count,heads=0; size_t stride=count*4+8;
    memset(f->rw,0,f->slots*sizeof(*f->rw)); memset(f->matrices,0,sizeof(f->matrices));
    memset(f->ro,0,count*sizeof(*f->ro));
    for (int i=0;i<count;i++)
    {
        const NodeSpec *s=&f->asset->nodes[i]; ModelNode *node=&f->nodes[i]; ModelRoData *r=&f->ro[i];
        *node=(ModelNode){s->op,r,Node(f,s->parent),Node(f,s->next),Node(f,s->prev),Node(f,s->child),i};
        switch (s->op&255)
        {
        case MODELNODE_OPCODE_HEADER: r->Header.MatrixIndex=s->matrix; r->Header.RwDataIndex=i*4; break;
        case MODELNODE_OPCODE_GROUP: case MODELNODE_OPCODE_OP03:
            r->Group.MatrixIDs[0]=r->Group.MatrixIDs[1]=r->Group.MatrixIDs[2]=s->matrix; break;
        case MODELNODE_OPCODE_GROUPSIMPLE: r->GroupSimple.Group1=s->matrix; break;
        case MODELNODE_OPCODE_LOD:
            r->LOD=(ModelRoData_LODRecord){s->min,s->max,Node(f,s->affects),i*4,0};
            for (size_t base=0;base<f->slots;base+=stride) { ((ModelRwData *)&f->rw[base+i*4])->LOD.visible=123; }
            break;
        case MODELNODE_OPCODE_SWITCH:
            r->Switch=(ModelRoData_SwitchRecord){Node(f,s->affects),i*4};
            for (size_t base=0;base<f->slots;base+=stride) { ((ModelRwData *)&f->rw[base+i*4])->Switch.visible=(i+switches)%2; }
            break;
        case MODELNODE_OPCODE_HEAD:
            r->HeadPlaceholder.RwDataIndex=i*4; f->heads[i].RootNode=Node(f,s->affects); heads++;
            for (size_t base=0;base<f->slots;base+=stride)
            {
                ModelRwData_HeadPlaceholderRecord *h=&((ModelRwData *)&f->rw[base+i*4])->HeadPlaceholder;
                h->ModelFileHeader=s->affects<0 ? NULL : &f->heads[i]; h->RwDatas=&f->rw[heads*stride];
            }
            break;
        default: break;
        }
    }
}
static unsigned long cases,oldrw,newrw,oldparents,newparents,olddepth,newdepth;
static void Compare(Fixture *f,ModelRenderData data,f32 scale,f32 view,f32 bias,bool disabled,int switches)
{
    ModelNode *oldtrace[2048],**children=malloc(f->asset->count*sizeof(*children));
    void *runtime=malloc(f->slots*sizeof(*f->rw)); RenderPosView matrices[256];
    assert(children && runtime);
    Prepare(f,switches); f->model.scale=scale; viewscale=view; g_ModelDistanceScale=bias; g_ModelDistanceDisabled=disabled;
    tracecount=rwlookups=matrixlookups=depthlookups=rwparents=matrixparents=0;
    referenceUpdateMatrices(&data,&f->model);
    unsigned traces=tracecount,rw=rwlookups,mtx=matrixlookups,dep=depthlookups,parents=rwparents+matrixparents;
    memcpy(oldtrace,trace,traces*sizeof(*trace)); memcpy(runtime,f->rw,f->slots*sizeof(*f->rw));
    memcpy(matrices,f->matrices,sizeof(matrices));
    for (int i=0;i<f->asset->count;i++) { children[i]=f->nodes[i].Child; }
    f32 finalscale=f->model.scale,finalview=viewscale,finalbias=g_ModelDistanceScale;
    Prepare(f,switches); f->model.scale=scale; viewscale=view; g_ModelDistanceScale=bias; g_ModelDistanceDisabled=disabled;
    tracecount=rwlookups=matrixlookups=depthlookups=rwparents=matrixparents=0;
    modelUpdateMatrices(&data,&f->model);
    if (tracecount!=traces || memcmp(oldtrace,trace,traces*sizeof(*trace))
        || memcmp(runtime,f->rw,f->slots*sizeof(*f->rw)) || memcmp(matrices,f->matrices,sizeof(matrices)))
    { fprintf(stderr,"Traversal differs: %s depth %g scale %g view %g bias %g disabled %d\n",f->asset->name,data.depth,scale,view,bias,disabled); abort(); }
    for (int i=0;i<f->asset->count;i++) { assert(children[i]==f->nodes[i].Child); }
    assert(!memcmp(&finalscale,&f->model.scale,sizeof(f32)) && !memcmp(&finalview,&viewscale,sizeof(f32)) && !memcmp(&finalbias,&g_ModelDistanceScale,sizeof(f32)));
    assert(rwlookups<=rw && matrixlookups<=mtx && depthlookups<=dep);
    cases++; oldrw+=rw; newrw+=rwlookups; olddepth+=dep; newdepth+=depthlookups;
    oldparents+=parents; newparents+=rwparents+matrixparents;
    free(children); free(runtime);
}
static void Native(void)
{
    for (unsigned a=0;a<sizeof(assets)/sizeof(*assets);a++)
    {
        Fixture f; Create(&f,&assets[a]);
        unsigned long beforeold=olddepth,beforenew=newdepth;
        for (int v=0;v<3;v++) for (int s=0;s<3;s++) for (int bias=0;bias<2;bias++)
        {
            f32 view=(f32[]){0.5f,1,1.3f}[v],scale=(f32[]){0.01f,0.1f,1}[s],factor=bias ? 1.2f : 1;
            for (int d=0;d<5;d++)
            {
                ModelRenderData data={(f32[]){-100,0,800,1800,1e8f}[d],d==2 ? 17.3f : 0,-1};
                Compare(&f,data,scale,view,factor,FALSE,d);
                Compare(&f,data,scale,view,factor,TRUE,d);
            }
            /* Around every authored minimum/maximum, including distant cull
               thresholds, test the adjacent representable floats. */
            for (int n=0;n<assets[a].count;n++) if ((assets[a].nodes[n].op&255)==MODELNODE_OPCODE_LOD)
            for (int bound=0;bound<2;bound++)
            {
                f32 distance=(bound ? assets[a].nodes[n].max : assets[a].nodes[n].min)*scale/view/factor;
                for (int side=-1;side<=1;side++)
                {
                    f32 depth=side<0 ? nextafterf(distance,-INFINITY) : side>0 ? nextafterf(distance,INFINITY) : distance;
                    Compare(&f,(ModelRenderData){depth,0,-1},scale,view,factor,FALSE,n);
                }
            }
        }
        if (!strcmp(assets[a].name,"CsnowguardZ") || !strcmp(assets[a].name,"CsuitbondZ"))
        { printf("%s: LOD depth evaluations %lu -> %lu across all test views.\n",assets[a].name,olddepth-beforeold,newdepth-beforenew); }
        Destroy(&f);
    }
    printf("PASS: %zu native character/head trees; authored thresholds, adjacent floats, varied joint depths, zoom/scale, switches and disabled LOD.\n",sizeof(assets)/sizeof(*assets));
}
static void Special(void)
{
    NodeSpec nodes[]={
        {1,-1,-1,-1,1,0,-1,0,0},
        {8,0,2,-1,3,0,3,0,1000},
        {8,0,-1,1,4,0,4,1000,1e8f},
        {24,1,-1,-1,-1,0,-1,0,0},
        {24,2,-1,-1,-1,0,-1,0,0},
    };
    Asset a={"edited LOD ranges",nodes,5,0}; Fixture f; Create(&f,&a);
    f32 values[]={-INFINITY,-100,-0.0f,0,1e-30f,0.1f,1000,nextafterf(1000,INFINITY),1e8f,INFINITY,NAN};
    for (unsigned x=0;x<sizeof(values)/sizeof(*values);x++) for (unsigned y=0;y<sizeof(values)/sizeof(*values);y++)
    {
        nodes[1].min=values[x]; nodes[1].max=values[y]; nodes[2].min=values[y]; nodes[2].max=values[x];
        for (unsigned d=0;d<sizeof(values)/sizeof(*values);d++)
        { Compare(&f,(ModelRenderData){values[d],0,-1},1,1,1.2f,FALSE,0); }
        Compare(&f,(ModelRenderData){999,0,-1},values[y],0.7f,values[x],TRUE,0);
    }
    nodes[1].min=0; nodes[1].max=1000; nodes[2].min=1000; nodes[2].max=1e8f;
    /* These active near-branch children all invalidate the cached context.
       Include every matrix opcode, repeated matrix IDs and callback changes. */
    int types[]={MODELNODE_OPCODE_HEADER,MODELNODE_OPCODE_GROUP,MODELNODE_OPCODE_OP03,MODELNODE_OPCODE_GROUPSIMPLE,
        MODELNODE_OPCODE_BSP,MODELNODE_OPCODE_SWITCH,MODELNODE_OPCODE_HEAD};
    for (unsigned t=0;t<sizeof(types)/sizeof(*types);t++)
    {
        nodes[3].op=types[t];
        Compare(&f,(ModelRenderData){800,0,3},1,1,1,FALSE,0);
    }
    Destroy(&f);
    /* The same head tree can belong to different body instances. Nested
       placeholders verify that relative runtime-data bases are respected. */
    NodeSpec headnodes[]={
        {1,-1,-1,-1,1,0,-1,0,0},
        {8,0,2,-1,3,0,3,0,1000},
        {8,0,-1,1,4,0,4,1000,1e8f},
        {23,1,-1,-1,-1,0,5,0,0},
        {24,2,-1,-1,-1,0,-1,0,0},
        {8,-1,6,-1,7,0,7,0,900},
        {8,-1,-1,5,8,0,8,900,1e8f},
        {23,5,-1,-1,-1,0,9,0,0},
        {24,6,-1,-1,-1,0,-1,0,0},
        {8,-1,10,-1,11,0,11,0,700},
        {8,-1,-1,9,12,0,12,700,1e8f},
        {24,9,-1,-1,-1,0,-1,0,0},
        {24,10,-1,-1,-1,0,-1,0,0},
    };
    Asset h={"nested attached heads",headnodes,13,0}; Create(&f,&h);
    for (int d=0;d<1500;d+=10)
    { Compare(&f,(ModelRenderData){d,0,-1},1,1,1,FALSE,0); Compare(&f,(ModelRenderData){d,0,-1},1,0.5f,1.2f,FALSE,0); }
    Destroy(&f);
    puts("PASS: overlapping/gapped/zero/negative/nonfinite ranges, matrix and relation invalidation, shared matrix indices, joint callbacks and nested attached-head runtime data.");
}
int main(void)
{
    setbuf(stdout,NULL); Native(); Special();
    assert(newdepth<olddepth && newparents<oldparents && newrw<oldrw);
    printf("PASS: %lu equivalent traversals. Runtime lookups %lu -> %lu; parent steps %lu -> %lu; depth calculations %lu -> %lu. These are operation counts, not N64 timings.\n",
        cases,oldrw,newrw,oldparents,newparents,olddepth,newdepth);
}
