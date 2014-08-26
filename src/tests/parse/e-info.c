/* parse/e-info */

#include "unit-test.h"
#include "unit-test-data.h"
#include "obj-tval.h"
#include "object.h"
#include "init.h"


int setup_tests(void **state) {
	*state = init_parse_e();
	return !*state;
}

int teardown_tests(void *state) {
	parser_destroy(state);
	return 0;
}

int test_order(void *state) {
	enum parser_error r = parser_parse(state, "X:3:4");
	eq(r, PARSE_ERROR_MISSING_FIELD);
	ok;
}

int test_n0(void *state) {
	enum parser_error r = parser_parse(state, "N:5:of Resist Lightning");
	struct ego_item *e;

	eq(r, PARSE_ERROR_NONE);
	e = parser_priv(state);
	require(e);
	eq(e->eidx, 5);
	require(streq(e->name, "of Resist Lightning"));
	ok;
}

int test_x0(void *state) {
	enum parser_error r = parser_parse(state, "X:2:4:6:8");
	struct ego_item *e;

	eq(r, PARSE_ERROR_NONE);
	e = parser_priv(state);
	require(e);
	eq(e->level, 2);
	eq(e->rarity, 4);
	eq(e->cost, 6);
	eq(e->rating, 8);
	ok;
}

int test_c0(void *state) {
	enum parser_error r = parser_parse(state, "C:1d2:3d4:5d6");
	struct ego_item *e;

	eq(r, PARSE_ERROR_NONE);
	e = parser_priv(state);
	require(e);
	eq(e->to_h.dice, 1);
	eq(e->to_h.sides, 2);
	eq(e->to_d.dice, 3);
	eq(e->to_d.sides, 4);
	eq(e->to_a.dice, 5);
	eq(e->to_a.sides, 6);
	ok;
}

int test_m0(void *state) {
	enum parser_error r = parser_parse(state, "M:10:13:4");
	struct ego_item *e;

	eq(r, PARSE_ERROR_NONE);
	e = parser_priv(state);
	require(e);
	eq(e->min_to_h, 10);
	eq(e->min_to_d, 13);
	eq(e->min_to_a, 4);
	ok;
}

int test_f0(void *state) {
	enum parser_error r = parser_parse(state, "F:SEE_INVIS");
	struct ego_item *e;

	eq(r, PARSE_ERROR_NONE);
	e = parser_priv(state);
	require(e);
	require(e->flags);
	ok;
}

int test_d0(void *state) {
	enum parser_error r = parser_parse(state, "D:foo");
	struct ego_item *e;
	eq(r, PARSE_ERROR_NONE);
	r = parser_parse(state, "D: bar");
	eq(r, PARSE_ERROR_NONE);

	e = parser_priv(state);
	require(e);
	require(streq(e->text, "foo bar"));
	ok;
}

const char *suite_name = "parse/e-info";
struct test tests[] = {
	{ "order", test_order },
	{ "n0", test_n0 },
	{ "x0", test_x0 },
	{ "c0", test_c0 },
	{ "m0", test_m0 },
	{ "f0", test_f0 },
	{ "d0", test_d0 },
	{ NULL, NULL }
};
