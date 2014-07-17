#ifndef DUNGEON_H
#define DUNGEON_H

#include "angband.h"

#define TOWN_DAWN		10000	/* Number of turns from dawn to dawn */
#define TOWN_DUSK         5000    /* Number of turns from dawn to dusk */

extern u16b daycount;
extern u32b seed_randart;
extern u32b seed_flavor;
extern s32b turn;
extern bool character_generated;
extern bool character_dungeon;
extern bool character_saved;
extern s16b character_xtra;

extern bool is_daytime(void);
extern void dungeon_change_level(int dlev);
extern void play_game(void);
extern int value_check_aux1(const object_type *o_ptr);
extern void idle_update(void);

#endif /* !DUNGEON_H */
