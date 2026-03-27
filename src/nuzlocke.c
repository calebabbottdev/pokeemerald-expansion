#include "global.h"
#include "event_data.h"
#include "nuzlocke.h"

// Start of flag range reserved for Nuzlocke (you may need to adjust this based on available flags)
#define NUZLOCKE_FLAG_START 0x500

EWRAM_DATA static NuzlockeData sNuzlockeData = {};
EWRAM_DATA static bool8 sNuzlockeDataInitialized = FALSE;

// Initialize Nuzlocke data from save
static void InitializeNuzlockeData(void)
{
    u16 i;
    
    if (sNuzlockeDataInitialized == TRUE)
        return;
    
    // Load saved Nuzlocke data from flags if needed
    // For now, initialize empty
    sNuzlockeData.encounteredCount = 0;
    for (i = 0; i < NUZLOCKE_MAX_ENCOUNTERS; i++)
    {
        sNuzlockeData.encounters[i].mapGroup = 0;
        sNuzlockeData.encounters[i].mapNum = 0;
    }
    
    sNuzlockeDataInitialized = TRUE;
}

bool8 IsNuzlockeModeEnabled(void)
{
    return FlagGet(FLAG_NUZLOCKE_MODE);
}

bool8 HasMapBeenEncountered(u8 mapGroup, u8 mapNum)
{
    u16 i;
    
    // Only check if Nuzlocke mode is enabled
    if (!IsNuzlockeModeEnabled())
        return FALSE;
    
    if (sNuzlockeDataInitialized == FALSE)
        InitializeNuzlockeData();
    
    // Search through encountered locations
    for (i = 0; i < sNuzlockeData.encounteredCount; i++)
    {
        if (sNuzlockeData.encounters[i].mapGroup == mapGroup && 
            sNuzlockeData.encounters[i].mapNum == mapNum)
        {
            return TRUE;
        }
    }
    
    return FALSE;
}

void MarkMapAsEncountered(u8 mapGroup, u8 mapNum)
{    
    if (!IsNuzlockeModeEnabled())
        return;
    
    if (sNuzlockeDataInitialized == FALSE)
        InitializeNuzlockeData();
    
    // Check if already marked
    if (HasMapBeenEncountered(mapGroup, mapNum))
        return;
    
    // Add new encounter location if there's space
    if (sNuzlockeData.encounteredCount < NUZLOCKE_MAX_ENCOUNTERS)
    {
        sNuzlockeData.encounters[sNuzlockeData.encounteredCount].mapGroup = mapGroup;
        sNuzlockeData.encounters[sNuzlockeData.encounteredCount].mapNum = mapNum;
        sNuzlockeData.encounteredCount++;
    }

    FlagSet(FLAG_NO_CATCHING);
}

void ResetNuzlockeEncounters(void)
{
    u16 i;
    
    sNuzlockeData.encounteredCount = 0;
    for (i = 0; i < NUZLOCKE_MAX_ENCOUNTERS; i++)
    {
        sNuzlockeData.encounters[i].mapGroup = 0;
        sNuzlockeData.encounters[i].mapNum = 0;
    }
}

NuzlockeData *GetNuzlockeData(void)
{
    if (sNuzlockeDataInitialized == FALSE)
        InitializeNuzlockeData();
    
    return &sNuzlockeData;
}
