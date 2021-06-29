#include "global.h"
#include "decompress.h"
#include "dynamic_placeholder_text_util.h"
#include "graphics.h"
#include "international_string_util.h"
#include "pokenav.h"
#include "sound.h"
#include "sprite.h"
#include "string_util.h"
#include "strings.h"
#include "text.h"
#include "trainer_pokemon_sprites.h"
#include "window.h"
#include "constants/songs.h"

enum
{
    RIBBONS_SUMMARY_FUNC_NONE,
    RIBBONS_SUMMARY_FUNC_SWITCH_MONS,
    RIBBONS_SUMMARY_FUNC_SELECT_RIBBON,
    RIBBONS_SUMMARY_FUNC_EXPANDED_CURSOR_MOVE,
    RIBBONS_SUMMARY_FUNC_EXPANDED_CANCEL,
    RIBBONS_SUMMARY_FUNC_EXIT,
};

#define GFXTAG_RIBBON_ICONS_BIG 9

#define PALTAG_RIBBON_ICONS_1 15
#define PALTAG_RIBBON_ICONS_2 16
#define PALTAG_RIBBON_ICONS_3 17
#define PALTAG_RIBBON_ICONS_4 18
#define PALTAG_RIBBON_ICONS_5 19

#define RIBBONS_PER_ROW 9
#define GIFT_RIBBON_ROW (1 + (FIRST_GIFT_RIBBON / RIBBONS_PER_ROW)) // Gift ribbons start on a new row after the normal ribbons. 
#define GIFT_RIBBON_START_POS (RIBBONS_PER_ROW * GIFT_RIBBON_ROW)

#define MON_SPRITE_X_ON  40
#define MON_SPRITE_X_OFF -32
#define MON_SPRITE_Y     104

struct PokenavSub13
{
    u8 unused1[8];
    struct PokenavSub18 *monList;
    u16 selectedPos;
    u16 normalRibbonLastRowStart;
    u16 numNormalRibbons;
    u16 numGiftRibbons;
    u32 ribbonIds[FIRST_GIFT_RIBBON];
    u32 giftRibbonIds[NUM_GIFT_RIBBONS];
    u32 unused2;
    u32 (*callback)(struct PokenavSub13 *);
};

struct PokenavSub14
{
    u32 (*callback)(void);
    u32 loopedTaskId;
    u16 nameWindowId;
    u16 ribbonCountWindowId;
    u16 listIdxWindowId;
    u16 unusedWindowId;
    u16 monSpriteId;
    struct Sprite *bigRibbonSprite;
    u32 unused;
    u8 tilemapBuffers[2][BG_SCREEN_SIZE];
};

// Used for the initial drawing of the ribbons
static u32 sRibbonDraw_Total;
static u32 sRibbonDraw_Current;

static void PrintCurrentMonRibbonCount(struct PokenavSub14 *);
static void PrintRibbbonsSummaryMonInfo(struct PokenavSub14 *);
static void PrintRibbonsMonListIndex(struct PokenavSub14 *);
static void ZoomOutSelectedRibbon(struct PokenavSub14 *);
static void UpdateAndZoomInSelectedRibbon(struct PokenavSub14 *);
static void PrintRibbonNameAndDescription(struct PokenavSub14 *);
static void ResetSpritesAndDrawMonFrontPic(struct PokenavSub14 *);
static void AddRibbonListIndexWindow(struct PokenavSub14 *);
static void DestroyRibbonsMonFrontPic(struct PokenavSub14 *);
static void SlideMonSpriteOff(struct PokenavSub14 *);
static void SlideMonSpriteOn(struct PokenavSub14 *);
static void AddRibbonCountWindow(struct PokenavSub14 *);
static void CreateBigRibbonSprite(struct PokenavSub14 *);
static void AddRibbonSummaryMonNameWindow(struct PokenavSub14 *);
static void DrawAllRibbonsSmall(struct PokenavSub14 *);
static bool32 IsRibbonAnimating(struct PokenavSub14 *);
static bool32 IsMonSpriteAnimating(struct PokenavSub14 *);
static void GetMonRibbons(struct PokenavSub13 *);
static u32 HandleExpandedRibbonInput(struct PokenavSub13 *);
static u32 RibbonsSummaryHandleInput(struct PokenavSub13 *);
static u32 ReturnToRibbonsListFromSummary(struct PokenavSub13 *);
static bool32 TrySelectRibbonUp(struct PokenavSub13 *);
static bool32 TrySelectRibbonRight(struct PokenavSub13 *);
static bool32 TrySelectRibbonLeft(struct PokenavSub13 *);
static bool32 TrySelectRibbonDown(struct PokenavSub13 *);
static bool32 GetCurrentLoopedTaskActive(void);
static u32 GetRibbonsSummaryCurrentIndex(void);
static u32 GetRibbonsSummaryMonListCount(void);
static u16 DrawRibbonsMonFrontPic(s32, s32);
static void StartMonSpriteSlide(struct Sprite *, s32, s32, s32);
static void SpriteCB_MonSpriteSlide(struct Sprite *);
static void ClearRibbonsSummaryBg(void);
static void BufferSmallRibbonGfxData(u16 *, u32);
static void DrawRibbonSmall(u32, u32);
static void SpriteCB_WaitForRibbonAnimation(struct Sprite *);
static u32 LoopedTask_OpenRibbonsSummaryMenu(s32);
static u32 LoopedTask_SwitchRibbonsSummaryMon(s32);
static u32 LoopedTask_ExpandSelectedRibbon(s32);
static u32 LoopedTask_MoveRibbonsCursorExpanded(s32);
static u32 LoopedTask_ShrinkExpandedRibbon(s32);
static u32 LoopedTask_ExitRibbonsSummaryMenu(s32);
static u32 GetRibbonId(void);

struct
{
    u8 numBits; // The number of bits needed to represent numRibbons
    u8 numRibbons; // Never read. The contest ribbons have 4 (1 for each rank), the rest are just 1 ribbon
    u8 ribbonId;
    bool8 isGiftRibbon;
} static  const sRibbonData[] =
{
    {1, 1, CHAMPION_RIBBON,      FALSE},
    {3, 4, COOL_RIBBON_NORMAL,   FALSE},
    {3, 4, BEAUTY_RIBBON_NORMAL, FALSE},
    {3, 4, CUTE_RIBBON_NORMAL,   FALSE},
    {3, 4, SMART_RIBBON_NORMAL,  FALSE},
    {3, 4, TOUGH_RIBBON_NORMAL,  FALSE},
    {1, 1, WINNING_RIBBON,       FALSE},
    {1, 1, VICTORY_RIBBON,       FALSE},
    {1, 1, ARTIST_RIBBON,        FALSE},
    {1, 1, EFFORT_RIBBON,        FALSE},
    {1, 1, MARINE_RIBBON,        TRUE},
    {1, 1, LAND_RIBBON,          TRUE},
    {1, 1, SKY_RIBBON,           TRUE},
    {1, 1, COUNTRY_RIBBON,       TRUE},
    {1, 1, NATIONAL_RIBBON,      TRUE},
    {1, 1, EARTH_RIBBON,         TRUE},
    {1, 1, WORLD_RIBBON,         TRUE}
};

#include "data/text/ribbon_descriptions.h"
#include "data/text/gift_ribbon_descriptions.h"

static const u16 sRibbonIcons1_Pal[] = INCBIN_U16("graphics/pokenav/ribbons/icons1.gbapal");
static const u16 sRibbonIcons2_Pal[] = INCBIN_U16("graphics/pokenav/ribbons/icons2.gbapal");
static const u16 sRibbonIcons3_Pal[] = INCBIN_U16("graphics/pokenav/ribbons/icons3.gbapal");
static const u16 sRibbonIcons4_Pal[] = INCBIN_U16("graphics/pokenav/ribbons/icons4.gbapal");
static const u16 sRibbonIcons5_Pal[] = INCBIN_U16("graphics/pokenav/ribbons/icons5.gbapal");
static const u16 sMonInfo_Pal[] = INCBIN_U16("graphics/pokenav/ribbons/mon_info.gbapal"); // palette for Pokémon's name/gender/level text
static const u32 sRibbonIconsSmall_Gfx[] = INCBIN_U32("graphics/pokenav/ribbons/icons.4bpp.lz");
static const u32 sRibbonIconsBig_Gfx[] = INCBIN_U32("graphics/pokenav/ribbons/icons_big.4bpp.lz");

static const struct BgTemplate sBgTemplates[] =
{
    {
        .bg = 1,
        .charBaseIndex = 3,
        .mapBaseIndex = 0x07,
        .screenSize = 0,
        .paletteMode = 0,
        .priority = 1,
        .baseTile = 0
    },
    {
        .bg = 2,
        .charBaseIndex = 1,
        .mapBaseIndex = 0x06,
        .screenSize = 0,
        .paletteMode = 0,
        .priority = 2,
        .baseTile = 0
    }
};

static const LoopedTask sRibbonsSummaryMenuLoopTaskFuncs[] =
{
    [RIBBONS_SUMMARY_FUNC_NONE]                 = NULL,
    [RIBBONS_SUMMARY_FUNC_SWITCH_MONS]          = LoopedTask_SwitchRibbonsSummaryMon,
    [RIBBONS_SUMMARY_FUNC_SELECT_RIBBON]        = LoopedTask_ExpandSelectedRibbon,
    [RIBBONS_SUMMARY_FUNC_EXPANDED_CURSOR_MOVE] = LoopedTask_MoveRibbonsCursorExpanded,
    [RIBBONS_SUMMARY_FUNC_EXPANDED_CANCEL]      = LoopedTask_ShrinkExpandedRibbon,
    [RIBBONS_SUMMARY_FUNC_EXIT]                 = LoopedTask_ExitRibbonsSummaryMenu
};

bool32 PokenavCallback_Init_RibbonsSummaryMenu(void)
{
    struct PokenavSub13 *structPtr = AllocSubstruct(POKENAV_SUBSTRUCT_RIBBONS_SUMMARY_LIST, sizeof(struct PokenavSub13));
    if (structPtr == NULL)
        return FALSE;

    structPtr->monList = GetSubstructPtr(POKENAV_SUBSTRUCT_MON_LIST);
    if (structPtr->monList == NULL)
        return FALSE;

    GetMonRibbons(structPtr);
    structPtr->callback = RibbonsSummaryHandleInput;
    gKeyRepeatContinueDelay = 3;
    gKeyRepeatStartDelay = 10;
    return TRUE;
}

u32 GetRibbonsSummaryMenuCallback(void)
{
    struct PokenavSub13 *structPtr = GetSubstructPtr(POKENAV_SUBSTRUCT_RIBBONS_SUMMARY_LIST);
    return structPtr->callback(structPtr);
}

void FreeRibbonsSummaryScreen1(void)
{
    FreePokenavSubstruct(POKENAV_SUBSTRUCT_RIBBONS_SUMMARY_LIST);
}

// Handles input when a specific ribbon is not currently selected
static u32 RibbonsSummaryHandleInput(struct PokenavSub13 *structPtr)
{
    // Handle Up/Down movement to select a new Pokémon to show ribbons for
    if (JOY_REPEAT(DPAD_UP) && structPtr->monList->currIndex != 0)
    {
        structPtr->monList->currIndex--;
        structPtr->selectedPos = 0;
        GetMonRibbons(structPtr);
        return RIBBONS_SUMMARY_FUNC_SWITCH_MONS;
    }
    if (JOY_REPEAT(DPAD_DOWN) && structPtr->monList->currIndex < structPtr->monList->listCount - 1)
    {
        structPtr->monList->currIndex++;
        structPtr->selectedPos = 0;
        GetMonRibbons(structPtr);
        return RIBBONS_SUMMARY_FUNC_SWITCH_MONS;
    }

    if (JOY_NEW(A_BUTTON))
    {
        // Enter ribbon selection
        structPtr->callback = HandleExpandedRibbonInput;
        return RIBBONS_SUMMARY_FUNC_SELECT_RIBBON;
    }
    if (JOY_NEW(B_BUTTON))
    {
        // Exit ribbon summary menu
        structPtr->callback = ReturnToRibbonsListFromSummary;
        return RIBBONS_SUMMARY_FUNC_EXIT;
    }
    return RIBBONS_SUMMARY_FUNC_NONE;
}

// Handles input when a ribbon is selected
static u32 HandleExpandedRibbonInput(struct PokenavSub13 *structPtr)
{
    // Handle movement while a ribbon is selected
    if (JOY_REPEAT(DPAD_UP) && TrySelectRibbonUp(structPtr))
        return RIBBONS_SUMMARY_FUNC_EXPANDED_CURSOR_MOVE;
    if (JOY_REPEAT(DPAD_DOWN) && TrySelectRibbonDown(structPtr))
        return RIBBONS_SUMMARY_FUNC_EXPANDED_CURSOR_MOVE;
    if (JOY_REPEAT(DPAD_LEFT) && TrySelectRibbonLeft(structPtr))
        return RIBBONS_SUMMARY_FUNC_EXPANDED_CURSOR_MOVE;
    if (JOY_REPEAT(DPAD_RIGHT) && TrySelectRibbonRight(structPtr))
        return RIBBONS_SUMMARY_FUNC_EXPANDED_CURSOR_MOVE;

    if (JOY_NEW(B_BUTTON))
    {
        // Exit ribbon selection
        structPtr->callback = RibbonsSummaryHandleInput;
        return RIBBONS_SUMMARY_FUNC_EXPANDED_CANCEL;
    }
	
	if (JOY_NEW(SELECT_BUTTON))
	{
		struct PokenavSub18 *mons = structPtr->monList;
		struct PokenavMonList *monInfo = &mons->monData[mons->currIndex];
		u32 ribbonId = GetRibbonId();
		u8 title = (u8)ribbonId + 1;
		if (monInfo->boxId == TOTAL_BOXES_COUNT)
		{
			struct Pokemon *mon = &gPlayerParty[monInfo->monId];
			SetMonData(mon, MON_DATA_TITLE, &title);
		}
		else
		{
			struct BoxPokemon *boxMon = GetBoxedMonPtr(monInfo->boxId, monInfo->monId);
			SetBoxMonData(boxMon, MON_DATA_TITLE, &title);
		}
	}
    return RIBBONS_SUMMARY_FUNC_NONE;
}

static u32 ReturnToRibbonsListFromSummary(struct PokenavSub13 *structPtr)
{
    return POKENAV_RIBBONS_RETURN_TO_MON_LIST;
}

static bool32 TrySelectRibbonUp(struct PokenavSub13 *structPtr)
{
    if (structPtr->selectedPos < FIRST_GIFT_RIBBON)
    {
        // In normal ribbons, try to move up a row
        if (structPtr->selectedPos < RIBBONS_PER_ROW)
            return FALSE;

        structPtr->selectedPos -= RIBBONS_PER_ROW;
        return TRUE;
    }
    if (structPtr->numNormalRibbons != 0)
    {
        // In gift ribbons, try to move up into normal ribbons
        // If there's > 1 row of gift ribbons (not normally possible)
        // it's impossible to move up between them
        u32 ribbonPos = structPtr->selectedPos - GIFT_RIBBON_START_POS;
        structPtr->selectedPos = ribbonPos + structPtr->normalRibbonLastRowStart;
        if (structPtr->selectedPos >= structPtr->numNormalRibbons)
            structPtr->selectedPos = structPtr->numNormalRibbons - 1;
        return TRUE;
    }
    return FALSE;
}

static bool32 TrySelectRibbonDown(struct PokenavSub13 *structPtr)
{
    if (structPtr->selectedPos >= FIRST_GIFT_RIBBON)
        return FALSE;
    if (structPtr->selectedPos < structPtr->normalRibbonLastRowStart)
    {
        // Not in last row of normal ribbons, advance to next row
        structPtr->selectedPos += RIBBONS_PER_ROW;
        if (structPtr->selectedPos >= structPtr->numNormalRibbons)
            structPtr->selectedPos = structPtr->numNormalRibbons - 1;
        return TRUE;
    }
    if (structPtr->numGiftRibbons != 0)
    {
        // In/beyond last of row of normal ribbons and gift ribbons present, move down to gift ribbon row
        int ribbonPos = structPtr->selectedPos - structPtr->normalRibbonLastRowStart;
        if (ribbonPos >= structPtr->numGiftRibbons)
            ribbonPos = structPtr->numGiftRibbons - 1;

        structPtr->selectedPos = ribbonPos + GIFT_RIBBON_START_POS;
        return TRUE;
    }
    return FALSE;
}

static bool32 TrySelectRibbonLeft(struct PokenavSub13 *structPtr)
{
    u16 column = structPtr->selectedPos % RIBBONS_PER_ROW;
    if (column != 0)
    {
        structPtr->selectedPos--;
        return TRUE;
    }

    return FALSE;
}

static bool32 TrySelectRibbonRight(struct PokenavSub13 *structPtr)
{
    int column = structPtr->selectedPos % RIBBONS_PER_ROW;

    if (column >= RIBBONS_PER_ROW - 1)
        return FALSE;

    if (structPtr->selectedPos < GIFT_RIBBON_START_POS)
    {
        // Move right in normal ribbon row
        if (structPtr->selectedPos < structPtr->numNormalRibbons - 1)
        {
            structPtr->selectedPos++;
            return TRUE;
        }
    }
    else
    {
        // Move right in gift ribbon row
        if (column < structPtr->numGiftRibbons - 1)
        {
            structPtr->selectedPos++;
            return TRUE;
        }
    }
    return FALSE;
}

static u32 GetRibbonsSummaryCurrentIndex(void)
{
    struct PokenavSub13 *structPtr = GetSubstructPtr(POKENAV_SUBSTRUCT_RIBBONS_SUMMARY_LIST);
    return structPtr->monList->currIndex;
}

static u32 GetRibbonsSummaryMonListCount(void)
{
    struct PokenavSub13 *structPtr = GetSubstructPtr(POKENAV_SUBSTRUCT_RIBBONS_SUMMARY_LIST);
    return structPtr->monList->listCount;
}

static void GetMonNicknameLevelGender(u8 *nick, u8 *level, u8 *gender)
{
    struct PokenavSub13 *structPtr = GetSubstructPtr(POKENAV_SUBSTRUCT_RIBBONS_SUMMARY_LIST);
    struct PokenavSub18 *mons = structPtr->monList;
    struct PokenavMonList *monInfo = &mons->monData[mons->currIndex];

    if (monInfo->boxId == TOTAL_BOXES_COUNT)
    {
        // Get info for party mon
        struct Pokemon *mon = &gPlayerParty[monInfo->monId];
        GetMonData(mon, MON_DATA_NICKNAME, nick);
        *level = GetLevelFromMonExp(mon);
        *gender = GetMonGender(mon);
    }
    else
    {
        // Get info for PC box mon
        struct BoxPokemon *boxMon = GetBoxedMonPtr(monInfo->boxId, monInfo->monId);
        *gender = GetBoxMonGender(boxMon);
        *level = GetLevelFromBoxMonExp(boxMon);
        GetBoxMonData(boxMon, MON_DATA_NICKNAME, nick);
    }
    StringGetEnd10(nick);
}

static void GetMonSpeciesPersonalityOtId(u16 *species, u32 *personality, u32 *otId)
{
    struct PokenavSub13 *structPtr = GetSubstructPtr(POKENAV_SUBSTRUCT_RIBBONS_SUMMARY_LIST);
    struct PokenavSub18 *mons = structPtr->monList;
    struct PokenavMonList *monInfo = &mons->monData[mons->currIndex];

    if (monInfo->boxId == TOTAL_BOXES_COUNT)
    {
        // Get info for party mon
        struct Pokemon *mon = &gPlayerParty[monInfo->monId];
        *species = GetMonData(mon, MON_DATA_SPECIES);
        *personality = GetMonData(mon, MON_DATA_PERSONALITY);
        *otId = GetMonData(mon, MON_DATA_OT_ID);
    }
    else
    {
        // Get info for PC box mon
        struct BoxPokemon *boxMon = GetBoxedMonPtr(monInfo->boxId, monInfo->monId);
        *species = GetBoxMonData(boxMon, MON_DATA_SPECIES);
        *personality = GetBoxMonData(boxMon, MON_DATA_PERSONALITY);
        *otId = GetBoxMonData(boxMon, MON_DATA_OT_ID);
    }
}

static u32 GetCurrMonRibbonCount(void)
{
    struct PokenavSub13 *structPtr = GetSubstructPtr(POKENAV_SUBSTRUCT_RIBBONS_SUMMARY_LIST);
    struct PokenavSub18 *mons = structPtr->monList;
    struct PokenavMonList *monInfo = &mons->monData[mons->currIndex];

    if (monInfo->boxId == TOTAL_BOXES_COUNT)
        return GetMonData(&gPlayerParty[monInfo->monId], MON_DATA_RIBBON_COUNT);
    else
        return GetBoxMonDataAt(monInfo->boxId, monInfo->monId, MON_DATA_RIBBON_COUNT);
}

static void GetMonRibbons(struct PokenavSub13 *structPtr)
{
    u32 ribbonFlags;
    s32 i, j;
    struct PokenavSub18 *mons = structPtr->monList;
    struct PokenavMonList *monInfo = &mons->monData[mons->currIndex];

    if (monInfo->boxId == TOTAL_BOXES_COUNT)
        ribbonFlags = GetMonData(&gPlayerParty[monInfo->monId], MON_DATA_RIBBONS);
    else
        ribbonFlags = GetBoxMonDataAt(monInfo->boxId, monInfo->monId, MON_DATA_RIBBONS);

    structPtr->numNormalRibbons = 0;
    structPtr->numGiftRibbons = 0;
    for (i = 0; i < ARRAY_COUNT(sRibbonData); i++)
    {
        // For all non-contest ribbons, numRibbons will be 1 if they have it, 0 if they don't
        // For contest ribbons, numRibbons will be 0-4
        s32 numRibbons = ((1 << sRibbonData[i].numBits) - 1) & ribbonFlags;
        if (!sRibbonData[i].isGiftRibbon)
        {
            for (j = 0; j < numRibbons; j++)
                structPtr->ribbonIds[structPtr->numNormalRibbons++] = sRibbonData[i].ribbonId + j;
        }
        else
        {
            for (j = 0; j < numRibbons; j++)
                structPtr->giftRibbonIds[structPtr->numGiftRibbons++] = sRibbonData[i].ribbonId + j;
        }
        ribbonFlags >>= sRibbonData[i].numBits;
    }

    if (structPtr->numNormalRibbons != 0)
    {
        structPtr->normalRibbonLastRowStart = ((structPtr->numNormalRibbons - 1) / RIBBONS_PER_ROW) * RIBBONS_PER_ROW;
        structPtr->selectedPos = 0;
    }
    else
    {
        // There are no normal ribbons, move cursor to first gift ribbon
        structPtr->normalRibbonLastRowStart = 0;
        structPtr->selectedPos = GIFT_RIBBON_START_POS;
    }
}

static u32 *GetNormalRibbonIds(u32 *size)
{
    struct PokenavSub13 *structPtr = GetSubstructPtr(POKENAV_SUBSTRUCT_RIBBONS_SUMMARY_LIST);
    *size = structPtr->numNormalRibbons;
    return structPtr->ribbonIds;
}

static u32 *GetGiftRibbonIds(u32 *size)
{
    struct PokenavSub13 *structPtr = GetSubstructPtr(POKENAV_SUBSTRUCT_RIBBONS_SUMMARY_LIST);
    *size = structPtr->numGiftRibbons;
    return structPtr->giftRibbonIds;
}

static u16 GetSelectedPosition(void)
{
    struct PokenavSub13 *structPtr = GetSubstructPtr(POKENAV_SUBSTRUCT_RIBBONS_SUMMARY_LIST);
    return structPtr->selectedPos;
}

static u32 GetRibbonId(void)
{
    struct PokenavSub13 *structPtr = GetSubstructPtr(POKENAV_SUBSTRUCT_RIBBONS_SUMMARY_LIST);
    int ribbonPos = structPtr->selectedPos;
    if (ribbonPos < FIRST_GIFT_RIBBON)
        return structPtr->ribbonIds[ribbonPos];
    else
        return structPtr->giftRibbonIds[ribbonPos - GIFT_RIBBON_START_POS];
}

bool32 OpenRibbonsSummaryMenu(void)
{
    struct PokenavSub14 *structPtr = AllocSubstruct(POKENAV_SUBSTRUCT_RIBBONS_SUMMARY_MENU, sizeof(struct PokenavSub14));
    if (structPtr == NULL)
        return FALSE;

    structPtr->loopedTaskId = CreateLoopedTask(LoopedTask_OpenRibbonsSummaryMenu, 1);
    structPtr->callback = GetCurrentLoopedTaskActive;
    return TRUE;
}

void CreateRibbonsSummaryLoopedTask(s32 id)
{
    struct PokenavSub14 *structPtr = GetSubstructPtr(POKENAV_SUBSTRUCT_RIBBONS_SUMMARY_MENU);
    structPtr->loopedTaskId = CreateLoopedTask(sRibbonsSummaryMenuLoopTaskFuncs[id], 1);
    structPtr->callback = GetCurrentLoopedTaskActive;
}

u32 IsRibbonsSummaryLoopedTaskActive(void)
{
    struct PokenavSub14 *structPtr = GetSubstructPtr(POKENAV_SUBSTRUCT_RIBBONS_SUMMARY_MENU);
    return structPtr->callback();
}

void FreeRibbonsSummaryScreen2(void)
{
    struct PokenavSub14 *structPtr = GetSubstructPtr(POKENAV_SUBSTRUCT_RIBBONS_SUMMARY_MENU);
    RemoveWindow(structPtr->ribbonCountWindowId);
    RemoveWindow(structPtr->nameWindowId);
    RemoveWindow(structPtr->listIdxWindowId);
#ifndef BUGFIX
    RemoveWindow(structPtr->unusedWindowId); // Removing window, but window id is never set
#endif
    DestroyRibbonsMonFrontPic(structPtr);
    FreeSpriteTilesByTag(GFXTAG_RIBBON_ICONS_BIG);
    FreeSpritePaletteByTag(PALTAG_RIBBON_ICONS_1);
    FreeSpritePaletteByTag(PALTAG_RIBBON_ICONS_2);
    FreeSpritePaletteByTag(PALTAG_RIBBON_ICONS_3);
    FreeSpritePaletteByTag(PALTAG_RIBBON_ICONS_4);
    FreeSpritePaletteByTag(PALTAG_RIBBON_ICONS_5);
    FreeSpriteOamMatrix(structPtr->bigRibbonSprite);
    DestroySprite(structPtr->bigRibbonSprite);
    FreePokenavSubstruct(POKENAV_SUBSTRUCT_RIBBONS_SUMMARY_MENU);
}

static bool32 GetCurrentLoopedTaskActive(void)
{
    struct PokenavSub14 *structPtr = GetSubstructPtr(POKENAV_SUBSTRUCT_RIBBONS_SUMMARY_MENU);
    return IsLoopedTaskActive(structPtr->loopedTaskId);
}

static u32 LoopedTask_OpenRibbonsSummaryMenu(s32 state)
{
    struct PokenavSub14 *structPtr = GetSubstructPtr(POKENAV_SUBSTRUCT_RIBBONS_SUMMARY_MENU);
    switch (state)
    {
    case 0:
        InitBgTemplates(sBgTemplates, ARRAY_COUNT(sBgTemplates));
        DecompressAndCopyTileDataToVram(2, gPokenavRibbonsSummaryBg_Gfx, 0, 0, 0);
        SetBgTilemapBuffer(2, structPtr->tilemapBuffers[0]);
        CopyToBgTilemapBuffer(2, gPokenavRibbonsSummaryBg_Tilemap, 0, 0);
        CopyPaletteIntoBufferUnfaded(gPokenavRibbonsSummaryBg_Pal, 0x10, 0x20);
        CopyBgTilemapBufferToVram(2);
        return LT_INC_AND_PAUSE;
    case 1:
        if (!FreeTempTileDataBuffersIfPossible())
        {
            sub_8199DF0(1, 0, 0, 1);
            DecompressAndCopyTileDataToVram(1, sRibbonIconsSmall_Gfx, 0, 1, 0);
            SetBgTilemapBuffer(1, structPtr->tilemapBuffers[1]);
            FillBgTilemapBufferRect_Palette0(1, 0, 0, 0, 32, 20);
            CopyPaletteIntoBufferUnfaded(sRibbonIcons1_Pal, 0x20, 0xA0);
            CopyPaletteIntoBufferUnfaded(sMonInfo_Pal, 0xA0, 0x20);
            CopyBgTilemapBufferToVram(1);
            return LT_INC_AND_PAUSE;
        }
        return LT_PAUSE;
    case 2:
        if (!FreeTempTileDataBuffersIfPossible())
        {
            AddRibbonCountWindow(structPtr);
            return LT_INC_AND_PAUSE;
        }
        return LT_PAUSE;
    case 3:
        if (!FreeTempTileDataBuffersIfPossible())
        {
            AddRibbonSummaryMonNameWindow(structPtr);
            return LT_INC_AND_PAUSE;
        }
        return LT_PAUSE;
    case 4:
        if (!FreeTempTileDataBuffersIfPossible())
        {
            AddRibbonListIndexWindow(structPtr);
            return LT_INC_AND_PAUSE;
        }
        return LT_PAUSE;
    case 5:
        if (!IsDma3ManagerBusyWithBgCopy())
        {
            CopyBgTilemapBufferToVram(2);
            return LT_INC_AND_PAUSE;
        }
        return LT_PAUSE;
    case 6:
        if (!IsDma3ManagerBusyWithBgCopy())
        {
            ResetSpritesAndDrawMonFrontPic(structPtr);
            return LT_INC_AND_CONTINUE;
        }
        return LT_PAUSE;
    case 7:
        DrawAllRibbonsSmall(structPtr);
        PrintHelpBarText(HELPBAR_RIBBONS_LIST);
        return LT_INC_AND_PAUSE;
    case 8:
        if (!IsDma3ManagerBusyWithBgCopy())
        {
            CreateBigRibbonSprite(structPtr);
            ChangeBgX(1, 0, 0);
            ChangeBgY(1, 0, 0);
            ChangeBgX(2, 0, 0);
            ChangeBgY(2, 0, 0);
            ShowBg(1);
            ShowBg(2);
            HideBg(3);
            PokenavFadeScreen(1);
            return LT_INC_AND_PAUSE;
        }
        return LT_PAUSE;
    case 9:
        if (IsPaletteFadeActive())
            return LT_PAUSE;
    }
    return LT_FINISH;
}

static u32 LoopedTask_ExitRibbonsSummaryMenu(s32 state)
{
    switch (state)
    {
    case 0:
        PlaySE(SE_SELECT);
        PokenavFadeScreen(0);
        return LT_INC_AND_PAUSE;
    case 1:
        if (IsPaletteFadeActive())
            return LT_PAUSE;
        return LT_FINISH;
    }
    return LT_FINISH;
}

static u32 LoopedTask_SwitchRibbonsSummaryMon(s32 state)
{
    struct PokenavSub14 *structPtr = GetSubstructPtr(POKENAV_SUBSTRUCT_RIBBONS_SUMMARY_MENU);
    switch (state)
    {
    case 0:
        PlaySE(SE_SELECT);
        SlideMonSpriteOff(structPtr);
        return LT_INC_AND_PAUSE;
    case 1:
        if (!IsMonSpriteAnimating(structPtr))
        {
            PrintRibbbonsSummaryMonInfo(structPtr);
            return LT_INC_AND_CONTINUE;
        }
        return LT_PAUSE;
    case 2:
        DrawAllRibbonsSmall(structPtr);
        return LT_INC_AND_CONTINUE;
    case 3:
        PrintRibbonsMonListIndex(structPtr);
        return LT_INC_AND_CONTINUE;
    case 4:
        PrintCurrentMonRibbonCount(structPtr);
        return LT_INC_AND_CONTINUE;
    case 5:
        if (!IsDma3ManagerBusyWithBgCopy())
        {
            SlideMonSpriteOn(structPtr);
            return LT_INC_AND_PAUSE;
        }
        return LT_PAUSE;
    case 6:
        if (IsMonSpriteAnimating(structPtr))
            return LT_PAUSE;
    }
    return LT_FINISH;
}

static u32 LoopedTask_ExpandSelectedRibbon(s32 state)
{
    struct PokenavSub14 *structPtr = GetSubstructPtr(POKENAV_SUBSTRUCT_RIBBONS_SUMMARY_MENU);
    switch (state)
    {
    case 0:
        PlaySE(SE_SELECT);
        UpdateAndZoomInSelectedRibbon(structPtr);
        return LT_INC_AND_PAUSE;
    case 1:
        if (!IsRibbonAnimating(structPtr))
        {
            PrintRibbonNameAndDescription(structPtr);
            PrintHelpBarText(HELPBAR_RIBBONS_CHECK);
            return LT_INC_AND_PAUSE;
        }
        return LT_PAUSE;
    case 2:
        if (IsDma3ManagerBusyWithBgCopy())
            return LT_PAUSE;
    }
    return LT_FINISH;
}

static u32 LoopedTask_MoveRibbonsCursorExpanded(s32 state)
{
    struct PokenavSub14 *structPtr = GetSubstructPtr(POKENAV_SUBSTRUCT_RIBBONS_SUMMARY_MENU);
    switch (state)
    {
    case 0:
        PlaySE(SE_SELECT);
        ZoomOutSelectedRibbon(structPtr);
        return LT_INC_AND_PAUSE;
    case 1:
        if (!IsRibbonAnimating(structPtr))
        {
            UpdateAndZoomInSelectedRibbon(structPtr);
            return LT_INC_AND_PAUSE;
        }
        return LT_PAUSE;
    case 2:
        if (!IsRibbonAnimating(structPtr))
        {
            PrintRibbonNameAndDescription(structPtr);
            return LT_INC_AND_PAUSE;
        }
        return LT_PAUSE;
    case 3:
        if (IsDma3ManagerBusyWithBgCopy())
            return LT_PAUSE;
    }
    return LT_FINISH;
}

static u32 LoopedTask_ShrinkExpandedRibbon(s32 state)
{
    struct PokenavSub14 *structPtr = GetSubstructPtr(POKENAV_SUBSTRUCT_RIBBONS_SUMMARY_MENU);
    switch (state)
    {
    case 0:
        PlaySE(SE_SELECT);
        ZoomOutSelectedRibbon(structPtr);
        return LT_INC_AND_PAUSE;
    case 1:
        if (!IsRibbonAnimating(structPtr))
        {
            PrintCurrentMonRibbonCount(structPtr);
            PrintHelpBarText(HELPBAR_RIBBONS_LIST);
            return LT_INC_AND_PAUSE;
        }
        return LT_PAUSE;
    case 2:
        if (IsDma3ManagerBusyWithBgCopy())
            return LT_PAUSE;
    }
    return LT_FINISH;
}

static const struct WindowTemplate sRibbonCountWindowTemplate =
{
    .bg = 2,
    .tilemapLeft = 12,
    .tilemapTop = 13,
    .width = 16,
    .height = 4,
    .paletteNum = 1,
    .baseBlock = 0x14,
};

static void AddRibbonCountWindow(struct PokenavSub14 *structPtr)
{
    structPtr->ribbonCountWindowId = AddWindow(&sRibbonCountWindowTemplate);
    PutWindowTilemap(structPtr->ribbonCountWindowId);
    PrintCurrentMonRibbonCount(structPtr);
}

static void PrintCurrentMonRibbonCount(struct PokenavSub14 *structPtr)
{
    u8 color[] = {TEXT_COLOR_RED, TEXT_COLOR_DARK_GRAY, TEXT_COLOR_LIGHT_GRAY};

    ConvertIntToDecimalStringN(gStringVar1, GetCurrMonRibbonCount(), STR_CONV_MODE_LEFT_ALIGN, 2);
    DynamicPlaceholderTextUtil_Reset();
    DynamicPlaceholderTextUtil_SetPlaceholderPtr(0, gStringVar1);
    DynamicPlaceholderTextUtil_ExpandPlaceholders(gStringVar4, gText_RibbonsF700);
    FillWindowPixelBuffer(structPtr->ribbonCountWindowId, PIXEL_FILL(4));
    AddTextPrinterParameterized3(structPtr->ribbonCountWindowId, 1, 0, 1, color, -1, gStringVar4);
    CopyWindowToVram(structPtr->ribbonCountWindowId, 2);
}

static void PrintRibbonNameAndDescription(struct PokenavSub14 *structPtr)
{
    s32 i;
    u32 ribbonId = GetRibbonId();
    u8 color[] = {TEXT_COLOR_RED, TEXT_COLOR_DARK_GRAY, TEXT_COLOR_LIGHT_GRAY};

    FillWindowPixelBuffer(structPtr->ribbonCountWindowId, PIXEL_FILL(4));
    if (ribbonId < FIRST_GIFT_RIBBON)
    {
        // Print normal ribbon name/description
        for (i = 0; i < 2; i++)
            AddTextPrinterParameterized3(structPtr->ribbonCountWindowId, 1, 0, (i * 16) + 1, color, -1, gRibbonDescriptionPointers[ribbonId][i]);
    }
    else
    {
        // ribbonId here is one of the 'gift' ribbon slots, used to read 
        // its actual value from giftRibbons to determine which specific
        // gift ribbon it is
        ribbonId = gSaveBlock1Ptr->giftRibbons[ribbonId - FIRST_GIFT_RIBBON];
        
        // If 0, this gift ribbon slot is unoccupied
        if (ribbonId == 0)
            return;

        // Print gift ribbon name/description
        ribbonId--;
        for (i = 0; i < 2; i++)
            AddTextPrinterParameterized3(structPtr->ribbonCountWindowId, 1, 0, (i * 16) + 1, color, -1, gGiftRibbonDescriptionPointers[ribbonId][i]);
    }

    CopyWindowToVram(structPtr->ribbonCountWindowId, 2);
}

static const struct WindowTemplate sRibbonSummaryMonNameWindowTemplate =
{
    .bg = 2,
    .tilemapLeft = 14,
    .tilemapTop = 1,
    .width = 13,
    .height = 2,
    .paletteNum = 10,
    .baseBlock = 0x54,
};

static void AddRibbonSummaryMonNameWindow(struct PokenavSub14 *structPtr)
{
    structPtr->nameWindowId = AddWindow(&sRibbonSummaryMonNameWindowTemplate);
    PutWindowTilemap(structPtr->nameWindowId);
    PrintRibbbonsSummaryMonInfo(structPtr);
}

static const u8 sMaleIconString[] = _("{COLOR_HIGHLIGHT_SHADOW}{LIGHT_RED}{WHITE}{GREEN}♂{COLOR_HIGHLIGHT_SHADOW}{DARK_GRAY}{WHITE}{LIGHT_GRAY}");
static const u8 sFemaleIconString[] = _("{COLOR_HIGHLIGHT_SHADOW}{LIGHT_GREEN}{WHITE}{BLUE}♀{COLOR_HIGHLIGHT_SHADOW}{DARK_GRAY}{WHITE}{LIGHT_GRAY}");
static const u8 sGenderlessIconString[] = _("{UNK_SPACER}");

static void PrintRibbbonsSummaryMonInfo(struct PokenavSub14 *structPtr)
{
    const u8 *genderTxt;
    u8 *txtPtr;
    u8 level, gender;
    u16 windowId = structPtr->nameWindowId;

    FillWindowPixelBuffer(windowId, PIXEL_FILL(1));
    GetMonNicknameLevelGender(gStringVar3, &level, &gender);
    AddTextPrinterParameterized(windowId, 1, gStringVar3, 0, 1, TEXT_SPEED_FF, NULL);
    switch (gender)
    {
    case MON_MALE:
        genderTxt = sMaleIconString;
        break;
    case MON_FEMALE:
        genderTxt = sFemaleIconString;
        break;
    default:
        genderTxt = sGenderlessIconString;
        break;
    }

    txtPtr = StringCopy(gStringVar1, genderTxt);
    *(txtPtr++) = CHAR_SLASH;
    *(txtPtr++) = CHAR_EXTRA_SYMBOL;
    *(txtPtr++) = CHAR_LV_2;
    ConvertIntToDecimalStringN(txtPtr, level, STR_CONV_MODE_LEFT_ALIGN, 3);
    AddTextPrinterParameterized(windowId, 1, gStringVar1, 60, 1, TEXT_SPEED_FF, NULL);
    CopyWindowToVram(windowId, 2);
}

static const struct WindowTemplate sRibbonMonListIndexWindowTemplate[] =
{
    {
        .bg = 2,
        .tilemapLeft = 1,
        .tilemapTop = 5,
        .width = 7,
        .height = 2,
        .paletteNum = 1,
        .baseBlock = 0x6E,
    },
    {},
};

static void AddRibbonListIndexWindow(struct PokenavSub14 *structPtr)
{
    structPtr->listIdxWindowId = AddWindow(sRibbonMonListIndexWindowTemplate);
    FillWindowPixelBuffer(structPtr->listIdxWindowId, PIXEL_FILL(1));
    PutWindowTilemap(structPtr->listIdxWindowId);
    PrintRibbonsMonListIndex(structPtr);
}

static void PrintRibbonsMonListIndex(struct PokenavSub14 *structPtr)
{
    s32 x;
    u8 *txtPtr;
    u32 id = GetRibbonsSummaryCurrentIndex() + 1;
    u32 count = GetRibbonsSummaryMonListCount();

    txtPtr = ConvertIntToDecimalStringN(gStringVar1, id, STR_CONV_MODE_RIGHT_ALIGN, 3);
    *(txtPtr++) = CHAR_SLASH;
    ConvertIntToDecimalStringN(txtPtr, count, STR_CONV_MODE_RIGHT_ALIGN, 3);
    x = GetStringCenterAlignXOffset(1, gStringVar1, 56);
    AddTextPrinterParameterized(structPtr->listIdxWindowId, 1, gStringVar1, x, 1, TEXT_SPEED_FF, NULL);
    CopyWindowToVram(structPtr->listIdxWindowId, 2);
}

static void ResetSpritesAndDrawMonFrontPic(struct PokenavSub14 *structPtr)
{
    u16 species;
    u32 personality, otId;

    GetMonSpeciesPersonalityOtId(&species, &personality, &otId);
    ResetAllPicSprites();
    structPtr->monSpriteId = DrawRibbonsMonFrontPic(MON_SPRITE_X_ON, MON_SPRITE_Y);
    PokenavFillPalette(15, 0);
}

static void DestroyRibbonsMonFrontPic(struct PokenavSub14 *structPtr)
{
    FreeAndDestroyMonPicSprite(structPtr->monSpriteId);
}

// x and y arguments are ignored
// y is always given as MON_SPRITE_Y
// x is given as either MON_SPRITE_X_ON or MON_SPRITE_X_OFF (but ignored and MON_SPRITE_X_ON is used)
static u16 DrawRibbonsMonFrontPic(s32 x, s32 y)
{
    u16 species, spriteId;
    u32 personality, otId;

    GetMonSpeciesPersonalityOtId(&species, &personality, &otId);
    spriteId = CreateMonPicSprite_HandleDeoxys(species, otId, personality, TRUE, MON_SPRITE_X_ON, MON_SPRITE_Y, 15, 0xFFFF);
    gSprites[spriteId].oam.priority = 0;
    return spriteId;
}

static void SlideMonSpriteOff(struct PokenavSub14 *structPtr)
{
    StartMonSpriteSlide(&gSprites[structPtr->monSpriteId], MON_SPRITE_X_ON, MON_SPRITE_X_OFF, 6);
}

static void SlideMonSpriteOn(struct PokenavSub14 *structPtr)
{
    // Switch to new mon sprite
    FreeAndDestroyMonPicSprite(structPtr->monSpriteId);
    structPtr->monSpriteId = DrawRibbonsMonFrontPic(MON_SPRITE_X_OFF, MON_SPRITE_Y);

    // Slide on
    StartMonSpriteSlide(&gSprites[structPtr->monSpriteId], MON_SPRITE_X_OFF, MON_SPRITE_X_ON, 6);
}

// Is Pokémon summary sprite still sliding off/on
static bool32 IsMonSpriteAnimating(struct PokenavSub14 *structPtr)
{
    return (gSprites[structPtr->monSpriteId].callback != SpriteCallbackDummy);
}

#define sCurrX    data[0]
#define sMoveIncr data[1]
#define sTime     data[2]
#define sDestX    data[3]

static void StartMonSpriteSlide(struct Sprite *sprite, s32 startX, s32 destX, s32 time)
{
    u32 delta = destX - startX;

    sprite->pos1.x = startX;
    sprite->sCurrX = startX << 4;
    sprite->sMoveIncr = (delta << 4) / time;
    sprite->sTime = time;
    sprite->sDestX = destX;

    sprite->callback = SpriteCB_MonSpriteSlide;
}

static void SpriteCB_MonSpriteSlide(struct Sprite *sprite)
{
    if (sprite->sTime != 0)
    {
        sprite->sTime--;
        sprite->sCurrX += sprite->sMoveIncr;
        sprite->pos1.x = sprite->sCurrX >> 4;
        if (sprite->pos1.x <= MON_SPRITE_X_OFF)
            sprite->invisible = TRUE;
        else
            sprite->invisible = FALSE;
    }
    else
    {
        sprite->pos1.x = sprite->sDestX;
        sprite->callback = SpriteCallbackDummy;
    }
}

#undef sCurrX
#undef sMoveIncr
#undef sTime
#undef sDestX

static void DrawAllRibbonsSmall(struct PokenavSub14 *structPtr)
{
    u32 *ribbonIds;

    ClearRibbonsSummaryBg();

    ribbonIds = GetNormalRibbonIds(&sRibbonDraw_Total);
    for (sRibbonDraw_Current = 0; sRibbonDraw_Current < sRibbonDraw_Total; sRibbonDraw_Current++)
        DrawRibbonSmall(sRibbonDraw_Current, *(ribbonIds++));

    ribbonIds = GetGiftRibbonIds(&sRibbonDraw_Total);
    for (sRibbonDraw_Current = 0; sRibbonDraw_Current < sRibbonDraw_Total; sRibbonDraw_Current++)
        DrawRibbonSmall(sRibbonDraw_Current + GIFT_RIBBON_START_POS, *(ribbonIds++));

    CopyBgTilemapBufferToVram(1);
}

// Redundant, the same FillBg is called in LoopedTask_OpenRibbonsSummaryMenu
static void ClearRibbonsSummaryBg(void)
{
    FillBgTilemapBufferRect_Palette0(1, 0, 0, 0, 32, 20);
}

static void DrawRibbonSmall(u32 i, u32 ribbonId)
{
    u16 bgData[4];
    u32 destX = (i % RIBBONS_PER_ROW) * 2 + 11;
    u32 destY = (i / RIBBONS_PER_ROW) * 2 + 4;

    BufferSmallRibbonGfxData(bgData, ribbonId);
    CopyToBgTilemapBufferRect(1, bgData, destX, destY, 2, 2);
}

// Below correspond to a ribbon icon in ribbons/icons.png and ribbons/icons_big.png; 0 at top, 11 at bottom
enum {
    RIBBONGFX_CHAMPION,
    RIBBONGFX_CONTEST_NORMAL,
    RIBBONGFX_CONTEST_SUPER,
    RIBBONGFX_CONTEST_HYPER,
    RIBBONGFX_CONTEST_MASTER,
    RIBBONGFX_WINNING,
    RIBBONGFX_VICTORY,
    RIBBONGFX_ARTIST,
    RIBBONGFX_EFFORT,
    RIBBONGFX_GIFT_1,
    RIBBONGFX_GIFT_2,
    RIBBONGFX_GIFT_3,
};

#define TO_PAL_OFFSET(palNum)((palNum) - PALTAG_RIBBON_ICONS_1)

struct
{
    u16 tileNumOffset;
    u16 palNumOffset;
} static const sRibbonGfxData[] =
{
    [CHAMPION_RIBBON]      = { RIBBONGFX_CHAMPION,       TO_PAL_OFFSET(PALTAG_RIBBON_ICONS_1)},
    [COOL_RIBBON_NORMAL]   = { RIBBONGFX_CONTEST_NORMAL, TO_PAL_OFFSET(PALTAG_RIBBON_ICONS_1)},
    [COOL_RIBBON_SUPER]    = { RIBBONGFX_CONTEST_SUPER,  TO_PAL_OFFSET(PALTAG_RIBBON_ICONS_1)},
    [COOL_RIBBON_HYPER]    = { RIBBONGFX_CONTEST_HYPER,  TO_PAL_OFFSET(PALTAG_RIBBON_ICONS_1)},
    [COOL_RIBBON_MASTER]   = { RIBBONGFX_CONTEST_MASTER, TO_PAL_OFFSET(PALTAG_RIBBON_ICONS_1)},
    [BEAUTY_RIBBON_NORMAL] = { RIBBONGFX_CONTEST_NORMAL, TO_PAL_OFFSET(PALTAG_RIBBON_ICONS_2)},
    [BEAUTY_RIBBON_SUPER]  = { RIBBONGFX_CONTEST_SUPER,  TO_PAL_OFFSET(PALTAG_RIBBON_ICONS_2)},
    [BEAUTY_RIBBON_HYPER]  = { RIBBONGFX_CONTEST_HYPER,  TO_PAL_OFFSET(PALTAG_RIBBON_ICONS_2)},
    [BEAUTY_RIBBON_MASTER] = { RIBBONGFX_CONTEST_MASTER, TO_PAL_OFFSET(PALTAG_RIBBON_ICONS_2)},
    [CUTE_RIBBON_NORMAL]   = { RIBBONGFX_CONTEST_NORMAL, TO_PAL_OFFSET(PALTAG_RIBBON_ICONS_3)},
    [CUTE_RIBBON_SUPER]    = { RIBBONGFX_CONTEST_SUPER,  TO_PAL_OFFSET(PALTAG_RIBBON_ICONS_3)},
    [CUTE_RIBBON_HYPER]    = { RIBBONGFX_CONTEST_HYPER,  TO_PAL_OFFSET(PALTAG_RIBBON_ICONS_3)},
    [CUTE_RIBBON_MASTER]   = { RIBBONGFX_CONTEST_MASTER, TO_PAL_OFFSET(PALTAG_RIBBON_ICONS_3)},
    [SMART_RIBBON_NORMAL]  = { RIBBONGFX_CONTEST_NORMAL, TO_PAL_OFFSET(PALTAG_RIBBON_ICONS_4)},
    [SMART_RIBBON_SUPER]   = { RIBBONGFX_CONTEST_SUPER,  TO_PAL_OFFSET(PALTAG_RIBBON_ICONS_4)},
    [SMART_RIBBON_HYPER]   = { RIBBONGFX_CONTEST_HYPER,  TO_PAL_OFFSET(PALTAG_RIBBON_ICONS_4)},
    [SMART_RIBBON_MASTER]  = { RIBBONGFX_CONTEST_MASTER, TO_PAL_OFFSET(PALTAG_RIBBON_ICONS_4)},
    [TOUGH_RIBBON_NORMAL]  = { RIBBONGFX_CONTEST_NORMAL, TO_PAL_OFFSET(PALTAG_RIBBON_ICONS_5)},
    [TOUGH_RIBBON_SUPER]   = { RIBBONGFX_CONTEST_SUPER,  TO_PAL_OFFSET(PALTAG_RIBBON_ICONS_5)},
    [TOUGH_RIBBON_HYPER]   = { RIBBONGFX_CONTEST_HYPER,  TO_PAL_OFFSET(PALTAG_RIBBON_ICONS_5)},
    [TOUGH_RIBBON_MASTER]  = { RIBBONGFX_CONTEST_MASTER, TO_PAL_OFFSET(PALTAG_RIBBON_ICONS_5)},
    [WINNING_RIBBON]       = { RIBBONGFX_WINNING,        TO_PAL_OFFSET(PALTAG_RIBBON_ICONS_1)},
    [VICTORY_RIBBON]       = { RIBBONGFX_VICTORY,        TO_PAL_OFFSET(PALTAG_RIBBON_ICONS_1)},
    [ARTIST_RIBBON]        = { RIBBONGFX_ARTIST,         TO_PAL_OFFSET(PALTAG_RIBBON_ICONS_2)},
    [EFFORT_RIBBON]        = { RIBBONGFX_EFFORT,         TO_PAL_OFFSET(PALTAG_RIBBON_ICONS_3)},
    [MARINE_RIBBON]        = { RIBBONGFX_GIFT_1,         TO_PAL_OFFSET(PALTAG_RIBBON_ICONS_2)},
    [LAND_RIBBON]          = { RIBBONGFX_GIFT_1,         TO_PAL_OFFSET(PALTAG_RIBBON_ICONS_4)},
    [SKY_RIBBON]           = { RIBBONGFX_GIFT_1,         TO_PAL_OFFSET(PALTAG_RIBBON_ICONS_5)},
    [COUNTRY_RIBBON]       = { RIBBONGFX_GIFT_2,         TO_PAL_OFFSET(PALTAG_RIBBON_ICONS_4)},
    [NATIONAL_RIBBON]      = { RIBBONGFX_GIFT_2,         TO_PAL_OFFSET(PALTAG_RIBBON_ICONS_5)},
    [EARTH_RIBBON]         = { RIBBONGFX_GIFT_3,         TO_PAL_OFFSET(PALTAG_RIBBON_ICONS_1)},
    [WORLD_RIBBON]         = { RIBBONGFX_GIFT_3,         TO_PAL_OFFSET(PALTAG_RIBBON_ICONS_2)},
};

#undef TO_PAL_OFFSET

static void BufferSmallRibbonGfxData(u16 *dst, u32 ribbonId)
{
    u16 palNum = sRibbonGfxData[ribbonId].palNumOffset + 2;
    u16 tileNum = (sRibbonGfxData[ribbonId].tileNumOffset * 2) + 1;

    dst[0] = tileNum | (palNum << 12);
    dst[1] = tileNum | (palNum << 12) | 0x400;
    dst[2] = (tileNum + 1) | (palNum << 12);
    dst[3] = (tileNum + 1) | (palNum << 12) | 0x400;
}

static const struct CompressedSpriteSheet sSpriteSheet_RibbonIconsBig =
{
    sRibbonIconsBig_Gfx, 0x1800, GFXTAG_RIBBON_ICONS_BIG
};

static const struct SpritePalette sSpritePalettes_RibbonIcons[] =
{
    {sRibbonIcons1_Pal, PALTAG_RIBBON_ICONS_1},
    {sRibbonIcons2_Pal, PALTAG_RIBBON_ICONS_2},
    {sRibbonIcons3_Pal, PALTAG_RIBBON_ICONS_3},
    {sRibbonIcons4_Pal, PALTAG_RIBBON_ICONS_4},
    {sRibbonIcons5_Pal, PALTAG_RIBBON_ICONS_5},
    {},
};

static const struct OamData sOamData_RibbonIconBig =
{
    .y = 0,
    .affineMode = ST_OAM_AFFINE_NORMAL,
    .objMode = ST_OAM_OBJ_NORMAL,
    .mosaic = 0,
    .bpp = ST_OAM_4BPP,
    .shape = SPRITE_SHAPE(32x32),
    .x = 0,
    .matrixNum = 0,
    .size = SPRITE_SIZE(32x32),
    .tileNum = 0,
    .priority = 1,
    .paletteNum = 0,
    .affineParam = 0
};

static const union AffineAnimCmd sAffineAnim_RibbonIconBig_Normal[] =
{
    AFFINEANIMCMD_FRAME(128, 128, 0, 0),
    AFFINEANIMCMD_END
};

static const union AffineAnimCmd sAffineAnim_RibbonIconBig_ZoomIn[] =
{
    AFFINEANIMCMD_FRAME(128, 128, 0, 0),
    AFFINEANIMCMD_FRAME(32, 32, 0, 4),
    AFFINEANIMCMD_END
};

static const union AffineAnimCmd sAffineAnim_RibbonIconBig_ZoomOut[] =
{
    AFFINEANIMCMD_FRAME(256, 256, 0, 0),
    AFFINEANIMCMD_FRAME(-32, -32, 0, 4),
    AFFINEANIMCMD_END
};

enum {
    RIBBONANIM_NORMAL,
    RIBBONANIM_ZOOM_IN,
    RIBBONANIM_ZOOM_OUT,
};

static const union AffineAnimCmd *const sAffineAnims_RibbonIconBig[] =
{
    [RIBBONANIM_NORMAL]   = sAffineAnim_RibbonIconBig_Normal,
    [RIBBONANIM_ZOOM_IN]  = sAffineAnim_RibbonIconBig_ZoomIn,
    [RIBBONANIM_ZOOM_OUT] = sAffineAnim_RibbonIconBig_ZoomOut
};

static const struct SpriteTemplate sSpriteTemplate_RibbonIconBig =
{
    .tileTag = GFXTAG_RIBBON_ICONS_BIG,
    .paletteTag = PALTAG_RIBBON_ICONS_1,
    .oam = &sOamData_RibbonIconBig,
    .anims = gDummySpriteAnimTable,
    .images = NULL,
    .affineAnims = sAffineAnims_RibbonIconBig,
    .callback = SpriteCallbackDummy,
};

// Create dummy sprite to be used for the zoomed in version of the selected ribbon
static void CreateBigRibbonSprite(struct PokenavSub14 *structPtr)
{
    u8 spriteId;

    LoadCompressedSpriteSheet(&sSpriteSheet_RibbonIconsBig);
    Pokenav_AllocAndLoadPalettes(sSpritePalettes_RibbonIcons);

    spriteId = CreateSprite(&sSpriteTemplate_RibbonIconBig, 0, 0, 0);
    structPtr->bigRibbonSprite = &gSprites[spriteId];
    structPtr->bigRibbonSprite->invisible = TRUE;
}

#define sInvisibleWhenDone data[0]

static void UpdateAndZoomInSelectedRibbon(struct PokenavSub14 *structPtr)
{
    u32 ribbonId;
    s32 position = GetSelectedPosition();
    s32 x = (position % RIBBONS_PER_ROW) * 16 + 96;
    s32 y = (position / RIBBONS_PER_ROW) * 16 + 40;

    structPtr->bigRibbonSprite->pos1.x = x;
    structPtr->bigRibbonSprite->pos1.y = y;

    // Set new selected ribbon's gfx data
    ribbonId = GetRibbonId();
    structPtr->bigRibbonSprite->oam.tileNum = (sRibbonGfxData[ribbonId].tileNumOffset * 16) + GetSpriteTileStartByTag(GFXTAG_RIBBON_ICONS_BIG);
    structPtr->bigRibbonSprite->oam.paletteNum = IndexOfSpritePaletteTag(sRibbonGfxData[ribbonId].palNumOffset + PALTAG_RIBBON_ICONS_1);

    // Start zoom in animation
    StartSpriteAffineAnim(structPtr->bigRibbonSprite, RIBBONANIM_ZOOM_IN);
    structPtr->bigRibbonSprite->invisible = FALSE;
    structPtr->bigRibbonSprite->sInvisibleWhenDone = FALSE;
    structPtr->bigRibbonSprite->callback = SpriteCB_WaitForRibbonAnimation;
}

// Start animation to zoom out of selected ribbon
static void ZoomOutSelectedRibbon(struct PokenavSub14 *structPtr)
{
    structPtr->bigRibbonSprite->sInvisibleWhenDone = TRUE;
    StartSpriteAffineAnim(structPtr->bigRibbonSprite, RIBBONANIM_ZOOM_OUT);
    structPtr->bigRibbonSprite->callback = SpriteCB_WaitForRibbonAnimation;
}

static bool32 IsRibbonAnimating(struct PokenavSub14 *structPtr)
{
    return (structPtr->bigRibbonSprite->callback != SpriteCallbackDummy);
}

static void SpriteCB_WaitForRibbonAnimation(struct Sprite *sprite)
{
    if (sprite->affineAnimEnded)
    {
        sprite->invisible = sprite->sInvisibleWhenDone;
        sprite->callback = SpriteCallbackDummy;
    }
}
