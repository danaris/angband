/*
 * File: src/obj-flag.h
 * Purpose: definitions and functions for object flags
 *
 * Copyright (c) 2011 Chris Carr
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
#ifndef INCLUDED_OBJFLAG_H
#define INCLUDED_OBJFLAG_H

#include "z-file.h"
#include "z-bitflag.h"

/*** Constants ***/

/* The object flags */
enum {
    #define OF(a, b, c, d, e, f, g, h, i, j, k, l, m, n, o, p, q, r) OF_##a,
    #include "list-object-flags.h"
    #undef OF
};

/* The object kind flags */
enum {
    #define KF(a, b) KF_##a,
    #include "list-kind-flags.h"
    #undef KF
};

/* Object modifiers here for now too */
enum {
    #define OBJ_MOD(a, b, c, d, e, f, g, h, i, j, k, l, m, n, o, p, q, r) OBJ_MOD_##a,
    #include "list-object-modifiers.h"
    #undef OBJ_MOD
};

/* The object flag types */
enum object_flag_type {
	OFT_PVAL = 1,	/* pval-related but not to a stat */
	OFT_STAT,		/* affects a stat */
	OFT_SUST,		/* sustains a stat */
	OFT_SLAY,		/* a "normal" creature-type slay */
	OFT_BRAND,		/* a brand against monsters lacking the resist */
	OFT_KILL,		/* a powerful creature-type slay */
	OFT_VULN,		/* lowers resistance to an element */
	OFT_IMM,		/* offers immunity to an element */
	OFT_LRES,		/* a "base" elemental resistance */
	OFT_HRES,		/* a "high" elemental resistance */
	OFT_IGNORE,		/* object ignores an element */
	OFT_HATES,		/* object can be destroyed by element */
	OFT_PROT,		/* protection from an effect */
	OFT_MISC,		/* a good property, suitable for ego items */
	OFT_LIGHT,		/* applicable only to light sources */
	OFT_MELEE,		/* applicable only to melee weapons */
	OFT_CURSE,		/* a "sticky" curse */
	OFT_BAD,		/* an undesirable flag that isn't a curse */
	OFT_INT,		/* an internal flag, not shown in the game */

	OFT_MAX
};

/* How object flags are IDd */
enum object_flag_id {
	OFID_NONE = 0,		/* never shown */
	OFID_NORMAL,		/* normal ID on use */
	OFID_TIMED,			/* obvious after time */
	OFID_WIELD			/* obvious on wield */
};

#define OF_SIZE                	FLAG_SIZE(OF_MAX)

#define of_has(f, flag)        	flag_has_dbg(f, OF_SIZE, flag, #f, #flag)
#define of_next(f, flag)       	flag_next(f, OF_SIZE, flag)
#define of_is_empty(f)         	flag_is_empty(f, OF_SIZE)
#define of_is_full(f)          	flag_is_full(f, OF_SIZE)
#define of_is_inter(f1, f2)    	flag_is_inter(f1, f2, OF_SIZE)
#define of_is_subset(f1, f2)   	flag_is_subset(f1, f2, OF_SIZE)
#define of_is_equal(f1, f2)    	flag_is_equal(f1, f2, OF_SIZE)
#define of_on(f, flag)         	flag_on_dbg(f, OF_SIZE, flag, #f, #flag)
#define of_off(f, flag)        	flag_off(f, OF_SIZE, flag)
#define of_wipe(f)             	flag_wipe(f, OF_SIZE)
#define of_setall(f)           	flag_setall(f, OF_SIZE)
#define of_negate(f)           	flag_negate(f, OF_SIZE)
#define of_copy(f1, f2)        	flag_copy(f1, f2, OF_SIZE)
#define of_union(f1, f2)       	flag_union(f1, f2, OF_SIZE)
#define of_comp_union(f1, f2)  	flag_comp_union(f1, f2, OF_SIZE)
#define of_inter(f1, f2)       	flag_inter(f1, f2, OF_SIZE)
#define of_diff(f1, f2)        	flag_diff(f1, f2, OF_SIZE)

#define KF_SIZE                	FLAG_SIZE(KF_MAX)

#define kf_has(f, flag)        	flag_has_dbg(f, KF_SIZE, flag, #f, #flag)
#define kf_next(f, flag)       	flag_next(f, KF_SIZE, flag)
#define kf_is_empty(f)         	flag_is_empty(f, KF_SIZE)
#define kf_is_full(f)          	flag_is_full(f, KF_SIZE)
#define kf_is_inter(f1, f2)    	flag_is_inter(f1, f2, KF_SIZE)
#define kf_is_subset(f1, f2)   	flag_is_subset(f1, f2, KF_SIZE)
#define kf_is_equal(f1, f2)    	flag_is_equal(f1, f2, KF_SIZE)
#define kf_on(f, flag)         	flag_on_dbg(f, KF_SIZE, flag, #f, #flag)
#define kf_off(f, flag)        	flag_off(f, KF_SIZE, flag)
#define kf_wipe(f)             	flag_wipe(f, KF_SIZE)
#define kf_setall(f)           	flag_setall(f, KF_SIZE)
#define kf_negate(f)           	flag_negate(f, KF_SIZE)
#define kf_copy(f1, f2)        	flag_copy(f1, f2, KF_SIZE)
#define kf_union(f1, f2)       	flag_union(f1, f2, KF_SIZE)
#define kf_comp_union(f1, f2)  	flag_comp_union(f1, f2, KF_SIZE)
#define kf_inter(f1, f2)       	flag_inter(f1, f2, KF_SIZE)
#define kf_diff(f1, f2)        	flag_diff(f1, f2, KF_SIZE)


/*** Structures ***/

/**
 * The object flag structure
 */
struct object_flag {
	u16b index;				/* the OF_ index */
	bool pval;				/* is it granular (TRUE) or binary (FALSE) */
	u16b id;				/* how is it identified */
	u16b type;				/* OFT_ category */
	s16b power;				/* base power rating */
	s16b pval_mult;			/* pval weight rating */
	s16b weapon;			/* power mult for melee weapon */
	s16b bow;				/* power mult for launcher */
	s16b ring;				/* etc. ... */
	s16b amulet;
	s16b light;
	s16b body;
	s16b cloak;
	s16b shield;
	s16b hat;
	s16b gloves;
	s16b boots;
	const char *message;	/* id message */
};
	

/*** Functions ***/
bool cursed_p(const bitflag *f);
void create_mask(bitflag *f, bool id, ...);
void flag_message(int flag, char *name);
s32b flag_power(int flag);
void log_flags(bitflag *f, ang_file *log_file);
const char *flag_name(int flag);
s16b slot_mult(int flag, int slot);
int obj_flag_type(int flag);
s32b mod_power(int mod);
int mod_mult(int mod);
const char *mod_name(int mod);
s16b slot_mod_mult(int mod, int slot);

#endif /* !INCLUDED_OBJFLAG_H */
