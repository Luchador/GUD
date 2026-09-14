typedef struct Fixture {
    ModelNode nodes[4], attachment;
    ModelRoData data[4], attachmentData;
    ModelFileHeader file, bodyfile;
    Model model, body;
    ObjectRecord object;
    PropRecord prop;
    ChrRecord chr;
    RenderPosView bodyMatrices[2];
} Fixture;

static void reset_pool(int count)
{
    assert(count >= 0 && count <= 80);
    memset(entries, 0, sizeof(entries));
    for (int i = 0; i < count; i++) {
        entries[i].prev = i ? entries + i - 1 : NULL;
        entries[i].next = i + 1 < count ? entries + i + 1 : NULL;
    }
    g_ModelHitFreeList = count ? entries : NULL;
}

static int check_links(ModelHitEntry *head)
{
    ModelHitEntry *prev = NULL;
    int count = 0;
    while (head) {
        assert(head->prev == prev && count++ < 80);
        prev = head; head = head->next;
    }
    return count;
}

static void fixture(Fixture *f, int asset)
{
    coord3d zero = {0};
    memset(f, 0, sizeof(*f));
    f->nodes[0].Opcode = MODELNODE_OPCODE_GROUPSIMPLE;
    f->nodes[1].Opcode = MODELNODE_OPCODE_BBOX;
    f->nodes[2].Opcode = MODELNODE_OPCODE_DL;
    for (int i = 0; i < 3; i++) {
        f->nodes[i].Data = f->data + i;
        f->nodes[i].Parent = i ? f->nodes + i - 1 : NULL;
        f->nodes[i].Child = i < 2 ? f->nodes + i + 1 : NULL;
    }
    f->data[0].GroupSimple.Origin = assets[asset].origin;
    f->file.RootNode = f->nodes; f->file.numMatrices = 1;
    f->model.obj = &f->file; f->model.attachedto_objinst = &f->attachment;
    f->object.obj = assets[asset].id; f->object.model = &f->model;
    f->prop.obj = &f->object;
    f->attachment.Opcode = MODELNODE_OPCODE_GROUPSIMPLE;
    f->attachment.Data = &f->attachmentData;
    f->body.obj = &f->bodyfile; f->body.render_pos = f->bodyMatrices;
    f->bodyfile.RootNode = &f->attachment; f->bodyfile.numMatrices = 2;
    f->chr.model = &f->body; f->chr.handle_positiondata_hat = &f->prop;
    f->chr.headnum = HEAD_START;
    matrix_4x4_set_identity_and_position(&zero, &f->bodyMatrices[0].pos);
    matrix_4x4_set_identity_and_position(&zero, &f->bodyMatrices[1].pos);
    f->bodyMatrices[1].pos.m[3][0] = 75.0f;
}

/* Original sequence, including generic model traversal and fitting only matrix
 * zero after that traversal. Uses production math, independently of the cache. */
static bool reference_pose(Fixture *f, Mtxf result[2])
{
    ModelRenderData data;
    coord3d offset, scale;
    int type, visible = TRUE;
    frameCount = 0;
    data.basemtx = modelFindNodeMtx(f->chr.model, f->model.attachedto_objinst, 0);
    data.mtxlist = dynAllocate(f->file.numMatrices * sizeof(Mtxf));
    instcalcmatrices(&data, &f->model);
    if (f->chr.headnum >= HEAD_START && f->chr.headnum < BODY_Female_Sally) {
        type = get_hat_model(&f->prop);
        if ((u32)type < 6) { /* Fixed out-of-bounds case has no old valid result. */
            struct headHat *fit = &g_HeadHatDefs[(f->chr.headnum - HEAD_START) * 6 + type];
            offset.x = fit->xoffset * 21.3f; offset.y = fit->yoffset * 21.3f; offset.z = fit->zoffset * 21.3f;
            scale.x = fit->xsize; scale.y = fit->ysize; scale.z = fit->zsize;
            matrix_4x4_apply_scale_and_translation(&f->model.render_pos[0].pos, &scale, &offset);
            visible = type != HATTYPE_PEAKED;
        }
    }
    memcpy(result, f->model.render_pos, f->file.numMatrices * sizeof(Mtxf));
    return visible;
}

static float largestError;
static void compare_pose(Fixture *f)
{
    Mtxf expected[2];
    ModelHitList list = {0}; ModelRenderData data;
    int visible = reference_pose(f, expected), generic = genericUpdates;
    frameCount = 0; reset_pool(8);
    assert(chrUpdateHat(&f->chr, &data, &list) == visible);
    assert(frameCount == f->file.numMatrices && data.mtxlist == (Mtxf *)(frameMatrices + frameCount));
    assert(f->prop.flags & PROPRUNTIMEFLAG_ONSCREEN);
    assert(genericUpdates == generic + !f->chr.hatcache.simple);
    assert(list.head->model == &f->model && list.head->rootnode == f->nodes);
    for (int m = 0; m < f->file.numMatrices; m++) for (int i = 0; i < 4; i++) for (int j = 0; j < 4; j++) {
        float a = expected[m].m[i][j], b = f->model.render_pos[m].pos.m[i][j];
        float error = fabsf(a - b);
        if (error > largestError) largestError = error;
        assert(isfinite(a) && isfinite(b) && error < 0.01f);
        if (i != 3 || m != 0) assert(a == b); /* Only reassociated root translation may differ. */
    }
}

static uint32_t seed = 42;
static float random_float(float range)
{
    seed = seed * 1664525u + 1013904223u;
    return (((seed >> 8) / 16777216.0f) * 2.0f - 1.0f) * range;
}

static void matrix_checks(void)
{
    Fixture f;
    int count = 0;
    for (int asset = 0; asset < 12; asset++) {
        fixture(&f, asset);
        assert(chrHatHasSimpleModel(&f.file));
        for (int head = HEAD_START; head < BODY_Female_Sally; head++) {
            f.chr.headnum = head;
            for (int pose = 0; pose < 32; pose++) {
                for (int i = 0; i < 4; i++) for (int j = 0; j < 3; j++)
                    f.bodyMatrices[0].pos.m[i][j] = random_float(i == 3 ? 10000.0f : 25.0f);
                compare_pose(&f); count++;
            }
        }
        f.chr.headnum = -1; compare_pose(&f);
        f.chr.headnum = BODY_Female_Sally; compare_pose(&f);
        f.chr.headnum = HEAD_START; f.object.obj = PROP_ALARM1; compare_pose(&f);
        f.object.obj = assets[asset].id; f.model.attachedto_objinst = NULL; compare_pose(&f);
    }
    printf("Hats: %d poses across all 12 assets and 168 fitting entries pass; largest matrix difference %.9g.\n", count, largestError);
}

static void cache_checks(void)
{
    Fixture f;
    ModelFileHeader replacementBody;
    ModelHitList list; ModelRenderData data;
    int types, matrices;
    fixture(&f, 0);
    replacementBody = f.bodyfile;
    for (int i = 0; i < 5; i++) {
        frameCount = 0; reset_pool(8); memset(&list, 0, sizeof(list));
        types = typeLookups; matrices = matrixLookups;
        chrUpdateHat(&f.chr, &data, &list);
        assert(typeLookups == types + (i == 0) && matrixLookups == matrices + (i == 0));
        f.bodyMatrices[0].pos.m[3][2] += 20.0f;
    }
    for (int change = 0; change < 5; change++) {
        if (change == 0) f.chr.headnum++;
        if (change == 1) f.object.obj = PROP_HATPEAKED;
        if (change == 2) f.model.attachedto_objinst = NULL;
        if (change == 3) f.chr.hatcache.model = NULL; /* Slot reuse / reattachment. */
        if (change == 4) f.body.obj = &replacementBody;
        frameCount = 0; reset_pool(8); memset(&list, 0, sizeof(list));
        types = typeLookups; matrices = matrixLookups;
        chrUpdateHat(&f.chr, &data, &list);
        assert(typeLookups == types + 1 && matrixLookups == matrices + 1);
    }
    f.model.attachedto_objinst = &f.attachment;
    f.attachmentData.GroupSimple.Group1 = 1; compare_pose(&f);
    f.data[0].GroupSimple.Origin.x += 150.0f; compare_pose(&f); /* Live local origin. */
    fixture(&f, 0);
    for (int drop = 0; drop < 2; drop++) for (int projectile = 0; projectile < 2; projectile++) {
        f.chr.hidden = drop ? CHRHIDDEN_DROP_HELD_ITEMS : 0;
        f.object.runtime_bitflags = projectile ? RUNTIMEBITFLAG_HASPROJECTILE : 0;
        frameCount = 0; reset_pool(8); memset(&list, 0, sizeof(list));
        chrUpdateHat(&f.chr, &data, &list);
        assert(frameCount == 1 && (list.head != NULL) == !(drop && projectile));
    }
    puts("Cache: reuse, moving views, head/hat/body/attachment changes, unknown hats, live origins and drop-frame poses pass.");
}

static void fallback_checks(void)
{
    Fixture f;
    for (int change = 0; change < 11; change++) {
        fixture(&f, 0);
        switch (change) {
        case 0: f.file.numMatrices = 2; break;
        case 1: f.nodes[0].Opcode = MODELNODE_OPCODE_GROUP; break;
        case 2: f.nodes[1].Opcode = MODELNODE_OPCODE_LOD; break;
        case 3: f.nodes[1].Opcode = MODELNODE_OPCODE_SWITCH; break;
        case 4: f.nodes[2].Opcode = MODELNODE_OPCODE_DLCOLLISION; break;
        case 5: f.nodes[2].Child = f.nodes + 3; break;
        case 6: f.nodes[1].Next = f.nodes + 3; break;
        case 7: f.nodes[2].Parent = f.nodes; break;
        case 8: f.data[0].GroupSimple.Group1 = 1; break;
        case 9: f.nodes[2].Data = NULL; break;
        case 10: f.nodes[0].Parent = f.nodes + 3; break;
        }
        assert(!chrHatHasSimpleModel(&f.file));
    }
    fixture(&f, 0);
    f.file.numMatrices = 2;
    f.nodes[2].Child = f.nodes + 3;
    f.nodes[3].Parent = f.nodes + 2; f.nodes[3].Data = f.data + 3;
    f.nodes[3].Opcode = MODELNODE_OPCODE_GROUPSIMPLE;
    f.data[3].GroupSimple.Group1 = 1; f.data[3].GroupSimple.Origin.x = 50.0f;
    compare_pose(&f);
    assert(!f.chr.hatcache.simple && genericUpdates > 0);
    puts("Fallback: non-stock node layouts rejected; extra-joint matrices and original fitting order preserved.");
}

static void list_checks(void)
{
    ModelNode nodes[30] = {0}; ModelFileHeader files[3] = {0}; Model models[3] = {0};
    ModelNode *expectedNodes[80]; Model *expectedModels[80];
    ModelHitList list;
    int expectedCount;
    for (int i = 0; i < 30; i++) {
        nodes[i].Opcode = i % 3 ? MODELNODE_OPCODE_GROUPSIMPLE : MODELNODE_OPCODE_BBOX;
        if (i % 10 != 9) nodes[i].Next = nodes + i + 1;
    }
    for (int m = 0; m < 3; m++) { models[m].obj = files + m; files[m].RootNode = nodes + m * 10; }
    for (int capacity = 0; capacity <= 80; capacity++) {
        ModelHitEntry *head = NULL, *entry;
        reset_pool(capacity);
        for (int m = 0; m < 3; m++) head = referenceBuildNodeList(head, models + m);
        expectedCount = 0;
        for (entry = head; entry; entry = entry->next) {
            expectedNodes[expectedCount] = entry->rootnode;
            expectedModels[expectedCount++] = entry->model;
        }
        reset_pool(capacity); memset(&list, 0, sizeof(list));
        for (int m = 0; m < 3; m++) modelHitAppendModel(&list, models + m);
        assert(check_links(list.head) == expectedCount);
        assert(check_links(g_ModelHitFreeList) + expectedCount == capacity);
        entry = list.head;
        for (int i = 0; i < expectedCount; i++, entry = entry->next) {
            assert(entry->rootnode == expectedNodes[i] && entry->model == expectedModels[i]);
            entry->sortvalue = (i % 4) * 2.0f;
            if (i == expectedCount - 1) assert(list.tail == entry && !entry->next);
        }
        head = modelHitSortByDepth(list.head);
        assert(check_links(head) == expectedCount);
        modelHitFreeChain(head); assert(check_links(g_ModelHitFreeList) == capacity);
        /* One-entry append has identical behavior when the pool is full/empty. */
        memset(&list, 0, sizeof(list));
        for (int i = 0; i < 82; i++) modelHitAppendNode(&list, models, nodes);
        assert(check_links(list.head) == capacity && !g_ModelHitFreeList);
        modelHitFreeChain(list.head); assert(check_links(g_ModelHitFreeList) == capacity);
        /* Legacy title/menu callers still append to a pre-existing chain. */
        head = NULL;
        for (int m = 0; m < 3; m++) head = modelHitBuildNodeList(head, models + m);
        assert(check_links(head) == expectedCount);
        modelHitFreeChain(head); assert(check_links(g_ModelHitFreeList) == capacity);
    }
    puts("Hit lists: exact pre-patch node order, both links, partial/full pool exhaustion, sorting, release and legacy callers pass.");
}

static void weapon_checks(void)
{
    Fixture f;
    ModelHitList list;
    Mtxf saved, expected, rotation;
    for (int hand = 0; hand < 2; hand++) for (int state = 0; state < 5; state++) {
        fixture(&f, 0); memset(&list, 0, sizeof(list)); reset_pool(8); frameCount = 0;
        f.chr.weapons_held[hand] = state == 4 ? NULL : &f.prop;
        if (state == 1) f.object.runtime_bitflags = RUNTIMEBITFLAG_00000800;
        if (state == 2) f.object.flags2 = PROPFLAG2_ONLYEXPLOSIONDAMAGE;
        if (state == 3) f.object.runtime_bitflags = RUNTIMEBITFLAG_HASPROJECTILE;
        saved = f.bodyMatrices[0].pos;
        chrRenderHeldWeapon(&f.chr, hand, &list);
        assert(frameCount == (state == 0 || state == 3));
        assert(check_links(list.head) == (state == 0));
        assert(!memcmp(&saved, &f.bodyMatrices[0].pos, sizeof(saved)));
        if (state == 0 || state == 3) {
            Mtxf *parent = &saved;
            if (hand == GUNLEFT) {
                matrix_4x4_set_rotation_around_z(M_PI_F, &rotation);
                matrix_4x4_multiply_in_place(&saved, &rotation); parent = &rotation;
            }
            matrix_4x4_multiply_translation(parent, &f.data[0].GroupSimple.Origin, &expected);
            assert(!memcmp(&expected, &f.model.render_pos[0].pos, sizeof(expected)));
            assert(f.prop.flags & PROPRUNTIMEFLAG_ONSCREEN);
        }
    }
    puts("Held weapons: typed builder, both hands, left-hand rotation, visibility exclusions and projectile suppression pass.");
}

int main(void)
{
    matrix_checks(); cache_checks(); fallback_checks(); list_checks(); weapon_checks();
    return 0;
}
