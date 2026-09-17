int main(void)
{
    Vertex vertices[5] = {{0}}, deformed = {1};
    Gfx lists[5] = {{0}}, frame[32];
    union ModelRoData ro[5] = {0};
    ModelNode nodes[5];
    ModelFileHeader header = {0};
    Model model = {0}, intact;
    MultiMonitorObjRecord console = {0};
    PropRecord prop = {0};
    ModelRenderData data = {0};
    for (int i = 0; i < 5; i++) {
        ro[i].DisplayListCollisions.Vertices = &vertices[i];
        ro[i].DisplayListCollisions.RwDataIndex = i;
        nodes[i].Opcode = MODELNODE_OPCODE_DLCOLLISION; nodes[i].Data = &ro[i];
        model.rw[i].DisplayListCollisions.gdl = &lists[i];
        model.rw[i].DisplayListCollisions.Vertices = &vertices[i];
        model.datas[i] = &vertices[i];
        if (i) header.Switches[i - 1] = &nodes[i];
    }
    model.obj = &header; model.body = &nodes[0]; intact = model;
    console.obj.model = &model; console.obj.type = PROPDEF_MULTI_MONITOR;
    prop.obj = &console.obj; prop.flags = PROPRUNTIMEFLAG_ONSCREEN;
    data.gdl = frame; data.flags = 1; data.PropType = PROP_TYPE_MAX;
    objRenderPropModel(&prop, &data, FALSE);
    assert(screenBuilds == 4 && draws == 1 && lastCull == CULLMODE_BACK);
    assert(lastFlags == 1 && data.flags == 1);

    /* The other console shares source nodes, but has independent runtime data. */
    console.obj.state = PROPSTATE_DESTROYED;
    objHideMonitorScreens(&console.obj);
    assert(model.rw[0].DisplayListCollisions.gdl == &lists[0]);
    for (int i = 1; i < 5; i++) {
        assert(!model.rw[i].DisplayListCollisions.gdl);
        assert(model.rw[i].DisplayListCollisions.Vertices == &vertices[i]);
        assert(intact.rw[i].DisplayListCollisions.gdl == &lists[i]);
    }
    for (int deform = 0; deform < 2; deform++) for (int stage = 0; stage < 4; stage++) {
        console.obj.maxdamage = stage * 4;
        model.datas[0] = deform ? &deformed : &vertices[0];
        for (int pass = 1; pass <= 3; pass++) {
            data.flags = pass; data.cullmode = CULLMODE_NONE;
            objRenderPropModel(&prop, &data, pass == 2);
            assert(screenBuilds == 4 && lastCull == CULLMODE_BACK);
            assert(lastFlags == ((pass & ~2u) | MODEL_RENDER_HIDE_TRANSLUCENT));
            assert(data.flags == (u32)pass);
            int alpha = (stage + 1) * 50 + 100;
            assert(lastEnv == (deform ? (alpha > 255 ? 255 : alpha) : 0));
        }
    }
    data.PropType = PROP_TYPE_PLAYER; data.envcolour.word = 123;
    objRenderPropModel(&prop, &data, TRUE);
    assert(lastCull == CULLMODE_BACK && data.envcolour.word == (123 | 0xff00));
    assert(screenBuilds == 4);

    /* Clearing destruction on respawn permits the builder to restore screens. */
    console.obj.state = 0; console.obj.maxdamage = 0; model.datas[0] = &vertices[0];
    data.flags = 1; data.PropType = PROP_TYPE_MAX;
    objRenderPropModel(&prop, &data, FALSE);
    assert(screenBuilds == 8 && lastEnv == 0);
    assert(lastFlags == 1 && data.flags == 1);
    for (int i = 1; i < 5; i++) assert(model.rw[i].DisplayListCollisions.gdl == &screenList);

    /* Single-screen monitors, missing nodes and unrelated objects. */
    console.obj.type = PROPDEF_MONITOR;
    objHideMonitorScreens(&console.obj);
    assert(!model.rw[1].DisplayListCollisions.gdl && model.rw[2].DisplayListCollisions.gdl);
    header.Switches[0] = NULL; objHideMonitorScreens(&console.obj);
    header.Switches[0] = &nodes[1]; nodes[1].Opcode = 1;
    objHideMonitorScreens(&console.obj); nodes[1].Opcode = MODELNODE_OPCODE_DLCOLLISION;
    console.obj.type = PROPDEF_OBJ;
    objHideMonitorScreens(&console.obj);
    assert(model.rw[2].DisplayListCollisions.gdl == &screenList);

    /* A destroyed parent must not hide the alpha pass of its live children;
     * a destroyed child must not affect its parent or following sibling. */
    ObjectRecord live = {.type=PROPDEF_OBJ, .model=&intact};
    ObjectRecord broken = {.type=PROPDEF_OBJ, .model=&model, .state=PROPSTATE_DESTROYED};
    PropRecord liveChild = {.obj=&live, .flags=PROPRUNTIMEFLAG_ONSCREEN};
    PropRecord brokenChild = {.obj=&broken, .flags=PROPRUNTIMEFLAG_ONSCREEN, .prev=&liveChild};
    prop.child = &brokenChild;
    for (int destroyed = 0; destroyed < 2; destroyed++) for (int pass = 1; pass <= 3; pass++) {
        int first = draws;
        console.obj.state = destroyed ? PROPSTATE_DESTROYED : 0;
        data.flags = pass; data.gdl = frame;
        objRenderPropModel(&prop, &data, pass != 1);
        assert(draws == first + 3);
        assert(drawnFlags[first] == (destroyed ? (pass & ~2u) | MODEL_RENDER_HIDE_TRANSLUCENT : (u32)pass));
        assert(drawnFlags[first+1] == ((pass & ~2u) | MODEL_RENDER_HIDE_TRANSLUCENT));
        assert(drawnFlags[first+2] == (u32)pass && data.flags == (u32)pass);
    }
    prop.child = NULL;
    /* State, not accumulated damage, controls hiding. Respawn restores alpha. */
    prop.obj = &broken; broken.state = 0; broken.maxdamage = 1000;
    data.flags = 2; objRenderPropModel(&prop,&data,TRUE);
    assert(lastFlags == 2 && data.flags == 2);
    broken.state = PROPSTATE_DESTROYED; broken.maxdamage = 0;
    data.flags = 3; data.PropType = PROP_TYPE_PLAYER;
    objRenderPropModel(&prop,&data,TRUE);
    assert(lastFlags == (1 | MODEL_RENDER_HIDE_TRANSLUCENT) && data.flags == 3);

    /* Doors retain their authored front/back culling convention. */
    DoorRecord door = {0}; door.obj.model = &model; door.obj.type = PROPDEF_DOOR;
    prop.obj = &door.obj; prop.door = &door;
    for (int flip = 0; flip < 2; flip++) {
        door.doorFlags = flip ? DOORFLAG_FLIP : 0;
        data.gdl = frame; objRenderPropModel(&prop, &data, FALSE);
        assert(lastCull == (flip ? CULLMODE_FRONT : CULLMODE_BACK));
    }
    puts("Destroyed props: per-instance screen removal, no rebuild while destroyed, respawn, damage stages, fading and door culling pass.");
    puts("Destroyed translucency: immediate removal, primary geometry during fading, independent children/siblings and respawn pass.");
    return 0;
}
