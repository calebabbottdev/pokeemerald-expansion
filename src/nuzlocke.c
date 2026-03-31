#include "global.h"
#include "event_data.h"
#include "nuzlocke.h"
#include "pokedex.h"

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
    if (!IsNuzlockeModeEnabled() && !FlagGet(FLAG_ADVENTURE_STARTED))
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

// DFS through an evolution tree rooted at 'species', returning TRUE if any member is caught.
static bool8 IsEvolutionLineOwnedHelper(u16 species, u8 depth)
{
    const struct Evolution *evolutions;
    u32 j;

    // Guard against unexpectedly deep or circular trees
    if (depth > 10)
        return FALSE;

    if (GetSetPokedexFlag(SpeciesToNationalPokedexNum(species), FLAG_GET_CAUGHT))
        return TRUE;

    evolutions = GetSpeciesEvolutions(species);
    // GetSpeciesEvolutions returns NULL when the species has no evolutions.
    // Dereferencing NULL reads garbage from the GBA BIOS area, so guard here.
    if (evolutions == NULL)
        return FALSE;

    for (j = 0; evolutions[j].method != EVOLUTIONS_END; j++)
    {
        u16 targetSpecies = evolutions[j].targetSpecies;
        // Bounds-check before calling IsSpeciesEnabled (which does not bounds-check gSpeciesInfo).
        if (targetSpecies > SPECIES_NONE && targetSpecies < NUM_SPECIES && IsSpeciesEnabled(targetSpecies))
        {
            if (IsEvolutionLineOwnedHelper(targetSpecies, depth + 1))
                return TRUE;
        }
    }

    return FALSE;
}

// Returns TRUE if the encountered species, or any member of its full evolution line, is already caught.
// Walks up to the base species via GetSpeciesPreEvolution, then performs a DFS through all evolutions.
bool8 IsEncounteredSpeciesInOwnedEvolutionLine(u16 species)
{
    u16 baseSpecies = species;
    u16 preEvo;

    if (species == SPECIES_NONE || species >= NUM_SPECIES || !IsSpeciesEnabled(species))
        return FALSE;

    // Walk up to the root of the evolution tree
    while ((preEvo = GetSpeciesPreEvolution(baseSpecies)) != SPECIES_NONE)
        baseSpecies = preEvo;

    return IsEvolutionLineOwnedHelper(baseSpecies, 0);
}

NuzlockeData *GetNuzlockeData(void)
{
    if (sNuzlockeDataInitialized == FALSE)
        InitializeNuzlockeData();
    
    return &sNuzlockeData;
}
