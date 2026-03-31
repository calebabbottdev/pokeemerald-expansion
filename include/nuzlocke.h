#ifndef GUARD_NUZLOCKE_H
#define GUARD_NUZLOCKE_H

#include "constants/maps.h"

// Maximum number of locations that can be tracked for Nuzlocke encounters
#define NUZLOCKE_MAX_ENCOUNTERS 256

// Structure to store a single encountered location
typedef struct {
    u8 mapGroup;
    u8 mapNum;
} NuzlockeEncounteredLocation;

// Main Nuzlocke data structure
typedef struct {
    NuzlockeEncounteredLocation encounters[NUZLOCKE_MAX_ENCOUNTERS];
    u16 encounteredCount;
} NuzlockeData;

// Check if Nuzlocke mode is enabled
bool8 IsNuzlockeModeEnabled(void);

// Check if player has already caught a Pokémon on this map
bool8 HasMapBeenEncountered(u8 mapGroup, u8 mapNum);

// Mark a map as encountered (after catching a Pokémon there)
void MarkMapAsEncountered(u8 mapGroup, u8 mapNum);

// Reset all encountered locations (for new game)
void ResetNuzlockeEncounters(void);

// Get the current Nuzlocke data
NuzlockeData *GetNuzlockeData(void);

// Returns TRUE if the encountered species (or any member of its evolution line) is already caught.
// Used to implement the Species Clause: allow catching even on a previously-visited map.
bool8 IsEncounteredSpeciesInOwnedEvolutionLine(u16 species);

#endif // GUARD_NUZLOCKE_H
