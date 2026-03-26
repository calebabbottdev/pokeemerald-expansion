#ifndef GUARD_RANDOMIZER_H
#define GUARD_RANDOMIZER_H

#include "global.h"
#include "pokemon.h"

// Species randomization - randomizes to a species with similar BST
u16 GetRandomSpeciesWithSimilarBST(u16 originalSpecies, u16 range);

// Move randomization - sets 4 random moves on a Pokemon
void RandomizeMonMoves(struct Pokemon *mon);

// Ability randomization - randomizes the ability slot (0, 1, or 2)
void RandomizeMonAbility(struct Pokemon *mon);

// Learnset randomization - randomizes moves at each level but preserves level structure
const struct LevelUpMove *GetRandomizedLearnset(u16 species);

#endif // GUARD_RANDOMIZER_H
