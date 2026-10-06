static s32 player_char[4], mp_char_cur_select_player[4], mp_char_prev_select_player[4];
static s32 mp_char_select_scroll_offset[4], player_has_selected_char[4], size_mp_select_image_player[4];
static s32 selected_num_players=4, selected_folder_num;
static s32 tab_start_selected, tab_next_selected, tab_prev_selected;
static s32 tab_prev_highlight, tab_next_highlight, tab_start_highlight;
enum {SP_LEVEL_CRADLE=1, DIFFICULTY_AGENT=0, STAGESTATUS_COMPLETED=2};
static int completed, walletloads;
static struct {u32 index;} mpcharselimages[16*4];
static int loaded[16*4];
static int fileIsStageUnlockedAtDifficulty(int folder, int stage, int difficulty)
{return completed ? STAGESTATUS_COMPLETED : 0;}
static void texLoadFromTextureNum(int image, void *pool)
{assert(image>=1000 && image<1064); loaded[image-1000]++;}
static void load_walletbond(void) {walletloads++;}
