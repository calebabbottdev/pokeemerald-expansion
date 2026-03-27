#include "randomizer.h"
#include "move.h"
#include "constants/abilities.h"
#include "constants/species.h"
// #include "constants/types.h"
#include "random.h"
#include "pokemon.h"

#define MAX_LEARNSET_MOVES 64
#define MAX_TYPES 2

static struct LevelUpMove sRandomizedLearnset[MAX_LEARNSET_MOVES];

// ============== SPECIES RANDOMIZATION ==============

// Returns the BST for a given species
static u16 GetSpeciesBST(u16 species) {
    const struct SpeciesInfo *info = &gSpeciesInfo[species];
    return info->baseHP + info->baseAttack + info->baseDefense + info->baseSpeed + info->baseSpAttack + info->baseSpDefense;
}

// Returns a random valid species with BST within a bracket of the original species
u16 GetRandomSpeciesWithSimilarBST(u16 originalSpecies, u16 range)
{
    u16 originalBST = GetSpeciesBST(originalSpecies);

    u16 min = (originalBST > range) ? (originalBST - range) : 0;
    u16 max = originalBST + range;

    u16 count = 0;

    // First pass: count valid candidates
    for (u16 i = 1; i < NUM_SPECIES - 1; i++)
    {
        if (gSpeciesInfo[i].speciesName[0] == 0)
            continue;

        u16 bst = GetSpeciesBST(i);
        if (bst >= min && bst <= max)
            count++;
    }

    if (count == 0)
        return originalSpecies;

    u16 target = Random() % count;

    // Second pass: pick the target-th candidate
    count = 0;
    for (u16 i = 1; i < NUM_SPECIES - 1; i++)
    {
        if (gSpeciesInfo[i].speciesName[0] == 0)
            continue;

        u16 bst = GetSpeciesBST(i);
        if (bst >= min && bst <= max)
        {
            if (count == target)
                return i;
            count++;
        }
    }

    return originalSpecies; // fallback (should never hit)
}

// ============== MOVE RANDOMIZATION ==============

// Returns a random move (excluding MOVE_NONE)
static enum Move GetRandomMove(void) {
    return (Random() % (MOVES_COUNT_ALL - 1)) + 1;
}

// Randomize all moveslots to random valid moves (excluding MOVE_NONE)
void RandomizeMonMoves(struct Pokemon *mon)
{
    for (int i = 0; i < MAX_MON_MOVES; i++) {
        u16 move = GetRandomMove();
        SetMonData(mon, MON_DATA_MOVE1 + i, &move);
        u32 pp = GetMovePP(move);
        SetMonData(mon, MON_DATA_PP1 + i, &pp);
    }
}

// ============== ABILITY RANDOMIZATION ==============

// Randomize ability slot (0, 1, or 2) among the Pokemon's available abilities
void RandomizeMonAbility(struct Pokemon *mon)
{
    u8 abilityNum = Random() % NUM_ABILITY_SLOTS; // 0, 1, or 2
    SetMonData(mon, MON_DATA_ABILITY_NUM, &abilityNum);
}

// ============== LEARNSET RANDOMIZATION ==============

// Returns a learnset with randomized moves but original level structure
const struct LevelUpMove *GetRandomizedLearnset(u16 species)
{
    const struct LevelUpMove *originalLearnset = gSpeciesInfo[SanitizeSpeciesId(species)].levelUpLearnset;
    if (originalLearnset == NULL)
        return gSpeciesInfo[SPECIES_NONE].levelUpLearnset;
    
    // Copy structure with randomized moves but keep original levels
    u32 moveCount = 0;
    for (u32 i = 0; originalLearnset[i].move != LEVEL_UP_MOVE_END && moveCount < MAX_LEARNSET_MOVES - 1; i++) {
        sRandomizedLearnset[moveCount].level = originalLearnset[i].level;
        sRandomizedLearnset[moveCount].move = GetRandomMove();
        moveCount++;
    }
    // Terminate the learnset
    sRandomizedLearnset[moveCount].move = LEVEL_UP_MOVE_END;
    sRandomizedLearnset[moveCount].level = 0;
    
    return sRandomizedLearnset;
}
