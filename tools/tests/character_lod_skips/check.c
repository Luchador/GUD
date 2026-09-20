static unsigned cases, skips;
static u32 seed = 7;
static u32 random32(void) { seed = seed * 1664525U + 1013904223U; return seed; }
static ModelNode *nodeAt(Fixture *f,int index) { return index < 0 ? NULL : &f->nodes[index]; }

static void loadAsset(Fixture *f,const Asset *asset)
{
    int i,j;
    memset(f,0,sizeof(*f));
    f->header.RootNode = &f->nodes[asset->root];
    f->header.numMatrices = asset->matrices;
    f->model.obj = &f->header;
    f->model.datas = (ModelRwData **)f->rw;
    f->model.render_pos = f->matrices;
    f->model.scale = 1;
    for (i=0;i<32;i++) for (j=0;j<4;j++) f->matrices[i].pos.m[j][j]=1;
    for (i=0;i<asset->count;i++)
    {
        const NodeSpec *s = &asset->nodes[i];
        ModelNode *n = &f->nodes[i];
        ModelRoData *r = &f->ro[i];
        int rwindex = i * sizeof(ModelRwData) / sizeof(void *);
        n->Opcode=s->op; n->Data=r; n->Parent=nodeAt(f,s->parent);
        n->Next=nodeAt(f,s->next); n->Child=nodeAt(f,s->child);
        switch (s->op & 255)
        {
        case 1: r->Header.MatrixIndex=s->matrix; break;
        case 2: case 3: r->Group.MatrixID0=s->matrix; r->Group.MatrixID1=s->extra; break;
        case 21: r->GroupSimple.Group1=s->matrix; break;
        case 8:
            r->LOD.MinDistance=s->min; r->LOD.MaxDistance=s->max;
            r->LOD.Affects=nodeAt(f,s->affects); n->Child=NULL; break;
        case 9: r->BSP.leftChild=nodeAt(f,s->affects); r->BSP.rightChild=nodeAt(f,s->other); break;
        case 18: r->Switch.Controls=nodeAt(f,s->affects); n->Child=NULL; break;
        case 23: r->HeadPlaceholder.RwDataIndex=rwindex; break;
        case 24:
            r->DisplayListCollisions.Primary=s->primary<0?NULL:&ram[s->primary];
            r->DisplayListCollisions.Secondary=s->secondary<0?NULL:&ram[s->secondary];
            r->DisplayListCollisions.BaseAddr=ram;
            r->DisplayListCollisions.RwDataIndex=rwindex;
            f->rw[i].DisplayListCollisions.gdl=r->DisplayListCollisions.Primary;
            break;
        case 4:
            r->DisplayList.Primary=s->primary<0?NULL:&ram[s->primary];
            r->DisplayList.Secondary=s->secondary<0?NULL:&ram[s->secondary];
            r->DisplayList.BaseAddr=ram; break;
        }
    }
}

static bool lodVisible(Model *model,ModelNode *node)
{
    float distance=0;
    ModelRoData_LODRecord *r=&node->Data->LOD;
    if (!g_ModelDistanceDisabled)
    {
        distance=-modelFindNodeMtx(model,node,0)->m[3][2]*viewscale;
        if (g_ModelDistanceScale!=1) distance*=g_ModelDistanceScale;
    }
    return (r->MinDistance==0 || distance>r->MinDistance*model->scale)
        && distance<=r->MaxDistance*model->scale;
}

/* Independent oracle: all matrix loads in every visible authored display
 * list, not the optimized plan's dependency/gate lists. */
static u32 visibleMatrices(Fixture *f,const Asset *asset)
{
    int i,j;
    u32 mask=0;
    for (i=0;i<asset->count;i++)
    {
        const NodeSpec *s=&asset->nodes[i];
        ModelNode *ancestor=&f->nodes[i];
        bool visible=TRUE;
        while (ancestor)
        {
            if ((ancestor->Opcode&255)==8 && !lodVisible(&f->model,ancestor)) visible=FALSE;
            ancestor=ancestor->Parent;
        }
        if (!visible) continue;
        for (j=0;j<2;j++)
        {
            int at=j?s->secondary:s->primary;
            if (at<0) continue;
            while (commands[at].words.w0>>24!=G_ENDDL)
            {
                if (commands[at].words.w0>>24==G_MTX)
                {
                    u32 address=commands[at].words.w1;
                    assert(address>>24==3 && (address&0xffffff)/64<32);
                    mask |= 1U<<((address&0xffffff)/64);
                }
                at++;
            }
        }
    }
    return mask;
}

static unsigned checkPose(Fixture *f,const Asset *asset)
{
    int i;
    unsigned omitted=0;
    u32 used=visibleMatrices(f,asset);
    modelLodBegin(&f->model);
    for (i=0;i<asset->count;i++)
    {
        if (f->nodes[i].Opcode==0x102)
        {
            ModelRoData_GroupRecord *g=&f->ro[i].Group;
            Mtxf primary=f->matrices[g->MatrixID0].pos;
            if (modelLodSkipJoint(&f->model,g))
            {
                assert(!(used & (1U<<g->MatrixID1)));
                Mtxf identity;
                matrix_4x4_set_identity(&identity);
                assert(!memcmp(&identity,&f->matrices[g->MatrixID1].pos,sizeof(identity)));
                omitted++;
            }
            assert(!memcmp(&primary,&f->matrices[g->MatrixID0].pos,sizeof(primary)));
        }
    }
    modelLodEnd();
    cases++; skips+=omitted;
    return omitted;
}

static void setDepth(Fixture *f,float depth)
{
    int i;
    for (i=0;i<32;i++) f->matrices[i].pos.m[3][2]=-depth;
}

static void testAssets(void)
{
    Fixture f;
    unsigned a,accepted=0,bodies=0;
    for (a=0;a<sizeof(assets)/sizeof(assets[0]);a++)
    {
        int sample,i;
        const Asset *asset=&assets[a];
        loadAsset(&f,asset);
        modelLodResetCache();
        if ((f.header.RootNode->Opcode&255)!=1) continue;
        bodies++;
        if (modelLodGetPlan(&f.header)) accepted++;
        for (sample=0;sample<256;sample++)
        {
            const float scales[]={0.1f,1.0f,2.5f};
            float depth=sample%2?16000.0f:9000.0f;
            f.model.scale=scales[sample%3];
            g_ModelDistanceScale=sample%4?1.2f:1.0f;
            viewscale=sample%3?1.0f:0.25f;
            g_ModelDistanceDisabled=sample%19==0;
            for (i=0;i<32;i++) f.matrices[i].pos.m[3][2]=-(depth+(int)(random32()%9000)-4500)*f.model.scale;
            checkPose(&f,asset);
        }
    }
    printf("Native bodies: %u checked, %u conservative plans accepted\n",bodies,accepted);
}

static void callback(s32 index,Mtxf *matrix) { matrix->m[0][0]+=0.125f*(index+1); }

static void testBuilders(Fixture *f,const Asset *asset)
{
    int i,blend,near;
    for (near=0;near<2;near++) for (blend=0;blend<2;blend++)
    {
        setDepth(f,near?1000:20000);
        for (i=0;i<asset->count;i++) if (f->nodes[i].Opcode==0x102)
        {
            ModelNode *node=&f->nodes[i];
            ModelRoData_GroupRecord *group=&f->ro[i].Group;
            coord3d rotation={{{0.4f,1.2f,-0.6f}}};
            quatf q;
            Mtxf base={{{0}}};
            ModelRenderData data={&base};
            ModelGroupMtxBuildArg arg={0x102,0,group,node->Parent};
            Mtxf primary,extra;
            unsigned builds;
            base.m[0][0]=base.m[1][1]=base.m[2][2]=base.m[3][3]=1;
            quaternion_set_rotation_around_xyzf(rotation.f,q);
            g_ModelJointPositionedFunc=callback;
            modelLodEnd();
            quaternionBuilds=0;
            if (blend) sub_GAME_7F06DB5C(&data,&f->model,node,q);
            else modelBuildGroupMatrices(&data.basemtx,&f->model,&arg,&rotation);
            assert(quaternionBuilds==2);
            primary=f->matrices[group->MatrixID0].pos;
            extra=f->matrices[group->MatrixID1].pos;
            /* Reset all parent bases before repeating the identical pose. */
            modelLodBegin(&f->model);
            builds=quaternionBuilds;
            if (blend) sub_GAME_7F06DB5C(&data,&f->model,node,q);
            else modelBuildGroupMatrices(&data.basemtx,&f->model,&arg,&rotation);
            assert(!memcmp(&primary,&f->matrices[group->MatrixID0].pos,sizeof(primary)));
            assert(quaternionBuilds-builds==(near?2U:1U));
            if (near) assert(!memcmp(&extra,&f->matrices[group->MatrixID1].pos,sizeof(extra)));
            modelLodEnd();
        }
    }
    g_ModelJointPositionedFunc=NULL;
}

static void testMarine(unsigned marine)
{
    const Asset *asset=&assets[marine];
    Fixture f,head;
    ModelLodPlan *plan;
    unsigned a;
    int i;
    loadAsset(&f,asset);
    g_ModelDistanceDisabled=0; g_ModelDistanceScale=viewscale=1;
    modelLodResetCache();
    setDepth(&f,1000); assert(checkPose(&f,asset)==0);
    setDepth(&f,20000); assert(checkPose(&f,asset)==5);
    plan=modelLodGetPlan(&f.header);
    assert(plan && plan->numJoints==5);
    for (i=0;i<plan->numJoints;i++)
    {
        ModelLodJoint *joint=&plan->joints[i];
        int j;
        assert(joint->count==2);
        for (j=0;j<joint->count;j++)
        {
            ModelLodGate *gate=&joint->gates[j];
            float boundary=gate->lod->MaxDistance;
            setDepth(&f,20000);
            f.matrices[gate->matrix].pos.m[3][2]=-boundary;
            modelLodBegin(&f.model);
            assert(!modelLodSkipJoint(&f.model,&joint->node->Data->Group));
            f.matrices[gate->matrix].pos.m[3][2]=-nextafterf(boundary,INFINITY);
            modelLodBegin(&f.model);
            assert(modelLodSkipJoint(&f.model,&joint->node->Data->Group));
            modelLodEnd();
            checkPose(&f,asset);
        }
    }
    setDepth(&f,20000);
    g_ModelDistanceDisabled=1; assert(checkPose(&f,asset)==0); g_ModelDistanceDisabled=0;
    viewscale=0.25f; assert(checkPose(&f,asset)==0); viewscale=1;
    f.model.scale=3; assert(checkPose(&f,asset)==0); f.model.scale=1;
    /* Runtime list replacement disables the optimization for this instance. */
    {
        ModelRoData_DisplayList_CollisionRecord *dl=plan->lists[0];
        ModelRwData_DisplayList_CollisionRecord *rw=(void *)&f.model.datas[dl->RwDataIndex];
        modelLodInvalidateInstance(&f.model);
        rw->gdl=NULL; assert(checkPose(&f,asset)==0); rw->gdl=dl->Primary;
    }
    for (a=0;a<sizeof(assets)/sizeof(assets[0]);a++)
    {
        ModelRwData_HeadPlaceholderRecord *rw;
        if ((assets[a].nodes[assets[a].root].op&255)==1) continue;
        modelLodResetCache(); plan=modelLodGetPlan(&f.header);
        loadAsset(&head,&assets[a]);
        rw=modelGetNodeRwData(&f.model,plan->head);
        rw->ModelFileHeader=&head.header; rw->RwDatas=head.model.datas;
        head.header.RootNode->Parent=plan->head;
        checkPose(&f,asset);
        rw->ModelFileHeader=NULL;
    }
    modelLodResetCache();
    testBuilders(&f,asset);
    /* An edited head may consume a body smoothing matrix even at a distance. */
    {
        ModelNode customNode={0};
        ModelRoData customData={0};
        ModelFileHeader customHeader={&customNode,0};
        ModelRwData customRw={0};
        ModelRwData_HeadPlaceholderRecord *rw;
        Gfx *custom=ram+sizeof(commands)/sizeof(commands[0]);
        int extra;
        modelLodResetCache(); plan=modelLodGetPlan(&f.header);
        extra=plan->joints[0].node->Data->Group.MatrixID1;
        custom[0].words.w0=0x01020040;
        custom[0].words.w1=0x03000000+64*extra;
        custom[1].words.w0=0xb8000000; custom[1].words.w1=0;
        customNode.Opcode=24; customNode.Data=&customData;
        customData.DisplayListCollisions.Primary=custom;
        customData.DisplayListCollisions.BaseAddr=ram;
        customRw.DisplayListCollisions.gdl=custom;
        rw=modelGetNodeRwData(&f.model,plan->head);
        rw->ModelFileHeader=&customHeader; rw->RwDatas=&customRw;
        setDepth(&f,20000);
        modelLodBegin(&f.model);
        assert(!modelLodSkipJoint(&f.model,&plan->joints[0].node->Data->Group));
        modelLodEnd();
        rw->ModelFileHeader=NULL;
        modelLodResetCache();
    }
    printf("CarmourguardZ: far pose skips 5/21 matrices; near pose and both joint builders preserved\n");
}

static void testFallbacks(unsigned marine)
{
    Fixture f;
    ModelLodPlan *plan;
    Gfx *list,saved;
    u32 mask;
    int i;
    ModelFileHeader headers[17];
    loadAsset(&f,&assets[marine]); modelLodResetCache();
    plan=modelLodGetPlan(&f.header); assert(plan);
    list=plan->lists[0]->Primary; assert(list);
    saved=*list;
    for (i=0;i<4;i++)
    {
        const u32 rejected[]={0x06000000,0xbc000006,0x03000000,0x01020080};
        list->words.w0=rejected[i];
        modelLodResetCache(); assert(!modelLodGetPlan(&f.header));
    }
    *list=saved; modelLodResetCache(); assert(modelLodGetPlan(&f.header));
    mask=0;
    assert(modelLodListMatrices((Gfx *)(uintptr_t)(0x05000000+K0_TO_PHYS(list)),ram,&mask));
    assert(!modelLodListMatrices((Gfx *)((u8 *)ram+sizeof(ram)-4),ram,&mask));
    assert(!modelLodListMatrices((Gfx *)(uintptr_t)0x06000000,ram,&mask));
    modelLodResetCache();
    for (i=0;i<17;i++)
    {
        headers[i]=f.header;
        assert((modelLodGetPlan(&headers[i])!=NULL)==(i<16));
    }
    modelLodResetCache(); assert(modelLodGetPlan(&headers[16]));
    modelLodResetCache();
    plan=modelLodGetPlan(&f.header);
    plan->joints[0].node->Data->Group.MatrixID1=1;
    modelLodResetCache(); assert(!modelLodGetPlan(&f.header));
    puts("Fallbacks: indirect/dynamic lists, custom matrices, cache exhaustion and invalidation checked");
}

int main(void)
{
    unsigned a,marine=0;
    assert(sizeof(commands)<sizeof(ram)); memcpy(ram,commands,sizeof(commands));
    for (a=0;a<sizeof(assets)/sizeof(assets[0]);a++) if (!strcmp(assets[a].name,"CarmourguardZ")) marine=a;
    testAssets(); testMarine(marine); testFallbacks(marine);
    printf("%u poses checked; %u unused transforms skipped; no live matrix omitted\n",cases,skips);
    return 0;
}
