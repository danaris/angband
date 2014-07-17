/**
   \file obj-ui.c
   \brief lists of objects and object pictures
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
#include "cmds.h"
#include "cmd-core.h"
#include "keymap.h"
#include "obj-desc.h"
#include "obj-gear.h"
#include "obj-identify.h"
#include "obj-tval.h"
#include "obj-ui.h"
#include "obj-util.h"
#include "store.h"
#include "ui-game.h"

/**
 * Determine if the attr and char should consider the item's flavor
 *
 * Identified scrolls should use their own tile.
 */
static bool use_flavor_glyph(const struct object_kind *kind)
{
	return kind->flavor && !(kind->tval == TV_SCROLL && kind->aware);
}

/**
 * Return the "attr" for a given item kind.
 * Use "flavor" if available.
 * Default to user definitions.
 */
byte object_kind_attr(const struct object_kind *kind)
{
	return use_flavor_glyph(kind) ? kind->flavor->x_attr : kind->x_attr;
}

/**
 * Return the "char" for a given item kind.
 * Use "flavor" if available.
 * Default to user definitions.
 */
wchar_t object_kind_char(const struct object_kind *kind)
{
	return use_flavor_glyph(kind) ? kind->flavor->x_char : kind->x_char;
}

/**
 * Return the "attr" for a given item.
 * Use "flavor" if available.
 * Default to user definitions.
 */
byte object_attr(const struct object *o_ptr)
{
	return object_kind_attr(o_ptr->kind);
}

/**
 * Return the "char" for a given item.
 * Use "flavor" if available.
 * Default to user definitions.
 */
wchar_t object_char(const struct object *o_ptr)
{
	return object_kind_char(o_ptr->kind);
}

/**
 * Convert a label into the gear index of an item in the inventory.
 *
 * Return "-1" if the label does not indicate a real item.
 */
s16b label_to_inven(int c)
{
	int i;

	/* Convert */
	i = (islower((unsigned char)c) ? A2I(c) : -1);

	/* Verify the index */
	if ((i < 0) || (i > INVEN_PACK)) return (-1);

	/* Empty slots can never be chosen */
	if (!player->gear[player->upkeep->inven[i]].kind) return (-1);

	/* Return the index */
	return (player->upkeep->inven[i]);
}


/**
 * Convert a label into the gear index of an item in the equipment or quiver.
 *
 * Return "-1" if the label does not indicate a real item.
 */
s16b label_to_equip(int c)
{
	int i;

	/* Convert */
	i = (islower((unsigned char)c) ? A2I(c) : -1);

	/* Verify the index */
	if ((i < 0) || (i >= player->body.count))
		return (-1);

	/* Empty slots can never be chosen */
	if (!equipped_item_by_slot(player, i)->kind)
		return (-1);

	return object_gear_index(player, equipped_item_by_slot(player, i));
}


/**
 * Convert a label into the gear index of an item in the equipment or quiver.
 *
 * Return "-1" if the label does not indicate a real item.
 */
s16b label_to_quiver(int c)
{
	int i;

	/* Convert */
	i = (islower((unsigned char)c) ? A2I(c) : -1);

	/* Verify the index */
	if ((i < 0) || (i >= QUIVER_SIZE))
		return (-1);

	/* Empty slots can never be chosen */
	if (!player->gear[player->upkeep->quiver[i]].kind) return (-1);

	/* Return the index */
	return player->upkeep->quiver[i];
}



/**
 * Display a list of objects.  Each object may be prefixed with a label.
 * Used by show_inven(), show_equip(), and show_floor().  Mode flags are
 * documented in object.h
 */
static void show_obj_list(int num_obj, int num_head, char labels[50][80],
						  object_type *objects[50], olist_detail_t mode)
{
	int i, row = 0, col = 0;
	int attr;
	size_t max_len = 0;
	int ex_width = 0, ex_offset, ex_offset_ctr;

	object_type *o_ptr;
	char o_name[50][80];
	char tmp_val[80];
	
	bool in_term = (mode & OLIST_WINDOW) ? TRUE : FALSE;
	bool terse = FALSE;

	if (in_term) max_len = 40;
	if (in_term && Term->wid < 40) mode &= ~(OLIST_WEIGHT);

	if (Term->wid < 50) terse = TRUE;

	/* Calculate name offset and max name length */
	for (i = 0; i < num_obj; i++)
	{
		o_ptr = objects[i];

		/* Null objects are used to skip lines, or display only a label */		
		if (!o_ptr || !o_ptr->kind)
		{
			if ((i < num_head) || !strcmp(labels[i], "In quiver"))
				strnfmt(o_name[i], sizeof(o_name[i]), "");
			else
				strnfmt(o_name[i], sizeof(o_name[i]), "(nothing)");
		}
		else
			object_desc(o_name[i], sizeof(o_name[i]), o_ptr, ODESC_PREFIX | ODESC_FULL |
				(terse ? ODESC_TERSE : 0));

		/* Max length of label + object name */
		max_len = MAX(max_len, strlen(labels[i]) + strlen(o_name[i]));
	}

	/* Take the quiver message into consideration */
	if (mode & OLIST_QUIVER && player->upkeep->quiver[0] != NO_OBJECT)
		max_len = MAX(max_len, 24);

	/* Width of extra fields */
	if (mode & OLIST_WEIGHT) ex_width += 9;
	if (mode & OLIST_PRICE) ex_width += 9;
	if (mode & OLIST_FAIL) ex_width += 10;

	/* Determine beginning row and column */
	if (in_term)
	{
		/* Term window */
		row = 0;
		col = 0;
	}
	else
	{
		/* Main window */
		row = 1;
		col = Term->wid - 1 - max_len - ex_width;

		if (col < 3) col = 0;
	}

	/* Column offset of the first extra field */
	ex_offset = MIN(max_len, (size_t)(Term->wid - 1 - ex_width - col));

	/* Output the list */
	for (i = 0; i < num_obj; i++)
	{
		o_ptr = objects[i];
		
		/* Clear the line */
		prt("", row + i, MAX(col - 2, 0));

		/* If we have no label then we won't display anything */
		if (!strlen(labels[i])) continue;

		/* Print the label */
		put_str(labels[i], row + i, col);

		/* Limit object name */
		if (strlen(labels[i]) + strlen(o_name[i]) > (size_t)ex_offset)
		{
			int truncate = ex_offset - strlen(labels[i]);
			
			if (truncate < 0) truncate = 0;
			if ((size_t)truncate > sizeof(o_name[i]) - 1) truncate = sizeof(o_name[i]) - 1;

			o_name[i][truncate] = '\0';
		}
		
		/* Item kind determines the color of the output */
		if (o_ptr && o_ptr->kind)
			attr = o_ptr->kind->base->attr;
		else
			attr = TERM_SLATE;

		/* Object name */
		c_put_str(attr, o_name[i], row + i, col + strlen(labels[i]));

		/* If we don't have an object, we can skip the rest of the output */
		if (!(o_ptr && o_ptr->kind)) continue;

		/* Extra fields */
		ex_offset_ctr = ex_offset;
		
		if (mode & OLIST_PRICE)
		{
			struct store *store = store_at(cave, player->py, player->px);
			if (store) {
				int price = price_item(store, o_ptr, TRUE, o_ptr->number);

				strnfmt(tmp_val, sizeof(tmp_val), "%6d au", price);
				put_str(tmp_val, row + i, col + ex_offset_ctr);
				ex_offset_ctr += 9;
			}
		}

		if (mode & OLIST_FAIL && obj_can_fail(o_ptr))
		{
			int fail = (9 + get_use_device_chance(o_ptr)) / 10;
			if (object_effect_is_known(o_ptr))
				strnfmt(tmp_val, sizeof(tmp_val), "%4d%% fail", fail);
			else
				my_strcpy(tmp_val, "    ? fail", sizeof(tmp_val));
			put_str(tmp_val, row + i, col + ex_offset_ctr);
			ex_offset_ctr += 10;
		}

		if (mode & OLIST_WEIGHT)
		{
			int weight = o_ptr->weight * o_ptr->number;
			strnfmt(tmp_val, sizeof(tmp_val), "%4d.%1d lb", weight / 10, weight % 10);
			put_str(tmp_val, row + i, col + ex_offset_ctr);
			ex_offset_ctr += 9;
		}
	}

	/* For the inventory: print the quiver count */
	if (mode & OLIST_QUIVER)
	{
		int count, j;
		int quiver_slots = player->upkeep->quiver_cnt / (MAX_STACK_SIZE - 1);

		/* Quiver may take multiple lines */
		for (j = 0; j < quiver_slots; j++, i++)
		{
			const char *fmt = "in Quiver: %d missile%s";
			char letter = inven_to_label(in_term ? i - 1 : i);

			/* Number of missiles in this "slot" */
			if (j == quiver_slots - 1)
				count = player->upkeep->quiver_cnt % (MAX_STACK_SIZE - 1);
			else
				count = MAX_STACK_SIZE-1;

			/* Clear the line */
			prt("", row + i, MAX(col - 2, 0));

			/* Print the (disabled) label */
			strnfmt(tmp_val, sizeof(tmp_val), "%c) ", letter);
			c_put_str(TERM_SLATE, tmp_val, row + i, col);

			/* Print the count */
			strnfmt(tmp_val, sizeof(tmp_val), fmt, count, count == 1 ? "" : "s");
			c_put_str(TERM_L_UMBER, tmp_val, row + i, col + 3);
		}
	}

	/* Clear term windows */
	if (in_term)
	{
		for (; i < Term->hgt; i++)
		{
			prt("", row + i, MAX(col - 2, 0));
		}
	}
	
	/* Print a drop shadow for the main window if necessary */
	else if (i > 0 && row + i < 24)
	{
		prt("", row + i, MAX(col - 2, 0));
	}
}

/**
 * Display the inventory.  Builds a list of objects and passes them
 * off to show_obj_list() for display.  Mode flags documented in
 * object.h
 */
void show_inven(int mode, item_tester tester)
{
	int i, last_slot = -1;
	int diff = weight_remaining();

	object_type *o_ptr;

	int num_obj = 0;
	char labels[50][80];
	object_type *objects[50];

	bool in_term = (mode & OLIST_WINDOW) ? TRUE : FALSE;

	/* Include burden for term windows */
	if (in_term)
	{
		strnfmt(labels[num_obj], sizeof(labels[num_obj]),
		        "Burden %d.%d lb (%d.%d lb %s) ",
		        player->upkeep->total_weight / 10,
				player->upkeep->total_weight % 10,
		        abs(diff) / 10, abs(diff) % 10,
		        (diff < 0 ? "overweight" : "remaining"));

		objects[num_obj] = NULL;
		num_obj++;
	}

	/* Find the last occupied inventory slot */
	for (i = 0; i < INVEN_PACK; i++)
		if (player->upkeep->inven[i] != NO_OBJECT) last_slot = i;

	/* Build the object list */
	for (i = 0; i <= last_slot; i++)
	{
		o_ptr = &player->gear[player->upkeep->inven[i]];

		/* Acceptable items get a label */
		if (object_test(tester, o_ptr))
			strnfmt(labels[num_obj], sizeof(labels[num_obj]), "%c) ",
					inven_to_label(i));

		/* Unacceptable items are still sometimes shown */
		else if (in_term)
			my_strcpy(labels[num_obj], "   ", sizeof(labels[num_obj]));

		/* Unacceptable items are skipped in the main window */
		else continue;

		/* Save the object */
		objects[num_obj] = o_ptr;
		num_obj++;
	}

	/* Display the object list */
	if (in_term)
		/* Term window starts with a burden header */
		show_obj_list(num_obj, 1, labels, objects, mode);
	else
		show_obj_list(num_obj, 0, labels, objects, mode);
}


/**
 * Display the quiver.  Builds a list of objects and passes them
 * off to show_obj_list() for display.  Mode flags documented in
 * object.h
 */
void show_quiver(int mode, item_tester tester)
{
	int i, last_slot = -1;

	object_type *o_ptr;

	int num_obj = 0;
	char labels[50][80];
	object_type *objects[50];

	bool in_term = (mode & OLIST_WINDOW) ? TRUE : FALSE;

	/* Find the last occupied quiver slot */
	for (i = 0; i < QUIVER_SIZE; i++)
		if (player->upkeep->quiver[i] != NO_OBJECT) last_slot = i;

	/* Build the object list */
	for (i = 0; i <= last_slot; i++)
	{
		o_ptr = &player->gear[player->upkeep->quiver[i]];

		/* Acceptable items get a label */
		if (object_test(tester, o_ptr))
			strnfmt(labels[num_obj], sizeof(labels[num_obj]), "%c) ",
					quiver_to_label(i));

		/* Unacceptable items are still sometimes shown */
		else if (in_term)
			my_strcpy(labels[num_obj], "   ", sizeof(labels[num_obj]));

		/* Unacceptable items are skipped in the main window */
		else continue;

		/* Save the object */
		objects[num_obj] = o_ptr;
		num_obj++;
	}

	/* Display the object list */
	show_obj_list(num_obj, 0, labels, objects, mode);
}


/**
 * Display the equipment.  Builds a list of objects and passes them
 * off to show_obj_list() for display.  Mode flags documented in
 * object.h
 */
void show_equip(int mode, item_tester tester)
{
	int i;

	object_type *o_ptr;

	int num_obj = 0;
	char labels[50][80];
	object_type *objects[50];

	char tmp_val[80];

	bool in_term = (mode & OLIST_WINDOW) ? TRUE : FALSE;
	bool show_empty = (mode & OLIST_SEMPTY) ? TRUE : FALSE;

	/* Build the object list */
	for (i = 0; i < player->body.count; i++)
	{
		o_ptr = equipped_item_by_slot(player, i);

		/* Acceptable items get a label */
		if (object_test(tester, o_ptr))
			strnfmt(labels[num_obj], sizeof(labels[num_obj]), "%c) ",
					equip_to_label(i));

		/* Unacceptable items are still sometimes shown */
		else if ((!o_ptr->kind && show_empty) || in_term)
			my_strcpy(labels[num_obj], "   ", sizeof(labels[num_obj]));

		/* Unacceptable items are skipped in the main window */
		else continue;

		/* Show full slot labels */
		strnfmt(tmp_val, sizeof(tmp_val), "%-14s: ", equip_mention(player, i));
		my_strcap(tmp_val);
		my_strcat(labels[num_obj], tmp_val, sizeof(labels[num_obj]));

		/* Save the object */
		objects[num_obj] = o_ptr;
		num_obj++;
	}

	/* Show the quiver in subwindows */
	if (in_term) {
		int last_slot = -1;

		strnfmt(labels[num_obj], sizeof(labels[num_obj]), "In quiver");
		objects[num_obj] = NULL;
		num_obj++;

		/* Find the last occupied quiver slot */
		for (i = 0; i < QUIVER_SIZE; i++)
			if (player->upkeep->quiver[i] != NO_OBJECT) last_slot = i;

		/* Extend the object list */
		for (i = 0; i <= last_slot; i++)
		{
			o_ptr = &player->gear[player->upkeep->quiver[i]];

			/* Acceptable items get a label */
			if (object_test(tester, o_ptr))
				strnfmt(labels[num_obj], sizeof(labels[num_obj]), "%c) ",
						quiver_to_label(i));

			/* Unacceptable items are still sometimes shown */
			else if (in_term)
				my_strcpy(labels[num_obj], "   ", sizeof(labels[num_obj]));

			/* Unacceptable items are skipped in the main window */
			else continue;

			/* Show full slot labels */
			strnfmt(tmp_val, sizeof(tmp_val), "Slot %-9d: ", i);
			my_strcat(labels[num_obj], tmp_val, sizeof(labels[num_obj]));

			/* Save the object */
			objects[num_obj] = o_ptr;
			num_obj++;
		}
	}

	/* Display the object list */
	show_obj_list(num_obj, 0, labels, objects, mode);
}


/**
 * Display the floor.  Builds a list of objects and passes them
 * off to show_obj_list() for display.  Mode flags documented in
 * object.h
 */
void show_floor(const int *floor_list, int floor_num, int mode, item_tester tester)
{
	int i;

	object_type *o_ptr;

	int num_obj = 0;
	char labels[50][80];
	object_type *objects[50];

	if (floor_num > MAX_FLOOR_STACK) floor_num = MAX_FLOOR_STACK;

	/* Build the object list */
	for (i = 0; i < floor_num; i++)
	{
		o_ptr = cave_object(cave, floor_list[i]);

		/* Tester always skips gold. When gold should be displayed,
		 * only test items that are not gold.
		 */
		if ((!tval_is_money(o_ptr) || !(mode & OLIST_GOLD)) &&
		    !object_test(tester, o_ptr))
			continue;

		strnfmt(labels[num_obj], sizeof(labels[num_obj]),
		        "%c) ", floor_to_label(i));

		/* Save the object */
		objects[num_obj] = o_ptr;
		num_obj++;
	}

	/* Display the object list */
	show_obj_list(num_obj, 0, labels, objects, mode);
}


/**
 * Verify the choice of an item.
 *
 * The item can be negative to mean "item on floor".
 */
bool verify_item(const char *prompt, int item)
{
	char o_name[80];

	char out_val[160];

	object_type *o_ptr;

	/* Inventory */
	if (item >= 0)
	{
		o_ptr = &player->gear[item];
	}

	/* Floor */
	else
	{
		o_ptr = cave_object(cave, 0 - item);
	}

	/* Describe */
	object_desc(o_name, sizeof(o_name), o_ptr, ODESC_PREFIX | ODESC_FULL);

	/* Prompt */
	strnfmt(out_val, sizeof(out_val), "%s %s? ", prompt, o_name);

	/* Query */
	return (get_check(out_val));
}


/**
 * Prevent certain choices depending on the inscriptions on the item.
 *
 * The item can be negative to mean "item on floor".
 */
bool get_item_allow(int item, unsigned char ch, cmd_code cmd, bool is_harmless)
{
	object_type *o_ptr;
	char verify_inscrip[] = "!*";

	unsigned n;

	/* Inventory or floor */
	if (item >= 0)
		o_ptr = &player->gear[item];
	else
		o_ptr = cave_object(cave, 0 - item);

	/* Hack - Only shift the command key if it actually needs to be shifted. */
	if (ch < 0x20)
		ch = UN_KTRL(ch);

	/* The inscription to look for */
	verify_inscrip[1] = ch;

	/* Look for the inscription */
	n = check_for_inscrip(o_ptr, verify_inscrip);

	/* Also look for for the inscription '!*' */
	if (!is_harmless)
		n += check_for_inscrip(o_ptr, "!*");

	/* Choose string for the prompt */
	if (n) {
		char prompt[1024];

		const char *verb = cmdq_pop_verb(cmd);
		if (!verb)
			verb = "do that with";

		strnfmt(prompt, sizeof(prompt), "Really %s", verb);

		/* Promt for confirmation n times */
		while (n--) {
			if (!verify_item(prompt, item))
				return (FALSE);
		}
	}

	/* Allow it */
	return (TRUE);
}



/**
 * Find the "first" inventory object with the given "tag" - now first in the
 * gear array, which is really arbitrary - NRM.
 *
 * A "tag" is a char "n" appearing as "@n" anywhere in the
 * inscription of an object.
 *
 * Also, the tag "@xn" will work as well, where "n" is a tag-char,
 * and "x" is the action that tag will work for.
 */
static int get_tag(int *cp, char tag, cmd_code cmd, bool quiver_tags)
{
	int i;
	const char *s;
	int mode = OPT(rogue_like_commands) ? KEYMAP_MODE_ROGUE : KEYMAP_MODE_ORIG;

	/* (f)ire is handled differently from all others, due to the quiver */
	if (quiver_tags)
	{
		i = tag - '0';
		if (player->upkeep->quiver[i] != NO_OBJECT)
		{
			*cp = player->upkeep->quiver[i];
			return (TRUE);
		}
		return (FALSE);
	}

	/* Check every object */
	for (i = 0; i < player->max_gear; ++i)
	{
		object_type *o_ptr = &player->gear[i];

		/* Skip non-objects */
		if (!o_ptr->kind) continue;

		/* Skip empty inscriptions */
		if (!o_ptr->note) continue;

		/* Find a '@' */
		s = strchr(quark_str(o_ptr->note), '@');

		/* Process all tags */
		while (s)
		{
			unsigned char cmdkey;

			/* Check the normal tags */
			if (s[1] == tag)
			{
				/* Save the actual inventory ID */
				*cp = i;

				/* Success */
				return (TRUE);
			}

			cmdkey = cmd_lookup_key(cmd, mode);

			/* Hack - Only shift the command key if it actually needs to be shifted. */
			if (cmdkey < 0x20)
				cmdkey = UN_KTRL(cmdkey);

			/* Check the special tags */
			if ((s[1] == cmdkey) && (s[2] == tag))
			{
				/* Save the actual inventory ID */
				*cp = i;

				/* Success */
				return (TRUE);
			}

			/* Find another '@' */
			s = strchr(s + 1, '@');
		}
	}

	/* No such tag */
	return (FALSE);
}



/**
 * Let the user select an item, save its gear array index
 *
 * Return TRUE only if an acceptable item was chosen by the user.
 *
 * The user is allowed to choose acceptable items from the equipment,
 * inventory, quiver, or floor, respectively, if the proper flag was given,
 * and there are any acceptable items in that location.
 *
 * The equipment, inventory or quiver are displayed (even if no acceptable
 * items are in that location) if the proper flag was given.
 *
 * If there are no acceptable items available anywhere, and "str" is
 * not NULL, then it will be used as the text of a warning message
 * before the function returns.
 *
 * Note that the user must press "-" to specify the item on the floor,
 * and there is no way to "examine" the item on the floor, while the
 * use of "capital" letters will "examine" an inventory/equipment item,
 * and prompt for its use.
 *
 * If a legal item index is selected from the gear, we save it in "cp"
 * directly and return TRUE.
 *
 * If a legal item is selected from the floor, we save it in "cp" as
 * a negative (-1 to -511), and return TRUE.
 *
 * If no item is available, we do nothing to "cp", and we display a
 * warning message, using "str" if available, and return FALSE.
 *
 * If no item is selected, we do nothing to "cp", and return FALSE.
 *
 * Global "player->upkeep->command_wrk" is used to choose between
 * equip/inven/floor listings.  It is equal to USE_INVEN or USE_EQUIP or
 * USE_QUIVER or USE_FLOOR, except when this function is first called, when it
 * is equal to zero, which will cause it to be set to USE_INVEN.
 *
 * We always erase the prompt when we are done, leaving a blank line,
 * or a warning message, if appropriate, if no items are available.
 *
 * Note that only "acceptable" floor objects get indexes, so between two
 * commands, the indexes of floor objects may change.  XXX XXX XXX
 */
bool get_item(int *cp, const char *pmt, const char *str, cmd_code cmd, 
			  item_tester tester, int mode)
{
	int py = player->py;
	int px = player->px;
	unsigned char cmdkey = cmd_lookup_key(cmd,
			OPT(rogue_like_commands) ? KEYMAP_MODE_ROGUE : KEYMAP_MODE_ORIG);

	ui_event press;

	int j, k;

	int i1, i2;
	int e1, e2;
	int f1, f2;
	int q1, q2;

	bool done, item;

	bool oops = FALSE;

	bool use_inven = ((mode & USE_INVEN) ? TRUE : FALSE);
	bool use_equip = ((mode & USE_EQUIP) ? TRUE : FALSE);
	bool use_quiver = ((mode & USE_QUIVER) ? TRUE : FALSE);
	bool use_floor = ((mode & USE_FLOOR) ? TRUE : FALSE);
	bool is_harmless = ((mode & IS_HARMLESS) ? TRUE : FALSE);
	bool quiver_tags = ((mode & QUIVER_TAGS) ? TRUE : FALSE);

	int olist_mode = 0;

	bool allow_inven = FALSE;
	bool allow_equip = FALSE;
	bool allow_quiver = FALSE;
	bool allow_floor = FALSE;

	bool toggle = FALSE;

	char tmp_val[160];
	char out_val[160];

	int floor_list[MAX_FLOOR_STACK];
	int floor_num;

	bool show_list = TRUE;

	/* Hack - Only shift the command key if it actually needs to be shifted. */
	if (cmdkey < 0x20)
		cmdkey = UN_KTRL(cmdkey);

	/* Object list display modes */
	if (mode & SHOW_FAIL)
		olist_mode |= OLIST_FAIL;
	else
		olist_mode |= OLIST_WEIGHT;

	if (mode & SHOW_PRICES)
		olist_mode |= OLIST_PRICE;

	if (mode & SHOW_EMPTY)
		olist_mode |= OLIST_SEMPTY;

	/* Paranoia XXX XXX XXX */
	message_flush();


	/* Not done */
	done = FALSE;

	/* No item selected */
	item = FALSE;


	/* Full inventory */
	i1 = 0;
	i2 = INVEN_PACK - 1;

	/* Forbid inventory */
	if (!use_inven) i2 = -1;

	/* Restrict inventory indexes */
	while ((i1 <= i2) && (!item_test(tester, player->upkeep->inven[i1]))) i1++;
	while ((i1 <= i2) && (!item_test(tester, player->upkeep->inven[i2]))) i2--;

	/* Accept inventory */
	if (i1 <= i2) allow_inven = TRUE;


	/* Full equipment */
	e1 = 0;
	e2 = player->body.count;

	/* Forbid equipment */
	if (!use_equip) e2 = -1;

	/* Restrict equipment indexes */
	while ((e1 <= e2) && (!item_test(tester, slot_index(player, e1)))) e1++;
	while ((e1 <= e2) && (!item_test(tester, slot_index(player, e2)))) e2--;

	/* Accept equipment */
	if (e1 <= e2) allow_equip = TRUE;

	/* Restrict quiver indexes */
	q1 = 0;
	q2 = QUIVER_SIZE - 1;

	/* Forbid quiver */
	if (!use_quiver) q2 = -1;

	/* Restrict quiver indexes */
	while ((q1 <= q2) && (!item_test(tester, player->upkeep->quiver[q1]))) q1++;
	while ((q1 <= q2) && (!item_test(tester, player->upkeep->quiver[q2]))) q2--;

	/* Accept quiver */
	if (q1 <= q2) allow_quiver = TRUE;

	/* Scan all non-gold objects in the grid */
	floor_num = scan_floor(floor_list, N_ELEMENTS(floor_list), py, px, 0x0B,
						   tester);

	/* Full floor */
	f1 = 0;
	f2 = floor_num - 1;

	/* Forbid floor */
	if (!use_floor) f2 = -1;

	/* Restrict floor indexes */
	while ((f1 <= f2) && (!item_test(tester, 0 - floor_list[f1]))) f1++;
	while ((f1 <= f2) && (!item_test(tester, 0 - floor_list[f2]))) f2--;

	/* Accept floor */
	if (f1 <= f2) allow_floor = TRUE;

	/* Require at least one legal choice */
	if (!allow_inven && !allow_equip && !allow_quiver && !allow_floor)
	{
		/* Oops */
		oops = TRUE;
		done = TRUE;
	}

	/* Analyze choices */
	else
	{
		/* Hack -- Start on equipment if requested */
		if ((player->upkeep->command_wrk == USE_EQUIP) && allow_equip)
			player->upkeep->command_wrk = USE_EQUIP;
		else if ((player->upkeep->command_wrk == USE_INVEN) && allow_inven)
			player->upkeep->command_wrk = USE_INVEN;
		else if ((player->upkeep->command_wrk == USE_QUIVER) && allow_quiver)
			player->upkeep->command_wrk = USE_QUIVER;
		else if ((player->upkeep->command_wrk == USE_FLOOR) && allow_floor)
			player->upkeep->command_wrk = USE_FLOOR;

		/* If we are using the quiver then start on quiver */
		else if (quiver_tags && allow_quiver)
			player->upkeep->command_wrk = USE_QUIVER;

		/* Use inventory if allowed */
		else if (use_inven && allow_inven)
			player->upkeep->command_wrk = USE_INVEN;

		/* Use equipment if allowed */
		else if (use_equip && allow_equip)
			player->upkeep->command_wrk = USE_EQUIP;

		/* Use quiver if allowed */
		else if (use_quiver && allow_quiver)
			player->upkeep->command_wrk = USE_QUIVER;

		/* Use floor if allowed */
		else if (use_floor && allow_floor)
			player->upkeep->command_wrk = USE_FLOOR;

		/* Hack -- Use (empty) inventory */
		else
			player->upkeep->command_wrk = USE_INVEN;
	}


	/* Start out in "display" mode */
	if (show_list)
	{
		/* Save screen */
		screen_save();
	}


	/* Repeat until done */
	while (!done)
	{
		int ni = 0;
		int ne = 0;

		/* Scan windows */
		for (j = 0; j < ANGBAND_TERM_MAX; j++)
		{
			/* Unused */
			if (!angband_term[j]) continue;

			/* Count windows displaying inven */
			if (window_flag[j] & (PW_INVEN)) ni++;

			/* Count windows displaying equip */
			if (window_flag[j] & (PW_EQUIP)) ne++;
		}

		/* Toggle if needed */
		if (((player->upkeep->command_wrk == USE_EQUIP) && ni && !ne) ||
		    ((player->upkeep->command_wrk == USE_INVEN) && !ni && ne))
		{
			/* Toggle */
			toggle_inven_equip();

			/* Track toggles */
			toggle = !toggle;
		}

		/* Redraw */
		player->upkeep->redraw |= (PR_INVEN | PR_EQUIP);

		/* Redraw windows */
		redraw_stuff(player->upkeep);

		/* Viewing inventory */
		if (player->upkeep->command_wrk == USE_INVEN)
		{
			int nmode = olist_mode;

			/* Show the quiver counts in certain cases, like the 'i' command */
			if (mode & SHOW_QUIVER)
				nmode |= OLIST_QUIVER;

			/* Redraw if needed */
			if (show_list)
				show_inven(nmode, tester);

			/* Begin the prompt */
			strnfmt(out_val, sizeof(out_val), "Inven:");

			/* List choices */
			if (i1 <= i2)
			{
				/* Build the prompt */
				strnfmt(tmp_val, sizeof(tmp_val), " %c-%c,",
				        inven_to_label(i1), inven_to_label(i2));

				/* Append */
				my_strcat(out_val, tmp_val, sizeof(out_val));
			}

			/* Indicate ability to "view" */
			if (!show_list)
			{
				my_strcat(out_val, " * to see,", sizeof(out_val));
			}

			/* Indicate legality of "toggle" */
			if (use_equip)
			{
				my_strcat(out_val, " / for Equip,", sizeof(out_val));
			}

			/* Indicate legality of "toggle" */
			if (use_quiver)
			{
				my_strcat(out_val, " . for Quiver,", sizeof(out_val));
			}

			/* Indicate legality of the "floor" */
			if (allow_floor)
			{
				my_strcat(out_val, " - for floor,", sizeof(out_val));
			}
		}

		/* Viewing equipment */
		else if (player->upkeep->command_wrk == USE_EQUIP)
		{
			/* Redraw if needed */
			if (show_list) show_equip(olist_mode, tester);

			/* Begin the prompt */
			strnfmt(out_val, sizeof(out_val), "Equip:");

			/* List choices */
			if (e1 <= e2)
			{
				/* Build the prompt */
				strnfmt(tmp_val, sizeof(tmp_val), " %c-%c,",
				        equip_to_label(e1),	equip_to_label(e2));

				/* Append */
				my_strcat(out_val, tmp_val, sizeof(out_val));
			}

			/* Indicate ability to "view" */
			if (!show_list)
			{
				my_strcat(out_val, " * to see,", sizeof(out_val));
			}

			/* Indicate legality of "toggle" */
			if (use_inven)
			{
				my_strcat(out_val, " / for Inven,", sizeof(out_val));
			}

			/* Indicate legality of "toggle" */
			if (use_quiver)
			{
				my_strcat(out_val, " . for Quiver,", sizeof(out_val));
			}

			/* Indicate legality of the "floor" */
			if (allow_floor)
			{
				my_strcat(out_val, " - for floor,", sizeof(out_val));
			}
		}

		/* Viewing quiver */
		else if (player->upkeep->command_wrk == USE_QUIVER)
		{
			/* Redraw if needed */
			if (show_list) show_quiver(olist_mode, tester);

			/* Begin the prompt */
			strnfmt(out_val, sizeof(out_val), "Quiver:");

			/* List choices */
			if (q1 <= q2)
			{
				/* Build the prompt */
				strnfmt(tmp_val, sizeof(tmp_val), " %c-%c,",
				        quiver_to_label(q1),
				        quiver_to_label(q2));

				/* Append */
				my_strcat(out_val, tmp_val, sizeof(out_val));
			}

			/* Indicate ability to "view" */
			if (!show_list)
			{
				my_strcat(out_val, " * to see,", sizeof(out_val));
			}

			/* Indicate legality of "toggle" */
			if (use_inven)
			{
				my_strcat(out_val, " / for Inven,", sizeof(out_val));
			}

			/* Indicate legality of the "floor" */
			if (allow_floor)
			{
				my_strcat(out_val, " - for floor,", sizeof(out_val));
			}
		}

		/* Viewing floor */
		else
		{
			/* Redraw if needed */
			if (show_list)
				show_floor(floor_list, floor_num, olist_mode, tester);

			/* Begin the prompt */
			strnfmt(out_val, sizeof(out_val), "Floor:");

			/* List choices */
			if (f1 <= f2)
			{
				/* Build the prompt */
				strnfmt(tmp_val, sizeof(tmp_val), " %c-%c,", I2A(f1), I2A(f2));

				/* Append */
				my_strcat(out_val, tmp_val, sizeof(out_val));
			}

			/* Indicate ability to "view" */
			if (!show_list)
			{
				my_strcat(out_val, " * to see,", sizeof(out_val));
			}

			/* Append */
			if (use_inven)
			{
				my_strcat(out_val, " / for Inven,", sizeof(out_val));
			}

			/* Append */
			else if (use_equip)
			{
				my_strcat(out_val, " / for Equip,", sizeof(out_val));
			}

			/* Indicate legality of "toggle" */
			if (use_quiver)
			{
				my_strcat(out_val, " . for Quiver,", sizeof(out_val));
			}

		}

		/* Finish the prompt */
		my_strcat(out_val, " ESC", sizeof(out_val));

		/* if we have a prompt header, show the part that we just built */
		if (pmt) {
			/* Build the prompt */
			strnfmt(tmp_val, sizeof(tmp_val), "(%s) %s", out_val, pmt);

			/* Show the prompt */
			prt(tmp_val, 0, 0);
		}

		/* Get a key */
		press = inkey_m();

		/* Parse it */
		if (press.type == EVT_MOUSE) {

#if 0 // Postponing mouse support - NRM

			if (press.mouse.button == 2) {
				done = TRUE;
			} else if (press.mouse.button == 1) {
				k = -1;
				if (player->upkeep->command_wrk == USE_INVEN) {
					if (press.mouse.y == 0) {
						if (use_equip) {
							player->upkeep->command_wrk = USE_EQUIP;
						} else if (allow_floor) {
							player->upkeep->command_wrk = USE_FLOOR;
						}
					} else if ((press.mouse.y <= i2 - i1 + 1) ) {
						/* get the item index, allowing for skipped indices */
						for (j = i1; j <= i2; j++) {
							if (item_test(tester, j)) {
								if (press.mouse.y == 1) {
									k = j;
									break;
								}
								press.mouse.y--;
							}
						}
					}
				} else if (player->upkeep->command_wrk == USE_EQUIP) {
					if (press.mouse.y == 0) {
						if (allow_floor) {
							player->upkeep->command_wrk = USE_FLOOR;
						} else if (use_inven) {
							player->upkeep->command_wrk = USE_INVEN;
						}
					} else if (press.mouse.y <= e2 - e1 + 1) {
						if (olist_mode & OLIST_SEMPTY) {
							/* If we are showing empties, just set the object
							 * (empty objects will just keep the loop going) */
							k = label_to_equip(index_to_label(e1 + press.mouse.y - 1));
						} else {
							/* get the item index, allow for skipped indices */
							for (j = e1; j <= e2; j++) {
								/* skip the quiver slot which is a blank line
								 * in the list */
								if (j == 36) {
									press.mouse.y--;
								} else if (item_test(tester, j)) {
									if (press.mouse.y == 1) {
										k = j;
										break;
									}
									press.mouse.y--;
								}
							}
						}
					}
				} else if (player->upkeep->command_wrk == USE_FLOOR) {
					if (press.mouse.y == 0) {
						if (use_inven) {
							player->upkeep->command_wrk = USE_INVEN;
						} else if (use_equip) {
							player->upkeep->command_wrk = USE_EQUIP;
						}
					} else if ((press.mouse.y <= floor_num)
							   && (press.mouse.y >= 1)) {
						/* Special index */
						k = 0 - floor_list[press.mouse.y-1];
						/* get the item index, allowing for skipped indices */
						for (j = f1; j <= f2; j++) {
							if (item_test(tester, 0 - floor_list[j])) {
								if (press.mouse.y == 1) {
									k = 0 - floor_list[j];
									break;
								}
								press.mouse.y--;
							}
						}
						/* check the bounds the item number */
						if (k < 0) {
							/* Allow player to "refuse" certain actions */
							if (!get_item_allow(k, cmdkey, cmd, is_harmless))
							{
								done = TRUE;
							}

							/* Accept that choice */
							(*cp) = k;
							item = TRUE;
							done = TRUE;
						} else {
							/* set k to a value that will be invalid below */
							k = -1;
						}
					}
				}
				if (k >= 0) {
					/* Validate the item */
					if (!item_test(tester, k)) {
						bell("Illegal object choice (normal)!");
					}

					/* Allow player to "refuse" certain actions */
					if (!get_item_allow(k, cmdkey, cmd, is_harmless))
						done = TRUE;

					/* Accept that choice */
					(*cp) = k;
					item = TRUE;
					done = TRUE;
				} else if (press.mouse.y == 0) {
					/* Hack -- Fix screen */
					if (show_list) {
						/* Load screen */
						screen_load();

						/* Save screen */
						screen_save();
					}
				}
			}
#endif
		} else
		switch (press.key.code)
		{
			case ESCAPE:
			case ' ':
			{
				done = TRUE;
				break;
			}

			case '/':
			{
				/* Toggle to inventory */
				if (use_inven && (player->upkeep->command_wrk != USE_INVEN))
					player->upkeep->command_wrk = USE_INVEN;

				/* Toggle to equipment */
				else if (use_equip &&
						 (player->upkeep->command_wrk != USE_EQUIP))
					player->upkeep->command_wrk = USE_EQUIP;

				/* No toggle allowed */
				else
				{
					bell("Cannot switch item selector!");
					break;
				}


				/* Hack -- Fix screen */
				if (show_list)
				{
					/* Load screen */
					screen_load();

					/* Save screen */
					screen_save();
				}

				/* Need to redraw */
				break;
			}

			case '.':
			{
				/* Paranoia */
				if (!allow_quiver)
				{
					bell("Cannot select quiver!");
					break;
				}

				/* Hack -- Fix screen */
				if (show_list)
				{
					/* Load screen */
					screen_load();

					/* Save screen */
					screen_save();
				}

				player->upkeep->command_wrk = (USE_QUIVER);

				break;
			}

			case '-':
			{
				/* Paranoia */
				if (!allow_floor)
				{
					bell("Cannot select floor!");
					break;
				}

				/* There is only one item */
				if (floor_num == 1)
				{
					/* Auto-select */
					if (player->upkeep->command_wrk == (USE_FLOOR))
					{
						/* Special index */
						k = 0 - floor_list[0];

						/* Allow player to "refuse" certain actions */
						if (!get_item_allow(k, cmdkey, cmd, is_harmless))
						{
							done = TRUE;
							break;
						}

						/* Accept that choice */
						(*cp) = k;
						item = TRUE;
						done = TRUE;

						break;
					}
				}

				/* Hack -- Fix screen */
				if (show_list)
				{
					/* Load screen */
					screen_load();

					/* Save screen */
					screen_save();
				}

				player->upkeep->command_wrk = (USE_FLOOR);

				break;
			}

			case '0':
			case '1': case '2': case '3':
			case '4': case '5': case '6':
			case '7': case '8': case '9':
			{
				/* Look up the tag */
				if (!get_tag(&k, press.key.code, cmd, quiver_tags))
				{
					bell("Illegal object choice (tag)!");
					break;
				}

				/* Hack -- Validate the item */
				if (!allow_quiver)
				{
					bell("Illegal object choice (tag)!");
					break;
				}

				/* Validate the item */
				if (!item_test(tester, k))
				{
					bell("Illegal object choice (tag)!");
					break;
				}

				/* Allow player to "refuse" certain actions */
				if (!get_item_allow(k, cmdkey, cmd, is_harmless))
				{
					done = TRUE;
					break;
				}

				/* Accept that choice */
				(*cp) = k;
				item = TRUE;
				done = TRUE;
				break;
			}

			case KC_ENTER:
			{
				/* Choose "default" inventory item */
				if (player->upkeep->command_wrk == USE_INVEN)
				{
					if (i1 != i2)
					{
						bell("Illegal object choice (default)!");
						break;
					}

					k = player->upkeep->inven[i1];
				}

				/* Choose the "default" slot (0) of the quiver */
				else if (quiver_tags)
					k = player->upkeep->quiver[q1];

				/* Choose "default" equipment item */
				else if (player->upkeep->command_wrk == USE_EQUIP)
				{
					if (e1 != e2)
					{
						bell("Illegal object choice (default)!");
						break;
					}

					k = slot_index(player, e1);
				}

				/* Choose "default" quiver item */
				else if (player->upkeep->command_wrk == USE_QUIVER)
				{
					if (q1 != q2)
					{
						bell("Illegal object choice (default)!");
						break;
					}

					k = player->upkeep->quiver[q1];
				}

				/* Choose "default" floor item */
				else
				{
					if (f1 != f2)
					{
						bell("Illegal object choice (default)!");
						break;
					}

					k = 0 - floor_list[f1];
				}

				/* Validate the item */
				if (!item_test(tester, k))
				{
					bell("Illegal object choice (default)!");
					break;
				}

				/* Allow player to "refuse" certain actions */
				if (!get_item_allow(k, cmdkey, cmd, is_harmless))
				{
					done = TRUE;
					break;
				}

				/* Accept that choice */
				(*cp) = k;
				item = TRUE;
				done = TRUE;
				break;
			}

			default:
			{
				bool verify;

				/* Note verify */
				verify = (isupper((unsigned char)press.key.code) ? TRUE : FALSE);

				/* Lowercase */
				press.key.code = tolower((unsigned char)press.key.code);

				/* Convert letter to inventory index */
				if (player->upkeep->command_wrk == USE_INVEN)
				{
					k = label_to_inven(press.key.code);

					if (k < 0)
					{
						bell("Illegal object choice (inven)!");
						break;
					}
				}

				/* Convert letter to equipment index */
				else if (player->upkeep->command_wrk == USE_EQUIP)
				{
					k = label_to_equip(press.key.code);

					if (k < 0)
					{
						bell("Illegal object choice (equip)!");
						break;
					}
				}

				/* Convert letter to quiver index */
				else if (player->upkeep->command_wrk == USE_QUIVER)
				{
					k = label_to_quiver(press.key.code);

					if (k < 0)
					{
						bell("Illegal object choice (quiver)!");
						break;
					}
				}

				/* Convert letter to floor index */
				else
				{
					k = (islower((unsigned char)press.key.code) ? A2I((unsigned char)press.key.code) : -1);

					if (k < 0 || k >= floor_num)
					{
						bell("Illegal object choice (floor)!");
						break;
					}

					/* Special index */
					k = 0 - floor_list[k];
				}

				/* Validate the item */
				if (!item_test(tester, k))
				{
					bell("Illegal object choice (normal)!");
					break;
				}

				/* Verify the item */
				if (verify && !verify_item("Try", k))
				{
					done = TRUE;
					break;
				}

				/* Allow player to "refuse" certain actions */
				if (!get_item_allow(k, cmdkey, cmd, is_harmless))
				{
					done = TRUE;
					break;
				}

				/* Accept that choice */
				(*cp) = k;
				item = TRUE;
				done = TRUE;
				break;
			}
		}
	}


	/* Fix the screen if necessary */
	if (show_list)
		screen_load();

	/* Toggle again if needed */
	if (toggle) toggle_inven_equip();

	/* Update */
	player->upkeep->redraw |= (PR_INVEN | PR_EQUIP);
	redraw_stuff(player->upkeep);


	/* Clear the prompt line */
	prt("", 0, 0);

	/* Warning if needed */
	if (oops && str) msg("%s", str);

	/* Result */
	return (item);
}

