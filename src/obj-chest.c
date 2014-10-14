/**
   \file obj-chest.c
   \brief Encapsulation of chest-related functions
 *
 * Copyright (c) 1997 Ben Harrison, James E. Wilson, Robert A. Koeneke
 * Copyright (c) 2012 Peter Denison
 *
 * This work is free software; you can redistribute it and/or modify it
 * under the terms of either:
 *
 * a) the GNU General Public License as published by the Free Software
 *    Foundation, version 2, or
 *
 * b) the "Angband licence":
 *    This software may be copied and distributed for educational, research,
 *    and not for profit purposes provided that this copyright and statement
 *    are included in all such copies.  Other copyrights may also apply.
 */

#include "angband.h"
#include "cave.h"
#include "mon-lore.h"
#include "obj-chest.h"
#include "obj-identify.h"
#include "obj-make.h"
#include "obj-tval.h"
#include "obj-util.h"
#include "player-timed.h"
#include "player-util.h"
#include "tables.h"

/**
 * Each chest has a certain set of traps, determined by pval
 * Each chest has a "pval" from 1 to the chest level (max 55)
 * If the "pval" is negative then the trap has been disarmed
 * The "pval" of a chest determines the quality of its treasure
 * Note that disarming a trap on a chest also removes the lock.
 */
static const byte chest_traps[64] =
{
	0,					/* 0 == empty */
	(CHEST_POISON),
	(CHEST_LOSE_STR),
	(CHEST_LOSE_CON),
	(CHEST_LOSE_STR),
	(CHEST_LOSE_CON),			/* 5 == best small wooden */
	0,
	(CHEST_POISON),
	(CHEST_POISON),
	(CHEST_LOSE_STR),
	(CHEST_LOSE_CON),
	(CHEST_POISON),
	(CHEST_LOSE_STR | CHEST_LOSE_CON),
	(CHEST_LOSE_STR | CHEST_LOSE_CON),
	(CHEST_LOSE_STR | CHEST_LOSE_CON),
	(CHEST_SUMMON),			/* 15 == best large wooden */
	0,
	(CHEST_LOSE_STR),
	(CHEST_LOSE_CON),
	(CHEST_PARALYZE),
	(CHEST_LOSE_STR | CHEST_LOSE_CON),
	(CHEST_SUMMON),
	(CHEST_PARALYZE),
	(CHEST_LOSE_STR),
	(CHEST_LOSE_CON),
	(CHEST_EXPLODE),			/* 25 == best small iron */
	0,
	(CHEST_POISON | CHEST_LOSE_STR),
	(CHEST_POISON | CHEST_LOSE_CON),
	(CHEST_LOSE_STR | CHEST_LOSE_CON),
	(CHEST_EXPLODE | CHEST_SUMMON),
	(CHEST_PARALYZE),
	(CHEST_POISON | CHEST_SUMMON),
	(CHEST_SUMMON),
	(CHEST_EXPLODE),
	(CHEST_EXPLODE | CHEST_SUMMON),	/* 35 == best large iron */
	0,
	(CHEST_SUMMON),
	(CHEST_EXPLODE),
	(CHEST_EXPLODE | CHEST_SUMMON),
	(CHEST_EXPLODE | CHEST_SUMMON),
	(CHEST_POISON | CHEST_PARALYZE),
	(CHEST_EXPLODE),
	(CHEST_EXPLODE | CHEST_SUMMON),
	(CHEST_EXPLODE | CHEST_SUMMON),
	(CHEST_POISON | CHEST_PARALYZE),	/* 45 == best small steel */
	0,
	(CHEST_LOSE_STR | CHEST_LOSE_CON),
	(CHEST_LOSE_STR | CHEST_LOSE_CON),
	(CHEST_POISON | CHEST_PARALYZE | CHEST_LOSE_STR),
	(CHEST_POISON | CHEST_PARALYZE | CHEST_LOSE_CON),
	(CHEST_POISON | CHEST_LOSE_STR | CHEST_LOSE_CON),
	(CHEST_POISON | CHEST_LOSE_STR | CHEST_LOSE_CON),
	(CHEST_POISON | CHEST_PARALYZE | CHEST_LOSE_STR | CHEST_LOSE_CON),
	(CHEST_POISON | CHEST_PARALYZE),
	(CHEST_POISON | CHEST_PARALYZE),	/* 55 == best large steel */
	(CHEST_EXPLODE | CHEST_SUMMON),
	(CHEST_EXPLODE | CHEST_SUMMON),
	(CHEST_EXPLODE | CHEST_SUMMON),
	(CHEST_EXPLODE | CHEST_SUMMON),
	(CHEST_EXPLODE | CHEST_SUMMON),
	(CHEST_EXPLODE | CHEST_SUMMON),
	(CHEST_EXPLODE | CHEST_SUMMON),
	(CHEST_EXPLODE | CHEST_SUMMON),
};

/**
 * The type of trap a chest has
 */
byte chest_trap_type(const object_type *o_ptr)
{
	s16b trap_value = o_ptr->pval;

	if (trap_value >= 0)
		return chest_traps[trap_value];
	else
		return chest_traps[-trap_value];
}

/**
 * Determine if a chest is trapped
 */
bool is_trapped_chest(const object_type *o_ptr)
{
	if (!tval_is_chest(o_ptr))
		return FALSE;

	/* Disarmed or opened chests are not trapped */
	if (o_ptr->pval <= 0)
		return FALSE;

	/* Some chests simply don't have traps */
	return (chest_traps[o_ptr->pval] != 0);
}


/**
 * Determine if a chest is locked or trapped
 */
bool is_locked_chest(const object_type *o_ptr)
{
	if (!tval_is_chest(o_ptr))
		return FALSE;

	/* Disarmed or opened chests are not locked */
	return (o_ptr->pval > 0);
}

/**
 * Unlock a chest
 */
void unlock_chest(object_type *o_ptr)
{
	o_ptr->pval = (0 - o_ptr->pval);
}

/**
 * Determine if a grid contains a chest matching the query type
 */
s16b chest_check(int y, int x, enum chest_query check_type)
{
	s16b this_o_idx, next_o_idx = 0;


	/* Scan all objects in the grid */
	for (this_o_idx = cave->o_idx[y][x]; this_o_idx; this_o_idx = next_o_idx)
	{
		object_type *o_ptr;

		/* Get the object */
		o_ptr = cave_object(cave, this_o_idx);

		/* Get the next object */
		next_o_idx = o_ptr->next_o_idx;

		/* Skip unknown chests XXX XXX */
		/* if (!o_ptr->marked) continue; */

		/* Check for chests */
		switch (check_type)
		{
		case CHEST_ANY:
			if (tval_is_chest(o_ptr))
				return this_o_idx;
			break;
		case CHEST_OPENABLE:
			if (tval_is_chest(o_ptr) && (o_ptr->pval != 0))
				return this_o_idx;
			break;
		case CHEST_TRAPPED:
			if (is_trapped_chest(o_ptr) && object_is_known(o_ptr))
				return this_o_idx;
			break;
		}
	}

	/* No chest */
	return (0);
}


/**
 * Return the number of chests around (or under) the character.
 * If requested, count only trapped chests.
 */
int count_chests(int *y, int *x, enum chest_query check_type)
{
	int d, count;

	/* Count how many matches */
	count = 0;

	/* Check around (and under) the character */
	for (d = 0; d < 9; d++)
	{
		/* Extract adjacent (legal) location */
		int yy = player->py + ddy_ddd[d];
		int xx = player->px + ddx_ddd[d];

		/* No (visible) chest is there */
		if (chest_check(yy, xx, check_type) == 0) continue;

		/* Count it */
		++count;

		/* Remember the location of the last chest found */
		*y = yy;
		*x = xx;
	}

	/* All done */
	return count;
}


/**
 * Allocate objects upon opening a chest
 *
 * Disperse treasures from the given chest, centered at (x,y).
 *
 * Small chests often contain "gold", while Large chests always contain
 * items.  Wooden chests contain 2 items, Iron chests contain 4 items,
 * and Steel chests contain 6 items.  The "value" of the items in a
 * chest is based on the level on which the chest is generated.
 *
 * Judgment of size and construction of chests is currently made from the name.
 */
static void chest_death(int y, int x, s16b o_idx)
{
	int number, value;

	bool tiny;

	object_type *o_ptr;

	object_type *i_ptr;
	object_type object_type_body;


	/* Get the chest */
	o_ptr = cave_object(cave, o_idx);

	/* Small chests often hold "gold" */
	tiny = strstr(o_ptr->kind->name, "Small") ? TRUE : FALSE;

	/* Determine how much to drop (see above) */
	if (strstr(o_ptr->kind->name, "wooden"))
		number = 2;
	else if (strstr(o_ptr->kind->name, "iron"))
		number = 4;
	else if (strstr(o_ptr->kind->name, "steel"))
		number = 6;
	else
		number = 2 * (randint1(3));

	/* Zero pval means empty chest */
	if (!o_ptr->pval) number = 0;

	/* Determine the "value" of the items */
	value = o_ptr->origin_depth - 10 + 2 * o_ptr->sval;
	if (value < 1)
		value = 1;

	/* Drop some objects (non-chests) */
	for (; number > 0; --number)
	{
		/* Get local object */
		i_ptr = &object_type_body;

		/* Wipe the object */
		object_wipe(i_ptr);

		/* Small chests often drop gold */
		if (tiny && (randint0(100) < 75))
			make_gold(i_ptr, value, "any");

		/* Otherwise drop an item, as long as it isn't a chest */
		else {
			if (!make_object(cave, i_ptr, value, FALSE, FALSE, FALSE, NULL, 0))
				continue;
			if (tval_is_chest(i_ptr)) continue;
		}

		/* Record origin */
		i_ptr->origin = ORIGIN_CHEST;
		i_ptr->origin_depth = o_ptr->origin_depth;

		/* Drop it in the dungeon */
		drop_near(cave, i_ptr, 0, y, x, TRUE);
	}

	/* Empty */
	o_ptr->pval = 0;

	/* Known */
	object_notice_everything(o_ptr);
}


/**
 * Chests have traps too.
 *
 * Exploding chest destroys contents (and traps).
 * Note that the chest itself is never destroyed.
 */
static void chest_trap(int y, int x, s16b o_idx)
{
	int trap;

	object_type *o_ptr = cave_object(cave, o_idx);


	/* Ignore disarmed chests */
	if (o_ptr->pval <= 0) return;

	/* Obtain the traps */
	trap = chest_traps[o_ptr->pval];

	/* Lose strength */
	if (trap & (CHEST_LOSE_STR))
	{
		msg("A small needle has pricked you!");
		take_hit(player, damroll(1, 4), "a poison needle");
		effect_simple(EF_DRAIN_STAT, "0", STAT_STR, 0, 0, NULL);
	}

	/* Lose constitution */
	if (trap & (CHEST_LOSE_CON))
	{
		msg("A small needle has pricked you!");
		take_hit(player, damroll(1, 4), "a poison needle");
		effect_simple(EF_DRAIN_STAT, "0", STAT_CON, 0, 0, NULL);
	}

	/* Poison */
	if (trap & (CHEST_POISON))
	{
		msg("A puff of green gas surrounds you!");
		effect_simple(EF_TIMED_INC, "10+1d20", TMD_POISONED, 0, 0, NULL);
	}

	/* Paralyze */
	if (trap & (CHEST_PARALYZE))
	{
		msg("A puff of yellow gas surrounds you!");
		effect_simple(EF_TIMED_INC, "10+1d20", TMD_PARALYZED, 0, 0, NULL);
	}

	/* Summon monsters */
	if (trap & (CHEST_SUMMON))
	{
		msg("You are enveloped in a cloud of smoke!");
		effect_simple(EF_SUMMON, "2+1d3", 0, 0, 0, NULL);
	}

	/* Explode */
	if (trap & (CHEST_EXPLODE))
	{
		msg("There is a sudden explosion!");
		msg("Everything inside the chest is destroyed!");
		o_ptr->pval = 0;
		take_hit(player, damroll(5, 8), "an exploding chest");
	}
}


/**
 * Attempt to open the given chest at the given location
 *
 * Assume there is no monster blocking the destination
 *
 * Returns TRUE if repeated commands may continue
 */
bool do_cmd_open_chest(int y, int x, s16b o_idx)
{
	int i, j;

	bool flag = TRUE;

	bool more = FALSE;

	object_type *o_ptr = cave_object(cave, o_idx);


	/* Attempt to unlock it */
	if (o_ptr->pval > 0)
	{
		/* Assume locked, and thus not open */
		flag = FALSE;

		/* Get the "disarm" factor */
		i = player->state.skills[SKILL_DISARM];

		/* Penalize some conditions */
		if (player->timed[TMD_BLIND] || no_light()) i = i / 10;
		if (player->timed[TMD_CONFUSED] || player->timed[TMD_IMAGE]) i = i / 10;

		/* Extract the difficulty */
		j = i - o_ptr->pval;

		/* Always have a small chance of success */
		if (j < 2) j = 2;

		/* Success -- May still have traps */
		if (randint0(100) < j)
		{
			msgt(MSG_LOCKPICK, "You have picked the lock.");
			player_exp_gain(player, 1);
			flag = TRUE;
		}

		/* Failure -- Keep trying */
		else
		{
			/* We may continue repeating */
			more = TRUE;
			flush();
			msgt(MSG_LOCKPICK_FAIL, "You failed to pick the lock.");
		}
	}

	/* Allowed to open */
	if (flag)
	{
		/* Apply chest traps, if any */
		chest_trap(y, x, o_idx);

		/* Let the Chest drop items */
		chest_death(y, x, o_idx);

		/* Ignore chest if autoignore calls for it */
		player->upkeep->notice |= PN_IGNORE;

	}

	/*
	 * empty chests were always ignored in ignore_item_okay so we
	 * might as well ignore it here
	 */
	if (o_ptr->pval == 0) {
		o_ptr->ignore = TRUE;
	}

	/* Redraw chest, to be on the safe side (it may have been ignored) */
	square_light_spot(cave, y, x);

	/* Refresh */
	Term_fresh();

	/* Result */
	return (more);
}


/**
 * Attempt to disarm the chest at the given location
 *
 * Assume there is no monster blocking the destination
 *
 * Returns TRUE if repeated commands may continue
 */
bool do_cmd_disarm_chest(int y, int x, s16b o_idx)
{
	int i, j;

	bool more = FALSE;

	object_type *o_ptr = cave_object(cave, o_idx);


	/* Get the "disarm" factor */
	i = player->state.skills[SKILL_DISARM];

	/* Penalize some conditions */
	if (player->timed[TMD_BLIND] || no_light()) i = i / 10;
	if (player->timed[TMD_CONFUSED] || player->timed[TMD_IMAGE]) i = i / 10;

	/* Extract the difficulty */
	j = i - o_ptr->pval;

	/* Always have a small chance of success */
	if (j < 2) j = 2;

	/* Must find the trap first. */
	if (!object_is_known(o_ptr))
	{
		msg("I don't see any traps.");
	}

	/* Already disarmed/unlocked or no traps */
	else if (!is_trapped_chest(o_ptr))
	{
		msg("The chest is not trapped.");
	}

	/* Success (get a lot of experience) */
	else if (randint0(100) < j)
	{
		msgt(MSG_DISARM, "You have disarmed the chest.");
		player_exp_gain(player, o_ptr->pval);
		o_ptr->pval = (0 - o_ptr->pval);
	}

	/* Failure -- Keep trying */
	else if ((i > 5) && (randint1(i) > 5))
	{
		/* We may keep trying */
		more = TRUE;
		flush();
		msg("You failed to disarm the chest.");
	}

	/* Failure -- Set off the trap */
	else
	{
		msg("You set off a trap!");
		chest_trap(y, x, o_idx);
	}

	/* Result */
	return (more);
}
