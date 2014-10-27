/**
 * \file player-attack.c
 * \brief Attacks (both throwing and melee) by the player
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
#include "cmds.h"
#include "game-event.h"
#include "mon-desc.h"
#include "mon-lore.h"
#include "mon-make.h"
#include "mon-msg.h"
#include "mon-timed.h"
#include "mon-util.h"
#include "monster.h"
#include "obj-desc.h"
#include "obj-gear.h"
#include "obj-identify.h"
#include "obj-slays.h"
#include "obj-ui.h"
#include "obj-util.h"
#include "player-attack.h"
#include "player-util.h"
#include "project.h"
#include "tables.h"
#include "target.h"
#include "ui-map.h"

/**
 * Returns percent chance of an object breaking after throwing or shooting.
 *
 * Artifacts will never break.
 *
 * Beyond that, each item kind has a percent chance to break (0-100). When the
 * object hits its target this chance is used.
 *
 * When an object misses it also has a chance to break. This is determined by
 * squaring the normaly breakage probability. So an item that breaks 100% of
 * the time on hit will also break 100% of the time on a miss, whereas a 50%
 * hit-breakage chance gives a 25% miss-breakage chance, and a 10% hit breakage
 * chance gives a 1% miss-breakage chance.
 */
int breakage_chance(const object_type *o_ptr, bool hit_target) {
	int perc = o_ptr->kind->base->break_perc;

	if (o_ptr->artifact) return 0;
	if (!hit_target) return (perc * perc) / 100;
	return perc;
}

static int chance_of_missile_hit(struct player *p, struct object *missile,
								 struct object *launcher, int y, int x)
{
	bool throw = (launcher ? FALSE : TRUE);
	int bonus = p->state.to_h + missile->to_h;
	int chance;

	if (throw)
		chance = p->state.skills[SKILL_TO_HIT_THROW] + bonus * BTH_PLUS_ADJ;
	else {
		bonus += launcher->to_h;
		chance = player->state.skills[SKILL_TO_HIT_BOW] + bonus * BTH_PLUS_ADJ;
	}

	return chance - distance(p->py, p->px, y, x);
}

/**
 * Determine if the player "hits" a monster.
 */
bool test_hit(int chance, int ac, int vis) {
	int k = randint0(100);

	/* There is an automatic 12% chance to hit,
	 * and 5% chance to miss.
	 */
	if (k < 17) return k < 12;

	/* Penalize invisible targets */
	if (!vis) chance /= 2;

	/* Starting a bit higher up on the scale */
	if (chance < 9) chance = 9;

	/* Power competes against armor */
	return randint0(chance) >= (ac * 2 / 3);
}


/**
 * Determine standard melee damage.
 *
 * Factor in damage dice, to-dam and any brand or slay.
 */
static int melee_damage(struct object *obj, const struct brand *b,
						const struct slay *s)
{
	int dmg = damroll(obj->dd, obj->ds);

	if (s)
		dmg *= s->multiplier;
	else if (b)
		dmg *= b->multiplier;

	dmg += obj->to_d;

	return dmg;
}

/**
 * Determine standard ranged damage.
 *
 * Factor in damage dice, to-dam, multiplier and any brand or slay.
 */
static int ranged_damage(struct object *missile, struct object *launcher, 
						 const struct brand *b, const struct slay *s, int mult)
{
	int dam;

	/* If we have a slay, modify the multiplier appropriately */
	if (b)
		mult += b->multiplier;
	else if (s)
		mult += s->multiplier;

	/* Apply damage: multiplier, slays, criticals, bonuses */
	dam = damroll(missile->dd, missile->ds);
	dam += missile->to_d;
	if (launcher)
		dam += launcher->to_d;
	dam *= mult;

	return dam;
}

/**
 * Determine damage for critical hits from shooting.
 *
 * Factor in item weight, total plusses, and player level.
 */
static int critical_shot(int weight, int plus, int dam, u32b *msg_type) {
	int chance = weight + (player->state.to_h + plus) * 4 + player->lev * 2;
	int power = weight + randint1(500);

	if (randint1(5000) > chance) {
		*msg_type = MSG_SHOOT_HIT;
		return dam;

	} else if (power < 500) {
		*msg_type = MSG_HIT_GOOD;
		return 2 * dam + 5;

	} else if (power < 1000) {
		*msg_type = MSG_HIT_GREAT;
		return 2 * dam + 10;

	} else {
		*msg_type = MSG_HIT_SUPERB;
		return 3 * dam + 15;
	}
}


/**
 * Determine damage for critical hits from shooting.
 *
 * Factor in item weight, total plusses, and player level.
 */
static int critical_shot_sneak(int weight, int plus, int dam, u32b *msg_type) {
	int chance = weight + (player->state.to_h + plus) * 4 + player->lev * 2 + player->state.skills[SKILL_STEALTH] * 1000;
	int power = weight + randint1(500) + player->lev * 20;
    
	if (randint1(5000) > chance) {
		*msg_type = MSG_SHOOT_HIT;
		return dam;
        
	} else if (power < 500) {
		*msg_type = MSG_HIT_GOOD;
		return 2 * dam + 5;
        
	} else if (power < 1000) {
		*msg_type = MSG_HIT_GREAT;
		return 2 * dam + 10;
        
	} else {
		*msg_type = MSG_HIT_SUPERB;
		return 3 * dam + 15;
	}
}


/**
 * Determine damage for critical hits from melee.
 *
 * Factor in weapon weight, total plusses, player level.
 */
static int critical_norm(int weight, int plus, int dam, u32b *msg_type) {
	int chance = weight + (player->state.to_h + plus) * 5 + player->lev * 3;
	int power = weight + randint1(650);

	if (randint1(5000) > chance) {
		*msg_type = MSG_HIT;
		return dam;

	} else if (power < 400) {
		*msg_type = MSG_HIT_GOOD;
		return 2 * dam + 5;

	} else if (power < 700) {
		*msg_type = MSG_HIT_GREAT;
		return 2 * dam + 10;

	} else if (power < 900) {
		*msg_type = MSG_HIT_SUPERB;
		return 3 * dam + 15;

	} else if (power < 1300) {
		*msg_type = MSG_HIT_HI_GREAT;
		return 3 * dam + 20;

	} else {
		*msg_type = MSG_HIT_HI_SUPERB;
		return 4 * dam + 20;
	}
}

/**
 * Determine damage for critical hits from melee.
 *
 * Factor in weapon weight, total plusses, player level.
 */
static int critical_sneak(int weight, int plus, int dam, u32b *msg_type) {
	int chance = weight + (player->state.to_h + plus) * 5 + player->lev * 3 + player->state.skills[SKILL_STEALTH] * 1000;
	int power = weight + randint1(650) + player->lev * 20;

	if (randint1(5000) > chance) {
		*msg_type = MSG_HIT;
		return dam;

	} else if (power < 400) {
		*msg_type = MSG_HIT_GOOD;
		return 2 * dam + 5;

	} else if (power < 700) {
		*msg_type = MSG_HIT_GREAT;
		return 2 * dam + 10;

	} else if (power < 900) {
		*msg_type = MSG_HIT_SUPERB;
		return 3 * dam + 15;

	} else if (power < 1300) {
		*msg_type = MSG_HIT_HI_GREAT;
		return 3 * dam + 20;

	} else {
		*msg_type = MSG_HIT_HI_SUPERB;
		return 4 * dam + 20;
	}
	
}

/**
 * Apply the player damage bonuses
 */
static int player_damage_bonus(struct player_state *state)
{
	return state->to_d;
}

/**
 * Apply blow side effects
 */
static void blow_side_effects(struct player *p, struct monster *mon)
{
	/* Confusion attack */
	if (p->confusing) {
		p->confusing = FALSE;
		msg("Your hands stop glowing.");

		mon_inc_timed(mon, MON_TMD_CONF, (10 + randint0(p->lev) / 10),
					  MON_TMD_FLG_NOTIFY, FALSE);
	}
}

/**
 * Apply blow after effects
 */
static bool blow_after_effects(int y, int x, bool quake)
{
	/* Apply earthquake brand */
	if (quake) {
		effect_simple(EF_EARTHQUAKE, "0", 0, 10, 0, NULL);

		/* Monster may be dead or moved */
		if (!square_monster(cave, y, x))
			return TRUE;
	}

	return FALSE;
}

/* A list of the different hit types and their associated special message */
static const struct {
	u32b msg;
	const char *text;
} melee_hit_types[] = {
	{ MSG_MISS, NULL },
	{ MSG_HIT, NULL },
	{ MSG_HIT_GOOD, "It was a good hit!" },
	{ MSG_HIT_GREAT, "It was a great hit!" },
	{ MSG_HIT_SUPERB, "It was a superb hit!" },
	{ MSG_HIT_HI_GREAT, "It was a *GREAT* hit!" },
	{ MSG_HIT_HI_SUPERB, "It was a *SUPERB* hit!" },
};

/**
 * Return the player's chance to hit with a particular weapon.
 */
int py_attack_hit_chance(const object_type *weapon)
{
	int bonus = player->state.to_h + weapon->to_h;
	int chance = player->state.skills[SKILL_TO_HIT_MELEE] + bonus * BTH_PLUS_ADJ;
	return chance;
}

/**
 * Attack the monster at the given location with a single blow.
 */
static bool py_attack_real(int y, int x, bool *fear) {
	size_t i;

	/* Information about the target of the attack */
	monster_type *m_ptr = square_monster(cave, y, x);
	char m_name[80];
	bool stop = FALSE;

	/* The weapon used */
	object_type *o_ptr = equipped_item_by_slot_name(player, "weapon");

	/* Information about the attack */
	int chance = py_attack_hit_chance(o_ptr);
	bool do_quake = FALSE;
	bool success = FALSE;

	/* Default to punching for one damage */
	char hit_verb[20];
	int dmg = 1;
	u32b msg_type = MSG_HIT;
	
	/* Sneak attack */
	bool sneak_attack = FALSE;

	/* Default to punching for one damage */
	my_strcpy(hit_verb, "punch", sizeof(hit_verb));

	/* Extract monster name (or "it") */
	monster_desc(m_name, sizeof(m_name), m_ptr, 
				 MDESC_OBJE | MDESC_IND_HID | MDESC_PRO_HID);

	/* Auto-Recall if possible and visible */
	if (mflag_has(m_ptr->mflag, MFLAG_VISIBLE))
		monster_race_track(player->upkeep, m_ptr->race);

	/* Track a new monster */
	if (mflag_has(m_ptr->mflag, MFLAG_VISIBLE))
		health_track(player->upkeep, m_ptr);

	/* Handle player fear (only for invisible monsters) */
	if (player_of_has(player, OF_AFRAID)) {
		msgt(MSG_AFRAID, "You are too afraid to attack %s!", m_name);
		return FALSE;
	}
	
	if (m_ptr->m_timed[MON_TMD_SLEEP]) {
		chance += player->state.skills[SKILL_STEALTH] * player->lev;
		if (player_has(PF_SNEAK_ATTACK)) {
			sneak_attack = TRUE;
		}
	}

	/* Disturb the monster */
	mon_clear_timed(m_ptr, MON_TMD_SLEEP, MON_TMD_FLG_NOMESSAGE, FALSE);

	/* See if the player hit */
	success = test_hit(chance, m_ptr->race->ac,
					   mflag_has(m_ptr->mflag, MFLAG_VISIBLE));

	/* If a miss, skip this hit */
	if (!success) {
		msgt(MSG_MISS, "You miss %s.", m_name);
		return FALSE;
	}

	/* Handle normal weapon */
	if (o_ptr->kind) {
		int j;
		const struct brand *b = NULL;
		const struct slay *s = NULL;

		my_strcpy(hit_verb, "hit", sizeof(hit_verb));

		/* Get the best attack from all slays or
		 * brands on all non-launcher equipment */
		for (j = 2; j < player->body.count; j++) { //Equip iteration - NRM
			struct object *obj = equipped_item_by_slot(player, j);
			if (obj->kind)
				improve_attack_modifier(obj, m_ptr, &b, &s, hit_verb,
										TRUE, FALSE);
		}

		improve_attack_modifier(o_ptr, m_ptr, &b, &s, hit_verb, 
								TRUE, FALSE);
		
		dmg = melee_damage(o_ptr, b, s);
		if (sneak_attack) {
			my_strcpy(hit_verb, "sneak attack", sizeof(hit_verb));
			dmg = critical_sneak(o_ptr->weight, o_ptr->to_h, dmg, &msg_type);
		} else {
			dmg = critical_norm(o_ptr->weight, o_ptr->to_h, dmg, &msg_type);
		}

		/* Learn by use for the weapon */
		object_notice_attack_plusses(o_ptr);

		if (player_of_has(player, OF_IMPACT) && dmg > 50) {
			do_quake = TRUE;
			wieldeds_notice_flag(player, OF_IMPACT);
		}
	}

	/* Learn by use for other equipped items */
	wieldeds_notice_on_attack();

	/* Apply the player damage bonuses */
	dmg += player_damage_bonus(&player->state);

	/* No negative damage; change verb if no damage done */
	if (dmg <= 0) {
		dmg = 0;
		msg_type = MSG_MISS;
		my_strcpy(hit_verb, "fail to harm", sizeof(hit_verb));
	}

	for (i = 0; i < N_ELEMENTS(melee_hit_types); i++) {
		const char *dmg_text = "";

		if (msg_type != melee_hit_types[i].msg)
			continue;

		if (OPT(show_damage))
			dmg_text = format(" (%d)", dmg);

		if (melee_hit_types[i].text)
			msgt(msg_type, "You %s %s%s. %s", hit_verb, m_name, dmg_text,
					melee_hit_types[i].text);
		else
			msgt(msg_type, "You %s %s%s.", hit_verb, m_name, dmg_text);
	}

	/* Pre-damage side effects */
	blow_side_effects(player, m_ptr);

	/* Damage, check for fear and death */
	stop = mon_take_hit(m_ptr, dmg, fear, NULL);

	if (stop)
		(*fear) = FALSE;

	/* Post-damage effects */
	if (blow_after_effects(y, x, do_quake))
		stop = TRUE;

	return stop;
}


/**
 * Attack the monster at the given location
 *
 * We get blows until energy drops below that required for another blow, or
 * until the target monster dies. Each blow is handled by py_attack_real().
 * We don't allow @ to spend more than 100 energy in one go, to avoid slower
 * monsters getting double moves.
 */
void py_attack(int y, int x) {
	int blow_energy = 10000 / player->state.num_blows;
	int blows = 0;
	bool fear = FALSE;
	monster_type *m_ptr = square_monster(cave, y, x);
	
	/* disturb the player */
	disturb(player, 0);

	/* Initialize the energy used */
	player->upkeep->energy_use = 0;

	/* Attack until energy runs out or enemy dies. We limit energy use to 100
	 * to avoid giving monsters a possible double move. */
	while (player->energy >= blow_energy * (blows + 1)) {
		bool stop = py_attack_real(y, x, &fear);
		player->upkeep->energy_use += blow_energy;
		if (stop || player->upkeep->energy_use + blow_energy > 100) break;
		blows++;
	}
	
	/* Hack - delay fear messages */
	if (fear && mflag_has(m_ptr->mflag, MFLAG_VISIBLE)) {
		char m_name[80];
		/* XXX Don't set monster_desc flags, since add_monster_message does string processing on m_name */
		monster_desc(m_name, sizeof(m_name), m_ptr, MDESC_DEFAULT);
		add_monster_message(m_name, m_ptr, MON_MSG_FLEE_IN_TERROR, TRUE);
	}
}


/* A list of the different hit types and their associated special message */
static const struct {
	u32b msg;
	const char *text;
} ranged_hit_types[] = {
	{ MSG_MISS, NULL },
	{ MSG_SHOOT_HIT, NULL },
	{ MSG_HIT_GOOD, "It was a good hit!" },
	{ MSG_HIT_GREAT, "It was a great hit!" },
	{ MSG_HIT_SUPERB, "It was a superb hit!" }
};

/**
 * This is a helper function used by do_cmd_throw and do_cmd_fire.
 *
 * It abstracts out the projectile path, display code, identify and clean up
 * logic, while using the 'attack' parameter to do work particular to each
 * kind of attack.
 */
static void ranged_helper(int item, int dir, int range, int shots, ranged_attack attack) {
	/* Get the ammo */
	object_type *o_ptr = object_from_item_idx(item);

	int i, j;

	object_type object_type_body;
	object_type *i_ptr = &object_type_body;

	char o_name[80];

	int path_n;
	struct loc path_g[256];

	/* Start at the player */
	int x = player->px;
	int y = player->py;

	/* Predict the "target" location */
	s16b ty = y + 99 * ddy[dir];
	s16b tx = x + 99 * ddx[dir];

	bool hit_target = FALSE;

	/* Check for target validity */
	if ((dir == 5) && target_okay()) {
		int taim;
		target_get(&tx, &ty);
		taim = distance(y, x, ty, tx);
		if (taim > range) {
			char msg[80];
			strnfmt(msg, sizeof(msg),
					"Target out of range by %d squares. Fire anyway? ",
				taim - range);
			if (!get_check(msg)) return;
		}
	}

	/* Sound */
	sound(MSG_SHOOT);

	/* Describe the object */
	object_desc(o_name, sizeof(o_name), o_ptr, ODESC_FULL | ODESC_SINGULAR);

	/* Actually "fire" the object -- Take a partial turn */
	player->upkeep->energy_use = (100 / shots);

	/* Calculate the path */
	path_n = project_path(path_g, range, y, x, ty, tx, 0);

	/* Hack -- Handle stuff */
	handle_stuff(player->upkeep);

	/* Start at the player */
	x = player->px;
	y = player->py;

	/* Project along the path */
	for (i = 0; i < path_n; ++i) {
		int ny = path_g[i].y;
		int nx = path_g[i].x;
		bool see = player_can_see_bold(ny, nx);

		/* Stop before hitting walls */
		if (!(square_ispassable(cave, ny, nx)) &&
			!(square_isprojectable(cave, ny, nx)))
			break;

		/* Advance */
		x = nx;
		y = ny;

		/* Tell the UI to display the missile */
		event_signal_missile(EVENT_MISSILE, 
							 object_char(o_ptr), object_attr(o_ptr), see, y, x);

		/* Try the attack on the monster at (x, y) if any */
		if (cave->m_idx[y][x] > 0) {
			monster_type *m_ptr = square_monster(cave, y, x);
			int visible = mflag_has(m_ptr->mflag, MFLAG_VISIBLE);

			bool fear = FALSE;
			const char *note_dies = monster_is_unusual(m_ptr->race) ? 
				" is destroyed." : " dies.";

			struct attack_result result = attack(o_ptr, y, x);
			int dmg = result.dmg;
			u32b msg_type = result.msg_type;
			char hit_verb[20];
			my_strcpy(hit_verb, result.hit_verb, sizeof(hit_verb));
			mem_free(result.hit_verb);

			if (result.success) {
				hit_target = TRUE;

				object_notice_attack_plusses(o_ptr);

				/* Learn by use for other equipped items */
				wieldeds_notice_to_hit_on_attack();

				/* No negative damage; change verb if no damage done */
				if (dmg <= 0) {
					dmg = 0;
					msg_type = MSG_MISS;
					my_strcpy(hit_verb, "fails to harm", sizeof(hit_verb));
				}

				if (!visible) {
					/* Invisible monster */
					msgt(MSG_SHOOT_HIT, "The %s finds a mark.", o_name);
				} else {
					for (j = 0; j < (int)N_ELEMENTS(ranged_hit_types); j++) {
						char m_name[80];
						const char *dmg_text = "";

						if (msg_type != ranged_hit_types[j].msg)
							continue;

						if (OPT(show_damage))
							dmg_text = format(" (%d)", dmg);

						monster_desc(m_name, sizeof(m_name), m_ptr, MDESC_OBJE);

						if (ranged_hit_types[j].text)
							msgt(msg_type, "Your %s %s %s%s. %s", o_name, 
								 hit_verb, m_name, dmg_text, 
								 ranged_hit_types[j].text);
						else
							msgt(msg_type, "Your %s %s %s%s.", o_name, hit_verb,
								 m_name, dmg_text);
					}
					
					/* Track this monster */
					if (mflag_has(m_ptr->mflag, MFLAG_VISIBLE)) {
						monster_race_track(player->upkeep, m_ptr->race);
						health_track(player->upkeep, m_ptr);
					}
				}

				/* Hit the monster, check for death */
				if (!mon_take_hit(m_ptr, dmg, &fear, note_dies)) {
					message_pain(m_ptr, dmg);
					if (fear && mflag_has(m_ptr->mflag, MFLAG_VISIBLE)) {
						char m_name[80];
						monster_desc(m_name, sizeof(m_name), m_ptr, 
									 MDESC_DEFAULT);
						add_monster_message(m_name, m_ptr, 
											MON_MSG_FLEE_IN_TERROR, TRUE);
					}
				}
			}
		}

		/* Stop if non-projectable but passable */
		if (!(square_isprojectable(cave, ny, nx))) 
			break;
	}

	/* Obtain a local object */
	object_copy(i_ptr, o_ptr);
	object_split(i_ptr, o_ptr, 1);

	/* Drop (or break) near that location */
	drop_near(cave, i_ptr, breakage_chance(i_ptr, hit_target), y, x, TRUE);

	if (item >= 0) {
		/* The ammo is from the inventory */
		inven_item_increase(item, -1);
		inven_item_describe(item);
		inven_item_optimize(item);
	} else {
		/* The ammo is from the floor */
		floor_item_increase(0 - item, -1);
		floor_item_optimize(0 - item);
	}
}


/**
 * Helper function used with ranged_helper by do_cmd_fire.
 */
static struct attack_result make_ranged_shot(object_type *o_ptr, int y, int x) {
	char *hit_verb = mem_alloc(20 * sizeof(char));
	struct attack_result result = {FALSE, 0, 0, hit_verb};

	object_type *j_ptr = equipped_item_by_slot_name(player, "shooting");

	monster_type *m_ptr = square_monster(cave, y, x);
	
	int chance = chance_of_missile_hit(player, o_ptr, j_ptr, y, x);

	int multiplier = player->state.ammo_mult;
	const struct brand *b = NULL;
	const struct slay *s = NULL;
    bool sneak_attack = FALSE;

	my_strcpy(hit_verb, "hits", sizeof(hit_verb));

	/* Did we hit it (penalize distance travelled) */
	if (!test_hit(chance, m_ptr->race->ac,
				  mflag_has(m_ptr->mflag, MFLAG_VISIBLE)))
		return result;

	result.success = TRUE;

	improve_attack_modifier(o_ptr, m_ptr, &b, &s, result.hit_verb, TRUE, FALSE);
	improve_attack_modifier(j_ptr, m_ptr, &b, &s, result.hit_verb, TRUE, FALSE);

	/* If we have a slay, modify the multiplier appropriately */
	
	if (m_ptr->m_timed[MON_TMD_SLEEP]) {
		chance += player->state.skills[SKILL_STEALTH] * player->lev;
		if (player_has(PF_SNEAK_ATTACK)) {
			sneak_attack = TRUE;
            result.hit_verb = "strikes";
		}
	} 
	
	
	result.dmg = ranged_damage(o_ptr, j_ptr, b, s, multiplier);
    if (sneak_attack) {
        result.dmg = critical_shot_sneak(o_ptr->weight, o_ptr->to_h, result.dmg, &result.msg_type);
    } else {
        result.dmg = critical_shot(o_ptr->weight, o_ptr->to_h, result.dmg, &result.msg_type);
    }

	object_notice_attack_plusses(j_ptr);

	return result;
}


/**
 * Helper function used with ranged_helper by do_cmd_throw.
 */
static struct attack_result make_ranged_throw(object_type *o_ptr, int y, int x) {
	char *hit_verb = mem_alloc(20*sizeof(char));
	struct attack_result result = {FALSE, 0, 0, hit_verb};

	monster_type *m_ptr = square_monster(cave, y, x);
	
	int chance = chance_of_missile_hit(player, o_ptr, NULL, y, x);

	int multiplier = 1;
	const struct brand *b = NULL;
	const struct slay *s = NULL;

	my_strcpy(hit_verb, "hits", sizeof(hit_verb));

	/* If we missed then we're done */
	if (!test_hit(chance, m_ptr->race->ac,
				  mflag_has(m_ptr->mflag, MFLAG_VISIBLE)))
		return result;

	result.success = TRUE;

	improve_attack_modifier(o_ptr, m_ptr, &b, &s, result.hit_verb, TRUE, FALSE);

	result.dmg = ranged_damage(o_ptr, NULL, b, s, multiplier);
	result.dmg = critical_norm(o_ptr->weight, o_ptr->to_h, result.dmg,
							   &result.msg_type);

	return result;
}


/**
 * Fire an object from the quiver, pack or floor at a target.
 */
void do_cmd_fire(struct command *cmd) {
	int item, dir;
	int range = MIN(6 + 2 * player->state.ammo_mult, MAX_RANGE);
	int shots = player->state.num_shots;

	ranged_attack attack = make_ranged_shot;

	object_type *j_ptr = equipped_item_by_slot_name(player, "shooting");
	object_type *o_ptr;

	/* Get arguments */
	if (cmd_get_item(cmd, "item", &item,
			/* Prompt */ "Fire which ammunition?",
			/* Error  */ "You have no ammunition to fire.",
			/* Filter */ obj_can_fire,
			/* Choice */ USE_INVEN | USE_QUIVER | USE_FLOOR | QUIVER_TAGS) == CMD_OK) {
		o_ptr = object_from_item_idx(item);
	} else {
		return;
	}

	if (cmd_get_target(cmd, "target", &dir) == CMD_OK)
		player_confuse_dir(player, &dir, FALSE);
	else
		return;

	/* Require a usable launcher */
	if (!j_ptr->tval || !player->state.ammo_tval) {
		msg("You have nothing to fire with.");
		return;
	}

	/* Check the item being fired is usable by the player. */
	if (!item_is_available(item, NULL, USE_QUIVER | USE_INVEN | USE_FLOOR)) {
		msg("That item is not within your reach.");
		return;
	}

	/* Check the ammo can be used with the launcher */
	if (o_ptr->tval != player->state.ammo_tval) {
		msg("That ammo cannot be fired by your current weapon.");
		return;
	}

	ranged_helper(item, dir, range, shots, attack);
}


/**
 * Throw an object from the quiver, pack or floor.
 */
void do_cmd_throw(struct command *cmd) {
	int item, dir;
	int shots = 1;
	int str = adj_str_blow[player->state.stat_ind[STAT_STR]];
	ranged_attack attack = make_ranged_throw;

	int weight;
	int range;
	object_type *o_ptr;

	/* Get arguments */
	if (cmd_get_item(cmd, "item", &item,
			/* Prompt */ "Throw which item?",
			/* Error  */ "You have nothing to throw.",
			/* Filter */ NULL,
			/* Choice */ USE_QUIVER | USE_INVEN | USE_FLOOR) == CMD_OK) {
		o_ptr = object_from_item_idx(item);
	} else {
		return;
	}

	if (cmd_get_target(cmd, "target", &dir) == CMD_OK)
		player_confuse_dir(player, &dir, FALSE);
	else
		return;


	weight = MAX(o_ptr->weight, 10);
	range = MIN(((str + 20) * 10) / weight, 10);

	/* Make sure the player isn't throwing wielded items */
	if (item_is_equipped(player, item)) {
		msg("You have cannot throw wielded items.");
		return;
	}

	ranged_helper(item, dir, range, shots, attack);
}

/**
 * Front-end command which fires at the nearest target with default ammo.
 */
void do_cmd_fire_at_nearest(void) {
	int i, dir = DIR_TARGET, item = -1;
	object_type *bow = equipped_item_by_slot_name(player, "shooting");

	/* Require a usable launcher */
	if (!bow->tval || !player->state.ammo_tval) {
		msg("You have nothing to fire with.");
		return;
	}

	/* Find first eligible ammo in the quiver */
	for (i = 0; i < QUIVER_SIZE; i++) {
		if (player->gear[player->upkeep->quiver[i]].tval !=
			player->state.ammo_tval)
			continue;
		item = player->upkeep->quiver[i];
		break;
	}

	/* Require usable ammo */
	if (item < 0) {
		msg("You have no ammunition in the quiver to fire.");
		return;
	}

	/* Require foe */
	if (!target_set_closest(TARGET_KILL | TARGET_QUIET)) return;

	/* Fire! */
	cmdq_push(CMD_FIRE);
	cmd_set_arg_item(cmdq_peek(), "item", item);
	cmd_set_arg_target(cmdq_peek(), "target", dir);
}
