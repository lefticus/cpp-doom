//
// Copyright(C) 2005-2014 Simon Howard
// Copyright(C) 2014 Fabian Greffrath
//
// This program is free software; you can redistribute it and/or
// modify it under the terms of the GNU General Public License
// as published by the Free Software Foundation; either version 2
// of the License, or (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
//
// Parses [CODEPTR] sections in BEX files
//

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <array>

#include "info.hpp"

#include "deh_io.hpp"
#include "deh_main.hpp"
#include "event_function_decls.hpp"

struct bex_codeptr_t {
    const char *mnemonic;
    const actionf_t pointer;
};

static const bex_codeptr_t bex_codeptrtable[] = {
    {"Light0", {A_Light0}},
    {"WeaponReady", {A_WeaponReady}},
    {"Lower", {A_Lower}},
    {"Raise", {A_Raise}},
    {"Punch", {A_Punch}},
    {"ReFire", {A_ReFire}},
    {"FirePistol", {A_FirePistol}},
    {"Light1", {A_Light1}},
    {"FireShotgun", {A_FireShotgun}},
    {"Light2", {A_Light2}},
    {"FireShotgun2", {A_FireShotgun2}},
    {"CheckReload", {A_CheckReload}},
    {"OpenShotgun2", {A_OpenShotgun2}},
    {"LoadShotgun2", {A_LoadShotgun2}},
    {"CloseShotgun2", {A_CloseShotgun2}},
    {"FireCGun", {A_FireCGun}},
    {"GunFlash", {A_GunFlash}},
    {"FireMissile", {A_FireMissile}},
    {"Saw", {A_Saw}},
    {"FirePlasma", {A_FirePlasma}},
    {"BFGsound", {A_BFGsound}},
    {"FireBFG", {A_FireBFG}},
    {"BFGSpray", {A_BFGSpray}},
    {"Explode", {A_Explode}},
    {"Pain", {A_Pain}},
    {"PlayerScream", {A_PlayerScream}},
    {"Fall", {A_Fall}},
    {"XScream", {A_XScream}},
    {"Look", {A_Look}},
    {"Chase", {A_Chase}},
    {"FaceTarget", {A_FaceTarget}},
    {"PosAttack", {A_PosAttack}},
    {"Scream", {A_Scream}},
    {"SPosAttack", {A_SPosAttack}},
    {"VileChase", {A_VileChase}},
    {"VileStart", {A_VileStart}},
    {"VileTarget", {A_VileTarget}},
    {"VileAttack", {A_VileAttack}},
    {"StartFire", {A_StartFire}},
    {"Fire", {A_Fire}},
    {"FireCrackle", {A_FireCrackle}},
    {"Tracer", {A_Tracer}},
    {"SkelWhoosh", {A_SkelWhoosh}},
    {"SkelFist", {A_SkelFist}},
    {"SkelMissile", {A_SkelMissile}},
    {"FatRaise", {A_FatRaise}},
    {"FatAttack1", {A_FatAttack1}},
    {"FatAttack2", {A_FatAttack2}},
    {"FatAttack3", {A_FatAttack3}},
    {"BossDeath", {A_BossDeath}},
    {"CPosAttack", {A_CPosAttack}},
    {"CPosRefire", {A_CPosRefire}},
    {"TroopAttack", {A_TroopAttack}},
    {"SargAttack", {A_SargAttack}},
    {"HeadAttack", {A_HeadAttack}},
    {"BruisAttack", {A_BruisAttack}},
    {"SkullAttack", {A_SkullAttack}},
    {"Metal", {A_Metal}},
    {"SpidRefire", {A_SpidRefire}},
    {"BabyMetal", {A_BabyMetal}},
    {"BspiAttack", {A_BspiAttack}},
    {"Hoof", {A_Hoof}},
    {"CyberAttack", {A_CyberAttack}},
    {"PainAttack", {A_PainAttack}},
    {"PainDie", {A_PainDie}},
    {"KeenDie", {A_KeenDie}},
    {"BrainPain", {A_BrainPain}},
    {"BrainScream", {A_BrainScream}},
    {"BrainDie", {A_BrainDie}},
    {"BrainAwake", {A_BrainAwake}},
    {"BrainSpit", {A_BrainSpit}},
    {"SpawnSound", {A_SpawnSound}},
    {"SpawnFly", {A_SpawnFly}},
    {"BrainExplode", {A_BrainExplode}},
    // [crispy] additional BOOM and MBF states, sprites and code pointers
    {"Stop", {A_Stop}},
    {"Die", {A_Die}},
    {"FireOldBFG", {A_FireOldBFG}},
    {"Detonate", {A_Detonate}},
    {"Mushroom", {A_Mushroom}},
    {"BetaSkullAttack", {A_BetaSkullAttack}},
    // [crispy] more MBF code pointers
    {"Spawn", {A_Spawn}},
    {"Turn", {A_Turn}},
    {"Face", {A_Face}},
    {"Scratch", {A_Scratch}},
    {"PlaySound", {A_PlaySound}},
    {"RandomJump", {A_RandomJump}},
    {"LineEffect", {A_LineEffect}},
    {"NULL", {}},
};

extern actionf_t codeptrs[NUMSTATES];

static void *DEH_BEXPtrStart(deh_context_t *context, char *line)
{
    char s[10];

    if (sscanf(line, "%9s", s) == 0 || strcmp("[CODEPTR]", s))
    {
	DEH_Warning(context, "Parse error on section start");
    }

    return NULL;
}

static void DEH_BEXPtrParseLine(deh_context_t *context, char *line, void *tag)
{
    state_t *state;
    char *variable_name, *value, frame_str[6];
    int frame_number, i;

    // parse "FRAME nn = mnemonic", where
    // variable_name = "FRAME nn" and value = "mnemonic"
    if (!DEH_ParseAssignment(line, &variable_name, &value))
    {
	DEH_Warning(context, "Failed to parse assignment: %s", line);
	return;
    }

    // parse "FRAME nn", where frame_number = "nn"
    if (sscanf(variable_name, "%5s %32d", frame_str, &frame_number) != 2 ||
        strcasecmp(frame_str, "FRAME"))
    {
	DEH_Warning(context, "Failed to parse assignment: %s", variable_name);
	return;
    }

    if (frame_number < 0 || frame_number >= NUMSTATES)
    {
	DEH_Warning(context, "Invalid frame number: %i", frame_number);
	return;
    }

    state = (state_t *) &states[frame_number];

    for (i = 0; i < arrlen(bex_codeptrtable); i++)
    {
	if (!strcasecmp(bex_codeptrtable[i].mnemonic, value))
	{
	    state->action = bex_codeptrtable[i].pointer;
	    return;
	}
    }

    DEH_Warning(context, "Invalid mnemonic '%s'", value);
}

deh_section_t deh_section_bexptr =
{
    "[CODEPTR]",
    NULL,
    DEH_BEXPtrStart,
    DEH_BEXPtrParseLine,
    NULL,
    NULL,
};
