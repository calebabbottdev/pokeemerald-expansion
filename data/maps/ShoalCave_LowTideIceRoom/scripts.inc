ShoalCave_LowTideIceRoom_MapScripts::
	.byte 0

ShoalCave_Inside_EventScript_Articuno::
	lock
	faceplayer
	waitse
	playmoncry SPECIES_ARTICUNO, CRY_MODE_ENCOUNTER
	delay 40
	waitmoncry
	seteventmon SPECIES_ARTICUNO, 30
	setflag FLAG_SYS_CTRL_OBJ_DELETE
	special BattleSetup_StartLegendaryBattle
	waitstate
	clearflag FLAG_SYS_CTRL_OBJ_DELETE
	specialvar VAR_RESULT, GetBattleOutcome
	goto_if_eq VAR_RESULT, B_OUTCOME_WON, ShoalCave_Inside_EventScript_DefeatedArticuno
	goto_if_eq VAR_RESULT, B_OUTCOME_RAN, ShoalCave_Inside_EventScript_DefeatedArticuno
	goto_if_eq VAR_RESULT, B_OUTCOME_PLAYER_TELEPORTED, ShoalCave_Inside_EventScript_RanFromArticuno
	setflag FLAG_DEFEATED_ARTICUNO_SHOAL_CAVE
	release
	end

ShoalCave_Inside_EventScript_DefeatedArticuno::
	setflag FLAG_DEFEATED_ARTICUNO_SHOAL_CAVE
	goto Common_EventScript_RemoveStaticPokemon
	end

ShoalCave_Inside_EventScript_RanFromArticuno::
	setvar VAR_0x8004, SPECIES_ARTICUNO
	goto Common_EventScript_LegendaryFlewAway
	end

