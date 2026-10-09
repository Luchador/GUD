static void CrossCopies(const BgDocument *clip,const BgDocument *doc,const BgFaceRef *refs,
    DWORD count,DWORD room,const double offset[3])
{
    DWORD index=0;
    for(DWORD r=1;r<=clip->roomcount;r++)for(DWORD f=0;f<clip->rooms[r].facecount;f++) {
        const BgDocumentRoom *a=&clip->rooms[r],*b;
        const BgDocumentFace *x=&a->faces[f],*y=BgDocumentFindFace(doc,&refs[index++],&b);
        assert(y&&y->room==room&&x->layer==y->layer&&x->uvseams==y->uvseams);
        assert(x->textureid==y->textureid&&x->cullbackfaces==y->cullbackfaces&&BgMaterialEqual(&x->material,&y->material));
        for(int c=0;c<3;c++) {
            const BgDocumentVertex *v=&a->vertices[x->vertexindices[c]],*w=&b->vertices[y->vertexindices[c]];
            double xyz[3]={v->x,v->y,v->z};int actual[3]={w->x,w->y,w->z};
            for(int axis=0;axis<3;axis++)
                assert(actual[axis]==round(((a->origin[axis]+xyz[axis])/clip->levelscale+offset[axis])*doc->levelscale-b->origin[axis]));
            assert(v->s==w->s&&v->t==w->t&&v->flag==w->flag&&!memcmp(&v->r,&w->r,4));
        }
        BgFaceRef old={x->id,r,x->layer,0};BgRenderState s,t;
        assert(BgDocumentGetFaceRenderStates(clip,&old,1,&s)&&BgDocumentGetFaceRenderStates(doc,&refs[index-1],1,&t));
        assert(s.othermode==t.othermode&&s.othermodehigh==t.othermodehigh&&s.environmentalpha==t.environmentalpha
            &&s.primitiveword0==t.primitiveword0&&s.primitiveword1==t.primitiveword1
            &&s.surfacepolicy==t.surfacepolicy&&s.surfacebasemode==t.surfacebasemode);
    }
    assert(index==count);
}
static void CrossGeometry(const char *dir)
{
    BgFile source=Fixture();BgDocument original={0},clip={0},beforeclip={0},dest={0},before={0};
    BgFaceRef refs[20],*pasted=NULL;DWORD count;const char *why="";double center[3],offset[3];
    assert(BgDocumentLoad(source.data,source.size,.25f,&original,&why));Refs(&original,refs);
    original.rooms[1].origin[0]=-100000;original.rooms[2].origin[0]=-99990;
    for(DWORD r=1;r<=2;r++)for(DWORD v=0;v<original.rooms[r].vertexcount;v++) {
        BgDocumentVertex *p=&original.rooms[r].vertices[v];p->s=v*211;p->t=v*71-300;p->r=v*33;p->g=201;p->b=80;p->a=150+v;
    }
    BgFaceRef selected[]={refs[1],refs[3],refs[6],refs[11],refs[16]};
    assert(BgDocumentCopyFaces(&original,selected,5,&clip,&why));
    assert(BgDocumentCopiedFaceCenter(&clip,center));assert(BgDocumentClone(&clip,&beforeclip,&why));
    /* Destination has fewer rooms, a different scale, and fractional origins. */
    BgDocument view=original;view.roomcount=1;view.facecount=view.rooms[1].facecount;
    assert(BgDocumentClone(&view,&dest,&why));dest.levelscale=.75f;
    dest.rooms[1].origin[0]=8192.5f;dest.rooms[1].origin[1]=-900.25f;dest.rooms[1].origin[2]=4096.75f;
    assert(BgDocumentClone(&dest,&before,&why));
    for(int axis=0;axis<3;axis++)offset[axis]=dest.rooms[1].origin[axis]/dest.levelscale+100.125-center[axis];
    assert(BgDocumentPasteFacesToRoom(&dest,&clip,1,offset,&pasted,&count,&why));
    CrossCopies(&clip,&dest,pasted,count,1,offset);UseCounts(&dest);Same(&clip,&beforeclip);
    assert(!memcmp(dest.rooms[1].vertices,before.rooms[1].vertices,before.rooms[1].vertexcount*sizeof(BgDocumentVertex)));
    RoundTrip(&dest,&source,dir);free(pasted);BgDocumentFree(&dest);
    BOOL succeeded=FALSE;
    for(int budget=0;budget<450&&!succeeded;budget++) {
        assert(BgDocumentClone(&before,&dest,&why));allocations=budget;
        BOOL ok=BgDocumentPasteFacesToRoom(&dest,&clip,1,offset,&pasted,&count,&why);allocations=-1;
        if(!ok){assert(!pasted&&!count&&why[0]);Same(&dest,&before);}else{succeeded=TRUE;free(pasted);}
        Same(&clip,&beforeclip);BgDocumentFree(&dest);
    }
    assert(succeeded&&BgDocumentClone(&before,&dest,&why));
    assert(!BgDocumentPasteFacesToRoom(&dest,&clip,0,offset,&pasted,&count,&why)&&!pasted&&!count);
    assert(!BgDocumentPasteFacesToRoom(&dest,&clip,2,offset,&pasted,&count,&why)&&!pasted&&!count);
    assert(!BgDocumentPasteFacesToRoom(&dest,&clip,1,(double[]){NAN,0,0},&pasted,&count,&why));
    assert(!BgDocumentPasteFacesToRoom(&dest,&clip,1,(double[]){1e10,0,0},&pasted,&count,&why));Same(&dest,&before);
    BgDocumentFree(&original);BgDocumentFree(&clip);BgDocumentFree(&beforeclip);BgDocumentFree(&before);BgDocumentFree(&dest);BgFileFree(&source);
    puts("PASS: cross-level/scaled paste, fractional room origins, large relocation, multi-room/layer snapshot, material/state/UV/color preservation, native save and atomic failures.");
}
static void CrossCommands(const char *dir)
{
    BgFile source=Fixture();BgDocument before={0},clip={0};BgFaceRef refs[20];const char *why="";
    tool=EDITOR_TOOL_FACE_SELECT;g_CurrentLevelIndex=40;
    assert(BgDocumentLoad(source.data,source.size,.25f,&g_CurrentBgDocument,&why));Refs(&g_CurrentBgDocument,refs);
    selection[0]=refs[1];selection[1]=refs[11];selectedcount=2;
    assert(GEditorCopySelectedBgFaces((HWND)1)&&g_FaceClipboardLevel==40);
    assert(BgDocumentClone(&g_FaceClipboard,&clip,&why));
    /* Successful level switch releases source geometry but retains its snapshot. */
    BgDocumentFree(&g_CurrentBgDocument);g_CurrentLevelIndex=41;selectedcount=0;
    assert(BgDocumentLoad(source.data,source.size,.5f,&g_CurrentBgDocument,&why));
    assert(BgDocumentClone(&g_CurrentBgDocument,&before,&why));
    EditHistoryReset(&g_EditHistory,&g_CurrentBgDocument,&g_CurrentSetup,&g_CurrentStan);
    assert(GEditorPasteBgFaces((HWND)1));CrossCopies(&clip,&g_CurrentBgDocument,selection,2,1,(double[]){0,10,0});
    assert(EditHistoryUndo(&g_EditHistory,&g_CurrentBgDocument,&g_CurrentSetup,&g_CurrentStan,NULL,&why));Same(&before,&g_CurrentBgDocument);
    ViewportObjectPaste target={.room=2,.position={50,80,120}};double center[3],offset[3];
    assert(BgDocumentCopiedFaceCenter(&clip,center));for(int a=0;a<3;a++)offset[a]=target.position[a]-center[a];
    assert(GEditorPasteBgFacesHere((HWND)1,&target)&&g_EditHistory.undocount==1);
    assert(!strcmp(EditHistoryGetUndoAction(&g_EditHistory),"Paste Faces Here"));
    CrossCopies(&clip,&g_CurrentBgDocument,selection,2,2,offset);RoundTrip(&g_CurrentBgDocument,&source,dir);
    assert(EditHistoryUndo(&g_EditHistory,&g_CurrentBgDocument,&g_CurrentSetup,&g_CurrentStan,NULL,&why));Same(&before,&g_CurrentBgDocument);
    assert(EditHistoryRedo(&g_EditHistory,&g_CurrentBgDocument,&g_CurrentSetup,&g_CurrentStan,NULL,&why));
    assert(EditHistoryUndo(&g_EditHistory,&g_CurrentBgDocument,&g_CurrentSetup,&g_CurrentStan,NULL,&why));
    failrebuild=TRUE;assert(!GEditorPasteBgFacesHere((HWND)1,&target));Same(&before,&g_CurrentBgDocument);
    failselection=TRUE;assert(!GEditorPasteBgFacesHere((HWND)1,&target));failselection=FALSE;Same(&before,&g_CurrentBgDocument);
    assert(!GEditorPasteBgFacesHere((HWND)1,NULL));target.room=0;assert(!GEditorPasteBgFacesHere((HWND)1,&target));
    target.room=1;target.position[0]=NAN;assert(!GEditorPasteBgFacesHere((HWND)1,&target));Same(&before,&g_CurrentBgDocument);Same(&clip,&g_FaceClipboard);
    /* Ordinary cross-level paste uses the selected destination face's room. */
    Refs(&g_CurrentBgDocument,refs);selection[0]=refs[11];selectedcount=1;
    assert(GEditorPasteBgFaces((HWND)1));CrossCopies(&clip,&g_CurrentBgDocument,selection,2,2,(double[]){0,10,0});
    EditHistoryFree(&g_EditHistory);BgDocumentFree(&g_CurrentBgDocument);BgDocumentFree(&g_FaceClipboard);BgDocumentFree(&before);BgDocumentFree(&clip);BgFileFree(&source);
    puts("PASS: level-switch clipboard ownership, Ctrl+V destination rooms, Paste Here center/room, undo/redo, native save/reload and rollback.");
}
static void CrossNative(const char *from,const char *to,const char *dir)
{
    BgFile files[2]={0};BgDocument docs[2]={0},clip={0};const char *why="";const char*paths[2]={from,to};
    for(int i=0;i<2;i++){
        FILE*f=fopen(paths[i],"rb");assert(f);fseek(f,0,SEEK_END);files[i].size=ftell(f);rewind(f);files[i].data=malloc(files[i].size);
        assert(fread(files[i].data,1,files[i].size,f)==files[i].size);fclose(f);strcpy(files[i].name,"bg/cross.seg");
        assert(BgDocumentLoad(files[i].data,files[i].size,i?.5f:.233333333f,&docs[i],&why));
    }
    BgFaceRef refs[20],*pasted;DWORD n=0,count;
    for(DWORD r=1;r<=docs[0].roomcount&&n<20;r++){
        const BgDocumentRoom*room=&docs[0].rooms[r];if(!room->facecount)continue;
        const BgDocumentFace*f=&room->faces[room->facecount-1];refs[n++]=(BgFaceRef){f->id,r,f->layer,0};
    }
    double center[3],offset[3];assert(BgDocumentCopyFaces(&docs[0],refs,n,&clip,&why)&&BgDocumentCopiedFaceCenter(&clip,center));
    for(int a=0;a<3;a++)offset[a]=docs[1].rooms[1].origin[a]/docs[1].levelscale-center[a];
    if(!BgDocumentPasteFacesToRoom(&docs[1],&clip,1,offset,&pasted,&count,&why)){fprintf(stderr,"Cross-native %s -> %s: %s\n",from,to,why);abort();}
    CrossCopies(&clip,&docs[1],pasted,count,1,offset);RoundTrip(&docs[1],&files[1],dir);free(pasted);
    for(int i=0;i<2;i++){BgFileFree(&files[i]);BgDocumentFree(&docs[i]);}BgDocumentFree(&clip);
    puts("PASS: native cross-level geometry copied from multiple rooms into another level, with scale conversion and export.");
}
