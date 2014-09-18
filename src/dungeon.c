/*
 * File: dungeon.c
 * Purpose: The game core bits, shared across platforms.
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
#include "birth.h"
#include "cave.h"
#include "cmds.h"
#include "dungeon.h"
#include "files.h"
#include "game-event.h"
#include "generate.h"
#include "grafmode.h"
#include "init.h"
#include "mon-list.h"
#include "mon-make.h"
#include "mon-spell.h"
#include "mon-util.h"
#include "monster.h"
#include "obj-desc.h"
#include "obj-gear.h"
#include "obj-identify.h"
#include "obj-randart.h"
#include "obj-tval.h"
#include "obj-util.h"
#include "object.h"
#include "pathfind.h"
#include "player-timed.h"
#include "player-util.h"
#include "prefs.h"
#include "savefile.h"
#include "spells.h"
#include "store.h"
#include "tables.h"
#include "target.h"
#include "ui-game.h"
#include "ui-input.h"
#include "ui-map.h"
#include "ui.h"

#include <math.h>

/* The minimum amount of energy a player has at the start of a new level */
#define INITIAL_DUNGEON_ENERGY 100

u16b daycount = 0;
u32b seed_randart;		/* Hack -- consistent random artifacts */
u32b seed_flavor;		/* Hack -- consistent object colors */
s32b turn;				/* Current game turn */
bool character_generated;	/* The character exists */
bool character_dungeon;		/* The character has a dungeon */
bool character_saved;		/* The character was just saved to a savefile */
s16b character_xtra;		/* Depth of the game in startup mode */

/*
 * Say whether it's daytime or not
 */
bool is_daytime(void)
{
	if ((turn % (10L * TOWN_DAWN)) < ((10L * TOWN_DAWN) / 2)) 
		return FALSE;

	return TRUE;
} 


/*
 * Change dungeon level - e.g. by going up stairs or with WoR.
 */
void dungeon_change_level(int dlev)
{
	/* New depth */
	player->depth = dlev;

	/* If we're returning to town, update the store contents
	   according to how long we've been away */
	if (!dlev && daycount)
	{
		if (OPT(cheat_xtra)) msg("Updating Shops...");
		while (daycount--)
		{
			int n;

			/* Maintain each shop (except home) */
			for (n = 0; n < MAX_STORES; n++)
			{
				/* Skip the home */
				if (n == STORE_HOME) continue;

				/* Maintain */
				store_maint(&stores[n]);
			}

			/* Sometimes, shuffle the shop-keepers */
			if (one_in_(STORE_SHUFFLE))
			{
				/* Message */
				if (OPT(cheat_xtra)) msg("Shuffling a Shopkeeper...");

				/* Pick a random shop (except home) */
				while (1)
				{
					n = randint0(MAX_STORES);
					if (n != STORE_HOME) break;
				}

				/* Shuffle it */
				store_shuffle(&stores[n]);
			}
		}
		daycount = 0;
		if (OPT(cheat_xtra)) msg("Done.");
	}

	/* Leaving */
	player->upkeep->leaving = TRUE;

	/* Save the game when we arrive on the new level. */
	player->upkeep->autosave = TRUE;
}


/*
 * Regenerate hit points
 */
static void regenhp(int percent)
{
	s32b new_chp, new_chp_frac;
	int old_chp;

	/* Save the old hitpoints */
	old_chp = player->chp;

	/* Extract the new hitpoints */
	new_chp = ((long)player->mhp) * percent + PY_REGEN_HPBASE;
	player->chp += (s16b)(new_chp >> 16);   /* div 65536 */

	/* check for overflow */
	if ((player->chp < 0) && (old_chp > 0)) player->chp = MAX_SHORT;
	new_chp_frac = (new_chp & 0xFFFF) + player->chp_frac;	/* mod 65536 */
	if (new_chp_frac >= 0x10000L)
	{
		player->chp_frac = (u16b)(new_chp_frac - 0x10000L);
		player->chp++;
	}
	else
	{
		player->chp_frac = (u16b)new_chp_frac;
	}

	/* Fully healed */
	if (player->chp >= player->mhp)
	{
		player->chp = player->mhp;
		player->chp_frac = 0;
	}

	/* Notice changes */
	if (old_chp != player->chp)
	{
		/* Redraw */
		player->upkeep->redraw |= (PR_HP);
		wieldeds_notice_flag(player, OF_REGEN);
		wieldeds_notice_flag(player, OF_IMPAIR_HP);
	}
}


/*
 * Regenerate mana points
 */
static void regenmana(int percent)
{
	s32b new_mana, new_mana_frac;
	int old_csp;

	old_csp = player->csp;
	new_mana = ((long)player->msp) * percent + PY_REGEN_MNBASE;
	player->csp += (s16b)(new_mana >> 16);	/* div 65536 */
	/* check for overflow */
	if ((player->csp < 0) && (old_csp > 0))
	{
		player->csp = MAX_SHORT;
	}
	new_mana_frac = (new_mana & 0xFFFF) + player->csp_frac;	/* mod 65536 */
	if (new_mana_frac >= 0x10000L)
	{
		player->csp_frac = (u16b)(new_mana_frac - 0x10000L);
		player->csp++;
	}
	else
	{
		player->csp_frac = (u16b)new_mana_frac;
	}

	/* Must set frac to zero even if equal */
	if (player->csp >= player->msp)
	{
		player->csp = player->msp;
		player->csp_frac = 0;
	}

	/* Redraw mana */
	if (old_csp != player->csp)
	{
		/* Redraw */
		player->upkeep->redraw |= (PR_MANA);
		wieldeds_notice_flag(player, OF_REGEN);
		wieldeds_notice_flag(player, OF_IMPAIR_MANA);
	}
}






/*
 * Regenerate the monsters (once per 100 game turns)
 *
 * XXX XXX XXX Should probably be done during monster turns.
 */
static void regen_monsters(void)
{
	int i, frac;

	/* Regenerate everyone */
	for (i = 1; i < cave_monster_max(cave); i++)
	{
		/* Check the i'th monster */
		monster_type *m_ptr = cave_monster(cave, i);

		/* Skip dead monsters */
		if (!m_ptr->race) continue;

		/* Allow regeneration (if needed) */
		if (m_ptr->hp < m_ptr->maxhp)
		{
			/* Hack -- Base regeneration */
			frac = m_ptr->maxhp / 100;

			/* Hack -- Minimal regeneration rate */
			if (!frac) frac = 1;

			/* Hack -- Some monsters regenerate quickly */
			if (rf_has(m_ptr->race->flags, RF_REGENERATE)) frac *= 2;

			/* Hack -- Regenerate */
			m_ptr->hp += frac;

			/* Do not over-regenerate */
			if (m_ptr->hp > m_ptr->maxhp) m_ptr->hp = m_ptr->maxhp;

			/* Redraw (later) if needed */
			if (player->upkeep->health_who == m_ptr) 
				player->upkeep->redraw |= (PR_HEALTH);
		}
	}
}


/*
 * If player has inscribed the object with "!!", let him know when it's
 * recharged. -LM-
 * Also inform player when first item of a stack has recharged. -HK-
 * Notify all recharges w/o inscription if notify_recharge option set -WP-
 */
static void recharged_notice(const object_type *o_ptr, bool all)
{
	char o_name[120];

	const char *s;

	bool notify = FALSE;

	if (OPT(notify_recharge))
	{
		notify = TRUE;
	}
	else if (o_ptr->note)
	{
		/* Find a '!' */
		s = strchr(quark_str(o_ptr->note), '!');

		/* Process notification request */
		while (s)
		{
			/* Find another '!' */
			if (s[1] == '!')
			{
				notify = TRUE;
				break;
			}

			/* Keep looking for '!'s */
			s = strchr(s + 1, '!');
		}
	}

	if (!notify) return;


	/* Describe (briefly) */
	object_desc(o_name, sizeof(o_name), o_ptr, ODESC_BASE);

	/* Disturb the player */
	disturb(player, 0);

	/* Notify the player */
	if (o_ptr->number > 1)
	{
		if (all) msg("Your %s have recharged.", o_name);
		else msg("One of your %s has recharged.", o_name);
	}

	/* Artifacts */
	else if (o_ptr->artifact)
	{
		msg("The %s has recharged.", o_name);
	}

	/* Single, non-artifact items */
	else msg("Your %s has recharged.", o_name);
}


/*
 * Recharge activatable objects in the player's equipment
 * and rods in the inventory and on the ground.
 */
static void recharge_objects(void)
{
	int i;

	bool discharged_stack;

	object_type *o_ptr;

	/* Recharge carried gear */
	for (i = 0; i < player->max_gear; i++)
	{
		o_ptr = &player->gear[i];

		/* Skip non-objects */
		if (!o_ptr->kind) continue;

		/* Recharge equipment */
		if (item_is_equipped(player, i)) {
			/* Recharge activatable objects */
			if (recharge_timeout(o_ptr)) {
				/* Message if an item recharged */
				recharged_notice(o_ptr, TRUE);

				/* Window stuff */
				player->upkeep->redraw |= (PR_EQUIP);
			}
		}
		/* Recharge the inventory */
		else {
			discharged_stack =
				(number_charging(o_ptr) == o_ptr->number) ? TRUE : FALSE;

			/* Recharge rods, and update if any rods are recharged */
			if (tval_can_have_timeout(o_ptr) && recharge_timeout(o_ptr)) {
				/* Entire stack is recharged */
				if (o_ptr->timeout == 0)
					recharged_notice(o_ptr, TRUE);

				/* Previously exhausted stack has acquired a charge */
				else if (discharged_stack)
					recharged_notice(o_ptr, FALSE);

				/* Combine pack */
				player->upkeep->notice |= (PN_COMBINE);

				/* Redraw stuff */
				player->upkeep->redraw |= (PR_INVEN);
			}
		}
	}

	/* Recharge the ground */
	for (i = 1; i < cave_object_max(cave); i++)
	{
		/* Get the object */
		o_ptr = cave_object(cave, i);

		/* Skip dead objects */
		if (!o_ptr->kind) continue;

		/* Recharge rods on the ground */
		if (tval_can_have_timeout(o_ptr))
			recharge_timeout(o_ptr);
	}
}


static void play_ambient_sound(void)
{
	/* Town sound */
	if (player->depth == 0) 
	{
		/* Hack - is it daytime or nighttime? */
		if (turn % (10L * TOWN_DAWN) < TOWN_DAWN / 2)
		{
			/* It's day. */
			sound(MSG_AMBIENT_DAY);
		} 
		else 
		{
			/* It's night. */
			sound(MSG_AMBIENT_NITE);
		}
		
	}

	/* Dungeon level 1-20 */
	else if (player->depth <= 20) 
	{
		sound(MSG_AMBIENT_DNG1);
	}

	/* Dungeon level 21-40 */
	else if (player->depth <= 40) 
	{
		sound(MSG_AMBIENT_DNG2);
	}

	/* Dungeon level 41-60 */
	else if (player->depth <= 60) 
	{
		sound(MSG_AMBIENT_DNG3);
	}

	/* Dungeon level 61-80 */
	else if (player->depth <= 80)
	{
		sound(MSG_AMBIENT_DNG4);
	}

	/* Dungeon level 80- */
	else
	{
		sound(MSG_AMBIENT_DNG5);
	}
}

/*
 * Helper for process_world -- decrement player->timed[] fields.
 */
static void decrease_timeouts(void)
{
	int adjust = (adj_con_fix[player->state.stat_ind[STAT_CON]] + 1);
	int i;

	/* Decrement all effects that can be done simply */
	for (i = 0; i < TMD_MAX; i++)
	{
		int decr = 1;
		if (!player->timed[i])
			continue;

		switch (i)
		{
			case TMD_CUT:
			{
				/* Hack -- check for truly "mortal" wound */
				decr = (player->timed[i] > 1000) ? 0 : adjust;
				break;
			}

			case TMD_POISONED:
			case TMD_STUN:
			{
				decr = adjust;
				break;
			}
		}
		/* Decrement the effect */
		player_dec_timed(player, i, decr, FALSE);
	}

	return;
}


/*
 * Handle certain things once every 10 game turns
 */
static void process_world(struct chunk *c)
{
	int i;

	int regen_amount;

	object_type *o_ptr;

	/* Every 10 game turns */
	if (turn % 10) return;


	/*** Check the Time ***/

	/* Play an ambient sound at regular intervals. */
	if (!(turn % ((10L * TOWN_DAWN) / 4)))
	{
		play_ambient_sound();
	}

	/*** Handle the "town" (stores and sunshine) ***/

	/* While in town */
	if (!player->depth)
	{
		/* Hack -- Daybreak/Nighfall in town */
		if (!(turn % ((10L * TOWN_DAWN) / 2)))
		{
			bool dawn;

			/* Check for dawn */
			dawn = (!(turn % (10L * TOWN_DAWN)));

			/* Day breaks */
			if (dawn)
				msg("The sun has risen.");

			/* Night falls */
			else
				msg("The sun has fallen.");

			/* Illuminate */
			cave_illuminate(c, dawn);
		}
	}


	/* While in the dungeon */
	else
	{
		/* Update the stores once a day (while in the dungeon).
		   The changes are not actually made until return to town,
		   to avoid giving details away in the knowledge menu. */
		if (!(turn % (10L * STORE_TURNS))) daycount++;
	}


	/*** Process the monsters ***/

	/* Check for creature generation */
	if (one_in_(MAX_M_ALLOC_CHANCE))
	{
		/* Make a new monster */
		(void)pick_and_place_distant_monster(cave, loc(player->px, player->py), MAX_SIGHT + 5, TRUE, player->depth);
	}

	/* Hack -- Check for creature regeneration */
	if (!(turn % 100)) regen_monsters();


	/*** Damage over Time ***/

	/* Take damage from poison */
	if (player->timed[TMD_POISONED])
	{
		/* Take damage */
		take_hit(player, 1, "poison");
	}

	/* Take damage from cuts */
	if (player->timed[TMD_CUT])
	{
		/* Mortal wound or Deep Gash */
		if (player->timed[TMD_CUT] > 200)
			i = 3;

		/* Severe cut */
		else if (player->timed[TMD_CUT] > 100)
			i = 2;

		/* Other cuts */
		else
			i = 1;

		/* Take damage */
		take_hit(player, i, "a fatal wound");
	}


	/*** Check the Food, and Regenerate ***/

	/* Digest normally */
	if (!(turn % 100))
	{
		/* Basic digestion rate based on speed */
		i = extract_energy[player->state.speed] * 2;

		/* Regeneration takes more food */
		if (player_of_has(player, OF_REGEN)) i += 30;

		/* Slow digestion takes less food */
		if (player_of_has(player, OF_SLOW_DIGEST)) i /= 5;

		/* Minimal digestion */
		if (i < 1) i = 1;

		/* Digest some food */
		player_set_food(player, player->food - i);
	}

	/* Getting Faint */
	if (player->food < PY_FOOD_FAINT)
	{
		/* Faint occasionally */
		if (!player->timed[TMD_PARALYZED] && one_in_(10))
		{
			/* Message */
			msg("You faint from the lack of food.");
			disturb(player, 1);

			/* Faint (bypass free action) */
			(void)player_inc_timed(player, TMD_PARALYZED, 1 + randint0(5), TRUE, FALSE);
		}
	}


	/* Starve to death (slowly) */
	if (player->food < PY_FOOD_STARVE)
	{
		/* Calculate damage */
		i = (PY_FOOD_STARVE - player->food) / 10;

		/* Take damage */
		take_hit(player, i, "starvation");
	}

	/** Regenerate HP **/

	/* Default regeneration */
	if (player->food >= PY_FOOD_WEAK)
		regen_amount = PY_REGEN_NORMAL;
	else if (player->food < PY_FOOD_STARVE)
		regen_amount = 0;
	else if (player->food < PY_FOOD_FAINT)
		regen_amount = PY_REGEN_FAINT;
	else /* if (player->food < PY_FOOD_WEAK) */
		regen_amount = PY_REGEN_WEAK;

	/* Various things speed up regeneration */
	if (player_of_has(player, OF_REGEN))
		regen_amount *= 2;
	if (player->searching || player_resting_can_regenerate(player))
		regen_amount *= 2;

	/* Some things slow it down */
	if (player_of_has(player, OF_IMPAIR_HP))
		regen_amount /= 2;

	/* Various things interfere with physical healing */
	if (player->timed[TMD_PARALYZED]) regen_amount = 0;
	if (player->timed[TMD_POISONED]) regen_amount = 0;
	if (player->timed[TMD_STUN]) regen_amount = 0;
	if (player->timed[TMD_CUT]) regen_amount = 0;

	/* Regenerate Hit Points if needed */
	if (player->chp < player->mhp)
		regenhp(regen_amount);


	/** Regenerate SP **/

	/* Default regeneration */
	regen_amount = PY_REGEN_NORMAL;

	/* Various things speed up regeneration */
	if (player_of_has(player, OF_REGEN))
		regen_amount *= 2;
	if (player->searching || player_resting_can_regenerate(player))
		regen_amount *= 2;

	/* Some things slow it down */
	if (player_of_has(player, OF_IMPAIR_MANA))
		regen_amount /= 2;

	/* Regenerate mana */
	if (player->csp < player->msp)
		regenmana(regen_amount);



	/*** Timeout Various Things ***/

	decrease_timeouts();



	/*** Process Light ***/

	/* Check for light being wielded */
	o_ptr = equipped_item_by_slot_name(player, "light");;

	/* Burn some fuel in the current light */
	if (tval_is_light(o_ptr)) {
		bool burn_fuel = TRUE;

		/* Turn off the wanton burning of light during the day in the town */
		if (!player->depth && ((turn % (10L * TOWN_DAWN)) < ((10L * TOWN_DAWN) / 2)))
			burn_fuel = FALSE;

		/* If the light has the NO_FUEL flag, well... */
		if (of_has(o_ptr->flags, OF_NO_FUEL))
		    burn_fuel = FALSE;

		/* Use some fuel (except on artifacts, or during the day) */
		if (burn_fuel && o_ptr->timeout > 0) {
			/* Decrease life-span */
			o_ptr->timeout--;

			/* Hack -- notice interesting fuel steps */
			if ((o_ptr->timeout < 100) || (!(o_ptr->timeout % 100)))
				/* Redraw stuff */
				player->upkeep->redraw |= (PR_EQUIP);

			/* Hack -- Special treatment when blind */
			if (player->timed[TMD_BLIND]) {
				/* Hack -- save some light for later */
				if (o_ptr->timeout == 0) o_ptr->timeout++;

			/* The light is now out */
			} else if (o_ptr->timeout == 0) {
				disturb(player, 0);
				msg("Your light has gone out!");

				/* If it's a torch, now is the time to delete it */
				if (of_has(o_ptr->flags, OF_BURNS_OUT)) {
					inven_item_increase(object_gear_index(player, o_ptr), -1);
					inven_item_optimize(object_gear_index(player, o_ptr));
				}
			}

			/* The light is getting dim */
			else if ((o_ptr->timeout < 50) && (!(o_ptr->timeout % 20))) {
				disturb(player, 0);
				msg("Your light is growing faint.");
			}
		}
	}

	/* Calculate torch radius */
	player->upkeep->update |= (PU_TORCH);


	/*** Process Inventory ***/

	/* Handle experience draining */
	if (player_of_has(player, OF_DRAIN_EXP))
	{
		if ((player->exp > 0) && one_in_(10)) {
			s32b d = damroll(10, 6) + (player->exp/100) * MON_DRAIN_LIFE;
			player_exp_lose(player, d / 10, FALSE);
		}

		wieldeds_notice_flag(player, OF_DRAIN_EXP);
	}

	/* Recharge activatable objects and rods */
	recharge_objects();

	/* Feel the inventory */
	sense_inventory();


	/*** Involuntary Movement ***/

	/* Random teleportation */
	if (player_of_has(player, OF_TELEPORT) && one_in_(50))
	{
		wieldeds_notice_flag(player, OF_TELEPORT);
		teleport_player(40);
		disturb(player, 0);
	}

	/* Delayed Word-of-Recall */
	if (player->word_recall)
	{
		/* Count down towards recall */
		player->word_recall--;

		/* Activate the recall */
		if (!player->word_recall)
		{
			/* Disturbing! */
			disturb(player, 0);

			/* Determine the level */
			if (player->depth)
			{
				msgt(MSG_TPLEVEL, "You feel yourself yanked upwards!");
				dungeon_change_level(0);
			}
			else
			{
				msgt(MSG_TPLEVEL, "You feel yourself yanked downwards!");
                
                /* Force descent to a lower level if allowed */
                if (OPT(birth_force_descend) && player->max_depth < MAX_DEPTH - 1
                  && !is_quest(player->max_depth)){

                    player->max_depth = player->max_depth + 1;
                }

				/* New depth - back to max depth or 1, whichever is deeper */
				dungeon_change_level(player->max_depth < 1 ? 1: player->max_depth);
			}
		}
	}

	/* Delayed Deep Descent */
	if (player->deep_descent) {
		/* Count down towards recall */
		player->deep_descent--;

		/* Activate the recall */
		if (player->deep_descent == 0) {
			int target_depth = player->max_depth;

			/* Calculate target depth */
			for (i = 5; i > 0; i--) {
				if (is_quest(target_depth)) break;
				if (target_depth >= MAX_DEPTH - 1) break;
				
				target_depth++;
			}

			disturb(player, 0);

			/* Determine the level */
			if (target_depth > player->depth) {
				msgt(MSG_TPLEVEL, "The floor opens beneath you!");
				dungeon_change_level(target_depth);
			} else {
				/* Otherwise do something disastrous */
				msgt(MSG_TPLEVEL, "You are thrown back in an explosion!");
				effect_simple(EF_DESTRUCTION, "0", 0, 5, 0, NULL);
			}		
		}
	}
}





/*
 * Hack -- helper function for "process_player()"
 *
 * Check for changes in the "monster memory"
 */
static void process_player_aux(void)
{
	int i;
	bool changed = FALSE;

	static monster_race *old_monster_race = 0;
	static bitflag old_flags[RF_SIZE];
	static bitflag old_spell_flags[RSF_SIZE];

	static byte old_blows[MONSTER_BLOW_MAX];

	static byte	old_cast_innate = 0;
	static byte	old_cast_spell = 0;

	/* Tracking a monster */
	if (player->upkeep->monster_race)
	{
		/* Get the monster lore */
		monster_lore *l_ptr = get_lore(player->upkeep->monster_race);

		for (i = 0; i < MONSTER_BLOW_MAX; i++)
		{
			if (old_blows[i] != l_ptr->blows[i])
			{
				changed = TRUE;
				break;
			}
		}

		/* Check for change of any kind */
		if (changed ||
		    (old_monster_race != player->upkeep->monster_race) ||
		    !rf_is_equal(old_flags, l_ptr->flags) ||
		    !rsf_is_equal(old_spell_flags, l_ptr->spell_flags) ||
		    (old_cast_innate != l_ptr->cast_innate) ||
		    (old_cast_spell != l_ptr->cast_spell))
		{
			/* Memorize old race */
			old_monster_race = player->upkeep->monster_race;

			/* Memorize flags */
			rf_copy(old_flags, l_ptr->flags);
			rsf_copy(old_spell_flags, l_ptr->spell_flags);

			/* Memorize blows */
			memmove(old_blows, l_ptr->blows, sizeof(byte)*MONSTER_BLOW_MAX);

			/* Memorize castings */
			old_cast_innate = l_ptr->cast_innate;
			old_cast_spell = l_ptr->cast_spell;

			/* Redraw stuff */
			player->upkeep->redraw |= (PR_MONSTER);
			redraw_stuff(player->upkeep);
		}
	}
}


/*
 * Place cursor on a monster or the player.
 */
static void place_cursor(void) {
	if (OPT(show_target) && target_sighted()) {
		s16b col, row;
		target_get(&col, &row);
		move_cursor_relative(row, col);
	}
}


/*
 * Process the player
 *
 * Notice the annoying code to handle "pack overflow", which
 * must come first just in case somebody manages to corrupt
 * the savefiles by clever use of menu commands or something.
 *
 * Notice the annoying code to handle "monster memory" changes,
 * which allows us to avoid having to update the window flags
 * every time we change any internal monster memory field, and
 * also reduces the number of times that the recall window must
 * be redrawn.
 *
 * Note that the code to check for user abort during repeated commands
 * and running and resting can be disabled entirely with an option, and
 * even if not disabled, it will only check during every 128th game turn
 * while resting, for efficiency.
 */
static void process_player(void)
{
	int i;

	/*** Check for interrupts ***/

	player_resting_complete_special(player);

	/* Check for "player abort" */
	if (player->upkeep->running ||
	    cmd_get_nrepeats() > 0 ||
	    (player_is_resting(player) && !(turn & 0x7F)))
	{
		ui_event e;

		/* Do not wait */
		inkey_scan = SCAN_INSTANT;

		/* Check for a key */
		e = inkey_ex();
		if (e.type != EVT_NONE) {
			/* Flush and disturb */
			flush();
			disturb(player, 0);
			msg("Cancelled.");
		}
	}


	/*** Handle actual user input ***/

	/* Repeat until energy is reduced */
	do
	{
		/* Notice stuff (if needed) */
		if (player->upkeep->notice) notice_stuff(player->upkeep);

		/* Update stuff (if needed) */
		if (player->upkeep->update) update_stuff(player->upkeep);

		/* Redraw stuff (if needed) */
		if (player->upkeep->redraw) redraw_stuff(player->upkeep);


		/* Place cursor on player/target */
		place_cursor();


		/* Refresh (optional) */
		Term_fresh();

		/* Hack -- Pack Overflow */
		pack_overflow();

		/* Assume free turn */
		player->upkeep->energy_use = 0;

		/* Dwarves detect treasure */
		if (player_has(PF_SEE_ORE))
		{
			/* Only if they are in good shape */
			if (!player->timed[TMD_IMAGE] &&
					!player->timed[TMD_CONFUSED] &&
					!player->timed[TMD_AMNESIA] &&
					!player->timed[TMD_STUN] &&
					!player->timed[TMD_PARALYZED] &&
					!player->timed[TMD_TERROR] &&
					!player->timed[TMD_AFRAID])
				effect_simple(EF_DETECT_GOLD, "3d3", 1, 0, 0, NULL);
		}

		/* Rogues detect traps */
		if (player_has(PF_SEE_TRAPS))
		{
			/* Only if they are in good shape */
			if (!player->timed[TMD_IMAGE] &&
					!player->timed[TMD_CONFUSED] &&
					!player->timed[TMD_AMNESIA] &&
					!player->timed[TMD_STUN] &&
					!player->timed[TMD_PARALYZED] &&
					!player->timed[TMD_TERROR] &&
					!player->timed[TMD_AFRAID]) {
				if (player->lev >= 5) {
					int rad = (int)floor(player->lev/5);
					char buf[7];
					sprintf(buf,"%dd%d",rad,rad);
					effect_simple(EF_DETECT_TRAPS, buf, 1, 1, 0, NULL);
					effect_simple(EF_DETECT_DOORS, buf, 1, 1, 0, NULL);
				}
			}
				
		}

		/* Paralyzed or Knocked Out */
		if ((player->timed[TMD_PARALYZED]) || (player->timed[TMD_STUN] >= 100))
		{
			/* Take a turn */
			player->upkeep->energy_use = 100;
		}

		/* Picking up objects */
		else if (player->upkeep->notice & PN_PICKUP)
		{
			player->upkeep->energy_use = do_autopickup() * 10;
			if (player->upkeep->energy_use > 100)
				player->upkeep->energy_use = 100;
			player->upkeep->notice &= ~(PN_PICKUP);
			
			/* Appropriate time for the player to see objects */
			event_signal(EVENT_SEEFLOOR);
		}

		/* Resting */
		else if (player_is_resting(player))
		{
			player_resting_step_turn(player);
		}

		/* Running */
		else if (player->upkeep->running)
		{
			/* Take a step */
			run_step(0);
		}

		/* Repeated command */
		else if (cmd_get_nrepeats() > 0)
		{
			/* Hack -- Assume messages were seen */
			msg_flag = FALSE;

			/* Clear the top line */
			prt("", 0, 0);

			/* Process the command */
			process_command(CMD_GAME, TRUE);
		}

		/* Normal command */
		else
		{
			/* Check monster recall */
			process_player_aux();

			/* Place cursor on player/target */
			place_cursor();

			/* Get and process a command */
			process_command(CMD_GAME, FALSE);

			/* Mega hack - redraw if big graphics - sorry NRM */
			if ((tile_width > 1) || (tile_height > 1)) 
			        player->upkeep->redraw |= (PR_MAP);
		}


		/*** Clean up ***/

		/* Significant */
		if (player->upkeep->energy_use)
		{
			/* Use some energy */
			/* Apply energy use to bonus move first, if we have it, then to our actual energy *
			if (player->state.bonus_move) {
				if (player->state.bonus_move > player->upkeep->energy_use) {
					player->state.bonus_move -= player->upkeep->energy_use;
				} else {
					player->energy -= (player->upkeep->energy_use - player->state.bonus_move);
					player->state.bonus_move = 0;
				}
			} else {
				player->energy -= player->upkeep->energy_use;
			}*/
			player->energy -= player->upkeep->energy_use;

			/* Increment the total energy counter */
			player->total_energy += player->upkeep->energy_use;
			
			/* Hack -- If we have bonus move (single or double), apply it appropriately *
			if (player->timed[TMD_BONUS_MOVE] || player->timed[TMD_BONUS_MOVE_2X]) {
				player->state.bonus_move = extract_energy[player->state.speed];
				if (player->timed[TMD_BONUS_MOVE_2X]) {
					player->state.bonus_move += extract_energy[player->state.speed];
				}
				player->upkeep->update |= PU_BONUS;
			}*/

			/* Hack -- constant hallucination */
			if (player->timed[TMD_IMAGE])
			{
				player->upkeep->redraw |= (PR_MAP);
			}

			/* Shimmer multi-hued monsters */
			for (i = 1; i < cave_monster_max(cave); i++)
			{
				struct monster *mon = cave_monster(cave, i);
				if (!mon->race)
					continue;
				if (!rf_has(mon->race->flags, RF_ATTR_MULTI))
					continue;
				square_light_spot(cave, mon->fy, mon->fx);
			}

			/* Clear NICE flag, and show marked monsters */
			for (i = 1; i < cave_monster_max(cave); i++)
			{
				struct monster *mon = cave_monster(cave, i);
				mon->mflag &= ~MFLAG_NICE;
				if (mon->mflag & MFLAG_MARK) {
					if (!(mon->mflag & MFLAG_SHOW)) {
						mon->mflag &= ~MFLAG_MARK;
						update_mon(mon, FALSE);
					}
				}
			}
		}

		/* Clear SHOW flag */
		for (i = 1; i < cave_monster_max(cave); i++)
		{
			struct monster *mon = cave_monster(cave, i);
			mon->mflag &= ~MFLAG_SHOW;
		}

		/* HACK: This will redraw the itemlist too frequently, but I'm don't
		   know all the individual places it should go. */
		player->upkeep->redraw |= PR_ITEMLIST;
	}

	while (!player->upkeep->energy_use && !player->upkeep->leaving);

	/* Notice stuff (if needed) */
	if (player->upkeep->notice) notice_stuff(player->upkeep);
}

static byte flicker = 0;
static byte color_flicker[MAX_COLORS][3] = 
{
	{TERM_DARK, TERM_L_DARK, TERM_L_RED},
	{TERM_WHITE, TERM_L_WHITE, TERM_L_BLUE},
	{TERM_SLATE, TERM_WHITE, TERM_L_DARK},
	{TERM_ORANGE, TERM_YELLOW, TERM_L_RED},
	{TERM_RED, TERM_L_RED, TERM_L_PINK},
	{TERM_GREEN, TERM_L_GREEN, TERM_L_TEAL},
	{TERM_BLUE, TERM_L_BLUE, TERM_SLATE},
	{TERM_UMBER, TERM_L_UMBER, TERM_MUSTARD},
	{TERM_L_DARK, TERM_SLATE, TERM_L_VIOLET},
	{TERM_WHITE, TERM_SLATE, TERM_L_WHITE},
	{TERM_L_PURPLE, TERM_PURPLE, TERM_L_VIOLET},
	{TERM_YELLOW, TERM_L_YELLOW, TERM_MUSTARD},
	{TERM_L_RED, TERM_RED, TERM_L_PINK},
	{TERM_L_GREEN, TERM_L_TEAL, TERM_GREEN},
	{TERM_L_BLUE, TERM_DEEP_L_BLUE, TERM_BLUE_SLATE},
	{TERM_L_UMBER, TERM_UMBER, TERM_MUD},
	{TERM_PURPLE, TERM_VIOLET, TERM_MAGENTA},
	{TERM_VIOLET, TERM_L_VIOLET, TERM_MAGENTA},
	{TERM_TEAL, TERM_L_TEAL, TERM_L_GREEN},
	{TERM_MUD, TERM_YELLOW, TERM_UMBER},
	{TERM_L_YELLOW, TERM_WHITE, TERM_L_UMBER},
	{TERM_MAGENTA, TERM_L_PINK, TERM_L_RED},
	{TERM_L_TEAL, TERM_L_WHITE, TERM_TEAL},
	{TERM_L_VIOLET, TERM_L_PURPLE, TERM_VIOLET},
	{TERM_L_PINK, TERM_L_RED, TERM_L_WHITE},
	{TERM_MUSTARD, TERM_YELLOW, TERM_UMBER},
	{TERM_BLUE_SLATE, TERM_BLUE, TERM_SLATE},
	{TERM_DEEP_L_BLUE, TERM_L_BLUE, TERM_BLUE},
};

static byte get_flicker(byte a)
{
	switch(flicker % 3)
	{
		case 1: return color_flicker[a][1];
		case 2: return color_flicker[a][2];
	}
	return a;
}

/*
 * This animates monsters and/or items as necessary.
 */
static void do_animation(void)
{
	int i;

	for (i = 1; i < cave_monster_max(cave); i++)
	{
		byte attr;
		monster_type *m_ptr = cave_monster(cave, i);

		if (!m_ptr || !m_ptr->race || !m_ptr->ml)
			continue;
		else if (rf_has(m_ptr->race->flags, RF_ATTR_MULTI))
			attr = randint1(BASIC_COLORS - 1);
		else if (rf_has(m_ptr->race->flags, RF_ATTR_FLICKER))
			attr = get_flicker(m_ptr->race->x_attr);
		else
			continue;

		m_ptr->attr = attr;
		player->upkeep->redraw |= (PR_MAP | PR_MONLIST);
	}

	flicker++;
}


/*
 * This is used when the user is idle to allow for simple animations.
 * Currently the only thing it really does is animate shimmering monsters.
 */
void idle_update(void)
{
	if (!character_dungeon) return;

	if (!OPT(animate_flicker) || (use_graphics != GRAPHICS_NONE)) return;

	/* Animate and redraw if necessary */
	do_animation();
	redraw_stuff(player->upkeep);

	/* Refresh the main screen */
	Term_fresh();
}


/*
 * Interact with the current dungeon level.
 *
 * This function will not exit until the level is completed,
 * the user dies, or the game is terminated.
 */
static void dungeon(struct chunk *c)
{
	monster_type *m_ptr;
	int i;



	/* Hack -- enforce illegal panel */
	Term->offset_y = DUNGEON_HGT;
	Term->offset_x = DUNGEON_WID;


	/* Not leaving */
	player->upkeep->leaving = FALSE;


	/* Cancel the target */
	target_set_monster(0);

	/* Cancel the health bar */
	health_track(player->upkeep, NULL);

	/* Disturb */
	disturb(player, 1);

	/*
	 * Because changing levels doesn't take a turn and PR_MONLIST might not be
	 * set for a few game turns, manually force an update on level change.
	 */
	monster_list_force_subwindow_update();

	/* Track maximum player level */
	if (player->max_lev < player->lev)
	{
		player->max_lev = player->lev;
	}


	/* Track maximum dungeon level */
	if (player->max_depth < player->depth)
	{
		player->max_depth = player->depth;
	}

	/* If autosave is pending, do it now. */
	if (player->upkeep->autosave)
	{
		save_game();
		player->upkeep->autosave = FALSE;
	}

	/* Choose panel */
	verify_panel();


	/* Flush messages */
	message_flush();


	/* Hack -- Increase "xtra" depth */
	character_xtra++;


	/* Clear */
	Term_clear();


	/* Update stuff */
	player->upkeep->update |= (PU_BONUS | PU_HP | PU_MANA | PU_SPELLS);

	/* Calculate torch radius */
	player->upkeep->update |= (PU_TORCH);

	/* Update stuff */
	update_stuff(player->upkeep);


	/* Fully update the visuals (and monster distances) */
	player->upkeep->update |= (PU_FORGET_VIEW | PU_UPDATE_VIEW | PU_DISTANCE);

	/* Fully update the flow */
	player->upkeep->update |= (PU_FORGET_FLOW | PU_UPDATE_FLOW);

	/* Redraw dungeon */
	player->upkeep->redraw |= (PR_BASIC | PR_EXTRA | PR_MAP);

	/* Redraw "statusy" things */
	player->upkeep->redraw |= (PR_INVEN | PR_EQUIP | PR_MONSTER | PR_MONLIST | PR_ITEMLIST);

	/* Update stuff */
	update_stuff(player->upkeep);

	/* Redraw stuff */
	redraw_stuff(player->upkeep);


	/* Hack -- Decrease "xtra" depth */
	character_xtra--;


	/* Update stuff */
	player->upkeep->update |= (PU_BONUS | PU_HP | PU_MANA | PU_SPELLS | PU_INVEN);

	/* Combine the pack */
	player->upkeep->notice |= (PN_COMBINE);

	/* Notice stuff */
	notice_stuff(player->upkeep);

	/* Update stuff */
	update_stuff(player->upkeep);

	/* Redraw stuff */
	redraw_stuff(player->upkeep);

	/* Refresh */
	Term_fresh();

	/* Handle delayed death */
	if (player->is_dead) return;

	/* Announce (or repeat) the feeling */
	if (player->depth) display_feeling(FALSE);

	/* Give player minimum energy to start a new level, but do not reduce higher value from savefile for level in progress */
	if (player->energy < INITIAL_DUNGEON_ENERGY)
		player->energy = INITIAL_DUNGEON_ENERGY;


	/*** Process this dungeon level ***/

	/* Main loop */
	while (TRUE)
	{
		/* Hack -- Compact the monster list occasionally */
		if (cave_monster_count(cave) + 32 > z_info->m_max) 
			compact_monsters(64);

		/* Hack -- Compress the monster list occasionally */
		if (cave_monster_count(cave) + 32 < cave_monster_max(cave)) 
			compact_monsters(0);

		/* Hack -- Compact the object list occasionally */
		if (cave_object_count(cave) + 32 > z_info->o_max) 
			compact_objects(64);

		/* Hack -- Compress the object list occasionally */
		if (cave_object_count(cave) + 32 < cave_object_max(cave)) 
			compact_objects(0);

		/* Can the player move? */
		while ((player->energy >= 100) && !player->upkeep->leaving)
		{
    		/* Do any necessary animations */
    		do_animation(); 

			/* process monster with even more energy first */
			process_monsters(c, (byte)(player->energy + 1));

			/* if still alive */
			if (!player->upkeep->leaving)
			{
			        /* Mega hack -redraw big graphics - sorry NRM */
			        if ((tile_width > 1) || (tile_height > 1)) 
				        player->upkeep->redraw |= (PR_MAP);

				/* Process the player */
				process_player();
			}
		}

		/* Notice stuff */
		if (player->upkeep->notice) notice_stuff(player->upkeep);

		/* Update stuff */
		if (player->upkeep->update) update_stuff(player->upkeep);

		/* Redraw stuff */
		if (player->upkeep->redraw) redraw_stuff(player->upkeep);

		/* Place cursor on player/target */
		place_cursor();

		/* Handle "leaving" */
		if (player->upkeep->leaving) break;


		/* Process all of the monsters */
		process_monsters(c, 100);

		/* Notice stuff */
		if (player->upkeep->notice) notice_stuff(player->upkeep);

		/* Update stuff */
		if (player->upkeep->update) update_stuff(player->upkeep);

		/* Redraw stuff */
		if (player->upkeep->redraw) redraw_stuff(player->upkeep);

		/* Place cursor on player/target */
		place_cursor();

		/* Handle "leaving" */
		if (player->upkeep->leaving) break;


		/* Process the world */
		process_world(c);

		/* Notice stuff */
		if (player->upkeep->notice) notice_stuff(player->upkeep);

		/* Update stuff */
		if (player->upkeep->update) update_stuff(player->upkeep);

		/* Redraw stuff */
		if (player->upkeep->redraw) redraw_stuff(player->upkeep);

		/* Place cursor on player/target */
		place_cursor();

		/* Handle "leaving" */
		if (player->upkeep->leaving) break;

		/*** Apply energy ***/

		/* Give the player some energy */
		player->energy += extract_energy[player->state.speed];
		
		/* If we have bonus move (single or double), apply it appropriately *
		if (player->timed[TMD_BONUS_MOVE]) {
			player->state.bonus_move = extract_energy[player->state.speed];
		} else if (player->timed[TMD_BONUS_MOVE_2X]) {
			player->state.bonus_move = 2*extract_energy[player->state.speed];
		}*/

		/* Give energy to all monsters */
		for (i = cave_monster_max(cave) - 1; i >= 1; i--)
		{
			int mspeed;
			
			/* Access the monster (if alive) */
			m_ptr = cave_monster(cave, i);
			if (!m_ptr->race) continue;

			/* Calculate the net speed */
			mspeed = m_ptr->mspeed;
			if (m_ptr->m_timed[MON_TMD_FAST])
				mspeed += 10;
			if (m_ptr->m_timed[MON_TMD_SLOW])
				mspeed -= 10;

			/* Give this monster some energy */
			m_ptr->energy += extract_energy[mspeed];
		}

		/* Count game turns */
		turn++;
	}
}



/*
 * Process some user pref files
 */
static void process_some_user_pref_files(void)
{
	bool found;
	char buf[1024];

	/* Process the "user.prf" file */
	process_pref_file("user.prf", TRUE, TRUE);

	/* Get the "PLAYER.prf" filename */
	strnfmt(buf, sizeof(buf), "%s.prf", player_safe_name(player, TRUE));

	/* Process the "PLAYER.prf" file, using the character name */
	found = process_pref_file(buf, TRUE, TRUE);

    /* Try pref file using savefile name if we fail using character name */
    if (!found) {
		int filename_index = path_filename_index(savefile);
		char filename[128];

		my_strcpy(filename, &savefile[filename_index], sizeof(filename));
		strnfmt(buf, sizeof(buf), "%s.prf", filename);
		process_pref_file(buf, TRUE, TRUE);
    }
}


/*
 * Actually play a game.
 *
 * This function is called from a variety of entry points, since both
 * the standard "main.c" file, as well as several platform-specific
 * "main-xxx.c" files, call this function to start a new game with a
 * new savefile, start a new game with an existing savefile, or resume
 * a saved game with an existing savefile.
 *
 * If the "new_game" parameter is true, and the savefile contains a
 * living character, then that character will be killed, so that the
 * player may start a new game with that savefile.  This is only used
 * by the "-n" option in "main.c".
 *
 * If the savefile does not exist, cannot be loaded, or contains a dead
 * character, then a new game will be started.
 *
 * Several platforms (Windows, Macintosh, Amiga) start brand new games
 * with "savefile" empty, and initialize it later based on the player
 * name.
 *
 * Note that we load the RNG state from savefiles (2.8.0 or later) and
 * so we only initialize it if we were unable to load it.  The loading
 * code marks successful loading of the RNG state using the "Rand_quick"
 * flag, which is a hack, but which optimizes loading of savefiles.
 */
void play_game(void)
{
	u32b default_window_flag[ANGBAND_TERM_MAX];
	/* Initialize */
	bool new_game = init_angband();

	/*** Do horrible, hacky things, to start the game off ***/

	/* Hack -- Increase "icky" depth */
	character_icky++;

	/* Verify main term */
	if (!term_screen)
		quit("main window does not exist");

	/* Make sure main term is active */
	Term_activate(term_screen);

	/* Verify minimum size */
	if ((Term->hgt < 24) || (Term->wid < 80))
		quit("main window is too small");

	/* Hack -- Turn off the cursor */
	(void)Term_set_cursor(FALSE);

	/* initialize window options that will be overridden by the savefile */
	memset(window_flag, 0, sizeof(u32b)*ANGBAND_TERM_MAX);
	memset(default_window_flag, 0, sizeof default_window_flag);
	if (ANGBAND_TERM_MAX > 1) default_window_flag[1] = (PW_MESSAGE);
	if (ANGBAND_TERM_MAX > 2) default_window_flag[2] = (PW_INVEN);
	if (ANGBAND_TERM_MAX > 3) default_window_flag[3] = (PW_MONLIST);
	if (ANGBAND_TERM_MAX > 4) default_window_flag[4] = (PW_ITEMLIST);
	if (ANGBAND_TERM_MAX > 5) default_window_flag[5] = (PW_MONSTER | PW_OBJECT);
	if (ANGBAND_TERM_MAX > 6) default_window_flag[6] = (PW_OVERHEAD);
	if (ANGBAND_TERM_MAX > 7) default_window_flag[7] = (PW_PLAYER_2);

	/* Set up the subwindows */
	subwindows_set_flags(default_window_flag, ANGBAND_TERM_MAX);

	/*** Try to load the savefile ***/

	player->is_dead = TRUE;

	if (savefile[0] && file_exists(savefile)) {
		if (!savefile_load(savefile))
			quit("broken savefile");

		if (player->is_dead && arg_wizard) {
				player->is_dead = FALSE;
				player->chp = player->mhp;
				player->noscore |= NOSCORE_WIZARD;
		}
	}

	/* No living character loaded */
	if (player->is_dead)
	{
		/* Make new player */
		new_game = TRUE;

		/* The dungeon is not ready */
		character_dungeon = FALSE;
	}


	/* Init RNG */
	if (Rand_quick)
	{
		u32b seed;

		/* Basic seed */
		seed = (u32b)(time(NULL));

#ifdef UNIX

		/* Mutate the seed on Unix machines */
		seed = ((seed >> 3) * (getpid() << 1));

#endif

		/* Use the complex RNG */
		Rand_quick = FALSE;

		/* Seed the "complex" RNG */
		Rand_state_init(seed);
	}

	/* Roll new character */
	if (new_game)
	{
		/* The dungeon is not ready */
		character_dungeon = FALSE;

		/* Start in town */
		player->depth = 0;

		/* Seed for flavors */
		seed_flavor = randint0(0x10000000);

		/* Roll up a new character. Quickstart is allowed if ht_birth is set */
		player_birth(player->ht_birth ? TRUE : FALSE);
	}

	/* Seed for random artifacts */
	if (!seed_randart || (new_game && !OPT(birth_keep_randarts)))
		seed_randart = randint0(0x10000000);

	/* Randomize the artifacts if required */
	if (OPT(birth_randarts))
		do_randart(seed_randart, TRUE);

	/* Initialize temporary fields sensibly */
	player->upkeep->object_idx = NO_OBJECT;
	player->upkeep->object_kind = NULL;
	player->upkeep->monster_race = NULL;

	/* Set the savefile name if it's not already set */
	if (!savefile[0])
		savefile_set_name(player_safe_name(player, TRUE));

	/* Stop the player being quite so dead */
	player->is_dead = FALSE;

	/* Flash a message */
	prt("Please wait...", 0, 0);

	/* Allow big cursor */
	smlcurs = FALSE;

	/* Flush the message */
	Term_fresh();

	/* Flavor the objects */
	flavor_init();

	/* Reset visuals */
	reset_visuals(TRUE);

	/* Tell the UI we've started. */
	event_signal(EVENT_ENTER_GAME);

	/* Redraw stuff */
	player->upkeep->redraw |= (PR_INVEN | PR_EQUIP | PR_MONSTER | PR_MESSAGE);
	redraw_stuff(player->upkeep);


	/* Process some user pref files */
	process_some_user_pref_files();


	/* React to changes */
	Term_xtra(TERM_XTRA_REACT, 0);


	/* Generate a dungeon level if needed */
	if (!character_dungeon)
		cave_generate(cave , player);


	/* Character is now "complete" */
	character_generated = TRUE;


	/* Hack -- Decrease "icky" depth */
	character_icky--;


	/* Start playing */
	player->upkeep->playing = TRUE;

	/* Save not required yet. */
	player->upkeep->autosave = FALSE;

	/* Hack -- Enforce "delayed death" */
	if (player->chp < 0) player->is_dead = TRUE;

	/* Process */
	while (TRUE)
	{
		/* Play ambient sound on change of level. */
		play_ambient_sound();

		/* Process the level */
		dungeon(cave);

		/* Notice stuff */
		if (player->upkeep->notice) notice_stuff(player->upkeep);

		/* Update stuff */
		if (player->upkeep->update) update_stuff(player->upkeep);

		/* Redraw stuff */
		if (player->upkeep->redraw) redraw_stuff(player->upkeep);


		/* Cancel the target */
		target_set_monster(0);

		/* Cancel the health bar */
		health_track(player->upkeep, NULL);


		/* Forget the view */
		forget_view(cave);


		/* Handle "quit and save" */
		if (!player->upkeep->playing && !player->is_dead) break;


		/* XXX XXX XXX */
		message_flush();

		/* Accidental Death */
		if (player->upkeep->playing && player->is_dead) {
			/* XXX-elly: this does not belong here. Refactor or
			 * remove. Very similar to do_cmd_wiz_cure_all(). */
			if ((player->wizard || OPT(cheat_live)) && !get_check("Die? ")) {
				/* Mark social class, reset age, if needed */
				player->age = 0;

				/* Increase age */
				player->age++;

				/* Mark savefile */
				player->noscore |= NOSCORE_WIZARD;

				/* Message */
				msg("You invoke wizard mode and cheat death.");
				message_flush();

				/* Cheat death */
				player->is_dead = FALSE;

				/* Restore hit points */
				player->chp = player->mhp;
				player->chp_frac = 0;

				/* Restore spell points */
				player->csp = player->msp;
				player->csp_frac = 0;

				/* Hack -- Healing */
				(void)player_clear_timed(player, TMD_BLIND, TRUE);
				(void)player_clear_timed(player, TMD_CONFUSED, TRUE);
				(void)player_clear_timed(player, TMD_POISONED, TRUE);
				(void)player_clear_timed(player, TMD_AFRAID, TRUE);
				(void)player_clear_timed(player, TMD_PARALYZED, TRUE);
				(void)player_clear_timed(player, TMD_IMAGE, TRUE);
				(void)player_clear_timed(player, TMD_STUN, TRUE);
				(void)player_clear_timed(player, TMD_CUT, TRUE);

				/* Hack -- Prevent starvation */
				player_set_food(player, PY_FOOD_MAX - 1);

				/* Hack -- cancel recall */
				if (player->word_recall)
				{
					/* Message */
					msg("A tension leaves the air around you...");
					message_flush();

					/* Hack -- Prevent recall */
					player->word_recall = 0;
				}

				/* Note cause of death XXX XXX XXX */
				my_strcpy(player->died_from, "Cheating death", sizeof(player->died_from));

				/* New depth */
				player->depth = 0;

				/* Leaving */
				player->upkeep->leaving = TRUE;
			}
		}

		/* Handle "death" */
		if (player->is_dead) break;

		/* Make a new level */
		cave_generate(cave, player);
	}

	/* Disallow big cursor */
	smlcurs = TRUE;

	/* Tell the UI we're done with the game state */
	event_signal(EVENT_LEAVE_GAME);

	/* Close stuff */
	close_game();
}
