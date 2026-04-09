#include "global.h"
#include "event_data.h"
#include "pokemon.h"
#include "dynamic_mon_levels.h"
#include "constants/trainers.h"

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

// Returns the scaling percentage (0–100) applied to targetLevel for a given
// trainer class before the intra-party spread is added.  100 = full player
// level; lower values keep that class's mons proportionally weaker.
// Edit the cases below — they are the primary difficulty knob per class.
//
// Tier 1 – 85%  : clearly young / beginner trainers
// Tier 2 – 90%  : casual hobbyist trainers
// Tier 3 – 95%  : trained specialists without elite status
// Tier 4 – 100% : gym leaders, rivals, evil teams, frontier brains, etc. (default)
static u8 GetTrainerClassScalePct(u16 trainerClass)
{
    switch (trainerClass)
    {
    // ---- Tier 1 (85%) ---- young/beginner trainers ----
    case TRAINER_CLASS_BUG_CATCHER:
    case TRAINER_CLASS_BUG_CATCHER_FRLG:
    case TRAINER_CLASS_YOUNGSTER:
    case TRAINER_CLASS_YOUNGSTER_FRLG:
    case TRAINER_CLASS_LASS:
    case TRAINER_CLASS_LASS_FRLG:
    case TRAINER_CLASS_TUBER_F:
    case TRAINER_CLASS_TUBER_M:
    case TRAINER_CLASS_TUBER_FRLG:
    case TRAINER_CLASS_SCHOOL_KID:
        return 85;

    // ---- Tier 2 (90%) ---- casual/hobbyist trainers ----
    case TRAINER_CLASS_CAMPER:
    case TRAINER_CLASS_CAMPER_FRLG:
    case TRAINER_CLASS_PICNICKER:
    case TRAINER_CLASS_PICNICKER_FRLG:
    case TRAINER_CLASS_FISHERMAN:
    case TRAINER_CLASS_FISHERMAN_FRLG:
    case TRAINER_CLASS_HIKER:
    case TRAINER_CLASS_HIKER_FRLG:
    case TRAINER_CLASS_SWIMMER_M:
    case TRAINER_CLASS_SWIMMER_F:
    case TRAINER_CLASS_SWIMMER_M_FRLG:
    case TRAINER_CLASS_SWIMMER_F_FRLG:
    case TRAINER_CLASS_GUITARIST:
    case TRAINER_CLASS_KINDLER:
    case TRAINER_CLASS_NINJA_BOY:
    case TRAINER_CLASS_POKEFAN:
    case TRAINER_CLASS_PKMN_BREEDER:
    case TRAINER_CLASS_PKMN_BREEDER_FRLG:
    case TRAINER_CLASS_BIRD_KEEPER:
    case TRAINER_CLASS_BIRD_KEEPER_FRLG:
    case TRAINER_CLASS_POKEMANIAC:
    case TRAINER_CLASS_POKEMANIAC_FRLG:
    case TRAINER_CLASS_SR_AND_JR:
    case TRAINER_CLASS_COLLECTOR:
    case TRAINER_CLASS_TWINS:
    case TRAINER_CLASS_TWINS_FRLG:
        return 90;

    // ---- Tier 3 (95%) ---- trained specialists ----
    case TRAINER_CLASS_PSYCHIC:
    case TRAINER_CLASS_PSYCHIC_FRLG:
    case TRAINER_CLASS_BLACK_BELT:
    case TRAINER_CLASS_BLACK_BELT_FRLG:
    case TRAINER_CLASS_EXPERT:
    case TRAINER_CLASS_BEAUTY:
    case TRAINER_CLASS_BEAUTY_FRLG:
    case TRAINER_CLASS_DRAGON_TAMER:
    case TRAINER_CLASS_COOLTRAINER:
    case TRAINER_CLASS_COOLTRAINER_2:
    case TRAINER_CLASS_COOLTRAINER_FRLG:
    case TRAINER_CLASS_BATTLE_GIRL:
    case TRAINER_CLASS_TRIATHLETE:
    case TRAINER_CLASS_SAILOR:
    case TRAINER_CLASS_SAILOR_FRLG:
    case TRAINER_CLASS_GENTLEMAN:
    case TRAINER_CLASS_GENTLEMAN_FRLG:
    case TRAINER_CLASS_RICH_BOY:
    case TRAINER_CLASS_HEX_MANIAC:
    case TRAINER_CLASS_PARASOL_LADY:
    case TRAINER_CLASS_AROMA_LADY:
    case TRAINER_CLASS_RUIN_MANIAC:
    case TRAINER_CLASS_PKMN_RANGER:
    case TRAINER_CLASS_PKMN_RANGER_FRLG:
        return 95;

    // ---- Tier 4 (100%) ---- gym leaders, rivals, evil teams, etc. ----
    default:
        return 100;
    }
}

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

u8 ScaleTrainerMonLevel(u8 baseLevel, u8 trainerAvg, u8 targetLevel, bool8 isAce, u16 trainerClass)
{
    // Look up how much of the player's target level this class should reach.
    // 100 = full player level; 85 = 85% of it, etc.  Unknown classes return 100.
    u8  pct         = GetTrainerClassScalePct(trainerClass);
    // Round to nearest whole level (+50 before dividing by 100).
    u32 scaledTarget = ((u32)targetLevel * pct + 50) / 100;

    // Spread: how far this mon sits above/below the trainer's own mean.
    // Adding the spread to the scaledTarget preserves the trainer's intended
    // intra-party level distribution (e.g. ace stays relatively stronger).
    s16 spread   = (s16)baseLevel - (s16)trainerAvg;
    s16 newLevel = (s16)scaledTarget + spread;

    if (isAce)
        newLevel += DYNAMIC_LEVEL_ACE_BONUS;

    if (newLevel < 1)
        newLevel = 1;
    if (newLevel > 100)
        newLevel = 100;

    return (u8)newLevel;
}
