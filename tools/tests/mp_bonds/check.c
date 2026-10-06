static void Roster(void)
{
    const char *names[]={"Brosnan","Connery","Dalton","Moore"};
    const int photos[]={IMG_MPC_BROSNAN,IMG_MPC_CONNERY,IMG_MPC_DALTON,IMG_MPC_MOORE};
    assert(ARRAYCOUNT(mp_chr_setup)==67);
    assert(ARRAYCOUNT(LtitleE)==292 && LtitleE[287]==NULL);
    assert(BODY_Brosnan_Tuxedo==5 && HEAD_Male_Brosnan_Tuxedo==78);
    for (int i=0;i<4;i++) {
        assert(mp_chr_setup[i].text_preset==0x9d20+i);
        assert(!strcmp(LtitleE[mp_chr_setup[i].text_preset & 0x3ff],names[i]));
        assert(mp_chr_setup[i].select_photo==photos[i]);
        player_char[i]=i;
        assert(get_player_mp_char_gender(i)==MALE && get_player_mp_char_height(i)==1.0f);
        assert(get_player_mp_char_body(i)==(i ? bodyids[i-1] : BODY_Brosnan_Tuxedo));
        assert(get_player_mp_char_head(i)==(i ? headids[i-1] : HEAD_Male_Brosnan_Tuxedo));
        player_char[i]=-1; /* Defaults match the first four roster entries. */
        assert(get_player_mp_char_body(i)==(i ? bodyids[i-1] : BODY_Brosnan_Tuxedo));
        assert(get_player_mp_char_head(i)==(i ? headids[i-1] : HEAD_Male_Brosnan_Tuxedo));
    }
    assert(mp_chr_setup[4].body==BODY_Natalya_Skirt);
    assert(mp_chr_setup[5].body==BODY_Trevelyan_Janus);
    assert(mp_chr_setup[10].head==HEAD_Male_Mishkin);
    assert(mp_chr_setup[35].body==BODY_Moonraker_Elite_2_Female);
    assert(mp_chr_setup[36].body==BODY_Rosika);
    assert(mp_chr_setup[66].head==HEAD_Female_Vivien);
    for (int i=4;i<67;i++) {
        player_char[0]=i;
        assert(get_player_mp_char_body(0)==mp_chr_setup[i].body);
        assert(get_player_mp_char_head(0)==mp_chr_setup[i].head);
        assert(get_player_mp_char_gender(0)==mp_chr_setup[i].gender);
        assert(get_player_mp_char_height(0)==mp_chr_setup[i].pov);
    }
}

static void ImportOrderAndFallback(void)
{
    for (int i=0;i<g_CustomPropCount;i++) if (fixtures[i].kind)
        fixtures[i].characterId=80+(fixtures[i].characterId-80+3)%6;
    for (int i=1;i<4;i++) {
        player_char[0]=i;
        assert(get_player_mp_char_body(0)==80+(bodyids[i-1]-80+3)%6);
        assert(get_player_mp_char_head(0)==80+(headids[i-1]-80+3)%6);
    }
    assert(customCharacterFind("CconneryZ",CUSTOM_CHARACTER_HEAD)==-1);
    assert(customCharacterFind("MissingZ",CUSTOM_CHARACTER_BODY)==-1);
    assert(customCharacterFind("CconneryZ",0)==-1);
    player_char[0]=1;
    for (int i=0;i<g_CustomPropCount;i++) if (!strcmp(fixtures[i].name,"CheadconneryZ")) {
        fixtures[i].kind=0; /* Both halves fall back when one asset is absent. */
        assert(get_player_mp_char_body(0)==BODY_Brosnan_Tuxedo);
        assert(get_player_mp_char_head(0)==HEAD_Male_Brosnan_Tuxedo);
        fixtures[i].kind=CUSTOM_CHARACTER_HEAD;
    }
    g_CustomProps=NULL; g_CustomPropCount=0; /* Catalog reset between stages. */
    assert(get_player_mp_char_body(0)==BODY_Brosnan_Tuxedo);
    assert(get_player_mp_char_head(0)==HEAD_Male_Brosnan_Tuxedo);
    player_char[0]=1000;
    assert(get_player_mp_char_body(0)==BODY_Brosnan_Tuxedo);
}

static void Unlocks(void)
{
    for (int i=0;i<64;i++) mpcharselimages[i].index=1000+i;
    for (int i=0;i<4;i++) player_char[i]=i;
    num_chars_selectable_mp=MP_CHARS_DEFAULT_COUNT;
    completed=0; init_menu0f_mpcharsel();
    assert(num_chars_selectable_mp==11 && walletloads==1);
    for (int i=0;i<64;i++) assert(loaded[i]>0); /* All 16 portraits, including the new Bonds. */
    for (int i=0;i<4;i++) assert(mp_char_cur_select_player[i]==i && !player_has_selected_char[i]);
    completed=1; init_menu0f_mpcharsel(); assert(num_chars_selectable_mp==36);
    unlock_all_mp_chars(); init_menu0f_mpcharsel(); assert(num_chars_selectable_mp==67);
    completed=0; init_menu0f_mpcharsel(); assert(num_chars_selectable_mp==67);
    /* Switching back to a locked folder does not leave an invalid selection. */
    num_chars_selectable_mp=36; player_char[0]=35;
    init_menu0f_mpcharsel(); assert(num_chars_selectable_mp==11 && player_char[0]<11);
}

int main(void)
{
    g_CustomProps=fixtures; g_CustomPropCount=ARRAYCOUNT(fixtures);
    Roster(); ImportOrderAndFallback(); Unlocks();
    puts("PASS: four Bond names/portraits, paired models, changed import IDs, safe fallbacks, all 67 roster entries and 11/36/67 unlock limits");
    return 0;
}
