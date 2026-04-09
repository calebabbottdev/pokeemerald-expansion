#include "global.h"
#include "event_data.h"
#include "pokemon.h"
#include "dynamic_mon_levels.h"

// Extra levels added to a trainer's last (ace) mon after spread scaling.
#define DYNAMIC_LEVEL_ACE_BONUS  1

// Weight applied to the player's highest-level party member when computing
// the weighted average.  Higher values bias the result more toward the ace.
#define DYNAMIC_LEVEL_ACE_WEIGHT 3

// Only the top N party members (by level) are used in the target calculation.
// This prevents a cheese party (e.g. one level 100 + five level 5s) from
// dragging the target down — low-level fillers are simply ignored.
#define DYNAMIC_LEVEL_TOP_MONS   3

// Minimum level at which trade evolutions are applied when scaling trainer mons.
// Represents the point in the game where a trainer would plausibly have traded.
// All classic trade mons (Kadabra, Machoke, Graveler, Haunter) reach their
// middle stage in the mid-20s, so 36 gives a natural buffer before the final form.
#define DYNAMIC_LEVEL_TRADE_EVO_MIN 36

// Minimum level at which item-use evolutions (Fire Stone, Thunder Stone, etc.)
// are applied when scaling trainer mons. Matches the trade evo threshold —
// by the mid-30s a trainer would plausibly have used an evolution stone.
// Note: for Pokémon with multiple item evolutions (e.g. Eevee), the first
// entry in their evolution table is used.
#define DYNAMIC_LEVEL_ITEM_EVO_MIN  36

// Minimum target level enforced per badge count (index = badge count 0–8).
// 0 badges: no floor, so route 1 stays natural until the player beats Brock.
static const u8 sBadgeLevelFloors[9] =
{
     0, // 0 badges  – no floor
    15, // 1 badge   (Brock)
    20, // 2 badges  (Misty)
    25, // 3 badges  (Lt. Surge)
    30, // 4 badges  (Erika)
    38, // 5 badges  (Koga)
    44, // 6 badges  (Sabrina)
    48, // 7 badges  (Blaine)
    52, // 8 badges  (Giovanni / Elite Four+)
};

static u8 GetBadgeCount(void)
{
    u8 count = 0;
    u16 i;
    for (i = FLAG_BADGE01_GET; i < FLAG_BADGE01_GET + NUM_BADGES; i++)
        if (FlagGet(i))
            count++;
    return count;
}

u8 GetDynamicTargetLevel(void)
{
    u8 levels[PARTY_SIZE];
    u8 count = 0;
    u8 useCount;
    u8 i, j;
    u32 sum = 0;
    u32 weighted;
    u8 floor;

    // Collect valid (non-egg, non-fainted) party levels.
    for (i = 0; i < PARTY_SIZE; i++)
    {
        u32 species = GetMonData(&gPlayerParty[i], MON_DATA_SPECIES_OR_EGG);
        if (species == SPECIES_EGG || species == SPECIES_NONE)
            continue;
        if (GetMonData(&gPlayerParty[i], MON_DATA_HP) == 0)
            continue;
        levels[count++] = (u8)GetMonData(&gPlayerParty[i], MON_DATA_LEVEL);
    }

    // Failsafe: no valid (non-fainted, non-egg) party members.
    if (count == 0)
        return 1;

    // Sort levels descending (insertion sort — at most PARTY_SIZE elements).
    for (i = 1; i < count; i++)
    {
        u8 key = levels[i];
        j = i;
        while (j > 0 && levels[j - 1] < key)
        {
            levels[j] = levels[j - 1];
            j--;
        }
        levels[j] = key;
    }

    // Only use the top DYNAMIC_LEVEL_TOP_MONS levels so that low-level fillers
    // cannot drag the target down when the player has a strong ace.
    useCount = count < DYNAMIC_LEVEL_TOP_MONS ? count : DYNAMIC_LEVEL_TOP_MONS;
    for (i = 0; i < useCount; i++)
        sum += levels[i];

    // Weighted average: levels[0] is the highest after sorting.
    weighted = (sum + (u32)levels[0] * DYNAMIC_LEVEL_ACE_WEIGHT)
             / (useCount + DYNAMIC_LEVEL_ACE_WEIGHT);

    {
        u8 badges = GetBadgeCount();
        if (badges > 8)
            badges = 8;
        floor = sBadgeLevelFloors[badges];
    }

    if (weighted < floor)
        weighted = floor;
    if (weighted < 1)
        weighted = 1;
    if (weighted > 100)
        weighted = 100;

    return (u8)weighted;
}

u16 GetLevelEvolvedSpecies(u16 species, u8 level)
{
    u16 current = species;
    u8 depth;

    // Walk at most 4 stages (deeper than any official evolution chain).
    // EVO_LEVEL and EVO_LEVEL_BATTLE_ONLY apply at their normal thresholds.
    // EVO_TRADE applies at or above DYNAMIC_LEVEL_TRADE_EVO_MIN.
    // EVO_ITEM applies at or above DYNAMIC_LEVEL_ITEM_EVO_MIN.
    // Friendship evolutions are skipped to avoid incorrect results.
    for (depth = 0; depth < 4; depth++)
    {
        const struct Evolution *evos = GetSpeciesEvolutions(current);
        u16 next = SPECIES_NONE;
        u32 i;

        if (evos == NULL)
            break;

        for (i = 0; evos[i].method != EVOLUTIONS_END; i++)
        {
            if (SanitizeSpeciesId(evos[i].targetSpecies) == SPECIES_NONE)
                continue;
            if ((evos[i].method == EVO_LEVEL || evos[i].method == EVO_LEVEL_BATTLE_ONLY)
                && evos[i].param <= level)
            {
                next = evos[i].targetSpecies;
                break; // Take the first valid match, same priority as GetEvolutionTargetSpecies.
            }
            // Trade evolutions have no level requirement, so apply them once the
            // mon has reached a level where a trainer would plausibly have traded.
            if (evos[i].method == EVO_TRADE && level >= DYNAMIC_LEVEL_TRADE_EVO_MIN)
            {
                next = evos[i].targetSpecies;
                break;
            }
            // Item evolutions (Fire Stone, Thunder Stone, etc.) are applied at
            // the same threshold — a trainer at this point would plausibly have
            // used an evolution stone on their mon.
            if (evos[i].method == EVO_ITEM && level >= DYNAMIC_LEVEL_ITEM_EVO_MIN)
            {
                next = evos[i].targetSpecies;
                break;
            }
        }

        if (next == SPECIES_NONE)
            break; // No further level-up evolution at this level.

        current = next;
    }

    return current;
}

u8 ScaleTrainerMonLevel(u8 baseLevel, u8 trainerAvg, u8 targetLevel, bool8 isAce)
{
    // Spread: how far this mon sits above/below the trainer's own mean.
    // Adding the spread to the target preserves the trainer's intended
    // intra-party level distribution (e.g. ace stays relatively stronger).
    s16 spread   = (s16)baseLevel - (s16)trainerAvg;
    s16 newLevel = (s16)targetLevel + spread;

    if (isAce)
        newLevel += DYNAMIC_LEVEL_ACE_BONUS;

    if (newLevel < 1)
        newLevel = 1;
    if (newLevel > 100)
        newLevel = 100;

    return (u8)newLevel;
}
