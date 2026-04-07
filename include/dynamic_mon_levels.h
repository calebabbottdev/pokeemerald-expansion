#ifndef GUARD_DYNAMIC_MON_LEVELS_H
#define GUARD_DYNAMIC_MON_LEVELS_H

// Returns the dynamic target level for enemy Pokémon, based on the
// player's party level (weighted toward the highest-level party member)
// and a badge-count-based minimum floor.
// Only meaningful when FLAG_DYNAMIC_MON_LEVELS is set.
u8  GetDynamicTargetLevel(void);

// Returns the highest-stage species reachable from `species` through
// EVO_LEVEL / EVO_LEVEL_BATTLE_ONLY evolutions at the given `level`.
// Item, trade, and friendship evolutions are intentionally ignored.
u16 GetLevelEvolvedSpecies(u16 species, u8 level);

// Scales a single trainer mon's level relative to:
//   baseLevel   - the mon's hardcoded level from trainer party data
//   trainerAvg  - mean level across the full trainer party
//   targetLevel - result of GetDynamicTargetLevel()
//   isAce       - TRUE for the last mon in the party (trainer's ace)
// Returns the scaled level clamped to [1, 100].
u8  ScaleTrainerMonLevel(u8 baseLevel, u8 trainerAvg, u8 targetLevel, bool8 isAce);

#endif // GUARD_DYNAMIC_MON_LEVELS_H
