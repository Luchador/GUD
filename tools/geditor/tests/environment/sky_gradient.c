static void SkyGradient(void)
{
    EnvironmentTable old={.count=1,.recordsize=112},next={.count=1,.recordsize=124},read;
    EnvironmentOverrides edits={0},saved;
    EditorEnvironment v;
    old.rows[0]=next.rows[0]=Row(29);
    Edit(&old.rows[0],"skyBody","2");Edit(&old.rows[0],"skyBodySize","5");
    Edit(&old.rows[0],"skyBodyY","1");next.rows[0]=old.rows[0];
    v=old.rows[0];Edit(&v,"skyBodyZ","-1");
    OK(EnvironmentSet(&old,&edits,&v,&why));
    OK(EnvironmentRebase(&old,&next,&edits,&why)); /* Existing body edit survives expanded stride. */
    OK(EnvironmentGet(&next,&edits,29,&v));
    Edit(&v,"skyGradient","1");Edit(&v,"skyGradientAngle","45");
    Edit(&v,"skyGradientRed","25");Edit(&v,"skyGradientGreen","75");Edit(&v,"skyGradientBlue","150");
    saved=edits;
    OK(!EnvironmentSet(&old,&edits,&v,&why)&&strstr(why,"gradient")&&!memcmp(&edits,&saved,sizeof(edits)));
    OK(EnvironmentSet(&next,&edits,&v,&why));saved=edits;
    OK((edits.rows[0].fields & ENVIRONMENT_SKY_GRADIENT_FIELDS)==ENVIRONMENT_SKY_GRADIENT_FIELDS);
    OK(!EnvironmentRebase(&next,&old,&edits,&why)&&!memcmp(&edits,&saved,sizeof(edits)));
    unsigned char bytes[3*124],before[sizeof(bytes)];memset(bytes,0xa5,sizeof(bytes));
    next.rows[0].data[123]=0x79;
    memcpy(bytes,next.rows[0].data,124);memset(bytes+124,0,124);memcpy(before,bytes,sizeof(bytes));
    RomFile rom={.data=bytes,.size=sizeof(bytes)};rom.info.entrycount=2;
    rom.info.entries[0]=(RomManifestEntry){0x434d4150,0,sizeof(bytes),0};
    rom.info.entries[1]=(RomManifestEntry){0x454e5654,0,0,124};
    OK(EnvironmentApplyRom(&rom,&edits,&why));
    v.data[123]=0x79;memcpy(before,v.data,124);OK(!memcmp(before,bytes,sizeof(bytes)));
    OK(EnvironmentReadRom(&rom,&read,NULL,&why)&&!memcmp(&read.rows[0],&v,sizeof(v)));
    RomSkyGradient gradient;EnvironmentPreviewSkyGradient(&v,&gradient);
    OK(gradient.enabled&&gradient.endangle==45&&gradient.color[2]==150&&gradient.horizonoffset==25);
    /* Zero-filled rows may retain angle zero while disabled; active gradients cannot. */
    Edit(&v,"skyGradientAngle","0");OK(!EnvironmentSet(&next,&edits,&v,&why));
    Edit(&v,"skyGradient","0");OK(EnvironmentSet(&next,&edits,&v,&why));
    OK(!EnvironmentParseField(&v,33,"90.01",&why)&&!EnvironmentParseField(&v,33,"NaN",&why));
    puts("PASS: 112-to-124 byte rebase, 64-bit gradient overrides, exact padded ROM export, preview and old-ROM rejection without mutation.");
}
