static void SkyBody(void)
{
    EnvironmentTable old={.count=1,.recordsize=88},next={.count=1,.recordsize=112};
    EnvironmentOverrides edits={0},saved;
    old.rows[0]=next.rows[0]=Row(29);
    EditorEnvironment r=old.rows[0],v;
    Edit(&r,"skyBody","1");Edit(&r,"skyBodySize","5");
    Edit(&r,"skyBodyRed","255");Edit(&r,"skyBodyGreen","224");Edit(&r,"skyBodyBlue","160");
    Edit(&r,"skyBodyY","0.5");Edit(&r,"skyBodyZ","-1");
    OK(!EnvironmentSet(&old,&edits,&r,&why)&&!edits.count&&strstr(why,"Rebase"));
    OK(EnvironmentSet(&next,&edits,&r,&why));saved=edits;
    OK(edits.rows[0].fields&0x80000000u); /* Direction Z owns bit 31, not a shift by 32. */
    OK(!EnvironmentValidateOverrides(&old,&edits,&why));
    OK(!EnvironmentRebase(&next,&old,&edits,&why)&&!memcmp(&saved,&edits,sizeof(edits)));
    next.rows[0].data[99]=0xa7; /* Preserve the new reserved byte on export. */
    unsigned char bytes[224]={0};memcpy(bytes,next.rows[0].data,112);
    RomFile rom={.data=bytes,.size=sizeof(bytes)};rom.info.entrycount=2;
    rom.info.entries[0]=(RomManifestEntry){0x434d4150,0,sizeof(bytes),0};
    rom.info.entries[1]=(RomManifestEntry){0x454e5654,0,0,112};
    OK(EnvironmentApplyRom(&rom,&edits,&why)&&bytes[99]==0xa7);
    EnvironmentTable read;OK(EnvironmentReadRom(&rom,&read,NULL,&why));
    OK(EnvironmentGet(&read,&(EnvironmentOverrides){0},29,&v)&&U32(v.data+88)==1&&F32(v.data+108)==-1);
    RomSkyBody body;Edit(&v,"clouds","0");EnvironmentPreviewSkyBody(&v,&body);
    OK(body.type==1&&body.angularsize==5&&body.color[1]==224&&body.direction[2]==-1);
    Edit(&r,"skyBodySize","0");OK(!EnvironmentSet(&next,&edits,&r,&why));
    Edit(&r,"skyBodySize","90");Edit(&r,"skyBodyY","0");Edit(&r,"skyBodyZ","0");
    OK(!EnvironmentSet(&next,&edits,&r,&why));
    Edit(&r,"skyBody","0");OK(EnvironmentSet(&next,&edits,&r,&why));
    OK(!EnvironmentParseField(&r,24,"3",&why)&&!EnvironmentParseField(&r,25,"91",&why));
    OK(!EnvironmentParseField(&r,30,"NaN",&why)&&!EnvironmentParseField(&r,29,"Infinity",&why));
    OK(!EnvironmentParseField(&r,26,"256",&why));
    puts("PASS: sun/moon validation and preview, 32-bit override mask, new-row padding, and safe refusal to apply/rebase sky settings onto old ROMs.");
}
