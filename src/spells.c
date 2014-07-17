/*
 * File: spells.c
 * Purpose: Various assorted spell effects
 *
 * Copyright (c) 1997 Ben Harrison, James E. Wilson, Robert A. Koeneke
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
#include "dungeon.h"
#include "generate.h"
#include "history.h"
#include "init.h"
#include "mon-lore.h"
#include "mon-make.h"
#include "mon-timed.h"
#include "mon-util.h"
#include "monster.h"
#include "obj-chest.h"
#include "obj-desc.h"
#include "obj-gear.h"
#include "obj-identify.h"
#include "obj-ignore.h"
#include "obj-make.h"
#include "obj-slays.h"
#include "obj-tval.h"
#include "obj-ui.h"
#include "obj-util.h"
#include "object.h"
#include "player-timed.h"
#include "player-util.h"
#include "project.h"
#include "spells.h"
#include "tables.h"
#include "target.h"
#include "trap.h"

/*
 * Increase players hit points, notice effects
 */
bool hp_player(int num)
{
	/* Healing needed */
	if (player->chp < player->mhp)
	{
		/* Gain hitpoints */
		player->chp += num;

		/* Enforce maximum */
		if (player->chp >= player->mhp)
		{
			player->chp = player->mhp;
			player->chp_frac = 0;
		}

		/* Redraw */
		player->upkeep->redraw |= (PR_HP);

		/* Print a nice message */
		if (num < 5)
			msg("You feel a little better.");
		else if (num < 15)
			msg("You feel better.");
		else if (num < 35)
			msg("You feel much better.");
		else
			msg("You feel very good.");

		/* Notice */
		return (TRUE);
	}

	/* Ignore */
	return (FALSE);
}


/*
 * Heal the player by a given percentage of his wounds, or a minimum
 * amount, whichever is larger.
 *
 * Copied wholesale from EyAngband.
 */
bool heal_player(int perc, int min)
{
	int i;

	/* Paranoia */
	if ((perc <= 0) && (min <= 0)) return (FALSE);


	/* No healing needed */
	if (player->chp >= player->mhp) return (FALSE);

	/* Figure healing level */
	i = ((player->mhp - player->chp) * perc) / 100;

	/* Enforce minimums */
	if (i < min) i = min;

	/* Actual healing */
	return hp_player(i);
}





/*
 * Leave a "glyph of warding" which prevents monster movement
 */
bool warding_glyph(void)
{
	int py = player->py;
	int px = player->px;

	if (!square_canward(cave, py, px))
	{
		msg("There is no clear floor on which to cast the spell.");
		return FALSE;
	}

	/* Create a glyph */
	square_add_ward(cave, py, px);
	return TRUE;
}

/**
 * Create a "glyph of warding" via a spell.
 *
 * We need to do this because the book-keeping is slightly different for
 * spells vs. scrolls.
 */
void warding_glyph_spell(void)
{
	int py = player->py;
	int px = player->px;

	/* See if the effect works */
	if (!warding_glyph()) return;

	/* Push objects off the grid */
	if (cave->o_idx[py][px]) push_object(py, px);
}
	



/*
 * Array of stat "descriptions"
 */
static const char *desc_stat_pos[] =
{
	"strong",
	"smart",
	"wise",
	"dextrous",
	"healthy",
	"cute"
};


/*
 * Array of stat "descriptions"
 */
static const char *desc_stat_neg[] =
{
	"weak",
	"stupid",
	"naive",
	"clumsy",
	"sickly",
	"ugly"
};


/*
 * Restore a stat.  Return TRUE only if this actually makes a difference.
 */
bool res_stat(int stat)
{
	/* Restore if needed */
	if (player->stat_cur[stat] != player->stat_max[stat])
	{
		/* Restore */
		player->stat_cur[stat] = player->stat_max[stat];

		/* Recalculate bonuses */
		player->upkeep->update |= (PU_BONUS);

		/* Success */
		return (TRUE);
	}

	/* Nothing to restore */
	return (FALSE);
}

/*
 * Lose a "point"
 */
bool do_dec_stat(int stat, bool perma)
{
	bool sust = FALSE;

	/* Get the "sustain" */
	switch (stat)
	{
		case STAT_STR:
			if (player_of_has(player, OF_SUST_STR)) sust = TRUE;
			wieldeds_notice_flag(player, OF_SUST_STR);
			break;
		case STAT_INT:
			if (player_of_has(player, OF_SUST_INT)) sust = TRUE;
			wieldeds_notice_flag(player, OF_SUST_INT);
			break;
		case STAT_WIS:
			if (player_of_has(player, OF_SUST_WIS)) sust = TRUE;
			wieldeds_notice_flag(player, OF_SUST_WIS);
			break;
		case STAT_DEX:
			if (player_of_has(player, OF_SUST_DEX)) sust = TRUE;
			wieldeds_notice_flag(player, OF_SUST_DEX);
			break;
		case STAT_CON:
			if (player_of_has(player, OF_SUST_CON)) sust = TRUE;
			wieldeds_notice_flag(player, OF_SUST_CON);
			break;
	}

	/* Sustain */
	if (sust && !perma)
	{
		/* Message */
		msg("You feel very %s for a moment, but the feeling passes.",
		           desc_stat_neg[stat]);


		/* Notice effect */
		return (TRUE);
	}

	/* Attempt to reduce the stat */
	if (player_stat_dec(player, stat, perma))
	{
		/* Message */
		msgt(MSG_DRAIN_STAT, "You feel very %s.", desc_stat_neg[stat]);

		/* Notice effect */
		return (TRUE);
	}

	/* Nothing obvious */
	return (FALSE);
}


/*
 * Restore lost "points" in a stat
 */
bool do_res_stat(int stat)
{
	/* Attempt to increase */
	if (res_stat(stat))
	{
		/* Message */
		msg("You feel less %s.", desc_stat_neg[stat]);

		/* Notice */
		return (TRUE);
	}

	/* Nothing obvious */
	return (FALSE);
}


/*
 * Gain a "point" in a stat
 */
bool do_inc_stat(int stat)
{
	bool res;

	/* Restore strength */
	res = res_stat(stat);

	/* Attempt to increase */
	if (player_stat_inc(player, stat))
	{
		/* Message */
		msg("You feel very %s!", desc_stat_pos[stat]);

		/* Notice */
		return (TRUE);
	}

	/* Restoration worked */
	if (res)
	{
		/* Message */
		msg("You feel less %s.", desc_stat_neg[stat]);

		/* Notice */
		return (TRUE);
	}

	/* Nothing obvious */
	return (FALSE);
}



/*
 * Identify everything being carried.
 * Done by a potion of "self knowledge".
 */
void identify_pack(void)
{
	int i;

	/* Simply identify and know every item */
	for (i = 0; i < player->max_gear; i++)
	{
		object_type *o_ptr = &player->gear[i];

		/* Skip non-objects */
		if (!o_ptr->kind) continue;

		/* Aware and Known */
		if (object_is_known(o_ptr)) continue;

		/* Identify it */
		do_ident_item(o_ptr);
	}
}






/*
 * Hack -- Removes curse from an object.
 */
static void uncurse_object(object_type *o_ptr)
{
	bitflag f[OF_SIZE];

	create_mask(f, FALSE, OFT_CURSE, OFT_MAX);

	of_diff(o_ptr->flags, f);
}


/*
 * Removes curses from items in inventory.
 *
 * \param heavy removes heavy curses if true
 *
 * \returns number of items uncursed
 */
static int remove_curse_aux(bool heavy)
{
	int i, cnt = 0;

	/* Attempt to uncurse items being worn */
	for (i = 0; i < player->body.count; i++)
	{
		object_type *o_ptr = equipped_item_by_slot(player, i);

		if (!o_ptr->kind) continue;
		if (!cursed_p(o_ptr->flags)) continue;

		/* Heavily cursed items need a special spell */
		if (of_has(o_ptr->flags, OF_HEAVY_CURSE) && !heavy) continue;

		/* Perma-cursed items can never be removed */
		if (of_has(o_ptr->flags, OF_PERMA_CURSE)) continue;

		/* Uncurse, and update things */
		uncurse_object(o_ptr);

		player->upkeep->update |= (PU_BONUS);
		player->upkeep->redraw |= (PR_EQUIP);

		/* Count the uncursings */
		cnt++;
	}

	/* Return "something uncursed" */
	return (cnt);
}


/*
 * Remove most curses
 */
bool remove_curse(void)
{
	return (remove_curse_aux(FALSE));
}

/*
 * Remove all curses
 */
bool remove_all_curse(void)
{
	return (remove_curse_aux(TRUE));
}



/*
 * Restores any drained experience
 */
bool restore_level(void)
{
	/* Restore experience */
	if (player->exp < player->max_exp)
	{
		/* Message */
		msg("You feel your life energies returning.");
		player_exp_gain(player, player->max_exp - player->exp);

		/* Did something */
		return (TRUE);
	}

	/* No effect */
	return (FALSE);
}


/*
 * Set word of recall as appropriate
 */
bool set_recall(void)
{
	/* No recall */
	if (OPT(birth_no_recall) && !player->total_winner)
	{
		msg("Nothing happens.");
		return FALSE;
	}
    
	/* No recall from quest levels with force_descend */
	if (OPT(birth_force_descend) && (is_quest(player->depth))) {
		msg("Nothing happens.");
		return TRUE;
	}

    /* Warn the player if they're descending to an unrecallable level */
	if (OPT(birth_force_descend) && !(player->depth) &&
			(is_quest(player->max_depth + 1))) {
		if (!get_check("Are you sure you want to descend? ")) {
			msg("You prevent the recall from taking place.");
			return FALSE;
		}
	}

	/* Activate recall */
	if (!player->word_recall)
	{
		/* Reset recall depth */
		if ((player->depth > 0) && (player->depth != player->max_depth))
		{
			/* ToDo: Add a new player_type field "recall_depth" */
			if (get_check("Reset recall depth? "))
				player->max_depth = player->depth;
		}

		player->word_recall = randint0(20) + 15;
		msg("The air about you becomes charged...");
	}

	/* Deactivate recall */
	else
	{
		if (!get_check("Word of Recall is already active.  Do you want to cancel it? "))
			return FALSE;

		player->word_recall = 0;
		msg("A tension leaves the air around you...");
	}

	/* Redraw status line */
	player->upkeep->redraw = PR_STATUS;
	handle_stuff(player->upkeep);

	return TRUE;
}


/*** Detection spells ***/

/*
 * Useful constants for the area around the player to detect.
 * This is instead of using circular detection spells.
 */
#define DETECT_DIST_X	40	/* Detect 42 grids to the left & right */
#define DETECT_DIST_Y	22	/* Detect 22 grids to the top & bottom */



/*
 * Map an area around the player.
 *
 * We must never attempt to map the outer dungeon walls, or we
 * might induce illegal cave grid references.
 */
void map_area(void)
{
	int i, x, y;
	int x1, x2, y1, y2;

	/* Pick an area to map */
	y1 = player->py - DETECT_DIST_Y;
	y2 = player->py + DETECT_DIST_Y;
	x1 = player->px - DETECT_DIST_X;
	x2 = player->px + DETECT_DIST_X;

	/* Drag the co-ordinates into the dungeon */
	if (y1 < 0) y1 = 0;
	if (x1 < 0) x1 = 0;
	if (y2 > cave->height - 1) y2 = cave->height - 1;
	if (x2 > cave->width - 1) x2 = cave->width - 1;

	/* Scan the dungeon */
	for (y = y1; y < y2; y++)
	{
		for (x = x1; x < x2; x++)
		{
			/* Some squares can't be mapped */
			if (square_is_no_map(cave, y, x)) continue;

			/* All non-walls are "checked" */
			if (!square_seemslikewall(cave, y, x))
			{
				if (!square_in_bounds_fully(cave, y, x)) continue;

				/* Memorize normal features */
				if (square_isinteresting(cave, y, x))
				{
					/* Memorize the object */
					sqinfo_on(cave->info[y][x], SQUARE_MARK);
					square_light_spot(cave, y, x);
				}

				/* Memorize known walls */
				for (i = 0; i < 8; i++)
				{
					int yy = y + ddy_ddd[i];
					int xx = x + ddx_ddd[i];

					/* Memorize walls (etc) */
					if (square_seemslikewall(cave, yy, xx))
					{
						/* Memorize the walls */
						sqinfo_on(cave->info[yy][xx], SQUARE_MARK);
						square_light_spot(cave, yy, xx);
					}
				}
			}
		}
	}
}



/*
 * Detect traps around the player.
 */
bool detect_traps(bool aware)
{
	int y, x;
	int x1, x2, y1, y2;

	bool detect = FALSE;

	object_type *o_ptr;

	(void)aware;

	/* Pick an area to map */
	y1 = player->py - DETECT_DIST_Y;
	y2 = player->py + DETECT_DIST_Y;
	x1 = player->px - DETECT_DIST_X;
	x2 = player->px + DETECT_DIST_X;

	if (y1 < 0) y1 = 0;
	if (x1 < 0) x1 = 0;


	/* Scan the dungeon */
	for (y = y1; y < y2; y++)
	{
		for (x = x1; x < x2; x++)
		{
			if (!square_in_bounds_fully(cave, y, x)) continue;

			/* Detect traps */
			if (square_player_trap(cave, y, x)) 
			{
				/* Reveal trap */
				if (square_reveal_trap(cave, y, x, 100, FALSE))
				{
					/* We found something to detect */
					detect = TRUE;
				}
			}

			/* Scan all objects in the grid to look for traps on chests */
			for (o_ptr = get_first_object(y, x); o_ptr; o_ptr = get_next_object(o_ptr))
			{
				/* Skip anything not a trapped chest */
				if (!is_trapped_chest(o_ptr)) continue;

				/* Identify once */
				if (!object_is_known(o_ptr))
				{
					/* Know the trap */
					object_notice_everything(o_ptr);

					/* Notice it */
					disturb(player, 0);

					/* We found something to detect */
					detect = TRUE;
				}
			}

			/* Mark as trap-detected */
			sqinfo_on(cave->info[y][x], SQUARE_DTRAP);
		}
	}

	/* Rescan the map for the new dtrap edge */
	for (y = y1 - 1; y < y2 + 1; y++)
	{
		for (x = x1 - 1; x < x2 + 1; x++)
		{
			if (!square_in_bounds_fully(cave, y, x)) continue;

			/* see if this grid is on the edge */
			if (dtrap_edge(y, x)) {
				sqinfo_on(cave->info[y][x], SQUARE_DEDGE);
			} else {
				sqinfo_off(cave->info[y][x], SQUARE_DEDGE);
			}

			/* Redraw */
			square_light_spot(cave, y, x);
		}
	}


	/* Describe */
	if (detect)
		msg("You sense the presence of traps!");

	/* Trap detection always makes you aware, even if no traps are present */
	else
		msg("You sense no traps.");

	/* Mark the redraw flag */
	player->upkeep->redraw |= (PR_DTRAP);

	/* Result */
	return (TRUE);
}



/*
 * Detect doors and stairs around the player.
 */
bool detect_doorstairs(bool aware)
{
	int y, x;
	int x1, x2, y1, y2;

	bool doors = FALSE, stairs = FALSE;


	/* Pick an area to map */
	y1 = player->py - DETECT_DIST_Y;
	y2 = player->py + DETECT_DIST_Y;
	x1 = player->px - DETECT_DIST_X;
	x2 = player->px + DETECT_DIST_X;

	if (y1 < 0) y1 = 0;
	if (x1 < 0) x1 = 0;


	/* Scan the dungeon */
	for (y = y1; y < y2; y++)
	{
		for (x = x1; x < x2; x++)
		{
			if (!square_in_bounds_fully(cave, y, x)) continue;

			/* Detect secret doors */
			if (square_issecretdoor(cave, y, x))
				place_closed_door(cave, y, x);

			/* Detect doors */
			if (square_isdoor(cave, y, x))
			{
				/* Hack -- Memorize */
				sqinfo_on(cave->info[y][x], SQUARE_MARK);

				/* Redraw */
				square_light_spot(cave, y, x);

				/* Obvious */
				doors = TRUE;
			}

			/* Detect stairs */
			if (square_isstairs(cave, y, x))
			{
				/* Hack -- Memorize */
				sqinfo_on(cave->info[y][x], SQUARE_MARK);

				/* Redraw */
				square_light_spot(cave, y, x);

				/* Obvious */
				stairs = TRUE;
			}

		}
	}

	/* Describe */
	if (doors && !stairs)      msg("You sense the presence of doors!");
	else if (!doors && stairs) msg("You sense the presence of stairs!");
	else if (doors && stairs)  msg("You sense the presence of doors and stairs!");
	else if (aware && !doors && !stairs) msg("You sense no doors or stairs.");

	/* Result */
	return (doors || stairs);
}


/*
 * Detect all treasure around the player.
 */
bool detect_treasure(bool aware, bool full)
{
	int i;
	int y, x;
	int x1, x2, y1, y2;

	bool gold_buried = FALSE;
	bool objects = FALSE;


	/* Pick an area to map */
	y1 = player->py - DETECT_DIST_Y;
	y2 = player->py + DETECT_DIST_Y;
	x1 = player->px - DETECT_DIST_X;
	x2 = player->px + DETECT_DIST_X;

	if (y1 < 0) y1 = 0;
	if (x1 < 0) x1 = 0;


	/* Scan the dungeon */
	for (y = y1; y < y2; y++) {
		for (x = x1; x < x2; x++) {
			if (!square_in_bounds_fully(cave, y, x)) continue;

			square_show_vein(cave, y, x);

			/* Magma/Quartz + Known Gold */
			if (square_hasgoldvein(cave, y, x)) {
				/* Hack -- Memorize */
				sqinfo_on(cave->info[y][x], SQUARE_MARK);

				/* Redraw */
				square_light_spot(cave, y, x);

				/* Detect */
				gold_buried = TRUE;
			}
		}
	}

	/* Scan objects */
	for (i = 1; i < cave_object_max(cave); i++)	{
		object_type *o_ptr = cave_object(cave, i);

		/* Skip dead objects */
		if (!o_ptr->kind) continue;

		/* Skip held objects */
		if (o_ptr->held_m_idx) continue;

		/* Location */
		y = o_ptr->iy;
		x = o_ptr->ix;

		/* Only detect nearby objects */
		if (x < x1 || y < y1 || x > x2 || y > y2) continue;

		/* Memorize it */
		if (o_ptr->marked < MARK_SEEN)
			o_ptr->marked = full ? MARK_SEEN : MARK_AWARE;

		/* Redraw */
		square_light_spot(cave, y, x);

		/* Detect */
		if (!ignore_item_ok(o_ptr) || !full)
			objects = TRUE;
	}

	if (gold_buried)
		msg("You sense the presence of buried treasure!");

	if (objects)
		msg("You sense the presence of objects!");

	if (aware && !gold_buried && !objects)
		msg("You sense no treasure or objects.");

	return gold_buried || objects;
}


/*
 * Quietly detect all buried treasure near the player.
 */
bool detect_close_buried_treasure(void)
{
	int y, x;
	int x1, x2, y1, y2;

	bool gold_buried = FALSE;


	/* Pick a small area to map */
	y1 = player->py - 3;
	y2 = player->py + 3;
	x1 = player->px - 3;
	x2 = player->px + 3;

	if (y1 < 0) y1 = 0;
	if (x1 < 0) x1 = 0;


	/* Scan the dungeon */
	for (y = y1; y < y2; y++)
	{
		for (x = x1; x < x2; x++)
		{
			if (!square_in_bounds_fully(cave, y, x)) continue;

			/* Notice embedded gold */
			square_show_vein(cave, y, x);

			/* Magma/Quartz + Known Gold */
			if (square_hasgoldvein(cave, y, x))
			{
				/* Hack -- Memorize */
				sqinfo_on(cave->info[y][x], SQUARE_MARK);

				/* Redraw */
				square_light_spot(cave, y, x);

				/* Detect */
				gold_buried = TRUE;
			}
		}
	}

	return (gold_buried);
}

/*
 * Detect "normal" monsters around the player.
 */
bool detect_monsters_normal(bool aware)
{
	int i, y, x;
	int x1, x2, y1, y2;

	bool flag = FALSE;


	/* Pick an area to map */
	y1 = player->py - DETECT_DIST_Y;
	y2 = player->py + DETECT_DIST_Y;
	x1 = player->px - DETECT_DIST_X;
	x2 = player->px + DETECT_DIST_X;

	if (y1 < 0) y1 = 0;
	if (x1 < 0) x1 = 0;



	/* Scan monsters */
	for (i = 1; i < cave_monster_max(cave); i++)
	{
		monster_type *m_ptr = cave_monster(cave, i);
		
		/* Skip dead monsters */
		if (!m_ptr->race) continue;

		/* Location */
		y = m_ptr->fy;
		x = m_ptr->fx;

		/* Only detect nearby monsters */
		if (x < x1 || y < y1 || x > x2 || y > y2) continue;

		/* Detect all non-invisible, obvious monsters */
		if (!rf_has(m_ptr->race->flags, RF_INVISIBLE) && !m_ptr->unaware)
		{
			/* Hack -- Detect the monster */
			m_ptr->mflag |= (MFLAG_MARK | MFLAG_SHOW);

			/* Update the monster */
			update_mon(m_ptr, FALSE);

			/* Detect */
			flag = TRUE;
		}
	}

	if (flag)
		msg("You sense the presence of monsters!");
	else if (aware && !flag)
		msg("You sense no monsters.");
		
	/* Result */
	return flag;
}


/*
 * Detect "invisible" monsters around the player.
 */
bool detect_monsters_invis(bool aware)
{
	int i, y, x;
	int x1, x2, y1, y2;

	bool flag = FALSE;

	/* Pick an area to map */
	y1 = player->py - DETECT_DIST_Y;
	y2 = player->py + DETECT_DIST_Y;
	x1 = player->px - DETECT_DIST_X;
	x2 = player->px + DETECT_DIST_X;

	if (y1 < 0) y1 = 0;
	if (x1 < 0) x1 = 0;


	/* Scan monsters */
	for (i = 1; i < cave_monster_max(cave); i++)
	{
		monster_type *m_ptr = cave_monster(cave, i);
		monster_lore *l_ptr;

		/* Skip dead monsters */
		if (!m_ptr->race) continue;
		
		l_ptr = get_lore(m_ptr->race);

		/* Location */
		y = m_ptr->fy;
		x = m_ptr->fx;

		/* Only detect nearby objects */
		if (x < x1 || y < y1 || x > x2 || y > y2) continue;

		/* Detect invisible monsters */
		if (rf_has(m_ptr->race->flags, RF_INVISIBLE))
		{
			/* Take note that they are invisible */
			rf_on(l_ptr->flags, RF_INVISIBLE);

			/* Update monster recall window */
			if (player->upkeep->monster_race == m_ptr->race)
			{
				/* Redraw stuff */
				player->upkeep->redraw |= (PR_MONSTER);
			}
			
			/* Detect the monster */
			m_ptr->mflag |= (MFLAG_MARK | MFLAG_SHOW);

			/* Update the monster */
			update_mon(m_ptr, FALSE);

			/* Detect */
			flag = TRUE;
		}
	}

	if (flag)
		msg("You sense the presence of invisible creatures!");
	else if (aware && !flag)
		msg("You sense no invisible creatures.");

	return (flag);
}



/*
 * Detect "evil" monsters around the player.
 */
bool detect_monsters_evil(bool aware)
{
	int i, y, x;
	int x1, x2, y1, y2;

	bool flag = FALSE;

	/* Pick an area to map */
	y1 = player->py - DETECT_DIST_Y;
	y2 = player->py + DETECT_DIST_Y;
	x1 = player->px - DETECT_DIST_X;
	x2 = player->px + DETECT_DIST_X;

	if (y1 < 0) y1 = 0;
	if (x1 < 0) x1 = 0;


	/* Scan monsters */
	for (i = 1; i < cave_monster_max(cave); i++)
	{
		monster_type *m_ptr = cave_monster(cave, i);
		monster_lore *l_ptr;

		/* Skip dead monsters */
		if (!m_ptr->race) continue;

		l_ptr = get_lore(m_ptr->race);
		
		/* Location */
		y = m_ptr->fy;
		x = m_ptr->fx;

		/* Only detect nearby objects */
		if (x < x1 || y < y1 || x > x2 || y > y2) continue;

		/* Detect evil monsters */
		if (rf_has(m_ptr->race->flags, RF_EVIL))
		{
			/* Take note that they are evil */
			rf_on(l_ptr->flags, RF_EVIL);

			/* Update monster recall window */
			if (player->upkeep->monster_race == m_ptr->race)
			{
				/* Redraw stuff */
				player->upkeep->redraw |= (PR_MONSTER);
			}

			/* Detect the monster */
			m_ptr->mflag |= (MFLAG_MARK | MFLAG_SHOW);

			/* Update the monster */
			update_mon(m_ptr, FALSE);

			/* Detect */
			flag = TRUE;
		}
	}

	if (flag)
		msg("You sense the presence of evil creatures!");
	else if (aware && !flag)
		msg("You sense no evil creatures.");

	return flag;
}

/* 
 * Detect all monsters on the level (used for *enlightenment* only)
 */
bool detect_monsters_entire_level(void)
{
	int i;
	bool detect = FALSE;
	
	for (i = 1; i < cave_monster_max(cave); i++)
	{
		monster_type *m_ptr = cave_monster(cave, i);
	
		/* Skip dead monsters */
		if (!m_ptr->race) continue;

		/* Detect the monster */
		m_ptr->mflag |= (MFLAG_MARK | MFLAG_SHOW);

		/* Update the monster */
		update_mon(m_ptr, FALSE);
		
		detect = TRUE;
	}
	
	if (detect)
		msg("An image of all nearby life-forms appears in your mind");
	else
		/* Let's see who the first person is to ever see this message! */
		msg("The level is devoid of life");
		
	return detect;
}
	
/*
 * Detect everything
 */
bool detect_all(bool aware)
{
	bool detect = FALSE;

	/* Detect everything */
	if (detect_traps(aware)) detect = TRUE;
	if (detect_doorstairs(aware)) detect = TRUE;
	if (detect_treasure(aware, FALSE)) detect = TRUE;
	if (detect_monsters_invis(aware)) detect = TRUE;
	if (detect_monsters_normal(aware)) detect = TRUE;

	/* Result */
	return (detect);
}



/*
 * Create stairs at the player location
 */
void stair_creation(void)
{
	int py = player->py;
	int px = player->px;

	/* Only allow stairs to be created on empty floor */
	if (!square_isfloor(cave, py, px))
	{
		msg("There is no empty floor here.");
		return;
	}

	/* Push objects off the grid */
	if (cave->o_idx[py][px]) push_object(py, px);

	square_add_stairs(cave, py, px, player->depth);
}




/*
 * Apply disenchantment to the player's stuff
 *
 * This function is also called from the "melee" code.
 *
 * The "mode" is currently unused.
 *
 * Return "TRUE" if the player notices anything.
 */
bool apply_disenchant(int mode)
{
	int i, count = 0;

	object_type *o_ptr;

	char o_name[80];


	/* Unused parameter */
	(void)mode;

	/* Count slots */
	for (i = 0; i < player->body.count; i++) {
		/* Ignore rings, amulets and lights */
		if (slot_type_is(i, EQUIP_RING)) continue;
		if (slot_type_is(i, EQUIP_AMULET)) continue;
		if (slot_type_is(i, EQUIP_LIGHT)) continue;

		/* Count disenchantable slots */
		count++;
	}

	/* Pick one at random */
	for (i = player->body.count - 1; i >= 0; i--) {
		/* Ignore rings, amulets and lights */
		if (slot_type_is(i, EQUIP_RING)) continue;
		if (slot_type_is(i, EQUIP_AMULET)) continue;
		if (slot_type_is(i, EQUIP_LIGHT)) continue;

		if (one_in_(count--)) break;
	}

	/* Get the item */
	o_ptr = equipped_item_by_slot(player, i);

	/* No item, nothing happens */
	if (!o_ptr->kind) return (FALSE);


	/* Nothing to disenchant */
	if ((o_ptr->to_h <= 0) && (o_ptr->to_d <= 0) && (o_ptr->to_a <= 0))
	{
		/* Nothing to notice */
		return (FALSE);
	}

	/* Describe the object */
	object_desc(o_name, sizeof(o_name), o_ptr, ODESC_BASE);


	/* Artifacts have 60% chance to resist */
	if (o_ptr->artifact && (randint0(100) < 60))
	{
		/* Message */
		msg("Your %s (%c) resist%s disenchantment!",
		           o_name, equip_to_label(i),
		           ((o_ptr->number != 1) ? "" : "s"));

		/* Notice */
		return (TRUE);
	}

	/* Apply disenchantment, depending on which kind of equipment */
	if (slot_type_is(i, EQUIP_WEAPON) || slot_type_is(i, EQUIP_BOW))
	{
		/* Disenchant to-hit */
		if (o_ptr->to_h > 0) o_ptr->to_h--;
		if ((o_ptr->to_h > 5) && (randint0(100) < 20)) o_ptr->to_h--;

		/* Disenchant to-dam */
		if (o_ptr->to_d > 0) o_ptr->to_d--;
		if ((o_ptr->to_d > 5) && (randint0(100) < 20)) o_ptr->to_d--;
	}
	else
	{
		/* Disenchant to-ac */
		if (o_ptr->to_a > 0) o_ptr->to_a--;
		if ((o_ptr->to_a > 5) && (randint0(100) < 20)) o_ptr->to_a--;
	}

	/* Message */
	msg("Your %s (%c) %s disenchanted!",
	           o_name, equip_to_label(i),
	           ((o_ptr->number != 1) ? "were" : "was"));

	/* Recalculate bonuses */
	player->upkeep->update |= (PU_BONUS);

	/* Window stuff */
	player->upkeep->redraw |= (PR_EQUIP);

	/* Notice */
	return (TRUE);
}

/*
 * Hook to specify "weapon"
 */
static bool item_tester_hook_weapon(const object_type *o_ptr)
{
	return tval_is_weapon(o_ptr);
}


/*
 * Hook to specify "armour"
 */
static bool item_tester_hook_armour(const object_type *o_ptr)
{
	return tval_is_armor(o_ptr);
}

/*
 * Hopefully this is OK now
 */
static bool item_tester_unknown(const object_type *o_ptr)
{
	return object_is_known(o_ptr) ? FALSE : TRUE;
}

/*
 * Used by the "enchant" function (chance of failure)
 */
static const int enchant_table[16] =
{
	0, 10,  20, 40, 80,
	160, 280, 400, 550, 700,
	800, 900, 950, 970, 990,
	1000
};

/**
 * Tries to increase an items bonus score, if possible.
 *
 * \returns true if the bonus was increased
 */
static bool enchant_score(s16b *score, bool is_artifact)
{
	int chance;

	/* Artifacts resist enchantment half the time */
	if (is_artifact && randint0(100) < 50) return FALSE;

	/* Figure out the chance to enchant */
	if (*score < 0) chance = 0;
	else if (*score > 15) chance = 1000;
	else chance = enchant_table[*score];

	/* If we roll less-than-or-equal to chance, it fails */
	if (randint1(1000) <= chance) return FALSE;

	/* Increment the score */
	++*score;

	return TRUE;
}

/**
 * Tries to uncurse a cursed item, if possible
 *
 * \returns true if a curse was broken
 */
static bool enchant_curse(object_type *o_ptr, bool is_artifact)
{
	/* If the item isn't cursed (or is perma-cursed) this doesn't work */
	if (!cursed_p(o_ptr->flags) || of_has(o_ptr->flags, OF_PERMA_CURSE)) 
		return FALSE;

	/* Artifacts resist enchanting curses away half the time */
	if (is_artifact && randint0(100) < 50) return FALSE;

	/* Normal items are uncursed 25% of the tiem */
	if (randint0(100) >= 25) return FALSE;

	/* Uncurse the item */
	msg("The curse is broken!");
	uncurse_object(o_ptr);
	return TRUE;
}

/**
 * Helper function for enchant() which tries to do the two things that
 * enchanting an item does, namely increasing its bonuses and breaking curses
 *
 * \returns true if a bonus was increased or a curse was broken
 */
static bool enchant2(object_type *o_ptr, s16b *score)
{
	bool result = FALSE;
	bool is_artifact = o_ptr->artifact ? TRUE : FALSE;
	if (enchant_score(score, is_artifact)) result = TRUE;
	if (enchant_curse(o_ptr, is_artifact)) result = TRUE;
	return result;
}

/**
 * Enchant an item
 *
 * Revamped!  Now takes item pointer, number of times to try enchanting, and a
 * flag of what to try enchanting.  Artifacts resist enchantment some of the
 * time. Also, any enchantment attempt (even unsuccessful) kicks off a parallel
 * attempt to uncurse a cursed item.
 *
 * Note that an item can technically be enchanted all the way to +15 if you
 * wait a very, very, long time.  Going from +9 to +10 only works about 5% of
 * the time, and from +10 to +11 only about 1% of the time.
 *
 * Note that this function can now be used on "piles" of items, and the larger
 * the pile, the lower the chance of success.
 *
 * \returns true if the item was changed in some way
 */
bool enchant(object_type *o_ptr, int n, int eflag)
{
	int i, prob;
	bool res = FALSE;

	/* Large piles resist enchantment */
	prob = o_ptr->number * 100;

	/* Missiles are easy to enchant */
	if (tval_is_ammo(o_ptr)) prob = prob / 20;

	/* Try "n" times */
	for (i = 0; i < n; i++)
	{
		/* Roll for pile resistance */
		if (prob > 100 && randint0(prob) >= 100) continue;

		/* Try the three kinds of enchantment we can do */
		if ((eflag & ENCH_TOHIT) && enchant2(o_ptr, &o_ptr->to_h)) res = TRUE;
		if ((eflag & ENCH_TODAM) && enchant2(o_ptr, &o_ptr->to_d)) res = TRUE;
		if ((eflag & ENCH_TOAC)  && enchant2(o_ptr, &o_ptr->to_a)) res = TRUE;
	}

	/* Failure */
	if (!res) return (FALSE);

	/* Recalculate bonuses, gear */
	player->upkeep->update |= (PU_BONUS | PU_INVEN);

	/* Combine the pack (later) */
	player->upkeep->notice |= (PN_COMBINE);

	/* Redraw stuff */
	player->upkeep->redraw |= (PR_INVEN | PR_EQUIP );

	/* Success */
	return (TRUE);
}



/*
 * Enchant an item (in the inventory or on the floor)
 * Note that "num_ac" requires armour, else weapon
 * Returns TRUE if attempted, FALSE if cancelled
 */
bool enchant_spell(int num_hit, int num_dam, int num_ac)
{
	int item;
	bool okay = FALSE;

	object_type *o_ptr;

	char o_name[80];

	const char *q, *s;

	/* Get an item */
	q = "Enchant which item? ";
	s = "You have nothing to enchant.";
	if (!get_item(&item, q, s, 0, 
		num_ac ? item_tester_hook_armour : item_tester_hook_weapon,
		(USE_EQUIP | USE_INVEN | USE_QUIVER | USE_FLOOR))) return (FALSE);

	o_ptr = object_from_item_idx(item);


	/* Description */
	object_desc(o_name, sizeof(o_name), o_ptr, ODESC_BASE);

	/* Describe */
	msg("%s %s glow%s brightly!",
	           ((item >= 0) ? "Your" : "The"), o_name,
	           ((o_ptr->number > 1) ? "" : "s"));

	/* Enchant */
	if (enchant(o_ptr, num_hit, ENCH_TOHIT)) okay = TRUE;
	if (enchant(o_ptr, num_dam, ENCH_TODAM)) okay = TRUE;
	if (enchant(o_ptr, num_ac, ENCH_TOAC)) okay = TRUE;

	/* Failure */
	if (!okay)
	{
		flush();

		/* Message */
		msg("The enchantment failed.");
	}

	/* Something happened */
	return (TRUE);
}


/*
 * Identify an object in the inventory (or on the floor)
 * This routine does *not* automatically combine objects.
 * Returns TRUE if something was identified, else FALSE.
 */
bool ident_spell(void)
{
	int item;

	object_type *o_ptr;

	const char *q, *s;

	/* Get an item */
	q = "Identify which item? ";
	s = "You have nothing to identify.";
	if (!get_item(&item, q, s, 0, item_tester_unknown, (USE_EQUIP | USE_INVEN | USE_QUIVER | USE_FLOOR))) return (FALSE);

	o_ptr = object_from_item_idx(item);


	/* Identify the object */
	do_ident_item(o_ptr);


	/* Something happened */
	return (TRUE);
}

/**
 * Return TRUE if there are any objects available to identify (whether on
 * floor or in gear)
 */
bool spell_identify_unknown_available(void)
{
	int floor_list[MAX_FLOOR_STACK];
	int floor_num;
	int i;
	bool unidentified_gear = FALSE;

	floor_num = scan_floor(floor_list, N_ELEMENTS(floor_list), player->py,
						   player->px, 0x0B, item_tester_unknown);

	for (i = 0; i < player->max_gear; i++) {
		if (item_test(item_tester_unknown, i)) {
			unidentified_gear = TRUE;
			break;
		}
	}

	return unidentified_gear || floor_num > 0;
}

/*
 * Hook for "get_item()".  Determine if something is rechargable.
 */
static bool item_tester_hook_recharge(const object_type *o_ptr)
{
	/* Recharge staves and wands */
	if (tval_can_have_charges(o_ptr)) return TRUE;

	return FALSE;
}


/*
 * Recharge a wand or staff from the pack or on the floor.
 *
 * It is harder to recharge high level, and highly charged wands.
 *
 * XXX XXX XXX Beware of "sliding index errors".
 *
 * Should probably not "destroy" over-charged items, unless we
 * "replace" them by, say, a broken stick or some such.  The only
 * reason this is okay is because "scrolls of recharging" appear
 * BEFORE all staves/wands in the inventory.  Note that the
 * new "auto_sort_pack" option would correctly handle replacing
 * the "broken" wand with any other item (i.e. a broken stick).
 */
bool recharge(int spell_strength)
{
	int i, t, item, lev;

	object_type *o_ptr;

	const char *q, *s;


	/* Get an item */
	q = "Recharge which item? ";
	s = "You have nothing to recharge.";
	if (!get_item(&item, q, s, 0, item_tester_hook_recharge, (USE_INVEN | USE_FLOOR))) return (FALSE);

	o_ptr = object_from_item_idx(item);


	/* Extract the object "level" */
	lev = o_ptr->kind->level;

	/* 
	 * Chance of failure = 1 time in 
	 * [Spell_strength + 100 - item_level - 10 * charge_per_item]/15 
	 */
	i = (spell_strength + 100 - lev - (10 * (o_ptr->pval / o_ptr->number))) / 15;

	/* Back-fire */
	if ((i <= 1) || one_in_(i))
	{
		msg("The recharge backfires!");
		msg("There is a bright flash of light.");

		/* Reduce the charges of rods/wands/staves */
		reduce_charges(o_ptr, 1);

		/* Reduce and describe inventory */
		if (item >= 0)
		{
			inven_item_increase(item, -1);
			inven_item_describe(item);
			inven_item_optimize(item);
		}
		/* Reduce and describe floor item */
		else
		{
			floor_item_increase(0 - item, -1);
			floor_item_describe(0 - item);
			floor_item_optimize(0 - item);
		}
	}

	/* Recharge */
	else
	{
		/* Extract a "power" */
		t = (spell_strength / (lev + 2)) + 1;

		/* Recharge based on the power */
		if (t > 0) o_ptr->pval += 2 + randint1(t);
	}

	/* Update the gear */
	player->upkeep->update |= (PU_INVEN);

	/* Combine the pack (later) */
	player->upkeep->notice |= (PN_COMBINE);

	/* Redraw stuff */
	player->upkeep->redraw |= (PR_INVEN);

	/* Something was done */
	return (TRUE);
}








/*
 * Apply a "project()" directly to all viewable monsters
 *
 * Note that affected monsters are NOT auto-tracked by this usage.
 */
bool project_los(int typ, int dam, bool obvious)
{
	int i, x, y;

	int flg = PROJECT_JUMP | PROJECT_KILL | PROJECT_HIDE;

	if (obvious) flg |= PROJECT_AWARE;

	/* Affect all (nearby) monsters */
	for (i = 1; i < cave_monster_max(cave); i++)
	{
		monster_type *m_ptr = cave_monster(cave, i);

		/* Paranoia -- Skip dead monsters */
		if (!m_ptr->race) continue;

		/* Location */
		y = m_ptr->fy;
		x = m_ptr->fx;

		/* Require line of sight */
		if (!player_has_los_bold(y, x)) continue;

		/* Jump directly to the target monster */
		if (project(-1, 0, y, x, dam, typ, flg, 0, 0)) obvious = TRUE;
	}

	/* Result */
	return (obvious);
}


/*
 * Speed monsters
 */
bool speed_monsters(void)
{
	return (project_los(GF_OLD_SPEED, 50, FALSE));
}

/*
 * Slow monsters
 */
bool slow_monsters(void)
{
	return (project_los(GF_OLD_SLOW, 20, FALSE));
}

/*
 * Sleep monsters
 */
bool sleep_monsters(bool aware)
{
	return (project_los(GF_OLD_SLEEP, player->lev, aware));
}

/*
 * Confuse monsters
 */
bool confuse_monsters(bool aware)
{
	return (project_los(GF_OLD_CONF, player->lev, aware));
}


/*
 * Banish evil monsters
 */
bool banish_evil(int dist)
{
	return (project_los(GF_AWAY_EVIL, dist, FALSE));
}


/*
 * Turn undead
 */
bool turn_undead(bool aware)
{
	return (project_los(GF_TURN_UNDEAD, player->lev, aware));
}


/*
 * Dispel undead monsters
 */
bool dispel_undead(int dam)
{
	return (project_los(GF_DISP_UNDEAD, dam, FALSE));
}

/*
 * Dispel evil monsters
 */
bool dispel_evil(int dam)
{
	return (project_los(GF_DISP_EVIL, dam, FALSE));
}

/*
 * Dispel all monsters
 */
bool dispel_monsters(int dam)
{
	return (project_los(GF_DISP_ALL, dam, FALSE));
}





/*
 * Wake up all monsters, and speed up "los" monsters.
 */
void aggravate_monsters(struct monster *who)
{
	int i;

	bool sleep = FALSE;

	/* Aggravate everyone nearby */
	for (i = 1; i < cave_monster_max(cave); i++)
	{
		monster_type *m_ptr = cave_monster(cave, i);

		/* Paranoia -- Skip dead monsters */
		if (!m_ptr->race) continue;

		/* Skip aggravating monster (or player) */
		if (m_ptr == who) continue;

		/* Wake up nearby sleeping monsters */
		if ((m_ptr->cdis < MAX_SIGHT * 2) && m_ptr->m_timed[MON_TMD_SLEEP]) {
			mon_clear_timed(m_ptr, MON_TMD_SLEEP, MON_TMD_FLG_NOMESSAGE, FALSE);
			sleep = TRUE;
		}

		/* Speed up monsters in line of sight */
		if (player_has_los_bold(m_ptr->fy, m_ptr->fx))
			mon_inc_timed(m_ptr, MON_TMD_FAST, 25, MON_TMD_FLG_NOTIFY, FALSE);
	}

	/* Messages */
	if (sleep) msg("You hear a sudden stirring in the distance!");
}



/*
 * Delete all non-unique monsters of a given "type" from the level
 */
bool banishment(void)
{
	int i;
	unsigned dam = 0;

	struct keypress typ;

	if (!get_com("Choose a monster race (by symbol) to banish: ", &typ))
		return FALSE;

	/* Delete the monsters of that "type" */
	for (i = 1; i < cave_monster_max(cave); i++)
	{
		monster_type *m_ptr = cave_monster(cave, i);
		
		/* Paranoia -- Skip dead monsters */
		if (!m_ptr->race) continue;

		/* Hack -- Skip Unique Monsters */
		if (rf_has(m_ptr->race->flags, RF_UNIQUE)) continue;

		/* Skip "wrong" monsters */
		if (!char_matches_key(m_ptr->race->d_char, typ.code)) continue;

		/* Delete the monster */
		delete_monster_idx(i);

		/* Take some damage */
		dam += randint1(4);
	}

	/* Hurt the player */
	take_hit(player, dam, "the strain of casting Banishment");

	/* Update monster list window */
	player->upkeep->redraw |= PR_MONLIST;

	/* Success */
	return TRUE;
}


/*
 * Delete all nearby (non-unique) monsters
 */
bool mass_banishment(void)
{
	int i;
	unsigned dam = 0;

	bool result = FALSE;


	/* Delete the (nearby) monsters */
	for (i = 1; i < cave_monster_max(cave); i++)
	{
		monster_type *m_ptr = cave_monster(cave, i);

		/* Paranoia -- Skip dead monsters */
		if (!m_ptr->race) continue;

		/* Hack -- Skip unique monsters */
		if (rf_has(m_ptr->race->flags, RF_UNIQUE)) continue;

		/* Skip distant monsters */
		if (m_ptr->cdis > MAX_SIGHT) continue;

		/* Delete the monster */
		delete_monster_idx(i);

		/* Take some damage */
		dam += randint1(3);
	}

	/* Hurt the player */
	take_hit(player, dam, "the strain of casting Mass Banishment");

	/* Calculate result */
	result = (dam > 0) ? TRUE : FALSE;

	/* Update monster list window */
	if (result) player->upkeep->redraw |= PR_MONLIST;

	return (result);
}



/*
 * Probe nearby monsters
 */
bool probing(void)
{
	int i;

	bool probe = FALSE;


	/* Probe all (nearby) monsters */
	for (i = 1; i < cave_monster_max(cave); i++)
	{
		monster_type *m_ptr = cave_monster(cave, i);

		/* Paranoia -- Skip dead monsters */
		if (!m_ptr->race) continue;

		/* Require line of sight */
		if (!player_has_los_bold(m_ptr->fy, m_ptr->fx)) continue;

		/* Probe visible monsters */
		if (m_ptr->ml)
		{
			char m_name[80];

			/* Start the message */
			if (!probe) msg("Probing...");

			/* Get "the monster" or "something" */
			monster_desc(m_name, sizeof(m_name), m_ptr,
					MDESC_IND_HID | MDESC_CAPITAL);

			/* Describe the monster */
			msg("%s has %d hit points.", m_name, m_ptr->hp);

			/* Learn all of the non-spell, non-treasure flags */
			lore_do_probe(m_ptr);

			/* Probe worked */
			probe = TRUE;
		}
	}

	/* Done */
	if (probe)
	{
		msg("That's all.");
	}

	/* Result */
	return (probe);
}



/*
 * Teleport a monster, normally up to "dis" grids away.
 *
 * Attempt to move the monster at least "dis/2" grids away.
 *
 * But allow variation to prevent infinite loops.
 */
void teleport_away(struct monster *m_ptr, int dis)
{
	int ny = 0, nx = 0, oy, ox, d, i, min;

	bool look = TRUE;


	/* Paranoia */
	if (!m_ptr->race) return;

	/* Save the old location */
	oy = m_ptr->fy;
	ox = m_ptr->fx;

	/* Minimum distance */
	min = dis / 2;

	/* Look until done */
	while (look)
	{
		/* Verify max distance */
		if (dis > 200) dis = 200;

		/* Try several locations */
		for (i = 0; i < 500; i++)
		{
			/* Pick a (possibly illegal) location */
			while (1)
			{
				ny = rand_spread(oy, dis);
				nx = rand_spread(ox, dis);
				d = distance(oy, ox, ny, nx);
				if ((d >= min) && (d <= dis)) break;
			}

			/* Ignore illegal locations */
			if (!square_in_bounds_fully(cave, ny, nx)) continue;

			/* Require "empty" floor space */
			if (!square_isempty(cave, ny, nx)) continue;

			/* Hack -- no teleport onto glyph of warding */
			if (square_iswarded(cave, ny, nx)) continue;

			/* No teleporting into vaults and such */
			/* if (cave->info[ny][nx] & square_isvault(cave, ny, nx)) continue; */

			/* This grid looks good */
			look = FALSE;

			/* Stop looking */
			break;
		}

		/* Increase the maximum distance */
		dis = dis * 2;

		/* Decrease the minimum distance */
		min = min / 2;
	}

	/* Sound */
	sound(MSG_TPOTHER);

	/* Swap the monsters */
	monster_swap(oy, ox, ny, nx);
}

/*
 * Teleport the player to a location up to "dis" grids away.
 *
 * If no such spaces are readily available, the distance may increase.
 * Try very hard to move the player at least a quarter that distance.
 */
void teleport_player(int dis)
{
	int py = player->py;
	int px = player->px;

	int d, i, min, y, x;

	bool look = TRUE;

	/* Check for a no teleport grid */
	if (square_is_no_teleport(cave, py, px) && (dis > 10)) {
		msg("Teleportation forbidden!");
		return;
	}

	/* Initialize */
	y = py;
	x = px;

	/* Minimum distance */
	min = dis / 2;

	/* Look until done */
	while (look)
	{
		/* Verify max distance */
		if (dis > 200) dis = 200;

		/* Try several locations */
		for (i = 0; i < 500; i++)
		{
			/* Pick a (possibly illegal) location */
			while (1)
			{
				y = rand_spread(py, dis);
				x = rand_spread(px, dis);
				d = distance(py, px, y, x);
				if ((d >= min) && (d <= dis)) break;
			}

			/* Ignore illegal locations */
			if (!square_in_bounds_fully(cave, y, x)) continue;

			/* Require "naked" floor space */
			if (!square_isempty(cave, y, x)) continue;

			/* No teleporting into vaults and such */
			if (square_isvault(cave, y, x)) continue;

			/* This grid looks good */
			look = FALSE;

			/* Stop looking */
			break;
		}

		/* Increase the maximum distance */
		dis = dis * 2;

		/* Decrease the minimum distance */
		min = min / 2;
	}

	/* Sound */
	sound(MSG_TELEPORT);

	/* Move player */
	monster_swap(py, px, y, x);

	/* Handle stuff XXX XXX XXX */
	handle_stuff(player->upkeep);
}

/*
 * Teleport player to a grid near the given location
 *
 * This function is slightly obsessive about correctness.
 * This function allows teleporting into vaults (!)
 */
void teleport_player_to(int ny, int nx)
{
	int py = player->py;
	int px = player->px;

	int y, x;

	int dis = 0, ctr = 0;

	/* Initialize */
	y = py;
	x = px;

	/* Find a usable location */
	while (1)
	{
		/* Pick a nearby legal location */
		while (1)
		{
			y = rand_spread(ny, dis);
			x = rand_spread(nx, dis);
			if (square_in_bounds_fully(cave, y, x)) break;
		}

		/* Accept "naked" floor grids */
		if (square_isempty(cave, y, x)) break;

		/* Occasionally advance the distance */
		if (++ctr > (4 * dis * dis + 4 * dis + 1))
		{
			ctr = 0;
			dis++;
		}
	}

	/* Sound */
	sound(MSG_TELEPORT);

	/* Move player */
	monster_swap(py, px, y, x);

	/* Handle stuff XXX XXX XXX */
	handle_stuff(player->upkeep);
}

/*
 * Teleport the player one level up or down (random when legal)
 */
void teleport_player_level(void)
{
	bool up = TRUE, down = TRUE;

	/* No going up with force_descend or in the town */
	if (OPT(birth_force_descend) || !player->depth)
		up = FALSE;

	/* No forcing player down to quest levels if they can't leave */
	if (!up && is_quest(player->max_depth + 1))
		down = FALSE;

	/* Can't leave quest levels or go down deeper than the dungeon */
	if (is_quest(player->depth) || (player->depth >= MAX_DEPTH-1))
		down = FALSE;

	/* Determine up/down if not already done */
	if (up && down) {
		if (randint0(100) < 50)
			up = FALSE;
		else
			down = FALSE;
	}

	/* Now actually do the level change */
	if (up) {
		msgt(MSG_TPLEVEL, "You rise up through the ceiling.");
		dungeon_change_level(player->depth - 1);
	} else if (down) {
		msgt(MSG_TPLEVEL, "You sink through the floor.");

		if (OPT(birth_force_descend))
			dungeon_change_level(player->max_depth + 1);
		else
			dungeon_change_level(player->depth + 1);
	} else {
		msg("Nothing happens.");
	}
}

/*
 * The spell of destruction
 *
 * This spell "deletes" monsters (instead of "killing" them).
 *
 * Later we may use one function for both "destruction" and
 * "earthquake" by using the "full" to select "destruction".
 */
void destroy_area(int y1, int x1, int r, bool full)
{
	int y, x, k;

	bool flag = FALSE;


	/* Unused parameter */
	(void)full;

	/* No effect in town */
	if (!player->depth)
	{
		msg("The ground shakes for a moment.");
		return;
	}

	/* Big area of affect */
	for (y = (y1 - r); y <= (y1 + r); y++)
	{
		for (x = (x1 - r); x <= (x1 + r); x++)
		{
			/* Skip illegal grids */
			if (!square_in_bounds_fully(cave, y, x)) continue;
			
			/* Extract the distance */
			k = distance(y1, x1, y, x);

			/* Stay in the circle of death */
			if (k > r) continue;

			/* Lose room and vault */
			sqinfo_off(cave->info[y][x], SQUARE_ROOM);
			sqinfo_off(cave->info[y][x], SQUARE_VAULT);

			/* Lose light */
			sqinfo_off(cave->info[y][x], SQUARE_GLOW);
			
			square_light_spot(cave, y, x);

			/* Hack -- Notice player affect */
			if (cave->m_idx[y][x] < 0)
			{
				/* Hurt the player later */
				flag = TRUE;

				/* Do not hurt this grid */
				continue;
			}

			/* Hack -- Skip the epicenter */
			if ((y == y1) && (x == x1)) continue;

			/* Delete the monster (if any) */
			delete_monster(y, x);
			
			/* Don't remove stairs */
			if (square_isstairs(cave, y, x)) continue;	
			
			/* Lose knowledge (keeping knowledge of stairs) */
			sqinfo_off(cave->info[y][x], SQUARE_MARK);

			/* Destroy any grid that isn't a permament wall */
			if (!square_isperm(cave, y, x))
			{
				/* Delete objects */
				delete_object(y, x);
				square_destroy(cave, y, x);
			}
		}
	}


	/* Hack -- Affect player */
	if (flag)
	{
		/* Message */
		msg("There is a searing blast of light!");

		/* Blind the player */
		wieldeds_notice_element(player, ELEM_LIGHT);
		if (!player_resists(player, ELEM_LIGHT)) {
			/* Become blind */
			(void)player_inc_timed(player, TMD_BLIND, 10 + randint1(10),
								   TRUE, TRUE);
		}
	}


	/* Fully update the visuals */
	player->upkeep->update |= (PU_FORGET_VIEW | PU_UPDATE_VIEW | PU_MONSTERS);

	/* Fully update the flow */
	player->upkeep->update |= (PU_FORGET_FLOW | PU_UPDATE_FLOW);

	/* Redraw monster list */
	player->upkeep->redraw |= (PR_MONLIST | PR_ITEMLIST);
}


/*
 * Induce an "earthquake" of the given radius at the given location.
 *
 * This will turn some walls into floors and some floors into walls.
 *
 * The player will take damage and "jump" into a safe grid if possible,
 * otherwise, he will "tunnel" through the rubble instantaneously.
 *
 * Monsters will take damage, and "jump" into a safe grid if possible,
 * otherwise they will be "buried" in the rubble, disappearing from
 * the level in the same way that they do when banished.
 *
 * Note that players and monsters (except eaters of walls and passers
 * through walls) will never occupy the same grid as a wall (or door).
 */
void earthquake(int cy, int cx, int r)
{
	int py = player->py;
	int px = player->px;

	int i, y, x, yy, xx, dy, dx;

	int damage = 0;

	int sn = 0, sy = 0, sx = 0;

	bool hurt = FALSE;

	bool map[32][32];

	/* No effect in town */
	if (!player->depth)
	{
		msg("The ground shakes for a moment.");
		return;
	}

	/* Paranoia -- Enforce maximum range */
	if (r > 12) r = 12;

	/* Clear the "maximal blast" area */
	for (y = 0; y < 32; y++)
	{
		for (x = 0; x < 32; x++)
		{
			map[y][x] = FALSE;
		}
	}

	/* Check around the epicenter */
	for (dy = -r; dy <= r; dy++)
	{
		for (dx = -r; dx <= r; dx++)
		{
			/* Extract the location */
			yy = cy + dy;
			xx = cx + dx;

			/* Skip illegal grids */
			if (!square_in_bounds_fully(cave, yy, xx)) continue;

			/* Skip distant grids */
			if (distance(cy, cx, yy, xx) > r) continue;

			/* Lose room and vault */
			sqinfo_off(cave->info[yy][xx], SQUARE_ROOM);
			sqinfo_off(cave->info[yy][xx], SQUARE_VAULT);

			/* Lose light and knowledge */
			sqinfo_off(cave->info[yy][xx], SQUARE_GLOW);
			sqinfo_off(cave->info[yy][xx], SQUARE_MARK);
			
			/* Skip the epicenter */
			if (!dx && !dy) continue;

			/* Skip most grids */
			if (randint0(100) < 85) continue;

			/* Damage this grid */
			map[16+yy-cy][16+xx-cx] = TRUE;

			/* Hack -- Take note of player damage */
			if ((yy == py) && (xx == px)) hurt = TRUE;
		}
	}

	/* First, affect the player (if necessary) */
	if (hurt)
	{
		/* Check around the player */
		for (i = 0; i < 8; i++)
		{
			/* Get the location */
			y = py + ddy_ddd[i];
			x = px + ddx_ddd[i];

			/* Skip non-empty grids */
			if (!square_isempty(cave, y, x)) continue;

			/* Important -- Skip "quake" grids */
			if (map[16+y-cy][16+x-cx]) continue;

			/* Count "safe" grids, apply the randomizer */
			if ((++sn > 1) && (randint0(sn) != 0)) continue;

			/* Save the safe location */
			sy = y; sx = x;
		}

		/* Random message */
		switch (randint1(3))
		{
			case 1:
			{
				msg("The cave ceiling collapses!");
				break;
			}
			case 2:
			{
				msg("The cave floor twists in an unnatural way!");
				break;
			}
			default:
			{
				msg("The cave quakes!");
				msg("You are pummeled with debris!");
				break;
			}
		}

		/* Hurt the player a lot */
		if (!sn)
		{
			/* Message and damage */
			msg("You are severely crushed!");
			damage = 300;
		}

		/* Destroy the grid, and push the player to safety */
		else
		{
			/* Calculate results */
			switch (randint1(3))
			{
				case 1:
				{
					msg("You nimbly dodge the blast!");
					damage = 0;
					break;
				}
				case 2:
				{
					msg("You are bashed by rubble!");
					damage = damroll(10, 4);
					(void)player_inc_timed(player, TMD_STUN, randint1(50), TRUE, TRUE);
					break;
				}
				case 3:
				{
					msg("You are crushed between the floor and ceiling!");
					damage = damroll(10, 4);
					(void)player_inc_timed(player, TMD_STUN, randint1(50), TRUE, TRUE);
					break;
				}
			}

			/* Move player */
			monster_swap(py, px, sy, sx);
		}

		/* Take some damage */
		if (damage) take_hit(player, damage, "an earthquake");
	}


	/* Examine the quaked region */
	for (dy = -r; dy <= r; dy++)
	{
		for (dx = -r; dx <= r; dx++)
		{
			/* Extract the location */
			yy = cy + dy;
			xx = cx + dx;

			/* Skip unaffected grids */
			if (!map[16+yy-cy][16+xx-cx]) continue;

			/* Process monsters */
			if (cave->m_idx[yy][xx] > 0)
			{
				monster_type *m_ptr = square_monster(cave, yy, xx);
				
				/* Most monsters cannot co-exist with rock */
				if (!flags_test(m_ptr->race->flags, RF_SIZE, RF_KILL_WALL, RF_PASS_WALL, FLAG_END))
				{
					char m_name[80];

					/* Assume not safe */
					sn = 0;

					/* Monster can move to escape the wall */
					if (!rf_has(m_ptr->race->flags, RF_NEVER_MOVE))
					{
						/* Look for safety */
						for (i = 0; i < 8; i++)
						{
							/* Get the grid */
							y = yy + ddy_ddd[i];
							x = xx + ddx_ddd[i];

							/* Skip non-empty grids */
							if (!square_isempty(cave, y, x)) continue;

							/* Hack -- no safety on glyph of warding */
							if (square_iswarded(cave, y, x))
								continue;

							/* Important -- Skip "quake" grids */
							if (map[16+y-cy][16+x-cx]) continue;

							/* Count "safe" grids, apply the randomizer */
							if ((++sn > 1) && (randint0(sn) != 0)) continue;

							/* Save the safe grid */
							sy = y;
							sx = x;
						}
					}

					/* Describe the monster */
					monster_desc(m_name, sizeof(m_name), m_ptr, MDESC_STANDARD);

					/* Scream in pain */
					msg("%s wails out in pain!", m_name);

					/* Take damage from the quake */
					damage = (sn ? damroll(4, 8) : (m_ptr->hp + 1));

					/* Monster is certainly awake */
					mon_clear_timed(m_ptr, MON_TMD_SLEEP,
							MON_TMD_FLG_NOMESSAGE, FALSE);

					/* If the quake finished the monster off, show message */
					if (m_ptr->hp < damage && m_ptr->hp >= 0)
						msg("%s is embedded in the rock!", m_name);

					/* Apply damage directly */
					m_ptr->hp -= damage;

					/* Delete (not kill) "dead" monsters */
					if (m_ptr->hp < 0)
					{
						/* Delete the monster */
						delete_monster(yy, xx);

						/* No longer safe */
						sn = 0;
					}

					/* Hack -- Escape from the rock */
					if (sn)
					{
						/* Move the monster */
						monster_swap(yy, xx, sy, sx);
					}
				}
			}
		}
	}


	/* XXX XXX XXX */

	/* New location */
	py = player->py;
	px = player->px;

	/* Important -- no wall on player */
	map[16+py-cy][16+px-cx] = FALSE;


	/* Examine the quaked region */
	for (dy = -r; dy <= r; dy++)
	{
		for (dx = -r; dx <= r; dx++)
		{
			/* Extract the location */
			yy = cy + dy;
			xx = cx + dx;

			/* ignore invalid grids */
			if (!square_in_bounds_fully(cave, yy, xx)) continue;

			/* Note unaffected grids for light changes, etc. */
			if (!map[16+yy-cy][16+xx-cx])
			{
				square_light_spot(cave, yy, xx);
			}

			/* Destroy location (if valid) */
			else if (square_valid_bold(yy, xx))
			{
				delete_object(yy, xx);
				square_earthquake(cave, yy, xx);
			}
		}
	}


	/* Fully update the visuals */
	player->upkeep->update |= (PU_FORGET_VIEW | PU_UPDATE_VIEW | PU_MONSTERS);

	/* Fully update the flow */
	player->upkeep->update |= (PU_FORGET_FLOW | PU_UPDATE_FLOW);

	/* Update the health bar */
	player->upkeep->redraw |= (PR_HEALTH);

	/* Window stuff */
	player->upkeep->redraw |= (PR_MONLIST | PR_ITEMLIST);
}

/*
 * This routine will Perma-Light all grids in the set passed in.
 *
 * This routine is used (only) by "light_room(..., LIGHT)"
 *
 * Dark grids are illuminated.
 *
 * Also, process all affected monsters.
 *
 * SMART monsters always wake up when illuminated
 * NORMAL monsters wake up 1/4 the time when illuminated
 * STUPID monsters wake up 1/10 the time when illuminated
 */
static void cave_light(struct point_set *ps)
{
	int i;

	/* Apply flag changes */
	for (i = 0; i < ps->n; i++)
	{
		int y = ps->pts[i].y;
		int x = ps->pts[i].x;

		/* Perma-Light */
		sqinfo_on(cave->info[y][x], SQUARE_GLOW);
	}

	/* Fully update the visuals */
	player->upkeep->update |= (PU_FORGET_VIEW | PU_UPDATE_VIEW | PU_MONSTERS);

	/* Update stuff */
	update_stuff(player->upkeep);

	/* Process the grids */
	for (i = 0; i < ps->n; i++)
	{
		int y = ps->pts[i].y;
		int x = ps->pts[i].x;

		/* Redraw the grid */
		square_light_spot(cave, y, x);

		/* Process affected monsters */
		if (cave->m_idx[y][x] > 0)
		{
			int chance = 25;

			monster_type *m_ptr = square_monster(cave, y, x);

			/* Stupid monsters rarely wake up */
			if (rf_has(m_ptr->race->flags, RF_STUPID)) chance = 10;

			/* Smart monsters always wake up */
			if (rf_has(m_ptr->race->flags, RF_SMART)) chance = 100;

			/* Sometimes monsters wake up */
			if (m_ptr->m_timed[MON_TMD_SLEEP] && (randint0(100) < chance))
			{
				/* Wake up! */
				mon_clear_timed(m_ptr, MON_TMD_SLEEP,
					MON_TMD_FLG_NOTIFY, FALSE);

			}
		}
	}
}



/*
 * This routine will "darken" all grids in the set passed in.
 *
 * In addition, some of these grids will be "unmarked".
 *
 * This routine is used (only) by "light_room(..., UNLIGHT)"
 */
static void cave_unlight(struct point_set *ps)
{
	int i;

	/* Apply flag changes */
	for (i = 0; i < ps->n; i++)
	{
		int y = ps->pts[i].y;
		int x = ps->pts[i].x;

		/* Darken the grid */
		sqinfo_off(cave->info[y][x], SQUARE_GLOW);

		/* Hack -- Forget "boring" grids */
		if (!square_isinteresting(cave, y, x))
			sqinfo_off(cave->info[y][x], SQUARE_MARK);
	}

	/* Fully update the visuals */
	player->upkeep->update |= (PU_FORGET_VIEW | PU_UPDATE_VIEW | PU_MONSTERS);

	/* Update stuff */
	update_stuff(player->upkeep);

	/* Process the grids */
	for (i = 0; i < ps->n; i++)
	{
		int y = ps->pts[i].y;
		int x = ps->pts[i].x;

		/* Redraw the grid */
		square_light_spot(cave, y, x);
	}
}

/*
 * Aux function -- see below
 */
static void cave_room_aux(struct point_set *seen, int y, int x)
{
	if (point_set_contains(seen, y, x))
		return;

	if (!square_isroom(cave, y, x))
		return;

	/* Add it to the "seen" set */
	add_to_point_set(seen, y, x);
}

#define LIGHT TRUE
#define UNLIGHT FALSE
/*
 * Illuminate or darken any room containing the given location.
 */
void light_room(int y1, int x1, bool light)
{
	int i, x, y;
	struct point_set *ps;

	ps = point_set_new(200);
	/* Add the initial grid */
	cave_room_aux(ps, y1, x1);

	/* While grids are in the queue, add their neighbors */
	for (i = 0; i < ps->n; i++)
	{
		x = ps->pts[i].x, y = ps->pts[i].y;

		/* Walls get lit, but stop light */
		if (!square_isprojectable(cave, y, x)) continue;

		/* Spread adjacent */
		cave_room_aux(ps, y + 1, x);
		cave_room_aux(ps, y - 1, x);
		cave_room_aux(ps, y, x + 1);
		cave_room_aux(ps, y, x - 1);

		/* Spread diagonal */
		cave_room_aux(ps, y + 1, x + 1);
		cave_room_aux(ps, y - 1, x - 1);
		cave_room_aux(ps, y - 1, x + 1);
		cave_room_aux(ps, y + 1, x - 1);
	}

	/* Now, lighten or darken them all at once */
	if (light) {
		cave_light(ps);
	} else {
		cave_unlight(ps);
	}
	point_set_dispose(ps);
}


/*
 * Hack -- call light around the player
 * Affect all monsters in the projection radius
 */
bool light_area(int dam, int rad)
{
	int py = player->py;
	int px = player->px;

	int flg = PROJECT_GRID | PROJECT_KILL;

	/* Hack -- Message */
	if (!player->timed[TMD_BLIND])
		msg("You are surrounded by a white light.");

	/* Hook into the "project()" function */
	(void)project(-1, rad, py, px, dam, GF_LIGHT_WEAK, flg, 0, 0);

	/* Light up the room */
	light_room(py, px, LIGHT);

	/* Assume seen */
	return (TRUE);
}


/*
 * Hack -- call darkness around the player
 * Affect all monsters in the projection radius
 */
bool unlight_area(int dam, int rad)
{
	int py = player->py;
	int px = player->px;

	int flg = PROJECT_GRID | PROJECT_KILL;

	/* Hack -- Message */
	if (!player->timed[TMD_BLIND])
	{
		msg("Darkness surrounds you.");
	}

	/* Hook into the "project()" function */
	(void)project(-1, rad, py, px, dam, GF_DARK_WEAK, flg, 0, 0);

	/* Darken the room */
	light_room(py, px, UNLIGHT);

	/* Assume seen */
	return (TRUE);
}



/*
 * Cast a ball spell
 * Stop if we hit a monster, act as a "ball"
 * Allow "target" mode to pass over monsters
 * Affect grids, objects, and monsters
 */
bool fire_ball(int typ, int dir, int dam, int rad)
{
	int py = player->py;
	int px = player->px;

	s16b ty, tx;

	int flg = PROJECT_STOP | PROJECT_GRID | PROJECT_ITEM | PROJECT_KILL;

	/* Use the given direction */
	ty = py + 99 * ddy[dir];
	tx = px + 99 * ddx[dir];

	/* Hack -- Use an actual "target" */
	if ((dir == 5) && target_okay())
	{
		flg &= ~(PROJECT_STOP);

		target_get(&tx, &ty);
	}

	/* Analyze the "dir" and the "target".  Hurt items on floor. */
	return (project(-1, rad, ty, tx, dam, typ, flg, 0, 0));
}


/*
 * Cast multiple non-jumping ball spells at the same target.
 *
 * Targets absolute coordinates instead of a specific monster, so that
 * the death of the monster doesn't change the target's location.
 */
bool fire_swarm(int num, int typ, int dir, int dam, int rad)
{
	bool noticed = FALSE;

	int py = player->py;
	int px = player->px;

	s16b ty, tx;

	int flg = PROJECT_THRU | PROJECT_STOP | PROJECT_GRID | PROJECT_ITEM | PROJECT_KILL;

	/* Use the given direction */
	ty = py + 99 * ddy[dir];
	tx = px + 99 * ddx[dir];

	/* Hack -- Use an actual "target" (early detonation) */
	if ((dir == 5) && target_okay())
		target_get(&tx, &ty);

	while (num--)
	{
		/* Analyze the "dir" and the "target".  Hurt items on floor. */
		if (project(-1, rad, ty, tx, dam, typ, flg, 0, 0)) noticed = TRUE;
	}

	return noticed;
}


/*
 * Hack -- apply a "projection()" in a direction (or at the target)
 */
static bool project_hook(int typ, int dir, int dam, int flg)
{
	int py = player->py;
	int px = player->px;

	s16b ty, tx;

	/* Pass through the target if needed */
	flg |= (PROJECT_THRU);

	/* Use the given direction */
	ty = py + ddy[dir];
	tx = px + ddx[dir];

	/* Hack -- Use an actual "target" */
	if ((dir == 5) && target_okay())
		target_get(&tx, &ty);

	/* Analyze the "dir" and the "target", do NOT explode */
	return (project(-1, 0, ty, tx, dam, typ, flg, 0, 0));
}


/*
 * Cast a bolt spell
 * Stop if we hit a monster, as a "bolt"
 * Affect monsters (not grids or objects)
 */
bool fire_bolt(int typ, int dir, int dam)
{
	int flg = PROJECT_STOP | PROJECT_KILL;
	return (project_hook(typ, dir, dam, flg));
}

/*
 * Cast a beam spell
 * Pass through monsters, as a "beam"
 * Affect monsters (not grids or objects)
 */
bool fire_beam(int typ, int dir, int dam)
{
	int flg = PROJECT_BEAM | PROJECT_KILL;
	return (project_hook(typ, dir, dam, flg));
}

/*
 * Cast a bolt spell, or rarely, a beam spell
 */
bool fire_bolt_or_beam(int prob, int typ, int dir, int dam)
{
	if (randint0(100) < prob)
	{
		return (fire_beam(typ, dir, dam));
	}
	else
	{
		return (fire_bolt(typ, dir, dam));
	}
}


/*
 * Some of the old functions
 */

bool light_line(int dir)
{
	int flg = PROJECT_BEAM | PROJECT_GRID | PROJECT_KILL;
	return (project_hook(GF_LIGHT_WEAK, dir, damroll(6, 8), flg));
}

bool strong_light_line(int dir)
{
	int flg = PROJECT_BEAM | PROJECT_GRID | PROJECT_KILL;
	return (project_hook(GF_LIGHT, dir, damroll(10, 8), flg));
}

bool drain_life(int dir, int dam)
{
	int flg = PROJECT_STOP | PROJECT_KILL;
	return (project_hook(GF_OLD_DRAIN, dir, dam, flg));
}

bool wall_to_mud(int dir)
{
	int flg = PROJECT_BEAM | PROJECT_GRID | PROJECT_ITEM | PROJECT_KILL;
	return (project_hook(GF_KILL_WALL, dir, 20 + randint1(30), flg));
}

bool destroy_door(int dir)
{
	int flg = PROJECT_BEAM | PROJECT_GRID | PROJECT_ITEM;
	return (project_hook(GF_KILL_DOOR, dir, 0, flg));
}

bool disarm_trap(int dir)
{
	int flg = PROJECT_BEAM | PROJECT_GRID | PROJECT_ITEM;
	return (project_hook(GF_KILL_TRAP, dir, 0, flg));
}

bool heal_monster(int dir)
{
	int flg = PROJECT_STOP | PROJECT_KILL;
	return (project_hook(GF_OLD_HEAL, dir, damroll(4, 6), flg));
}

bool speed_monster(int dir)
{
	int flg = PROJECT_STOP | PROJECT_KILL;
	return (project_hook(GF_OLD_SPEED, dir, 100, flg));
}

bool slow_monster(int dir)
{
	int flg = PROJECT_STOP | PROJECT_KILL;
	return (project_hook(GF_OLD_SLOW, dir, 20, flg));
}

bool sleep_monster(int dir, bool aware)
{
	int flg = PROJECT_STOP | PROJECT_KILL;
	if (aware) flg |= PROJECT_AWARE;
	return (project_hook(GF_OLD_SLEEP, dir, player->lev, flg));
}

bool confuse_monster(int dir, int plev, bool aware)
{
	int flg = PROJECT_STOP | PROJECT_KILL;
	if (aware) flg |= PROJECT_AWARE;
	return (project_hook(GF_OLD_CONF, dir, plev, flg));
}

bool poly_monster(int dir)
{
	int flg = PROJECT_STOP | PROJECT_KILL;
	return (project_hook(GF_OLD_POLY, dir, player->lev, flg));
}

bool clone_monster(int dir)
{
	int flg = PROJECT_STOP | PROJECT_KILL;
	return (project_hook(GF_OLD_CLONE, dir, 0, flg));
}

bool fear_monster(int dir, int plev, bool aware)
{
	int flg = PROJECT_STOP | PROJECT_KILL;
	if (aware) flg |= PROJECT_AWARE;
	return (project_hook(GF_TURN_ALL, dir, plev, flg));
}

bool teleport_monster(int dir)
{
	int flg = PROJECT_STOP | PROJECT_KILL;
	return (project_hook(GF_AWAY_ALL, dir, MAX_SIGHT * 5, flg));
}



/*
 * Hooks -- affect adjacent grids (radius 1 ball attack)
 */

bool door_creation(void)
{
	int py = player->py;
	int px = player->px;

	int flg = PROJECT_GRID | PROJECT_ITEM | PROJECT_HIDE;
	return (project(-1, 1, py, px, 0, GF_MAKE_DOOR, flg, 0, 0));
}

bool trap_creation(void)
{
	int py = player->py;
	int px = player->px;

	int flg = PROJECT_GRID | PROJECT_ITEM | PROJECT_HIDE;
	return (project(-1, 1, py, px, 0, GF_MAKE_TRAP, flg, 0, 0));
}

bool destroy_doors_touch(void)
{
	int py = player->py;
	int px = player->px;

	int flg = PROJECT_GRID | PROJECT_ITEM | PROJECT_HIDE;
	return (project(-1, 1, py, px, 0, GF_KILL_DOOR, flg, 0, 0));
}

bool sleep_monsters_touch(bool aware)
{
	int py = player->py;
	int px = player->px;

	int flg = PROJECT_KILL | PROJECT_HIDE;
	if (aware) flg |= PROJECT_AWARE;
	return (project(-1, 1, py, px, player->lev, GF_OLD_SLEEP, flg, 0, 0));
}


/*
 * Curse the players armor
 */
bool curse_armor(void)
{
	object_type *o_ptr;

	char o_name[80];


	/* Curse the body armor */
	o_ptr = equipped_item_by_slot_name(player, "body");

	/* Nothing to curse */
	if (!o_ptr->kind) return (FALSE);

	/* Describe */
	object_desc(o_name, sizeof(o_name), o_ptr, ODESC_FULL);

	/* Attempt a saving throw for artifacts */
	if (o_ptr->artifact && (randint0(100) < 50))
	{
		/* Cool */
		msg("A %s tries to %s, but your %s resists the effects!",
		           "terrible black aura", "surround your armor", o_name);
	}

	/* not artifact or failed save... */
	else
	{
		/* Oops */
		msg("A terrible black aura blasts your %s!", o_name);

		/* Take down bonus a wee bit */
		o_ptr->to_a -= randint1(3);

		/* Curse it */
		flags_set(o_ptr->flags, OF_SIZE, OF_LIGHT_CURSE, OF_HEAVY_CURSE, FLAG_END);

		/* Recalculate bonuses */
		player->upkeep->update |= (PU_BONUS);

		/* Recalculate mana */
		player->upkeep->update |= (PU_MANA);

		/* Window stuff */
		player->upkeep->redraw |= (PR_INVEN | PR_EQUIP);
	}

	return (TRUE);
}


/*
 * Curse the players weapon
 */
bool curse_weapon(void)
{
	object_type *o_ptr;

	char o_name[80];


	/* Curse the weapon */
	o_ptr = equipped_item_by_slot_name(player, "weapon");

	/* Nothing to curse */
	if (!o_ptr->kind) return (FALSE);

	/* Describe */
	object_desc(o_name, sizeof(o_name), o_ptr, ODESC_FULL);

	/* Attempt a saving throw */
	if (o_ptr->artifact && (randint0(100) < 50))
	{
		/* Cool */
		msg("A %s tries to %s, but your %s resists the effects!",
		           "terrible black aura", "surround your weapon", o_name);
	}

	/* not artifact or failed save... */
	else
	{
		/* Oops */
		msg("A terrible black aura blasts your %s!", o_name);

		/* Hurt it a bit */
		o_ptr->to_h = 0 - randint1(3);
		o_ptr->to_d = 0 - randint1(3);

		/* Curse it */
		flags_set(o_ptr->flags, OF_SIZE, OF_LIGHT_CURSE, OF_HEAVY_CURSE, FLAG_END);

		/* Recalculate bonuses */
		player->upkeep->update |= (PU_BONUS);

		/* Recalculate mana */
		player->upkeep->update |= (PU_MANA);

		/* Window stuff */
		player->upkeep->redraw |= (PR_INVEN | PR_EQUIP);
	}

	/* Notice */
	return (TRUE);
}


/*
 * Brand weapons (or ammo)
 *
 * Turns the (non-magical) object into an ego-item of 'brand_type'.
 */
void brand_object(object_type *o_ptr, const char *name)
{
	int i;
	ego_item_type *e_ptr;
	bool ok = FALSE;

	/* you can never modify artifacts / ego-items */
	/* you can never modify cursed / worthless items */
	if (o_ptr->kind && !cursed_p(o_ptr->flags) && o_ptr->kind->cost &&
	    !o_ptr->artifact && !o_ptr->ego)
	{
		char o_name[80];
		char brand[20];

		object_desc(o_name, sizeof(o_name), o_ptr, ODESC_BASE);
		strnfmt(brand, sizeof(brand), "of %s", name);

		/* Describe */
		msg("The %s %s surrounded with an aura of %s.", o_name,
			(o_ptr->number > 1) ? "are" : "is", name);

		/* Get the right ego type for the object */
		for (i = 0; i < z_info->e_max; i++) {
			e_ptr = &e_info[i];

			/* Match the name */
			if (!e_ptr->name) continue;
			if (streq(e_ptr->name, brand)) {
				struct ego_poss_item *poss;
				for (poss = e_ptr->poss_items; poss; poss = poss->next)
					if (poss->kidx == o_ptr->kind->kidx)
						ok = TRUE;
			}
			if (ok) break;
		}

		/* Make it an ego item */
		o_ptr->ego = &e_info[i];
		ego_apply_magic(o_ptr, 0);
		object_notice_ego(o_ptr);

		/* Update the gear */
		player->upkeep->update |= (PU_INVEN);

		/* Combine the pack (later) */
		player->upkeep->notice |= (PN_COMBINE);

		/* Window stuff */
		player->upkeep->redraw |= (PR_INVEN | PR_EQUIP);

		/* Enchant */
		enchant(o_ptr, randint0(3) + 4, ENCH_TOHIT | ENCH_TODAM);
	}
	else
	{
		flush();
		msg("The branding failed.");
	}
}


/*
 * Brand the current weapon
 */
void brand_weapon(void)
{
	object_type *o_ptr = equipped_item_by_slot_name(player, "weapon");

	/* Select the brand */
	const char *brand = one_in_(2) ? "Flame" : "Frost";

	/* Brand the weapon */
	brand_object(o_ptr, brand);
}


/*
 * Hook to specify "ammo"
 */
static bool item_tester_hook_ammo(const object_type *o_ptr)
{
	return tval_is_ammo(o_ptr);
}


/*
 * Brand some (non-magical) ammo
 */
bool brand_ammo(void)
{
	int item;
	object_type *o_ptr;
	const char *q, *s;

	/* Select the brand */
	const char *brand = one_in_(3) ? "Flame" : (one_in_(2) ? "Frost" : "Venom");

	/* Get an item */
	q = "Brand which kind of ammunition? ";
	s = "You have nothing to brand.";
	if (!get_item(&item, q, s, 0, item_tester_hook_ammo, (USE_INVEN | USE_QUIVER | USE_FLOOR))) return (FALSE);

	o_ptr = object_from_item_idx(item);

	/* Brand the ammo */
	brand_object(o_ptr, brand);

	/* Done */
	return (TRUE);
}

static bool item_tester_hook_bolt(const struct object *o)
{
	return o->tval == TV_BOLT;
}

/*
 * Enchant some (non-magical) bolts
 */
bool brand_bolts(void)
{
	int item;
	object_type *o_ptr;
	const char *q, *s;

	/* Get an item */
	q = "Brand which bolts? ";
	s = "You have no bolts to brand.";
	if (!get_item(&item, q, s, 0, item_tester_hook_bolt, (USE_INVEN | USE_QUIVER | USE_FLOOR))) return (FALSE);

	o_ptr = object_from_item_idx(item);

	/* Brand the bolts */
	brand_object(o_ptr, "of Flame");

	/* Done */
	return (TRUE);
}


/*
 * Hack -- activate the ring of power
 */
void ring_of_power(int dir)
{
	/* Pick a random effect */
	switch (randint1(10))
	{
		case 1:
		case 2:
		{
			/* Message */
			msg("You are surrounded by a malignant aura.");

			/* Decrease all stats (permanently) */
			player_stat_dec(player, STAT_STR, TRUE);
			player_stat_dec(player, STAT_INT, TRUE);
			player_stat_dec(player, STAT_WIS, TRUE);
			player_stat_dec(player, STAT_DEX, TRUE);
			player_stat_dec(player, STAT_CON, TRUE);

			/* Lose some experience (permanently) */
			player_exp_lose(player, player->exp / 4, TRUE);

			break;
		}

		case 3:
		{
			/* Message */
			msg("You are surrounded by a powerful aura.");

			/* Dispel monsters */
			dispel_monsters(1000);

			break;
		}

		case 4:
		case 5:
		case 6:
		{
			/* Mana Ball */
			fire_ball(GF_MANA, dir, 300, 3);

			break;
		}

		case 7:
		case 8:
		case 9:
		case 10:
		{
			/* Mana Bolt */
			fire_bolt(GF_MANA, dir, 250);

			break;
		}
	}
}


/*
 * Identify an item.
 */
void do_ident_item(object_type *o_ptr)
{
	char o_name[80];

	u32b msg_type = 0;
	int i, index, slot;
	bool bad = TRUE;

    /* Identify and apply autoinscriptions. */
	object_flavor_aware(o_ptr);
	object_notice_everything(o_ptr);
	apply_autoinscription(o_ptr);

	/* Set ignore flag */
	player->upkeep->notice |= PN_IGNORE;

	/* Recalculate bonuses */
	player->upkeep->update |= (PU_BONUS);

	/* Window stuff */
	player->upkeep->redraw |= (PR_INVEN | PR_EQUIP);

	/* Description */
	object_desc(o_name, sizeof(o_name), o_ptr, ODESC_PREFIX | ODESC_FULL);

	/* Determine the message type. */
	/* CC: we need to think more carefully about how we define "bad" with
	 * multiple modifiers - currently using "all nonzero modifiers < 0" */
	for (i = 0; i < OBJ_MOD_MAX; i++)
		if (o_ptr->modifiers[i] > 0)
			bad = FALSE;

	if (bad)
		msg_type = MSG_IDENT_BAD;
	else if (o_ptr->artifact)
		msg_type = MSG_IDENT_ART;
	else if (o_ptr->ego)
		msg_type = MSG_IDENT_EGO;
	else
		msg_type = MSG_GENERIC;

	/* Log artifacts to the history list. */
	if (o_ptr->artifact)
		history_add_artifact(o_ptr->artifact, TRUE, TRUE);

	/* Describe */
	index = object_gear_index(player, o_ptr);
	slot = equipped_item_slot(player->body, index);
	if (item_is_equipped(player, index)) {
		/* Format and capitalise */
		char *msg = format("%s: %s (%c).", equip_describe(player, slot),
						   o_name, equip_to_label(slot));
		my_strcap(msg);

		msgt(msg_type, msg);
	} else if (index != NO_OBJECT)
		msgt(msg_type, "In your pack: %s (%c).", o_name, gear_to_label(index));
	else
		msgt(msg_type, "On the ground: %s.", o_name);
}
