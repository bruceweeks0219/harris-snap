#pragma once
/*
	harris - a strategy game
	Copyright (C) 2012-2013 Edward Cree

	licensed under GPLv3+ - see top of harris.c for details
	
	bits: misc. functions, mostly for string buffers
*/
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>

typedef struct
{
	char *buf; // string data buffer (may contain embedded NULs)
	size_t l; // size of RAM allocation
	size_t i; // length of string data
	// invariant: i<l
}
string;

#define min(a,b)	((a)<(b)?(a):(b))
#define max(a,b)	((a)>(b)?(a):(b))

char *fgetl(FILE *); // gets a line of string data; returns a malloc-like pointer
char *slurp(FILE *); // gets a file of string data; returns a malloc-like pointer
string sslurp(FILE *fp); // gets a file of string data possibly containing NULs; return contains a malloc-like pointer
string init_string(void); // initialises a string buffer in heap
string null_string(void); // returns a null string (no allocation)
string make_string(const char *str); // initialises a string buffer in heap, with initial contents copied from str
void append_char(string *s, char c); // adds a character to a string buffer in heap (and realloc()s if needed)
void append_str(string *s, const char *str); // adds a cstring to a string buffer in heap (and realloc()s if needed)
void append_string(string *s, const string t); // adds a string to a string buffer in heap (and realloc()s if needed)
void free_string(string *s); // frees a string (is just free(s->buf); really)

typedef uint32_t acid; // a/c ID

void pacid(acid id, char buf[8]); // print an a/c id as hex
int gacid(const char from[8], acid *buf); // parse an a/c id from hex
