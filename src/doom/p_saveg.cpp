//
// Copyright(C) 1993-1996 Id Software, Inc.
// Copyright(C) 2005-2014 Simon Howard
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
// DESCRIPTION:
//	Archiving: SaveGame I/O.
//


#include <stdio.h>
#include <stdlib.h>

#include "dstrings.hpp"
#include "deh_main.hpp"
#include "i_system.hpp"
#include "z_zone.hpp"
#include "p_local.hpp"
#include "p_saveg.hpp"

// State.
#include "../../utils/memory.hpp"
#include "doomstat.hpp"
#include "g_game.hpp"
#include "m_misc.hpp"
#include "r_state.hpp"

FILE *save_stream;
int savegamelength;
boolean savegame_error;
static int restoretargets_fail;

// Get the filename of a temporary file to write the savegame to.  After
// the file has been successfully saved, it will be renamed to the 
// real file.

char *P_TempSaveGameFile(void)
{
    static char *filename = NULL;

    if (filename == NULL)
    {
        filename = M_StringJoin(savegamedir, "temp.dsg", NULL);
    }

    return filename;
}

// Get the filename of the save game file to use for the specified slot.

char *P_SaveGameFile(int slot)
{
    static char *filename = NULL;
    static size_t filename_size = 0;
    char basename[32];

    if (filename == NULL)
    {
        filename_size = strlen(savegamedir) + 32;
        filename = static_cast<char *>(malloc(filename_size));
    }

    DEH_snprintf(basename, 32, SAVEGAMENAME "%d.dsg", slot);
    M_snprintf(filename, filename_size, "%s%s", savegamedir, basename);

    return filename;
}

// Endian-safe integer read/write functions

static byte saveg_read8(void)
{
    byte result = -1;

    if (fread(&result, 1, 1, save_stream) < 1)
    {
        if (!savegame_error)
        {
            fprintf(stderr, "saveg_read8: Unexpected end of file while "
                            "reading save game\n");

            savegame_error = true;
        }
    }

    return result;
}

static void saveg_write8(byte value)
{
    if (fwrite(&value, 1, 1, save_stream) < 1)
    {
        if (!savegame_error)
        {
            fprintf(stderr, "saveg_write8: Error while writing save game\n");

            savegame_error = true;
        }
    }
}

static short saveg_read16(void)
{
    int result;

    result = saveg_read8();
    result |= saveg_read8() << 8;

    return result;
}

static void saveg_write16(short value)
{
    saveg_write8(value & 0xff);
    saveg_write8((value >> 8) & 0xff);
}

static int saveg_read32(void)
{
    int result;

    result = saveg_read8();
    result |= saveg_read8() << 8;
    result |= saveg_read8() << 16;
    result |= saveg_read8() << 24;

    return result;
}

static void saveg_write32(int value)
{
    saveg_write8(value & 0xff);
    saveg_write8((value >> 8) & 0xff);
    saveg_write8((value >> 16) & 0xff);
    saveg_write8((value >> 24) & 0xff);
}

// Pad to 4-byte boundaries

static void saveg_read_pad(void)
{
    unsigned long pos;
    int padding;
    int i;

    pos = ftell(save_stream);

    padding = (4 - (pos & 3)) & 3;

    for (i=0; i<padding; ++i)
    {
        saveg_read8();
    }
}

static void saveg_write_pad(void)
{
    unsigned long pos;
    int padding;
    int i;

    pos = ftell(save_stream);

    padding = (4 - (pos & 3)) & 3;

    for (i=0; i<padding; ++i)
    {
        saveg_write8(0);
    }
}


// Pointers

static void *saveg_readp(void)
{
    return (void *) (intptr_t) saveg_read32();
}

static void saveg_writep(const void *p)
{
    saveg_write32((intptr_t) p);
}

// Enum values are 32-bit integers.

#define saveg_read_enum saveg_read32
#define saveg_write_enum saveg_write32

//
// Structure read/write functions
//

//
// mapthing_t
//

static void saveg_read_mapthing_t(mapthing_t *str)
{
    // short x;
    str->x = saveg_read16();

    // short y;
    str->y = saveg_read16();

    // short angle;
    str->angle = saveg_read16();

    // short type;
    str->type = saveg_read16();

    // short options;
    str->options = saveg_read16();
}

static void saveg_write_mapthing_t(mapthing_t *str)
{
    // short x;
    saveg_write16(str->x);

    // short y;
    saveg_write16(str->y);

    // short angle;
    saveg_write16(str->angle);

    // short type;
    saveg_write16(str->type);

    // short options;
    saveg_write16(str->options);
}

//
// actionf_t
// 

static void saveg_read_actionf_t(actionf_t *str)
{
    // actionf_p1 acp1;
    *str = saveg_readp();
}

static void saveg_write_actionf_t(actionf_t *str)
{
    // actionf_p1 acp1;
  saveg_writep(static_cast<const void *>(*str));
}

//
// think_t
//
// This is just an actionf_t.
//

#define saveg_read_think_t saveg_read_actionf_t
#define saveg_write_think_t saveg_write_actionf_t

//
// thinker_t
//

static void saveg_read_thinker_t(thinker_t *str)
{
    // struct thinker_s* prev;
    str->prev = static_cast<thinker_t *>(saveg_readp());

    // struct thinker_s* next;
    str->next = static_cast<thinker_t *>(saveg_readp());

    // think_t function;
    saveg_read_think_t(&str->function);
}

static void saveg_write_thinker_t(thinker_t *str)
{
    // struct thinker_s* prev;
    saveg_writep(str->prev);

    // struct thinker_s* next;
    saveg_writep(str->next);

    // think_t function;
    saveg_write_think_t(&str->function);
}

//
// mobj_t
//

static void saveg_read_mobj_t(mobj_t *str)
{
    int pl;

    // thinker_t thinker;
    saveg_read_thinker_t(str);

    // fixed_t x;
    str->x = saveg_read32();

    // fixed_t y;
    str->y = saveg_read32();

    // fixed_t z;
    str->z = saveg_read32();

    // struct mobj_s* snext;
    str->snext = static_cast<mobj_t *>(saveg_readp());

    // struct mobj_s* sprev;
    str->sprev = static_cast<mobj_t *>(saveg_readp());

    // angle_t angle;
    str->angle = saveg_read32();

    // spritenum_t sprite;
    str->sprite = static_cast<spritenum_t>(saveg_read_enum());

    // int frame;
    str->frame = saveg_read32();

    // struct mobj_s* bnext;
    str->bnext = static_cast<mobj_t *>(saveg_readp());

    // struct mobj_s* bprev;
    str->bprev = static_cast<mobj_t *>(saveg_readp());

    // struct subsector_s* subsector;
    str->subsector = static_cast<subsector_s *>(saveg_readp());

    // fixed_t floorz;
    str->floorz = saveg_read32();

    // fixed_t ceilingz;
    str->ceilingz = saveg_read32();

    // fixed_t radius;
    str->radius = saveg_read32();

    // fixed_t height;
    str->height = saveg_read32();

    // fixed_t momx;
    str->momx = saveg_read32();

    // fixed_t momy;
    str->momy = saveg_read32();

    // fixed_t momz;
    str->momz = saveg_read32();

    // int validcount;
    str->validcount = saveg_read32();

    // mobjtype_t type;
    str->type = static_cast<mobjtype_t>(saveg_read_enum());

    // mobjinfo_t* info;
    str->info = static_cast<mobjinfo_t *>(saveg_readp());

    // int tics;
    str->tics = saveg_read32();

    // state_t* state;
    str->state = &states[saveg_read32()];

    // int flags;
    str->flags = saveg_read32();

    // int health;
    str->health = saveg_read32();

    // int movedir;
    str->movedir = saveg_read32();

    // int movecount;
    str->movecount = saveg_read32();

    // struct mobj_s* target;
    str->target = static_cast<mobj_t *>(saveg_readp());

    // int reactiontime;
    str->reactiontime = saveg_read32();

    // int threshold;
    str->threshold = saveg_read32();

    // struct player_s* player;
    pl = saveg_read32();

    if (pl > 0)
    {
        str->player = &players[pl - 1];
        str->player->mo = str;
        str->player->so = Crispy_PlayerSO(pl - 1); // [crispy] weapon sound sources
    }
    else
    {
        str->player = NULL;
    }

    // int lastlook;
    str->lastlook = saveg_read32();

    // mapthing_t spawnpoint;
    saveg_read_mapthing_t(&str->spawnpoint);

    // struct mobj_s* tracer;
    str->tracer = static_cast<mobj_t *>(saveg_readp());
}

// [crispy] enumerate all thinker pointers
uint32_t P_ThinkerToIndex (thinker_t* thinker)
{
    thinker_t*	th;
    uint32_t	i;

    if (!thinker)
	return 0;

    for (th = thinkercap.next, i = 0; th != &thinkercap; th = th->next)
    {
	if (th->function == P_MobjThinker)
	{
	    i++;
	    if (th == thinker)
		return i;
	}
    }

    return 0;
}

// [crispy] replace indizes with corresponding pointers
thinker_t* P_IndexToThinker (uint32_t index)
{
    thinker_t*	th;
    uint32_t	i;

    if (!index)
	return NULL;

    for (th = thinkercap.next, i = 0; th != &thinkercap; th = th->next)
    {
	if (th->function == P_MobjThinker)
	{
	    i++;
	    if (i == index)
		return th;
	}
    }

    restoretargets_fail++;

    return NULL;
}

static void saveg_write_mobj_t(mobj_t *str)
{
    // thinker_t thinker;
    saveg_write_thinker_t(str);

    // fixed_t x;
    saveg_write32(str->x);

    // fixed_t y;
    saveg_write32(str->y);

    // fixed_t z;
    saveg_write32(str->z);

    // struct mobj_s* snext;
    saveg_writep(str->snext);

    // struct mobj_s* sprev;
    saveg_writep(str->sprev);

    // angle_t angle;
    saveg_write32(str->angle);

    // spritenum_t sprite;
    saveg_write_enum(str->sprite);

    // int frame;
    saveg_write32(str->frame);

    // struct mobj_s* bnext;
    saveg_writep(str->bnext);

    // struct mobj_s* bprev;
    saveg_writep(str->bprev);

    // struct subsector_s* subsector;
    saveg_writep(str->subsector);

    // fixed_t floorz;
    saveg_write32(str->floorz);

    // fixed_t ceilingz;
    saveg_write32(str->ceilingz);

    // fixed_t radius;
    saveg_write32(str->radius);

    // fixed_t height;
    saveg_write32(str->height);

    // fixed_t momx;
    saveg_write32(str->momx);

    // fixed_t momy;
    saveg_write32(str->momy);

    // fixed_t momz;
    saveg_write32(str->momz);

    // int validcount;
    saveg_write32(str->validcount);

    // mobjtype_t type;
    saveg_write_enum(str->type);

    // mobjinfo_t* info;
    saveg_writep(str->info);

    // int tics;
    saveg_write32(str->tics);

    // state_t* state;
    saveg_write32(str->state - states);

    // int flags;
    saveg_write32(str->flags);

    // int health;
    saveg_write32(str->health);

    // int movedir;
    saveg_write32(str->movedir);

    // int movecount;
    saveg_write32(str->movecount);

    // struct mobj_s* target;
    // [crispy] instead of the actual pointer, store the
    // corresponding index in the mobj->target field
    saveg_writep((void *)(uintptr_t) P_ThinkerToIndex((thinker_t *) str->target));

    // int reactiontime;
    saveg_write32(str->reactiontime);

    // int threshold;
    saveg_write32(str->threshold);

    // struct player_s* player;
    if (str->player)
    {
        saveg_write32(str->player - players + 1);
    }
    else
    {
        saveg_write32(0);
    }

    // int lastlook;
    saveg_write32(str->lastlook);

    // mapthing_t spawnpoint;
    saveg_write_mapthing_t(&str->spawnpoint);

    // struct mobj_s* tracer;
    // [crispy] instead of the actual pointer, store the
    // corresponding index in the mobj->tracers field
    saveg_writep((void *)(uintptr_t) P_ThinkerToIndex((thinker_t *) str->tracer));
}


//
// ticcmd_t
//

static void saveg_read_ticcmd_t(ticcmd_t *str)
{

    // signed char forwardmove;
    str->forwardmove = saveg_read8();

    // signed char sidemove;
    str->sidemove = saveg_read8();

    // short angleturn;
    str->angleturn = saveg_read16();

    // short consistancy;
    str->consistancy = saveg_read16();

    // byte chatchar;
    str->chatchar = saveg_read8();

    // byte buttons;
    str->buttons = saveg_read8();
}

static void saveg_write_ticcmd_t(ticcmd_t *str)
{

    // signed char forwardmove;
    saveg_write8(str->forwardmove);

    // signed char sidemove;
    saveg_write8(str->sidemove);

    // short angleturn;
    saveg_write16(str->angleturn);

    // short consistancy;
    saveg_write16(str->consistancy);

    // byte chatchar;
    saveg_write8(str->chatchar);

    // byte buttons;
    saveg_write8(str->buttons);
}

//
// pspdef_t
//

static void saveg_read_pspdef_t(pspdef_t *str)
{
    int state;

    // state_t* state;
    state = saveg_read32();

    if (state > 0)
    {
        str->state = &states[state];
    }
    else
    {
        str->state = NULL;
    }

    // int tics;
    str->tics = saveg_read32();

    // fixed_t sx;
    str->sx = saveg_read32();

    // fixed_t sy;
    str->sy = saveg_read32();

    // [crispy] variable weapon sprite bob
    str->dy = 0;
    str->sx2 = str->sx;
    str->sy2 = str->sy;
}

static void saveg_write_pspdef_t(pspdef_t *str)
{
    // state_t* state;
    if (str->state)
    {
        saveg_write32(str->state - states);
    }
    else
    {
        saveg_write32(0);
    }

    // int tics;
    saveg_write32(str->tics);

    // fixed_t sx;
    saveg_write32(str->sx);

    // fixed_t sy;
    saveg_write32(str->sy);
}

//
// player_t
//

static void saveg_read_player_t(player_t *str)
{
    int i;

    // mobj_t* mo;
    str->mo = static_cast<mobj_t *>(saveg_readp());

    // playerstate_t playerstate;
    str->playerstate = static_cast<playerstate_t>(saveg_read_enum());

    // ticcmd_t cmd;
    saveg_read_ticcmd_t(&str->cmd);

    // fixed_t viewz;
    str->viewz = saveg_read32();

    // fixed_t viewheight;
    str->viewheight = saveg_read32();

    // fixed_t deltaviewheight;
    str->deltaviewheight = saveg_read32();

    // fixed_t bob;
    str->bob = saveg_read32();
    // [crispy] variable player view bob
    str->bob2 = str->bob;

    // int health;
    str->health = saveg_read32();
    // [crispy] negative player health
    str->neghealth = str->health;

    // int armorpoints;
    str->armorpoints = saveg_read32();

    // int armortype;
    str->armortype = saveg_read32();

    // int powers[NUMPOWERS];
    for (i=0; i<NUMPOWERS; ++i)
    {
        str->powers[i] = saveg_read32();
    }

    // boolean cards[NUMCARDS];
    for (i=0; i<NUMCARDS; ++i)
    {
        str->cards[i] = saveg_read32();
    }

    // boolean backpack;
    str->backpack = saveg_read32();

    // int frags[MAXPLAYERS];
    for (i=0; i<MAXPLAYERS; ++i)
    {
        str->frags[i] = saveg_read32();
    }

    // weapontype_t readyweapon;
    str->readyweapon = static_cast<weapontype_t>(saveg_read_enum());

    // weapontype_t pendingweapon;
    str->pendingweapon = static_cast<weapontype_t>(saveg_read_enum());

    // boolean weaponowned[NUMWEAPONS];
    for (i=0; i<NUMWEAPONS; ++i)
    {
        str->weaponowned[i] = saveg_read32();
    }

    // int ammo[NUMAMMO];
    for (i=0; i<NUMAMMO; ++i)
    {
        str->ammo[i] = saveg_read32();
    }

    // int maxammo[NUMAMMO];
    for (i=0; i<NUMAMMO; ++i)
    {
        str->maxammo[i] = saveg_read32();
    }

    // int attackdown;
    str->attackdown = saveg_read32();

    // int usedown;
    str->usedown = saveg_read32();

    // int cheats;
    str->cheats = saveg_read32();

    // int refire;
    str->refire = saveg_read32();

    // int killcount;
    str->killcount = saveg_read32();

    // int itemcount;
    str->itemcount = saveg_read32();

    // int secretcount;
    str->secretcount = saveg_read32();

    // char* message;
    str->message = static_cast<const char *>(saveg_readp());

    // int damagecount;
    str->damagecount = saveg_read32();

    // int bonuscount;
    str->bonuscount = saveg_read32();

    // mobj_t* attacker;
    str->attacker = static_cast<mobj_t *>(saveg_readp());

    // int extralight;
    str->extralight = saveg_read32();

    // int fixedcolormap;
    str->fixedcolormap = saveg_read32();

    // int colormap;
    str->colormap = saveg_read32();

    // pspdef_t psprites[NUMPSPRITES];
    for (i=0; i<NUMPSPRITES; ++i)
    {
        saveg_read_pspdef_t(&str->psprites[i]);
    }

    // boolean didsecret;
    str->didsecret = saveg_read32();
}

static void saveg_write_player_t(player_t *str)
{
    int i;

    // mobj_t* mo;
    saveg_writep(str->mo);

    // playerstate_t playerstate;
    saveg_write_enum(str->playerstate);

    // ticcmd_t cmd;
    saveg_write_ticcmd_t(&str->cmd);

    // fixed_t viewz;
    saveg_write32(str->viewz);

    // fixed_t viewheight;
    saveg_write32(str->viewheight);

    // fixed_t deltaviewheight;
    saveg_write32(str->deltaviewheight);

    // fixed_t bob;
    saveg_write32(str->bob);

    // int health;
    saveg_write32(str->health);

    // int armorpoints;
    saveg_write32(str->armorpoints);

    // int armortype;
    saveg_write32(str->armortype);

    // int powers[NUMPOWERS];
    for (i=0; i<NUMPOWERS; ++i)
    {
        saveg_write32(str->powers[i]);
    }

    // boolean cards[NUMCARDS];
    for (i=0; i<NUMCARDS; ++i)
    {
        saveg_write32(str->cards[i]);
    }

    // boolean backpack;
    saveg_write32(str->backpack);

    // int frags[MAXPLAYERS];
    for (i=0; i<MAXPLAYERS; ++i)
    {
        saveg_write32(str->frags[i]);
    }

    // weapontype_t readyweapon;
    saveg_write_enum(str->readyweapon);

    // weapontype_t pendingweapon;
    saveg_write_enum(str->pendingweapon);

    // boolean weaponowned[NUMWEAPONS];
    for (i=0; i<NUMWEAPONS; ++i)
    {
        saveg_write32(str->weaponowned[i]);
    }

    // int ammo[NUMAMMO];
    for (i=0; i<NUMAMMO; ++i)
    {
        saveg_write32(str->ammo[i]);
    }

    // int maxammo[NUMAMMO];
    for (i=0; i<NUMAMMO; ++i)
    {
        saveg_write32(str->maxammo[i]);
    }

    // int attackdown;
    saveg_write32(str->attackdown);

    // int usedown;
    saveg_write32(str->usedown);

    // int cheats;
    saveg_write32(str->cheats);

    // int refire;
    saveg_write32(str->refire);

    // int killcount;
    saveg_write32(str->killcount);

    // int itemcount;
    saveg_write32(str->itemcount);

    // int secretcount;
    saveg_write32(str->secretcount);

    // char* message;
    saveg_writep(str->message);

    // int damagecount;
    saveg_write32(str->damagecount);

    // int bonuscount;
    saveg_write32(str->bonuscount);

    // mobj_t* attacker;
    saveg_writep(str->attacker);

    // int extralight;
    saveg_write32(str->extralight);

    // int fixedcolormap;
    saveg_write32(str->fixedcolormap);

    // int colormap;
    saveg_write32(str->colormap);

    // pspdef_t psprites[NUMPSPRITES];
    for (i=0; i<NUMPSPRITES; ++i)
    {
        saveg_write_pspdef_t(&str->psprites[i]);
    }

    // boolean didsecret;
    saveg_write32(str->didsecret);
}


//
// ceiling_t
//

static void saveg_read_ceiling_t(ceiling_t *str)
{
    int sector;

    // thinker_t thinker;
    saveg_read_thinker_t(str);

    // ceiling_e type;
    str->type = static_cast<ceiling_e>(saveg_read_enum());

    // sector_t* sector;
    sector = saveg_read32();
    str->sector = &sectors[sector];

    // fixed_t bottomheight;
    str->bottomheight = saveg_read32();

    // fixed_t topheight;
    str->topheight = saveg_read32();

    // fixed_t speed;
    str->speed = saveg_read32();

    // boolean crush;
    str->crush = saveg_read32();

    // int direction;
    str->direction = saveg_read32();

    // int tag;
    str->tag = saveg_read32();

    // int olddirection;
    str->olddirection = saveg_read32();
}

static void saveg_write_ceiling_t(ceiling_t *str)
{
    // thinker_t thinker;
    saveg_write_thinker_t(str);

    // ceiling_e type;
    saveg_write_enum(str->type);

    // sector_t* sector;
    saveg_write32(str->sector - sectors);

    // fixed_t bottomheight;
    saveg_write32(str->bottomheight);

    // fixed_t topheight;
    saveg_write32(str->topheight);

    // fixed_t speed;
    saveg_write32(str->speed);

    // boolean crush;
    saveg_write32(str->crush);

    // int direction;
    saveg_write32(str->direction);

    // int tag;
    saveg_write32(str->tag);

    // int olddirection;
    saveg_write32(str->olddirection);
}

//
// vldoor_t
//

static void saveg_read_vldoor_t(vldoor_t *str)
{
    int sector;

    // thinker_t thinker;
    saveg_read_thinker_t(str);

    // vldoor_e type;
    str->type = static_cast<vldoor_e>(saveg_read_enum());

    // sector_t* sector;
    sector = saveg_read32();
    str->sector = &sectors[sector];

    // fixed_t topheight;
    str->topheight = saveg_read32();

    // fixed_t speed;
    str->speed = saveg_read32();

    // int direction;
    str->direction = saveg_read32();

    // int topwait;
    str->topwait = saveg_read32();

    // int topcountdown;
    str->topcountdown = saveg_read32();
}

static void saveg_write_vldoor_t(vldoor_t *str)
{
    // thinker_t thinker;
    saveg_write_thinker_t(str);

    // vldoor_e type;
    saveg_write_enum(str->type);

    // sector_t* sector;
    saveg_write32(str->sector - sectors);

    // fixed_t topheight;
    saveg_write32(str->topheight);

    // fixed_t speed;
    saveg_write32(str->speed);

    // int direction;
    saveg_write32(str->direction);

    // int topwait;
    saveg_write32(str->topwait);

    // int topcountdown;
    saveg_write32(str->topcountdown);
}

//
// floormove_t
//

static void saveg_read_floormove_t(floormove_t *str)
{
    int sector;

    // thinker_t thinker;
    saveg_read_thinker_t(str);

    // floor_e type;
    str->type = static_cast<floor_e>(saveg_read_enum());

    // boolean crush;
    str->crush = saveg_read32();

    // sector_t* sector;
    sector = saveg_read32();
    str->sector = &sectors[sector];

    // int direction;
    str->direction = saveg_read32();

    // int newspecial;
    str->newspecial = saveg_read32();

    // short texture;
    str->texture = saveg_read16();

    // fixed_t floordestheight;
    str->floordestheight = saveg_read32();

    // fixed_t speed;
    str->speed = saveg_read32();
}

static void saveg_write_floormove_t(floormove_t *str)
{
    // thinker_t thinker;
    saveg_write_thinker_t(str);

    // floor_e type;
    saveg_write_enum(str->type);

    // boolean crush;
    saveg_write32(str->crush);

    // sector_t* sector;
    saveg_write32(str->sector - sectors);

    // int direction;
    saveg_write32(str->direction);

    // int newspecial;
    saveg_write32(str->newspecial);

    // short texture;
    saveg_write16(str->texture);

    // fixed_t floordestheight;
    saveg_write32(str->floordestheight);

    // fixed_t speed;
    saveg_write32(str->speed);
}

//
// plat_t
//

static void saveg_read_plat_t(plat_t *str)
{
    int sector;

    // thinker_t thinker;
    saveg_read_thinker_t(str);

    // sector_t* sector;
    sector = saveg_read32();
    str->sector = &sectors[sector];

    // fixed_t speed;
    str->speed = saveg_read32();

    // fixed_t low;
    str->low = saveg_read32();

    // fixed_t high;
    str->high = saveg_read32();

    // int wait;
    str->wait = saveg_read32();

    // int count;
    str->count = saveg_read32();

    // plat_e status;
    str->status = static_cast<plat_e>(saveg_read_enum());

    // plat_e oldstatus;
    str->oldstatus = static_cast<plat_e>(saveg_read_enum());

    // boolean crush;
    str->crush = saveg_read32();

    // int tag;
    str->tag = saveg_read32();

    // plattype_e type;
    str->type = static_cast<plattype_e>(saveg_read_enum());
}

static void saveg_write_plat_t(plat_t *str)
{
    // thinker_t thinker;
    saveg_write_thinker_t(str);

    // sector_t* sector;
    saveg_write32(str->sector - sectors);

    // fixed_t speed;
    saveg_write32(str->speed);

    // fixed_t low;
    saveg_write32(str->low);

    // fixed_t high;
    saveg_write32(str->high);

    // int wait;
    saveg_write32(str->wait);

    // int count;
    saveg_write32(str->count);

    // plat_e status;
    saveg_write_enum(str->status);

    // plat_e oldstatus;
    saveg_write_enum(str->oldstatus);

    // boolean crush;
    saveg_write32(str->crush);

    // int tag;
    saveg_write32(str->tag);

    // plattype_e type;
    saveg_write_enum(str->type);
}

//
// lightflash_t
//

static void saveg_read_lightflash_t(lightflash_t *str)
{
    int sector;

    // thinker_t thinker;
    saveg_read_thinker_t(str);

    // sector_t* sector;
    sector = saveg_read32();
    str->sector = &sectors[sector];

    // int count;
    str->count = saveg_read32();

    // int maxlight;
    str->maxlight = saveg_read32();

    // int minlight;
    str->minlight = saveg_read32();

    // int maxtime;
    str->maxtime = saveg_read32();

    // int mintime;
    str->mintime = saveg_read32();
}

static void saveg_write_lightflash_t(lightflash_t *str)
{
    // thinker_t thinker;
    saveg_write_thinker_t(str);

    // sector_t* sector;
    saveg_write32(str->sector - sectors);

    // int count;
    saveg_write32(str->count);

    // int maxlight;
    saveg_write32(str->maxlight);

    // int minlight;
    saveg_write32(str->minlight);

    // int maxtime;
    saveg_write32(str->maxtime);

    // int mintime;
    saveg_write32(str->mintime);
}

//
// strobe_t
//

static void saveg_read_strobe_t(strobe_t *str)
{
    int sector;

    // thinker_t thinker;
    saveg_read_thinker_t(str);

    // sector_t* sector;
    sector = saveg_read32();
    str->sector = &sectors[sector];

    // int count;
    str->count = saveg_read32();

    // int minlight;
    str->minlight = saveg_read32();

    // int maxlight;
    str->maxlight = saveg_read32();

    // int darktime;
    str->darktime = saveg_read32();

    // int brighttime;
    str->brighttime = saveg_read32();
}

static void saveg_write_strobe_t(strobe_t *str)
{
    // thinker_t thinker;
    saveg_write_thinker_t(str);

    // sector_t* sector;
    saveg_write32(str->sector - sectors);

    // int count;
    saveg_write32(str->count);

    // int minlight;
    saveg_write32(str->minlight);

    // int maxlight;
    saveg_write32(str->maxlight);

    // int darktime;
    saveg_write32(str->darktime);

    // int brighttime;
    saveg_write32(str->brighttime);
}

//
// glow_t
//

static void saveg_read_glow_t(glow_t *str)
{
    int sector;

    // thinker_t thinker;
    saveg_read_thinker_t(str);

    // sector_t* sector;
    sector = saveg_read32();
    str->sector = &sectors[sector];

    // int minlight;
    str->minlight = saveg_read32();

    // int maxlight;
    str->maxlight = saveg_read32();

    // int direction;
    str->direction = saveg_read32();
}

static void saveg_write_glow_t(glow_t *str)
{
    // thinker_t thinker;
    saveg_write_thinker_t(str);

    // sector_t* sector;
    saveg_write32(str->sector - sectors);

    // int minlight;
    saveg_write32(str->minlight);

    // int maxlight;
    saveg_write32(str->maxlight);

    // int direction;
    saveg_write32(str->direction);
}

//
// Write the header for a savegame
//

void P_WriteSaveGameHeader(char *description)
{
    char name[VERSIONSIZE]; 
    int i; 
	
    for (i=0; description[i] != '\0'; ++i)
        saveg_write8(description[i]);
    for (; i<SAVESTRINGSIZE; ++i)
        saveg_write8(0);

    memset(name, 0, sizeof(name));
    M_snprintf(name, sizeof(name), "version %i", G_VanillaVersionCode());

    for (i=0; i<VERSIONSIZE; ++i)
        saveg_write8(name[i]);
	 
    saveg_write8(gameskill);
    saveg_write8(gameepisode);
    saveg_write8(gamemap);

    for (i=0 ; i<MAXPLAYERS ; i++) 
        saveg_write8(playeringame[i]);

    saveg_write8((leveltime >> 16) & 0xff);
    saveg_write8((leveltime >> 8) & 0xff);
    saveg_write8(leveltime & 0xff);
}

// 
// Read the header for a savegame
//

boolean P_ReadSaveGameHeader(void)
{
    int	 i; 
    byte a, b, c; 
    char vcheck[VERSIONSIZE]; 
    char read_vcheck[VERSIONSIZE];
	 
    // skip the description field 

    for (i=0; i<SAVESTRINGSIZE; ++i)
        saveg_read8();
    
    for (i=0; i<VERSIONSIZE; ++i)
        read_vcheck[i] = saveg_read8();

    memset(vcheck, 0, sizeof(vcheck));
    M_snprintf(vcheck, sizeof(vcheck), "version %i", G_VanillaVersionCode());
    if (strcmp(read_vcheck, vcheck) != 0)
	return false;				// bad version 

    gameskill = static_cast<skill_t>(saveg_read8());
    gameepisode = saveg_read8();
    gamemap = saveg_read8();

    for (i=0 ; i<MAXPLAYERS ; i++) 
	playeringame[i] = saveg_read8();

    // get the times 
    a = saveg_read8();
    b = saveg_read8();
    c = saveg_read8();
    leveltime = (a<<16) + (b<<8) + c; 

    return true;
}

//
// Read the end of file marker.  Returns true if read successfully.
// 

boolean P_ReadSaveGameEOF(void)
{
    int value;

    value = saveg_read8();

    return value == SAVEGAME_EOF;
}

//
// Write the end of file marker
//

void P_WriteSaveGameEOF(void)
{
    saveg_write8(SAVEGAME_EOF);
}

//
// P_ArchivePlayers
//
void P_ArchivePlayers (void)
{
    int		i;
		
    for (i=0 ; i<MAXPLAYERS ; i++)
    {
	if (!playeringame[i])
	    continue;
	
	saveg_write_pad();

        saveg_write_player_t(&players[i]);
    }
}



//
// P_UnArchivePlayers
//
void P_UnArchivePlayers (void)
{
    int		i;
	
    for (i=0 ; i<MAXPLAYERS ; i++)
    {
	if (!playeringame[i])
	    continue;
	
	saveg_read_pad();

        saveg_read_player_t(&players[i]);
	
	// will be set when unarc thinker
	players[i].mo = NULL;	
	players[i].message = NULL;
	players[i].attacker = NULL;
    }
}


//
// P_ArchiveWorld
//
void P_ArchiveWorld (void)
{
    int			i;
    int			j;
    sector_t*		sec;
    line_t*		li;
    side_t*		si;
    
    // do sectors
    for (i=0, sec = sectors ; i<numsectors ; i++,sec++)
    {
	saveg_write16(sec->floorheight >> FRACBITS);
	saveg_write16(sec->ceilingheight >> FRACBITS);
	saveg_write16(sec->floorpic);
	saveg_write16(sec->ceilingpic);
	saveg_write16(sec->lightlevel);
	saveg_write16(sec->special);		// needed?
	saveg_write16(sec->tag);		// needed?
    }

    
    // do lines
    for (i=0, li = lines ; i<numlines ; i++,li++)
    {
	saveg_write16(li->flags);
	saveg_write16(li->special);
	saveg_write16(li->tag);
	for (j=0 ; j<2 ; j++)
	{
	    if (li->sidenum[j] == NO_INDEX) // [crispy] extended nodes
		continue;
	    
	    si = &sides[li->sidenum[j]];

	    saveg_write16(si->textureoffset >> FRACBITS);
	    saveg_write16(si->rowoffset >> FRACBITS);
	    saveg_write16(si->toptexture);
	    saveg_write16(si->bottomtexture);
	    saveg_write16(si->midtexture);	
	}
    }
}



//
// P_UnArchiveWorld
//
void P_UnArchiveWorld (void)
{
    int			i;
    int			j;
    sector_t*		sec;
    line_t*		li;
    side_t*		si;
    
    // do sectors
    for (i=0, sec = sectors ; i<numsectors ; i++,sec++)
    {
	// [crispy] add overflow guard for the flattranslation[] array
	short floorpic, ceilingpic;
	extern int numflats;
	sec->floorheight = saveg_read16() << FRACBITS;
	sec->ceilingheight = saveg_read16() << FRACBITS;
	floorpic = saveg_read16();
	ceilingpic = saveg_read16();
	sec->lightlevel = saveg_read16();
	sec->special = saveg_read16();		// needed?
	sec->tag = saveg_read16();		// needed?
	sec->specialdata = 0;
	sec->soundtarget = 0;
	// [crispy] add overflow guard for the flattranslation[] array
	if (floorpic >= 0 && floorpic < numflats)
	{
	    sec->floorpic = floorpic;
	}
	if (ceilingpic >= 0 && ceilingpic < numflats)
	{
	    sec->ceilingpic = ceilingpic;
	}
    }
    
    // do lines
    for (i=0, li = lines ; i<numlines ; i++,li++)
    {
	li->flags = saveg_read16();
	li->special = saveg_read16();
	li->tag = saveg_read16();
	for (j=0 ; j<2 ; j++)
	{
	    if (li->sidenum[j] == NO_INDEX) // [crispy] extended nodes
		continue;
	    si = &sides[li->sidenum[j]];
	    si->textureoffset = saveg_read16() << FRACBITS;
	    si->rowoffset = saveg_read16() << FRACBITS;
	    si->toptexture = saveg_read16();
	    si->bottomtexture = saveg_read16();
	    si->midtexture = saveg_read16();
	}
    }
}





//
// Thinkers
//
typedef enum
{
    tc_end,
    tc_mobj

} thinkerclass_t;


//
// P_ArchiveThinkers
//
void P_ArchiveThinkers (void)
{
    thinker_t*		th;

    // save off the current thinkers
    for (th = thinkercap.next ; th != &thinkercap ; th=th->next)
    {
	if (th->function == P_MobjThinker)
	{
            saveg_write8(tc_mobj);
	    saveg_write_pad();
            saveg_write_mobj_t((mobj_t *) th);

	    continue;
	}
		
	// I_Error ("P_ArchiveThinkers: Unknown thinker function");
    }

    // add a terminating marker
    saveg_write8(tc_end);
}



//
// P_UnArchiveThinkers
//
void P_UnArchiveThinkers (void)
{
    byte		tclass;
    thinker_t*		currentthinker;
    thinker_t*		next;
    mobj_t*		mobj;
    
    // remove all the current thinkers
    currentthinker = thinkercap.next;
    while (currentthinker != &thinkercap)
    {
	next = currentthinker->next;
	
	if (currentthinker->function == P_MobjThinker)
	    P_RemoveMobj ((mobj_t *)currentthinker);
	else
	    Z_Free (currentthinker);

	currentthinker = next;
    }
    P_InitThinkers ();
    
    // read in saved thinkers
    while (1)
    {
	tclass = saveg_read8();
	switch (tclass)
	{
	  case tc_end:
	    return; 	// end of list
			
	  case tc_mobj:
	    saveg_read_pad();
	    mobj = zmalloc<decltype(mobj)> (sizeof(*mobj), PU_LEVEL, NULL);
            saveg_read_mobj_t(mobj);

	    // [crispy] restore mobj->target and mobj->tracer fields
	    //mobj->target = NULL;
            //mobj->tracer = NULL;
	    P_SetThingPosition (mobj);
	    mobj->info = &mobjinfo[mobj->type];
	    // [crispy] killough 2/28/98: Fix for falling down into a wall after savegame loaded
//	    mobj->floorz = mobj->subsector->sector->floorheight;
//	    mobj->ceilingz = mobj->subsector->sector->ceilingheight;
	    mobj->function = P_MobjThinker;
	    P_AddThinker (mobj);
	    break;

	  default:
	    I_Error ("Unknown tclass %i in savegame",tclass);
	}
	
    }

}

// [crispy] after all the thinkers have been restored, replace all indices in
// the mobj->target and mobj->tracers fields by the corresponding current pointers again
void P_RestoreTargets (void)
{
    mobj_t*	mo;
    thinker_t*	th;

    for (th = thinkercap.next; th != &thinkercap; th = th->next)
    {
	if (th->function == P_MobjThinker)
	{
	    mo = (mobj_t*) th;
	    mo->target = (mobj_t*) P_IndexToThinker((uintptr_t) mo->target);
	    mo->tracer = (mobj_t*) P_IndexToThinker((uintptr_t) mo->tracer);
	}
    }

    if (restoretargets_fail)
    {
	fprintf (stderr, "P_RestoreTargets: Failed to restore %d target pointers.\n", restoretargets_fail);
	restoretargets_fail = 0;
    }
}

//
// P_ArchiveSpecials
//
enum
{
    tc_ceiling,
    tc_door,
    tc_floor,
    tc_plat,
    tc_flash,
    tc_strobe,
    tc_glow,
    tc_endspecials

} specials_e;	



//
// Things to handle:
//
// T_MoveCeiling, (ceiling_t: sector_t * swizzle), - active list
// T_VerticalDoor, (vldoor_t: sector_t * swizzle),
// T_MoveFloor, (floormove_t: sector_t * swizzle),
// T_LightFlash, (lightflash_t: sector_t * swizzle),
// T_StrobeFlash, (strobe_t: sector_t *),
// T_Glow, (glow_t: sector_t *),
// T_PlatRaise, (plat_t: sector_t *), - active list
//
void P_ArchiveSpecials (void)
{
    thinker_t*		th;
    int			i;
	
    // save off the current thinkers
    for (th = thinkercap.next ; th != &thinkercap ; th=th->next)
    {
	if (th->function == actionf_t{})
	{
	    for (i = 0; i < MAXCEILINGS;i++)
		if (activeceilings[i] == (ceiling_t *)th)
		    break;
	    
	    if (i<MAXCEILINGS)
	    {
                saveg_write8(tc_ceiling);
		saveg_write_pad();
                saveg_write_ceiling_t((ceiling_t *) th);
	    }
	    // [crispy] save plats in statis
	    for (i = 0; i < MAXPLATS; i++)
		if (activeplats[i] == (plat_t *)th)
		    break;

	    if (i < MAXPLATS)
	    {
		saveg_write8(tc_plat);
		saveg_write_pad();
		saveg_write_plat_t((plat_t *)th);
	    }
	    continue;
	}
			
	if (th->function == T_MoveCeiling)
	{
            saveg_write8(tc_ceiling);
	    saveg_write_pad();
            saveg_write_ceiling_t((ceiling_t *) th);
	    continue;
	}
			
	if (th->function == T_VerticalDoor)
	{
            saveg_write8(tc_door);
	    saveg_write_pad();
            saveg_write_vldoor_t((vldoor_t *) th);
	    continue;
	}
			
	if (th->function == T_MoveFloor)
	{
            saveg_write8(tc_floor);
	    saveg_write_pad();
            saveg_write_floormove_t((floormove_t *) th);
	    continue;
	}
			
	if (th->function == T_PlatRaise)
	{
            saveg_write8(tc_plat);
	    saveg_write_pad();
            saveg_write_plat_t((plat_t *) th);
	    continue;
	}
			
	if (th->function == T_LightFlash)
	{
            saveg_write8(tc_flash);
	    saveg_write_pad();
            saveg_write_lightflash_t((lightflash_t *) th);
	    continue;
	}
			
	if (th->function == T_StrobeFlash)
	{
            saveg_write8(tc_strobe);
	    saveg_write_pad();
            saveg_write_strobe_t((strobe_t *) th);
	    continue;
	}
			
	if (th->function == T_Glow)
	{
            saveg_write8(tc_glow);
	    saveg_write_pad();
            saveg_write_glow_t((glow_t *) th);
	    continue;
	}
    }
	
    // add a terminating marker
    saveg_write8(tc_endspecials);

}


//
// P_UnArchiveSpecials
//
void P_UnArchiveSpecials (void)
{
    byte		tclass;
    ceiling_t*		ceiling;
    vldoor_t*		door;
    floormove_t*	floor;
    plat_t*		plat;
    lightflash_t*	flash;
    strobe_t*		strobe;
    glow_t*		glow;
	
	
    // read in saved thinkers
    while (1)
    {
	tclass = saveg_read8();

	switch (tclass)
	{
	  case tc_endspecials:
	    return;	// end of list
			
	  case tc_ceiling:
	    saveg_read_pad();
	    ceiling = zmalloc<decltype(ceiling)> (sizeof(*ceiling), PU_LEVEL, NULL);
            saveg_read_ceiling_t(ceiling);
	    ceiling->sector->specialdata = ceiling;

	    if (ceiling->function)
		ceiling->function = T_MoveCeiling;

	    P_AddThinker (ceiling);
	    P_AddActiveCeiling(ceiling);
	    break;
				
	  case tc_door:
	    saveg_read_pad();
	    door = zmalloc<decltype(door)> (sizeof(*door), PU_LEVEL, NULL);
            saveg_read_vldoor_t(door);
	    door->sector->specialdata = door;
	    door->function = T_VerticalDoor;
	    P_AddThinker (door);
	    break;
				
	  case tc_floor:
	    saveg_read_pad();
	    floor = zmalloc<decltype(floor)> (sizeof(*floor), PU_LEVEL, NULL);
            saveg_read_floormove_t(floor);
	    floor->sector->specialdata = floor;
	    floor->function = T_MoveFloor;
	    P_AddThinker (floor);
	    break;
				
	  case tc_plat:
	    saveg_read_pad();
	    plat = zmalloc<decltype(plat)> (sizeof(*plat), PU_LEVEL, NULL);
            saveg_read_plat_t(plat);
	    plat->sector->specialdata = plat;

	    if (plat->function)
		plat->function = T_PlatRaise;

	    P_AddThinker (plat);
	    P_AddActivePlat(plat);
	    break;
				
	  case tc_flash:
	    saveg_read_pad();
	    flash = zmalloc<decltype(flash)> (sizeof(*flash), PU_LEVEL, NULL);
            saveg_read_lightflash_t(flash);
	    flash->function = T_LightFlash;
	    P_AddThinker (flash);
	    break;
				
	  case tc_strobe:
	    saveg_read_pad();
	    strobe = zmalloc<decltype(strobe)> (sizeof(*strobe), PU_LEVEL, NULL);
            saveg_read_strobe_t(strobe);
	    strobe->function = T_StrobeFlash;
	    P_AddThinker (strobe);
	    break;
				
	  case tc_glow:
	    saveg_read_pad();
	    glow = zmalloc<decltype(glow)> (sizeof(*glow), PU_LEVEL, NULL);
            saveg_read_glow_t(glow);
	    glow->function = T_Glow;
	    P_AddThinker (glow);
	    break;
				
	  default:
	    I_Error ("P_UnarchiveSpecials:Unknown tclass %i "
		     "in savegame",tclass);
	}
	
    }

}

