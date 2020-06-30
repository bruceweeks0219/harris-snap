/*
	harris - a strategy game
	Copyright (C) 2012-2015 Edward Cree

	licensed under GPLv3+ - see top of harris.c for details
	
	control: the raid control screen
*/

#include <atg.h>

#include "types.h"

extern atg_element *control_box;

void game_preinit(game *state); // post-load and -setup, but pre-SCRN_CONTROL entry

void fill_flights(game *state);
bool ensure_crewed(game *state, unsigned int i);
void fixup_crew_assignments(game *state, unsigned int i, bool kill, double wskill);

extern int filter_nav[NNAVAIDS], filter_pff, filter_elite, filter_student;
