/*
	harris - a strategy game
	Copyright (C) 2012-2015 Edward Cree

	licensed under GPLv3+ - see top of harris.c for details
	
	control: the raid control screen
*/

#include <stdio.h>
#include <stdlib.h>
#include <math.h>

#include "control.h"
#include "ui.h"
#include "globals.h"
#include "widgets.h"
#include "date.h"
#include "bits.h"
#include "almanack.h"
#include "history.h"
#include "render.h"
#include "routing.h"
#include "weather.h"
#include "intel_bombers.h"
#include "intel_targets.h"
#include "rand.h"
#include "run_raid.h"
#include "setup_difficulty.h"

extern game state;

atg_element *control_box;
atg_element *GB_resize, *GB_full, *GB_exit;
atg_element *GB_map;
atg_element *GB_overlay[NUM_OVERLAYS];
atg_element **GB_btrow, **GB_btpc, **GB_btnew, **GB_btp, **GB_btw, **GB_btpic, **GB_btint, **GB_navrow, *(*GB_navbtn)[NNAVAIDS], *(*GB_navgraph)[NNAVAIDS];
atg_element *GB_go, *GB_msgbox, *GB_msgrow[MAXMSGS], *GB_save, *GB_intel[3], *GB_hsquad, *GB_hcrews, *GB_cshort[CREW_CLASSES], *GB_diff, *GB_clamp;
atg_element *GB_ttl, *GB_train, **GB_ttrow, **GB_ttdmg, **GB_ttflk, **GB_ttint;
atg_element *GB_zhbox, *GB_zh, **GB_rbpic, **GB_rbrow, *(*GB_raidloadbox)[2], *(*GB_raidload)[2];
char **GB_btnum, **GB_raidnum, **GB_estcap;
char *GB_datestring, *GB_budget_label, *GB_confid_label, *GB_morale_label, *GB_raid_label;
char *GB_suntimes[2];
SDL_Surface *GB_moonimg, *GB_tfav[2], *GB_ifav[2];
int filter_nav[NNAVAIDS];
atg_element *GB_fi_nav[NNAVAIDS];
bool filter_marks[MAX_MARKS];
atg_element *GB_filter_marks;
bool filter_groups[7];
atg_element *GB_filter_groups;

bool filter_apply(const game *state, unsigned int i);
int update_raidbox(const game *state, int seltarg);
void update_btcount(const game *state, unsigned int type, bool shownav);
int update_raidnums(const game *state, int seltarg);

bool shortof[CREW_CLASSES];

int control_create(void)
{
	control_box=atg_create_element_box(ATG_BOX_PACK_HORIZONTAL, GAME_BG_COLOUR);
	if(!control_box)
	{
		fprintf(stderr, "atg_create_element_box failed\n");
		return(1);
	}
	atg_element *GB_bt=atg_create_element_box(ATG_BOX_PACK_VERTICAL, GAME_BG_COLOUR);
	if(!GB_bt)
	{
		fprintf(stderr, "atg_create_element_box failed\n");
		return(1);
	}
	if(atg_ebox_pack(control_box, GB_bt))
	{
		perror("atg_ebox_pack");
		return(1);
	}
	GB_datestring=malloc(11);
	if(!GB_datestring)
	{
		perror("malloc");
		return(1);
	}
	atg_element *GB_datetimebox=atg_create_element_box(ATG_BOX_PACK_HORIZONTAL, GAME_BG_COLOUR);
	if(!GB_datetimebox)
	{
		fprintf(stderr, "atg_create_element_box failed\n");
		return(1);
	}
	if(atg_ebox_pack(GB_bt, GB_datetimebox))
	{
		perror("atg_ebox_pack");
		return(1);
	}
	atg_element *GB_date=atg_create_element_label_nocopy(GB_datestring, 12, (atg_colour){175, 199, 255, ATG_ALPHA_OPAQUE});
	if(!GB_date)
	{
		fprintf(stderr, "atg_create_element_label failed\n");
		return(1);
	}
	GB_date->w=80;
	if(atg_ebox_pack(GB_datetimebox, GB_date))
	{
		perror("atg_ebox_pack");
		return(1);
	}
	GB_moonimg=SDL_CreateRGBSurface(SDL_HWSURFACE | SDL_SRCALPHA, 14, 14, 32, 0xff000000, 0xff0000, 0xff00, 0xff);
	if(!GB_moonimg)
	{
		fprintf(stderr, "moonimg: SDL_CreateRGBSurface: %s\n", SDL_GetError());
		return(1);
	}
	atg_element *GB_moonpic=atg_create_element_image(GB_moonimg);
	if(!GB_moonpic)
	{
		fprintf(stderr, "atg_create_element_image failed\n");
		return(1);
	}
	GB_moonpic->w=18;
	if(atg_ebox_pack(GB_datetimebox, GB_moonpic))
	{
		perror("atg_ebox_pack");
		return(1);
	}
	GB_resize=atg_create_element_image(resizebtn);
	if(!GB_resize)
	{
		fprintf(stderr, "atg_create_element_image failed\n");
		return(1);
	}
	GB_resize->w=16;
	GB_resize->clickable=true;
	if(atg_ebox_pack(GB_datetimebox, GB_resize))
	{
		perror("atg_ebox_pack");
		return(1);
	}
	GB_full=atg_create_element_image(fullbtn);
	if(!GB_full)
	{
		fprintf(stderr, "atg_create_element_image failed\n");
		return(1);
	}
	GB_full->w=16;
	GB_full->clickable=true;
	if(atg_ebox_pack(GB_datetimebox, GB_full))
	{
		perror("atg_ebox_pack");
		return(1);
	}
	GB_exit=atg_create_element_image(exitbtn);
	if(!GB_exit)
	{
		fprintf(stderr, "atg_create_element_image failed\n");
		return(1);
	}
	GB_exit->clickable=true;
	if(atg_ebox_pack(GB_datetimebox, GB_exit))
	{
		perror("atg_ebox_pack");
		return(1);
	}
	if(!(GB_btrow=calloc(ntypes, sizeof(atg_element *))))
	{
		perror("calloc");
		return(1);
	}
	if(!(GB_btpc=calloc(ntypes, sizeof(atg_element *))))
	{
		perror("calloc");
		return(1);
	}
	if(!(GB_btnew=calloc(ntypes, sizeof(atg_element *))))
	{
		perror("calloc");
		return(1);
	}
	if(!(GB_btp=calloc(ntypes, sizeof(atg_element *))))
	{
		perror("calloc");
		return(1);
	}
	if(!(GB_btw=calloc(ntypes, sizeof(atg_element *))))
	{
		perror("calloc");
		return(1);
	}
	if(!(GB_btnum=calloc(ntypes, sizeof(char *))))
	{
		perror("calloc");
		return(1);
	}
	if(!(GB_btpic=calloc(ntypes, sizeof(atg_element *))))
	{
		perror("calloc");
		return(1);
	}
	if(!(GB_btint=calloc(ntypes, sizeof(atg_element *))))
	{
		perror("calloc");
		return(1);
	}
	if(!(GB_navrow=calloc(ntypes, sizeof(atg_element *))))
	{
		perror("calloc");
		return(1);
	}
	if(!(GB_navbtn=calloc(ntypes, sizeof(atg_element * [NNAVAIDS]))))
	{
		perror("calloc");
		return(1);
	}
	if(!(GB_navgraph=calloc(ntypes, sizeof(atg_element * [NNAVAIDS]))))
	{
		perror("calloc");
		return(1);
	}
	for(unsigned int i=0;i<ntypes;i++)
	{
		if(!(GB_btrow[i]=atg_create_element_box(ATG_BOX_PACK_HORIZONTAL, (atg_colour){47, 31, 31, ATG_ALPHA_OPAQUE})))
		{
			fprintf(stderr, "atg_create_element_box failed\n");
			return(1);
		}
		if(atg_ebox_pack(GB_bt, GB_btrow[i]))
		{
			perror("atg_ebox_pack");
			return(1);
		}
		GB_btrow[i]->w=239;
		// Clone the picture, as we're going to write to it (grey overlay if target out of range)
		SDL_Surface *pic=SDL_ConvertSurface(types[i].picture, types[i].picture->format, SDL_HWSURFACE);
		if(!pic)
		{
			fprintf(stderr, "SDL_ConvertSurface: %s\n", SDL_GetError());
			return(1);
		}
		GB_btpic[i]=atg_create_element_image(pic);
		if(!GB_btpic[i])
		{
			fprintf(stderr, "atg_create_element_image failed\n");
			return(1);
		}
		GB_btpic[i]->w=38;
		GB_btpic[i]->clickable=true;
		if(atg_ebox_pack(GB_btrow[i], GB_btpic[i]))
		{
			perror("atg_ebox_pack");
			return(1);
		}
		atg_element *vbox=atg_create_element_box(ATG_BOX_PACK_VERTICAL, (atg_colour){47, 31, 31, ATG_ALPHA_OPAQUE});
		if(!vbox)
		{
			fprintf(stderr, "atg_create_element_box failed\n");
			return(1);
		}
		if(atg_ebox_pack(GB_btrow[i], vbox))
		{
			perror("atg_ebox_pack");
			return(1);
		}
		atg_element *nibox=atg_create_element_box(ATG_BOX_PACK_HORIZONTAL, (atg_colour){47, 31, 31, ATG_ALPHA_OPAQUE});
		if(!nibox)
		{
			fprintf(stderr, "atg_create_element_box failed\n");
			return(1);
		}
		if(atg_ebox_pack(vbox, nibox))
		{
			perror("atg_ebox_pack");
			return(1);
		}
		GB_btint[i]=atg_create_element_image(rawtypes[i].text?intelbtn:nointelbtn);
		if(!GB_btint[i])
		{
			fprintf(stderr, "atg_create_element_image failed\n");
			return(1);
		}
		GB_btint[i]->clickable=true;
		GB_btint[i]->w=10;
		GB_btint[i]->h=12;
		if(atg_ebox_pack(nibox, GB_btint[i]))
		{
			perror("atg_ebox_pack");
			return(1);
		}
		if(types[i].manu&&types[i].name)
		{
			size_t len=strlen(types[i].manu)+strlen(types[i].name)+2;
			char *fullname=malloc(len);
			if(fullname)
			{
				snprintf(fullname, len, "%s %s", types[i].manu, types[i].name);
				atg_element *btname=atg_create_element_label(fullname, 10, (atg_colour){175, 199, 255, ATG_ALPHA_OPAQUE});
				if(!btname)
				{
					fprintf(stderr, "atg_create_element_label failed\n");
					return(1);
				}
				btname->w=191;
				btname->cache=true;
				if(atg_ebox_pack(nibox, btname))
				{
					perror("atg_ebox_pack");
					return(1);
				}
			}
			else
			{
				perror("malloc");
				return(1);
			}
			free(fullname);
		}
		else
		{
			fprintf(stderr, "Missing manu or name in type %u\n", i);
			return(1);
		}
		if(!(GB_btnum[i]=malloc(20)))
		{
			perror("malloc");
			return(1);
		}
		snprintf(GB_btnum[i], 20, " ");
		atg_element *btnum=atg_create_element_label_nocopy(GB_btnum[i], 12, (atg_colour){159, 191, 255, ATG_ALPHA_OPAQUE});
		if(!btnum)
		{
			fprintf(stderr, "atg_create_element_label failed\n");
			return(1);
		}
		if(atg_ebox_pack(vbox, btnum))
		{
			perror("atg_ebox_pack");
			return(1);
		}
		atg_element *hbox=atg_create_element_box(ATG_BOX_PACK_HORIZONTAL, (atg_colour){47, 31, 31, ATG_ALPHA_OPAQUE});
		if(!hbox)
		{
			fprintf(stderr, "atg_create_element_box failed\n");
			return(1);
		}
		if(atg_ebox_pack(vbox, hbox))
		{
			perror("atg_ebox_pack");
			return(1);
		}
		if(atg_ebox_pack(hbox, types[i].prio_selector))
		{
			perror("atg_ebox_pack");
			return(1);
		}
		atg_element *pcbox=atg_create_element_box(ATG_BOX_PACK_VERTICAL, (atg_colour){47, 79, 31, ATG_ALPHA_OPAQUE});
		if(!pcbox)
		{
			fprintf(stderr, "atg_create_element_box failed\n");
			return(1);
		}
		pcbox->w=3;
		pcbox->h=18;
		if(atg_ebox_pack(hbox, pcbox))
		{
			perror("atg_ebox_pack");
			return(1);
		}
		if(!(GB_btpc[i]=atg_create_element_box(ATG_BOX_PACK_VERTICAL, (atg_colour){47, 47, 47, ATG_ALPHA_OPAQUE})))
		{
			fprintf(stderr, "atg_create_element_box failed\n");
			return(1);
		}
		GB_btpc[i]->w=3;
		if(atg_ebox_pack(pcbox, GB_btpc[i]))
		{
			perror("atg_ebox_pack");
			return(1);
		}
		if(!(GB_btnew[i]=atg_create_element_label("N", 12, (atg_colour){159, 191, 63, ATG_ALPHA_OPAQUE})))
		{
			fprintf(stderr, "atg_create_element_label failed\n");
			return(1);
		}
		if(atg_ebox_pack(hbox, GB_btnew[i]))
		{
			perror("atg_ebox_pack");
			return(1);
		}
		if(!(GB_btp[i]=atg_create_element_label("P!", 12, (atg_colour){191, 159, 31, ATG_ALPHA_OPAQUE})))
		{
			fprintf(stderr, "atg_create_element_label failed\n");
			return(1);
		}
		if(atg_ebox_pack(hbox, GB_btp[i]))
		{
			perror("atg_ebox_pack");
			return(1);
		}
		if(!(GB_btw[i]=atg_create_element_label("W!", 12, (atg_colour){191, 79, 79, ATG_ALPHA_OPAQUE})))
		{
			fprintf(stderr, "atg_create_element_label failed\n");
			return(1);
		}
		if(atg_ebox_pack(hbox, GB_btw[i]))
		{
			perror("atg_ebox_pack");
			return(1);
		}
		GB_navrow[i]=atg_create_element_box(ATG_BOX_PACK_HORIZONTAL, (atg_colour){47, 31, 31, ATG_ALPHA_OPAQUE});
		if(!GB_navrow[i])
		{
			fprintf(stderr, "atg_create_element_box failed\n");
			return(1);
		}
		if(atg_ebox_pack(vbox, GB_navrow[i]))
		{
			perror("atg_ebox_pack");
			return(1);
		}
		for(unsigned int n=0;n<NNAVAIDS;n++)
		{
			SDL_Surface *pic=SDL_CreateRGBSurface(SDL_HWSURFACE, 16, 16, navpic[n]->format->BitsPerPixel, navpic[n]->format->Rmask, navpic[n]->format->Gmask, navpic[n]->format->Bmask, navpic[n]->format->Amask);
			if(!pic)
			{
				fprintf(stderr, "pic=SDL_CreateRGBSurface: %s\n", SDL_GetError());
				return(1);
			}
			SDL_FillRect(pic, &(SDL_Rect){.x=0, .y=0, .w=16, .h=16}, SDL_MapRGBA(pic->format, 0, 0, 0, SDL_ALPHA_TRANSPARENT));
			SDL_BlitSurface(navpic[n], NULL, pic, NULL);
			if(!(GB_navbtn[i][n]=atg_create_element_image(pic)))
			{
				fprintf(stderr, "atg_create_element_image failed\n");
				return(1);
			}
			GB_navbtn[i][n]->w=GB_navbtn[i][n]->h=16;
			GB_navbtn[i][n]->clickable=true;
			if(!types[i].nav[n])
				SDL_FillRect(pic, &(SDL_Rect){.x=0, .y=0, .w=16, .h=16}, SDL_MapRGBA(pic->format, 63, 63, 63, SDL_ALPHA_OPAQUE));
			SDL_FreeSurface(pic); // Drop the extra reference
			if(atg_ebox_pack(GB_navrow[i], GB_navbtn[i][n]))
			{
				perror("atg_ebox_pack");
				return(1);
			}
			atg_element *graphbox=atg_create_element_box(ATG_BOX_PACK_VERTICAL, (atg_colour){47, 79, 31, ATG_ALPHA_OPAQUE});
			if(!graphbox)
			{
				fprintf(stderr, "atg_create_element_box failed\n");
				return(1);
			}
			graphbox->w=3;
			graphbox->h=16;
			if(atg_ebox_pack(GB_navrow[i], graphbox))
			{
				perror("atg_ebox_pack");
				return(1);
			}
			if(!(GB_navgraph[i][n]=atg_create_element_box(ATG_BOX_PACK_VERTICAL, (atg_colour){47, 47, 47, ATG_ALPHA_OPAQUE})))
			{
				fprintf(stderr, "atg_create_element_box failed\n");
				return(1);
			}
			GB_navgraph[i][n]->w=3;
			if(atg_ebox_pack(graphbox, GB_navgraph[i][n]))
			{
				perror("atg_ebox_pack");
				return(1);
			}
		}
	}
	atg_element *intelbox=atg_create_element_box(ATG_BOX_PACK_HORIZONTAL, GAME_BG_COLOUR);
	if(!intelbox)
	{
		fprintf(stderr, "atg_create_element_box failed\n");
		return(1);
	}
	intelbox->w=239;
	if(atg_ebox_pack(GB_bt, intelbox))
	{
		perror("atg_ebox_pack");
		return(1);
	}
	atg_element *intellbl=atg_create_element_label("Intel:", 24, (atg_colour){127, 159, 223, ATG_ALPHA_OPAQUE});
	if(!intellbl)
	{
		fprintf(stderr, "atg_create_element_label failed\n");
		return(1);
	}
	if(atg_ebox_pack(intelbox, intellbl))
	{
		perror("atg_ebox_pack");
		return(1);
	}
	for(unsigned int i=0;i<3;i++)
	{
		GB_intel[i]=atg_create_element_button_image(intelscreenbtn[i], (atg_colour){127, 159, 223, ATG_ALPHA_OPAQUE}, (atg_colour){63, 31, 31, ATG_ALPHA_OPAQUE});
		if(!GB_intel[i])
		{
			fprintf(stderr, "atg_create_element_button_image failed\n");
			return(1);
		}
		if(atg_ebox_pack(intelbox, GB_intel[i]))
		{
			perror("atg_ebox_pack");
			return(1);
		}
	}
	GB_hsquad=atg_create_element_button("Stations & Squadrons", (atg_colour){127, 223, 159, ATG_ALPHA_OPAQUE}, (atg_colour){31, 31, 63, ATG_ALPHA_OPAQUE});
	if(!GB_hsquad)
	{
		fprintf(stderr, "atg_create_element_button failed\n");
		return(1);
	}
	GB_hsquad->w=159;
	if(atg_ebox_pack(GB_bt, GB_hsquad))
	{
		perror("atg_ebox_pack");
		return(1);
	}
	GB_hcrews=atg_create_element_button("Crews & Training", (atg_colour){127, 223, 159, ATG_ALPHA_OPAQUE}, (atg_colour){31, 31, 63, ATG_ALPHA_OPAQUE});
	if(!GB_hcrews)
	{
		fprintf(stderr, "atg_create_element_button failed\n");
		return(1);
	}
	GB_hcrews->w=159;
	if(atg_ebox_pack(GB_bt, GB_hcrews))
	{
		perror("atg_ebox_pack");
		return(1);
	}
	atg_element *csbox=atg_create_element_box(ATG_BOX_PACK_HORIZONTAL, GAME_BG_COLOUR);
	if(!csbox)
	{
		fprintf(stderr, "atg_create_element_box failed\n");
		return(1);
	}
	csbox->w=159;
	if(atg_ebox_pack(GB_bt, csbox))
	{
		perror("atg_ebox_pack");
		return(1);
	}
	atg_element *csshim=atg_create_element_box(ATG_BOX_PACK_HORIZONTAL, GAME_BG_COLOUR);
	if(!csshim)
	{
		fprintf(stderr, "atg_create_element_box failed\n");
		return(1);
	}
	csshim->w=4;
	if(atg_ebox_pack(csbox, csshim))
	{
		perror("atg_ebox_pack");
		return(1);
	}
	atg_element *cslbl=atg_create_element_label("Crew req.: ", 12, (atg_colour){191, 127, 127, ATG_ALPHA_OPAQUE});
	if(!cslbl)
	{
		fprintf(stderr, "atg_create_element_label failed\n");
		return(1);
	}
	cslbl->w=155-10*CREW_CLASSES;
	if(atg_ebox_pack(csbox, cslbl))
	{
		perror("atg_ebox_pack");
		return(1);
	}
	for(unsigned int c=0;c<CREW_CLASSES;c++)
	{
		char text[2]={cclasses[c].letter, 0};
		GB_cshort[c]=atg_create_element_label(text, 12, (atg_colour){0, 0, 0, ATG_ALPHA_OPAQUE});
		if(!GB_cshort[c])
		{
			fprintf(stderr, "atg_create_element_label failed\n");
			return(1);
		}
		GB_cshort[c]->w=10;
		if(atg_ebox_pack(csbox, GB_cshort[c]))
		{
			perror("atg_ebox_pack");
			return(1);
		}
	}
	GB_diff=atg_create_element_button("Show Difficulty", (atg_colour){159, 159, 159, ATG_ALPHA_OPAQUE}, (atg_colour){47, 47, 31, ATG_ALPHA_OPAQUE});
	if(!GB_diff)
	{
		fprintf(stderr, "atg_create_element_button failed\n");
		return(1);
	}
	GB_diff->w=159;
	if(atg_ebox_pack(GB_bt, GB_diff))
	{
		perror("atg_ebox_pack");
		return(1);
	}
	GB_go=atg_create_element_button("Run tonight's raids", (atg_colour){159, 191, 255, ATG_ALPHA_OPAQUE}, (atg_colour){31, 63, 31, ATG_ALPHA_OPAQUE});
	if(!GB_go)
	{
		fprintf(stderr, "atg_create_element_button failed\n");
		return(1);
	}
	GB_go->w=159;
	if(atg_ebox_pack(GB_bt, GB_go))
	{
		perror("atg_ebox_pack");
		return(1);
	}
	GB_save=atg_create_element_button("Save game", (atg_colour){159, 255, 191, ATG_ALPHA_OPAQUE}, (atg_colour){31, 63, 31, ATG_ALPHA_OPAQUE});
	if(!GB_save)
	{
		fprintf(stderr, "atg_create_element_button failed\n");
		return(1);
	}
	GB_save->w=159;
	if(atg_ebox_pack(GB_bt, GB_save))
	{
		perror("atg_ebox_pack");
		return(1);
	}
	GB_clamp=atg_create_element_label("Base Weather limits ops", 10, (atg_colour){223, 15, 15, ATG_ALPHA_OPAQUE});
	if(!GB_clamp)
	{
		fprintf(stderr, "atg_create_element_label failed\n");
		return(1);
	}
	GB_clamp->w=159;
	if(atg_ebox_pack(GB_bt, GB_clamp))
	{
		perror("atg_ebox_pack");
		return(1);
	}
	GB_budget_label=malloc(32);
	if(!GB_budget_label)
	{
		perror("malloc");
		return(1);
	}
	atg_element *GB_budget=atg_create_element_label_nocopy(GB_budget_label, 12, (atg_colour){191, 255, 191, ATG_ALPHA_OPAQUE});
	if(!GB_budget)
	{
		fprintf(stderr, "atg_create_element_label failed\n");
		return(1);
	}
	GB_budget->w=159;
	if(atg_ebox_pack(GB_bt, GB_budget))
	{
		perror("atg_ebox_pack");
		return(1);
	}
	GB_confid_label=malloc(32);
	if(!GB_confid_label)
	{
		perror("malloc");
		return(1);
	}
	atg_element *GB_confid=atg_create_element_label_nocopy(GB_confid_label, 12, (atg_colour){191, 255, 191, ATG_ALPHA_OPAQUE});
	if(!GB_confid)
	{
		fprintf(stderr, "atg_create_element_label failed\n");
		return(1);
	}
	GB_confid->w=159;
	if(atg_ebox_pack(GB_bt, GB_confid))
	{
		perror("atg_ebox_pack");
		return(1);
	}
	GB_morale_label=malloc(32);
	if(!GB_morale_label)
	{
		perror("malloc");
		return(1);
	}
	atg_element *GB_morale=atg_create_element_label_nocopy(GB_morale_label, 12, (atg_colour){191, 255, 191, ATG_ALPHA_OPAQUE});
	if(!GB_morale)
	{
		fprintf(stderr, "atg_create_element_label failed\n");
		return(1);
	}
	GB_morale->w=159;
	if(atg_ebox_pack(GB_bt, GB_morale))
	{
		perror("atg_ebox_pack");
		return(1);
	}
	for(unsigned int i=0;i<2;i++)
	{
		atg_element *favbox=atg_create_element_box(ATG_BOX_PACK_HORIZONTAL, GAME_BG_COLOUR);
		if(!favbox)
		{
			fprintf(stderr, "atg_create_element_box failed\n");
			return(1);
		}
		favbox->w=239;
		if(atg_ebox_pack(GB_bt, favbox))
		{
			perror("atg_ebox_pack");
			return(1);
		}
		atg_element *favlbl=atg_create_element_label(i?"  Ignore:":"Priority:", 12, (atg_colour){255, 255, 191, ATG_ALPHA_OPAQUE});
		if(!favlbl)
		{
			fprintf(stderr, "atg_create_element_label failed\n");
			return(1);
		}
		if(atg_ebox_pack(favbox, favlbl))
		{
			perror("atg_ebox_pack");
			return(1);
		}
		GB_tfav[i]=SDL_CreateRGBSurface(SDL_HWSURFACE | SDL_SRCALPHA, 11, 11, 32, 0xff000000, 0xff0000, 0xff00, 0xff);
		if(!GB_tfav[i])
		{
			fprintf(stderr, "tfav[%d]: SDL_CreateRGBSurface: %s\n", i,SDL_GetError());
			return(1);
		}
		atg_element *tfav=atg_create_element_image(GB_tfav[i]);
		if(!tfav)
		{
			fprintf(stderr, "atg_create_element_image failed\n");
			return(1);
		}
		tfav->w=12;
		tfav->cache=false;
		if(atg_ebox_pack(favbox, tfav))
		{
			perror("atg_ebox_pack");
			return(1);
		}
		GB_ifav[i]=SDL_CreateRGBSurface(SDL_HWSURFACE | SDL_SRCALPHA, 11, 11, 32, 0xff000000, 0xff0000, 0xff00, 0xff);
		if(!GB_ifav[i])
		{
			fprintf(stderr, "ifav[%d]: SDL_CreateRGBSurface: %s\n", i,SDL_GetError());
			return(1);
		}
		atg_element *ifav=atg_create_element_image(GB_ifav[i]);
		if(!ifav)
		{
			fprintf(stderr, "atg_create_element_image failed\n");
			return(1);
		}
		ifav->w=12;
		ifav->cache=false;
		if(atg_ebox_pack(favbox, ifav))
		{
			perror("atg_ebox_pack");
			return(1);
		}
	}
	atg_element *GB_msglbl=atg_create_element_label("Messages:", 12, (atg_colour){255, 255, 255, ATG_ALPHA_OPAQUE});
	if(!GB_msglbl)
	{
		fprintf(stderr, "atg_create_element_label failed\n");
		return(1);
	}
	if(atg_ebox_pack(GB_bt, GB_msglbl))
	{
		perror("atg_ebox_pack");
		return(1);
	}
	if(!(GB_msgbox=atg_create_element_box(ATG_BOX_PACK_VERTICAL, GAME_BG_COLOUR)))
	{
		fprintf(stderr, "atg_create_element_box failed\n");
		return(1);
	}
	if(atg_ebox_pack(GB_bt, GB_msgbox))
	{
		perror("atg_ebox_pack");
		return(1);
	}
	atg_element *GB_mpad=atg_create_element_box(ATG_BOX_PACK_HORIZONTAL, GAME_BG_COLOUR);
	if(!GB_mpad)
	{
		fprintf(stderr, "atg_create_element_box failed\n");
		return(1);
	}
	if(atg_ebox_pack(GB_bt, GB_mpad))
	{
		perror("atg_ebox_pack");
		return(1);
	}
	GB_mpad->h=4;
	atg_element *GB_filters=atg_create_element_box(ATG_BOX_PACK_HORIZONTAL, GAME_BG_COLOUR);
	if(!GB_filters)
	{
		fprintf(stderr, "atg_create_element_box failed\n");
		return(1);
	}
	if(atg_ebox_pack(GB_bt, GB_filters))
	{
		perror("atg_ebox_pack");
		return(1);
	}
	atg_element *GB_ft=atg_create_element_label("Filters:", 12, (atg_colour){239, 239, 0, ATG_ALPHA_OPAQUE});
	if(!GB_ft)
	{
		fprintf(stderr, "atg_create_element_label failed\n");
		return(1);
	}
	if(atg_ebox_pack(GB_filters, GB_ft))
	{
		perror("atg_ebox_pack");
		return(1);
	}
	atg_element *GB_pad=atg_create_element_box(ATG_BOX_PACK_HORIZONTAL, GAME_BG_COLOUR);
	if(!GB_pad)
	{
		fprintf(stderr, "atg_create_element_box failed\n");
		return(1);
	}
	if(atg_ebox_pack(GB_filters, GB_pad))
	{
		perror("atg_ebox_pack");
		return(1);
	}
	GB_pad->w=4;
	for(unsigned int n=0;n<NNAVAIDS;n++)
	{
		filter_nav[n]=0;
		if(!(GB_fi_nav[n]=create_filter_switch(navpic[n], filter_nav+n)))
		{
			fprintf(stderr, "create_filter_switch failed\n");
			return(1);
		}
		if(atg_ebox_pack(GB_filters, GB_fi_nav[n]))
		{
			perror("atg_ebox_pack");
			return(1);
		}
	}
	if (!(GB_filter_marks=create_bfilter_switch(MAX_MARKS, GAME_BG_COLOUR, markpic, filter_marks)))
	{
		fprintf(stderr, "create_bfilter_switch failed\n");
		return(1);
	}
	if(atg_ebox_pack(GB_filters, GB_filter_marks))
	{
		perror("atg_ebox_pack");
		return(1);
	}
	atg_element *GB_gfilters=atg_create_element_box(ATG_BOX_PACK_HORIZONTAL, GAME_BG_COLOUR);
	if(!GB_gfilters)
	{
		fprintf(stderr, "atg_create_element_box failed\n");
		return(1);
	}
	if(atg_ebox_pack(GB_bt, GB_gfilters))
	{
		perror("atg_ebox_pack");
		return(1);
	}
	atg_element *GB_gft=atg_create_element_label("Groups:", 12, (atg_colour){239, 239, 0, ATG_ALPHA_OPAQUE});
	if(!GB_gft)
	{
		fprintf(stderr, "atg_create_element_label failed\n");
		return(1);
	}
	if(atg_ebox_pack(GB_gfilters, GB_gft))
	{
		perror("atg_ebox_pack");
		return(1);
	}
	if (!(GB_filter_groups=create_bfilter_switch(7, GAME_BG_COLOUR, grouppic, filter_groups)))
	{
		fprintf(stderr, "create_bfilter_switch failed\n");
		return(1);
	}
	if(atg_ebox_pack(GB_gfilters, GB_filter_groups))
	{
		perror("atg_ebox_pack");
		return(1);
	}
	atg_element *GB_middle=atg_create_element_box(ATG_BOX_PACK_VERTICAL, (atg_colour){31, 31, 39, ATG_ALPHA_OPAQUE});
	if(!GB_middle)
	{
		fprintf(stderr, "atg_create_element_box failed\n");
		return(1);
	}
	if(atg_ebox_pack(control_box, GB_middle))
	{
		perror("atg_ebox_pack");
		return(1);
	}
	GB_map=atg_create_element_image(terrain);
	if(!GB_map)
	{
		fprintf(stderr, "atg_create_element_image failed\n");
		return(1);
	}
	GB_map->h=terrain->h+2;
	GB_map->clickable=true;
	if(atg_ebox_pack(GB_middle, GB_map))
	{
		perror("atg_ebox_pack");
		return(1);
	}
	atg_element *overbox=atg_create_element_box(ATG_BOX_PACK_HORIZONTAL, (atg_colour){31, 31, 39, ATG_ALPHA_OPAQUE});
	if (!overbox)
	{
		fprintf(stderr, "atg_create_element_box failed\n");
		return(1);
	}
	overbox->w=256;
	if(atg_ebox_pack(GB_middle, overbox))
	{
		perror("atg_ebox_pack");
		return(1);
	}
	atg_element *overlbl=atg_create_element_label("Overlays: ", 12, (atg_colour){223, 223, 215, ATG_ALPHA_OPAQUE});
	if (!overlbl)
	{
		fprintf(stderr, "atg_create_element_label failed\n");
		return(1);
	}
	if(atg_ebox_pack(overbox, overlbl))
	{
		perror("atg_ebox_pack");
		return(1);
	}
	for(unsigned int i=0;i<NUM_OVERLAYS;i++)
	{
		GB_overlay[i]=atg_create_element_toggle_empty(overlays[i].selected, (atg_colour){223, 223, 215, ATG_ALPHA_OPAQUE}, (atg_colour){31, 31, 39, ATG_ALPHA_OPAQUE});
		if(!GB_overlay[i])
		{
			fprintf(stderr, "atg_create_element_toggle_empty failed\n");
			return(1);
		}
		if(atg_ebox_pack(overbox, GB_overlay[i]))
		{
			perror("atg_ebox_pack");
			return(1);
		}
		atg_element *icon=atg_create_element_image(overlays[i].icon);
		if(!icon)
		{
			fprintf(stderr, "atg_create_element_image failed\n");
			return(1);
		}
		if(atg_ebox_pack(GB_overlay[i], icon))
		{
			perror("atg_ebox_pack");
			return(1);
		}
	}
	atg_element *sunbox=atg_create_element_box(ATG_BOX_PACK_HORIZONTAL, (atg_colour){31, 31, 39, ATG_ALPHA_OPAQUE});
	if(!sunbox)
	{
		fprintf(stderr, "atg_create_element_box failed\n");
		return(1);
	}
	sunbox->w=256;
	if(atg_ebox_pack(GB_middle, sunbox))
	{
		perror("atg_ebox_pack");
		return(1);
	}
	atg_element *riselbl=atg_create_element_label("Sunrise: ", 12, (atg_colour){223, 223, 0, ATG_ALPHA_OPAQUE});
	if (!riselbl)
	{
		fprintf(stderr, "atg_create_element_label failed\n");
		return(1);
	}
	if(atg_ebox_pack(sunbox, riselbl))
	{
		perror("atg_ebox_pack");
		return(1);
	}
	if(!(GB_suntimes[0]=malloc(6)))
	{
		perror("malloc");
		return(1);
	}
	atg_element *risetime=atg_create_element_label_nocopy(GB_suntimes[0], 12, (atg_colour){223, 223, 0, ATG_ALPHA_OPAQUE});
	if(!risetime)
	{
		fprintf(stderr, "atg_create_element_label failed\n");
		return(1);
	}
	if(atg_ebox_pack(sunbox, risetime))
	{
		perror("atg_ebox_pack");
		return(1);
	}
	atg_element *setlbl=atg_create_element_label(" set: ", 12, (atg_colour){223, 223, 0, ATG_ALPHA_OPAQUE});
	if (!setlbl)
	{
		fprintf(stderr, "atg_create_element_label failed\n");
		return(1);
	}
	if(atg_ebox_pack(sunbox, setlbl))
	{
		perror("atg_ebox_pack");
		return(1);
	}
	if(!(GB_suntimes[1]=malloc(6)))
	{
		perror("malloc");
		return(1);
	}
	atg_element *settime=atg_create_element_label_nocopy(GB_suntimes[1], 12, (atg_colour){223, 223, 0, ATG_ALPHA_OPAQUE});
	if(!settime)
	{
		fprintf(stderr, "atg_create_element_label failed\n");
		return(1);
	}
	if(atg_ebox_pack(sunbox, settime))
	{
		perror("atg_ebox_pack");
		return(1);
	}
	if(!(GB_raid_label=malloc(48)))
	{
		perror("malloc");
		return(1);
	}
	snprintf(GB_raid_label, 48, "Select a Target");
	atg_element *raid_label=atg_create_element_label_nocopy(GB_raid_label, 12, (atg_colour){255, 255, 239, ATG_ALPHA_OPAQUE});
	if(!raid_label)
	{
		fprintf(stderr, "atg_create_element_label failed\n");
		return(1);
	}
	if(atg_ebox_pack(GB_middle, raid_label))
	{
		perror("atg_ebox_pack");
		return(1);
	}
	atg_element *GB_raid=atg_create_element_box(ATG_BOX_PACK_VERTICAL, (atg_colour){31, 31, 39, ATG_ALPHA_OPAQUE});
	if(!GB_raid)
	{
		fprintf(stderr, "atg_create_element_box failed\n");
		return(1);
	}
	if(atg_ebox_pack(GB_middle, GB_raid))
	{
		perror("atg_ebox_pack");
		return(1);
	}
	if(!(GB_rbrow=calloc(ntypes, sizeof(atg_element *))))
	{
		perror("calloc");
		return(1);
	}
	if(!(GB_rbpic=calloc(ntypes, sizeof(atg_element *))))
	{
		perror("calloc");
		return(1);
	}
	if(!(GB_raidnum=calloc(ntypes, sizeof(char *))))
	{
		perror("calloc");
		return(1);
	}
	if(!(GB_raidload=calloc(ntypes, sizeof(atg_element *[2]))))
	{
		perror("calloc");
		return(1);
	}
	if(!(GB_raidloadbox=calloc(ntypes, sizeof(atg_element *[2]))))
	{
		perror("calloc");
		return(1);
	}
	if(!(GB_estcap=calloc(ntypes, sizeof(char *))))
	{
		perror("calloc");
		return(1);
	}
	for(unsigned int i=0;i<ntypes;i++)
	{
		if(!(GB_rbrow[i]=atg_create_element_box(ATG_BOX_PACK_HORIZONTAL, (atg_colour){31, 31, 39, ATG_ALPHA_OPAQUE})))
		{
			fprintf(stderr, "atg_create_element_box failed\n");
			return(1);
		}
		if(atg_ebox_pack(GB_raid, GB_rbrow[i]))
		{
			perror("atg_ebox_pack");
			atg_free_element(GB_rbrow[i]);
			return(1);
		}
		GB_rbrow[i]->w=256;
		atg_element *picture=atg_create_element_image(types[i].picture);
		if(!picture)
		{
			fprintf(stderr, "atg_create_element_image failed\n");
			return(1);
		}
		picture->w=38;
		picture->cache=true;
		(GB_rbpic[i]=picture)->clickable=true;
		if(atg_ebox_pack(GB_rbrow[i], picture))
		{
			perror("atg_ebox_pack");
			atg_free_element(picture);
			return(1);
		}
		atg_element *vbox=atg_create_element_box(ATG_BOX_PACK_VERTICAL, (atg_colour){31, 31, 39, ATG_ALPHA_OPAQUE});
		if(!vbox)
		{
			fprintf(stderr, "atg_create_element_box failed\n");
			return(1);
		}
		vbox->w=186;
		if(atg_ebox_pack(GB_rbrow[i], vbox))
		{
			perror("atg_ebox_pack");
			atg_free_element(vbox);
			return(1);
		}
		if(types[i].manu&&types[i].name)
		{
			char fullname[96];
			snprintf(fullname, 96, "%s %s", types[i].manu, types[i].name);
			atg_element *name=atg_create_element_label(fullname, 10, (atg_colour){175, 199, 255, ATG_ALPHA_OPAQUE});
			if(!name)
			{
				fprintf(stderr, "atg_create_element_label failed\n");
				return(1);
			}
			name->cache=true;
			name->w=184;
			if(atg_ebox_pack(vbox, name))
			{
				perror("atg_ebox_pack");
				atg_free_element(name);
				return(1);
			}
		}
		else
		{
			fprintf(stderr, "Missing manu or name in type %u\n", i);
		}
		if(!(GB_raidnum[i]=malloc(32)))
		{
			perror("malloc");
			return(1);
		}
		atg_element *raidnum=atg_create_element_label_nocopy(GB_raidnum[i], 12, (atg_colour){159, 191, 255, ATG_ALPHA_OPAQUE});
		if(!raidnum)
		{
			fprintf(stderr, "atg_create_element_label failed\n");
			return(1);
		}
		if(atg_ebox_pack(vbox, raidnum))
		{
			perror("atg_ebox_pack");
			return(1);
		}
		for(int j=1;j>=0;j--)
		{
			if(!(GB_raidloadbox[i][j]=atg_create_element_box(ATG_BOX_PACK_HORIZONTAL, (atg_colour){31, 31, 39, ATG_ALPHA_OPAQUE})))
			{
				fprintf(stderr, "atg_create_element_box failed\n");
				return(1);
			}
			GB_raidloadbox[i][j]->w=16;
			if(atg_ebox_pack(GB_rbrow[i], GB_raidloadbox[i][j]))
			{
				perror("atg_ebox_pack");
				return(1);
			}
			GB_raidload[i][j]=NULL;
		}
		if(!(GB_estcap[i]=malloc(24)))
		{
			perror("malloc");
			return(1);
		}
		atg_element *estcap=atg_create_element_label_nocopy(GB_estcap[i], 9, (atg_colour){127, 127, 159, ATG_ALPHA_OPAQUE});
		if(!estcap)
		{
			fprintf(stderr, "atg_create_element_label failed\n");
			return(1);
		}
		if(atg_ebox_pack(vbox, estcap))
		{
			perror("atg_ebox_pack");
			return(1);
		}
	}
	if(!(GB_zhbox=atg_create_element_box(ATG_BOX_PACK_HORIZONTAL, (atg_colour){31, 31, 39, ATG_ALPHA_OPAQUE})))
	{
		fprintf(stderr, "atg_create_element_box failed\n");
		return(1);
	}
	if(atg_ebox_pack(GB_middle, GB_zhbox))
	{
		perror("atg_ebox_pack");
		return(1);
	}
	atg_element *shim=atg_create_element_box(ATG_BOX_PACK_HORIZONTAL, (atg_colour){31, 31, 39, ATG_ALPHA_OPAQUE});
	if(!shim)
	{
		fprintf(stderr, "atg_create_element_box failed\n");
		return(1);
	}
	shim->w=108;
	if(atg_ebox_pack(GB_zhbox, shim))
	{
		perror("atg_ebox_pack");
		return(1);
	}
	atg_element *zhlbl=atg_create_element_label("Zero Hour: ", 15, (atg_colour){127, 127, 127, ATG_ALPHA_OPAQUE});
	if(!zhlbl)
	{
		fprintf(stderr, "atg_create_element_label failed\n");
		return(1);
	}
	if(atg_ebox_pack(GB_zhbox, zhlbl))
	{
		perror("atg_ebox_pack");
		return(1);
	}
	if(!(GB_zh=create_time_spinner(0, RRT(18,0), RRT(6,0), 12, RRT(1,0), "%02u:%02u", (atg_colour){224, 224, 239, ATG_ALPHA_OPAQUE}, (atg_colour){39, 39, 47, ATG_ALPHA_OPAQUE})))
	{
		fprintf(stderr, "create_time_spinner failed\n");
		return(1);
	}
	if(atg_ebox_pack(GB_zhbox, GB_zh))
	{
		perror("atg_ebox_pack");
		return(1);
	}
	atg_element *GB_tt=atg_create_element_box(ATG_BOX_PACK_VERTICAL, (atg_colour){95, 95, 103, ATG_ALPHA_OPAQUE});
	if(!GB_tt)
	{
		fprintf(stderr, "atg_create_element_box failed\n");
		return(1);
	}
	if(atg_ebox_pack(control_box, GB_tt))
	{
		perror("atg_ebox_pack");
		return(1);
	}
	GB_ttl=atg_create_element_label("Targets", 12, (atg_colour){255, 255, 239, ATG_ALPHA_OPAQUE});
	if(!GB_ttl)
	{
		fprintf(stderr, "atg_create_element_label failed\n");
		return(1);
	}
	GB_ttl->clickable=true;
	if(atg_ebox_pack(GB_tt, GB_ttl))
	{
		perror("atg_ebox_pack");
		return(1);
	}
	if(!(GB_ttrow=calloc(ntargs, sizeof(atg_element *))))
	{
		perror("calloc");
		return(1);
	}
	if(!(GB_ttdmg=calloc(ntargs, sizeof(atg_element *))))
	{
		perror("calloc");
		return(1);
	}
	if(!(GB_ttflk=calloc(ntargs, sizeof(atg_element *))))
	{
		perror("calloc");
		return(1);
	}
	if(!(GB_ttint=calloc(ntargs, sizeof(atg_element *))))
	{
		perror("calloc");
		return(1);
	}
	for(unsigned int i=0;i<ntargs;i++)
	{
		GB_ttrow[i]=atg_create_element_box(ATG_BOX_PACK_HORIZONTAL, (atg_colour){95, 95, 103, ATG_ALPHA_OPAQUE});
		if(!GB_ttrow[i])
		{
			fprintf(stderr, "atg_create_element_box failed\n");
			return(1);
		}
		GB_ttrow[i]->w=305;
		GB_ttrow[i]->h=12;
		GB_ttrow[i]->clickable=true;
		if(atg_ebox_pack(GB_tt, GB_ttrow[i]))
		{
			perror("atg_ebox_pack");
			return(1);
		}
		GB_ttint[i]=atg_create_element_image(targs[i].p_intel?intelbtn:nointelbtn);
		if(!GB_ttint[i])
		{
			fprintf(stderr, "atg_create_element_image failed\n");
			return(1);
		}
		GB_ttint[i]->clickable=true;
		GB_ttint[i]->w=8;
		GB_ttint[i]->h=12;
		if(atg_ebox_pack(GB_ttrow[i], GB_ttint[i]))
		{
			perror("atg_ebox_pack");
			return(1);
		}
		atg_element *item=atg_create_element_label(targs[i].name, 10, (atg_colour){255, 255, 239, ATG_ALPHA_OPAQUE});
		if(!item)
		{
			fprintf(stderr, "atg_create_element_label failed\n");
			return(1);
		}
		item->w=192;
		item->cache=true;
		if(atg_ebox_pack(GB_ttrow[i], item))
		{
			perror("atg_ebox_pack");
			return(1);
		}
		item=atg_create_element_box(ATG_BOX_PACK_VERTICAL, (atg_colour){95, 95, 103, ATG_ALPHA_OPAQUE});
		if(!item)
		{
			fprintf(stderr, "atg_create_element_box failed\n");
			return(1);
		}
		item->w=105;
		if(atg_ebox_pack(GB_ttrow[i], item))
		{
			perror("atg_ebox_pack");
			return(1);
		}
		if(!(GB_ttdmg[i]=atg_create_element_box(ATG_BOX_PACK_HORIZONTAL, (atg_colour){0, 0, 255, ATG_ALPHA_OPAQUE})))
		{
			fprintf(stderr, "atg_create_element_box failed\n");
			return(1);
		}
		GB_ttdmg[i]->w=1;
		GB_ttdmg[i]->h=6;
		if(atg_ebox_pack(item, GB_ttdmg[i]))
		{
			perror("atg_ebox_pack");
			return(1);
		}
		if(!(GB_ttflk[i]=atg_create_element_box(ATG_BOX_PACK_HORIZONTAL, (atg_colour){183, 183, 199, ATG_ALPHA_OPAQUE})))
		{
			fprintf(stderr, "atg_create_element_box failed\n");
			return(1);
		}
		GB_ttflk[i]->w=1;
		GB_ttflk[i]->h=6;
		if(atg_ebox_pack(item, GB_ttflk[i]))
		{
			perror("atg_ebox_pack");
			return(1);
		}
	}
	if(!(GB_train=atg_create_element_label("Training unit", 10, (atg_colour){239, 239, 159, ATG_ALPHA_OPAQUE})))
	{
		fprintf(stderr, "atg_create_element_label failed\n");
		return(1);
	}
	GB_train->clickable=true;
	if(atg_ebox_pack(GB_tt, GB_train))
	{
		perror("atg_ebox_pack");
		return(1);
	}
	return(0);
}

screen_id control_screen(atg_canvas *canvas, game *state)
{
	atg_event e;
	
	if(state->weather.seed)
	{
		srand(state->weather.seed);
		w_init(&state->weather, 256, lorw);
		state->weather.seed=0;
	}
	
	snprintf(GB_datestring, 11, "%02u-%02u-%04u", state->now.day, state->now.month, state->now.year);
	snprintf(GB_budget_label, 32, "Budget: £%u/day", state->cshr);
	snprintf(GB_confid_label, 32, "Confidence: %u%%", (unsigned int)floor(state->confid+0.5));
	snprintf(GB_morale_label, 32, "Morale: %u%%", (unsigned int)floor(state->morale+0.5));
	for(unsigned int i=0;i<2;i++)
	{
		SDL_Surface *src;
		SDL_Rect all={0, 0, 11, 11};
		SDL_FillRect(GB_tfav[i], &all, SDL_MapRGB(GB_tfav[i]->format, 95, 95, 103));
		if(state->tfav[i]<TCLASS_INDUSTRY)
			src=ttype_icons[state->tfav[i]];
		else if(state->tfav[i]==TCLASS_INDUSTRY)
			src=ttype_icons[TCLASS_INDUSTRY+ICLASS_MIXED];
		else
			src=ttype_nothing;
		SDL_BlitSurface(src, &all, GB_tfav[i], &all);
		SDL_FillRect(GB_ifav[i], &all, SDL_MapRGB(GB_ifav[i]->format, 95, 95, 103));
		if(state->ifav[i]<I_CLASSES)
			src=ttype_icons[TCLASS_INDUSTRY+state->ifav[i]];
		else
			src=ttype_nothing;
		SDL_BlitSurface(src, &all, GB_ifav[i], &all);
	}
	double moonphase=pom(state->now);
	drawmoon(GB_moonimg, moonphase);
	memset(shortof, 0, sizeof(shortof));
	fill_flights(state);
	for(unsigned int j=0;j<state->nbombers;j++)
	{
		state->bombers[j].landed=true;
		state->bombers[j].damage=0;
		state->bombers[j].ld.ds=DS_NONE;
		if(!state->bombers[j].train && state->bombers[j].squadron>=0)
			ensure_crewed(state, j);
	}
	bool shownav=false;
	atg_element **fg_btns=GB_filter_groups->userdata;
	if((fg_btns[6]->hidden=datebefore(state->now, event[EVENT_PFF])))
		filter_groups[6]=false;
	for(unsigned int n=0;n<NNAVAIDS;n++)
	{
		GB_fi_nav[n]->hidden=datebefore(state->now, event[navevent[n]]);
		if(!datebefore(state->now, event[navevent[n]])) shownav=true;
	}
	for(unsigned int i=0;i<ntypes;i++)
		types[i].nestab=0;
	for(unsigned int b=0;b<nbases;b++)
	{
		bases[b].wp=england_weather_p(&state->weather, bases[b].lon, bases[b].lat);
		bases[b].wt=state->weather.t[(int)base_lon(bases[b])][(int)base_lat(bases[b])] + seasonal_temp(state->now) + ((int)bases[b].lat-160)*0.01;
		bases[b].clamped=(bases[b].wp+bases[b].wt)<(bases[b].paved?996.0:1001.0);
	}
	bool clampany=false, clamptype[ntypes];
	memset(clamptype, 0, sizeof(clamptype));
	for(unsigned int s=0;s<state->nsquads;s++)
	{
		unsigned int flights=state->squads[s].third_flight?3:2;
		unsigned int ucls[CREW_CLASSES]={0};
		unsigned int type=state->squads[s].btype;
		if(bases[state->squads[s].base].clamped)
		{
			clampany=true;
			clamptype[type]=true;
		}
		types[type].nestab+=flights*10;
		/* Count up unfilled I.E. of each crew class */
		for(unsigned int j=0;j<MAX_CREW;j++)
			for(unsigned int f=0;f<flights;f++)
				ucls[types[type].crew[j]]+=(10-state->squads[s].nb[f]);
		bool surplus=false;
		for(unsigned int c=0;c<CREW_CLASSES;c++)
			if(state->squads[s].nc[c]>ucls[c])
			{
				surplus=true;
				break;
			}
		if(surplus) /* Release surplus to group pool */
			for(unsigned int i=0;i<state->ncrews;i++)
			{
				if(state->crews[i].status!=CSTATUS_CREWMAN)
					continue;
				if(state->crews[i].squadron!=(int)s)
					continue;
				if(state->crews[i].assignment>=0)
					continue;
				if(ucls[state->crews[i].class])
				{
					ucls[state->crews[i].class]--;
					continue;
				}
				if(!(state->squads[s].nc[state->crews[i].class]--)) /* can't happen */
					fprintf(stderr, "Warning: sqn nc went negative for %d.%d\n", s, state->crews[i].class);
				state->crews[i].squadron=-1;
			}
	}
	GB_clamp->hidden=!clampany;
	for(unsigned int i=0;i<ntypes;i++)
	{
		if(GB_btrow[i])
			GB_btrow[i]->hidden=!datewithin(state->now, types[i].entry, types[i].train)||!state->btypes[i];
		if(GB_btpc[i])
			GB_btpc[i]->h=18-min(types[i].pcbuf/10000, 18);
		if(GB_btnew[i])
			GB_btnew[i]->hidden=!datebefore(state->now, types[i].novelty);
		if(GB_btp[i])
			GB_btp[i]->hidden=(types[i].pribuf<8)||(state->cash<newstats(types[i]).cost)||(types[i].pcbuf>=newstats(types[i]).cost);
		if(GB_btw[i])
			GB_btw[i]->hidden=!clamptype[i];
		update_btcount(state, i, shownav);
	}
	if(GB_go&&GB_go->elemdata&&((atg_button *)GB_go->elemdata)->content)
		((atg_button *)GB_go->elemdata)->content->bgcolour=(atg_colour){31, 63, 31, ATG_ALPHA_OPAQUE};
	if(GB_msgbox)
	{
		atg_ebox_empty(GB_msgbox);
		for(unsigned int i=0;i<MAXMSGS;i++)
		{
			if(state->msg[i])
			{
				char msgbuf[48];
				size_t j;
				for(j=0;state->msg[i][j]&&(state->msg[i][j]!='\n')&&j<47;j++)
					msgbuf[j]=state->msg[i][j];
				msgbuf[j]=0;
				if(!(GB_msgrow[i]=atg_create_element_button(msgbuf, (atg_colour){0, 0, 0, ATG_ALPHA_OPAQUE}, (atg_colour){255, 255, 239, ATG_ALPHA_OPAQUE})))
					continue; // nothing we can do about it
				GB_msgrow[i]->w=GB_btrow[0]->w;
				if(atg_ebox_pack(GB_msgbox, GB_msgrow[i])==0)
					continue; // we're good
			}
			GB_msgrow[i]=NULL;
		}
	}
	for(unsigned int i=0;i<ntargs;i++)
	{
		if(GB_ttrow[i])
			GB_ttrow[i]->hidden=!datewithin(state->now, targs[i].entry, targs[i].exit);
		if(GB_ttdmg[i])
			switch(targs[i].class)
			{
				case TCLASS_CITY:
				case TCLASS_LEAFLET: // for LEAFLET shows morale rather than damage
				case TCLASS_MINING: // for MINING shows how thoroughly mined the lane is
				case TCLASS_BRIDGE:
				case TCLASS_INDUSTRY:
				case TCLASS_AIRFIELD:
				case TCLASS_ROAD:
					GB_ttdmg[i]->w=floor(state->dmg[i]);
					GB_ttdmg[i]->hidden=false;
				break;
				case TCLASS_SHIPPING:
					GB_ttdmg[i]->w=0;
				break;
				default: // shouldn't ever get here
					fprintf(stderr, "Bad targs[%d].class = %d\n", i, targs[i].class);
					GB_ttdmg[i]->w=0;
				break;
			}
		if(GB_ttflk[i])
			GB_ttflk[i]->w=floor(state->flk[i]);
		for(unsigned int j=0;j<state->raids[i].nbombers;j++)
			state->bombers[state->raids[i].bombers[j]].landed=false;
	}
	atg_image *map_img=GB_map->elemdata;
	SDL_FreeSurface(map_img->data);
	map_img->data=SDL_ConvertSurface(terrain, terrain->format, terrain->flags);
	SDL_FreeSurface(flak_overlay);
	flak_overlay=render_flak(state->now);
	SDL_FreeSurface(city_overlay);
	city_overlay=render_cities();
	SDL_FreeSurface(target_overlay);
	target_overlay=render_targets(state->now);
	SDL_FreeSurface(weather_overlay);
	weather_overlay=render_weather(&state->weather);
	SDL_FreeSurface(xhair_overlay);
	xhair_overlay=render_xhairs(state);
	SDL_FreeSurface(seltarg_overlay);
	sun_precalc(state->now, &todays_delta, &todays_eqn);
	snprintf(GB_suntimes[0], 6, "--:--");
	int seltarg=-1;
	int routegrab=-1;
	seltarg_overlay=render_seltarg(seltarg);
	for(unsigned int i=0;i<NUM_OVERLAYS;i++)
	{
		atg_event e;
		if(atg_value_event(GB_overlay[i], &e)==0 && e.type==ATG_EV_TOGGLE)
			overlays[i].selected=e.event.toggle.state;
	}
	update_raidbox(state, seltarg);
	bool rfsh=true;
	while(1)
	{
		for(unsigned int c=0;c<CREW_CLASSES;c++)
		{
			atg_label *l=(atg_label *)GB_cshort[c]->elemdata;
			l->colour=shortof[c]?(atg_colour){231, 95, 95, ATG_ALPHA_OPAQUE}:(atg_colour){15, 31, 15, ATG_ALPHA_OPAQUE};
		}
		if(rfsh)
		{
			for(unsigned int i=0;i<ntargs;i++)
			{
				if(GB_ttrow[i]&&GB_ttrow[i]->elemdata)
				{
					unsigned int raid[ntypes];
					for(unsigned int j=0;j<ntypes;j++)
						raid[j]=0;
					for(unsigned int j=0;j<state->raids[i].nbombers;j++)
						raid[state->bombers[state->raids[i].bombers[j]].type]++;
					if((int)i==seltarg)
					{
						for(unsigned int j=0;j<ntypes;j++)
							if(GB_rbrow[j])
								GB_rbrow[j]->hidden=!datewithin(state->now, types[j].entry, types[j].exit)||!state->btypes[j]||!raid[j];
					}
					atg_box *b=GB_ttrow[i]->elemdata;
					b->bgcolour=(state->raids[i].nbombers?(atg_colour){127, 103, 95, ATG_ALPHA_OPAQUE}:(atg_colour){95, 95, 103, ATG_ALPHA_OPAQUE});
					if((int)i==seltarg)
					{
						b->bgcolour.r=b->bgcolour.g=b->bgcolour.r+64;
					}
					if(b->nelems>1)
					{
						if(b->elems[1]&&!strcmp(b->elems[1]->type, "__builtin_box")&&b->elems[1]->elemdata)
						{
							((atg_box *)b->elems[1]->elemdata)->bgcolour=b->bgcolour;
						}
					}
				}
			}
			update_raidnums(state, seltarg);
			bool avail[ntypes], reach[ntypes];
			memset(avail, 0, sizeof(avail));
			memset(reach, 0, sizeof(reach));
			if(seltarg>=0)
				for(unsigned int j=0;j<state->nbombers;j++)
				{
					unsigned int type=state->bombers[j].type;
					if(state->bombers[j].train) continue;
					if(state->bombers[j].squadron<0)
						continue;
					unsigned int s=state->bombers[j].squadron;
					if(state->squads[s].rtime)
						continue;
					unsigned int b=state->squads[s].base;
					if(bases[b].clamped) continue;
					signed int blon=base_lon(bases[b]), blat=base_lat(bases[b]);
					bool unpaved=types[type].heavy&&!bases[b].paved;
					double dist=hypot(blat-(signed)targs[seltarg].lat, blon-(signed)targs[seltarg].lon)*(types[type].ovltank?1.3:1.5);
					if(state->bombers[j].failed) continue;
					if(!state->bombers[j].landed) continue;
					if(!filter_apply(state, j)) continue;
					avail[type]=true;
					if(bstats(state->bombers[j]).range*(unpaved?0.8:1.0)>=dist)
						reach[type]=true;
				}
			for(unsigned int i=0;i<ntypes;i++)
			{
				if(!GB_btrow[i]->hidden&&GB_btpic[i])
				{
					atg_image *img=GB_btpic[i]->elemdata;
					if(img)
					{
						SDL_Surface *pic=img->data;
						SDL_FillRect(pic, &(SDL_Rect){0, 0, pic->w, pic->h}, SDL_MapRGB(pic->format, 0, 0, 0));
						SDL_BlitSurface(types[i].picture, NULL, pic, NULL);
						if((avail[i]&&!reach[i])||(seltarg==-2&&types[i].noarm))
							SDL_BlitSurface(grey_overlay, NULL, pic, NULL);
					}
				}
			}
			SDL_FreeSurface(map_img->data);
			map_img->data=SDL_ConvertSurface(terrain, terrain->format, terrain->flags);
			if (overlays[OVERLAY_CITY].selected)
				SDL_BlitSurface(city_overlay, NULL, map_img->data, NULL);
			if (overlays[OVERLAY_FLAK].selected)
				SDL_BlitSurface(flak_overlay, NULL, map_img->data, NULL);
			if (overlays[OVERLAY_TARGET].selected)
				SDL_BlitSurface(target_overlay, NULL, map_img->data, NULL);
			if (overlays[OVERLAY_WEATHER].selected)
				SDL_BlitSurface(weather_overlay, NULL, map_img->data, NULL);
			if (overlays[OVERLAY_ROUTE].selected)
			{
				route_overlay=render_current_route(state, seltarg);
				SDL_BlitSurface(route_overlay, NULL, map_img->data, NULL);
			}
			SDL_FreeSurface(xhair_overlay);
			xhair_overlay=render_xhairs(state);
			SDL_BlitSurface(xhair_overlay, NULL, map_img->data, NULL);
			SDL_FreeSurface(seltarg_overlay);
			seltarg_overlay=render_seltarg(seltarg);
			SDL_BlitSurface(seltarg_overlay, NULL, map_img->data, NULL);
			atg_flip(canvas);
			rfsh=false;
		}
		while(atg_poll_event(&e, canvas))
		{
			if(e.type!=ATG_EV_RAW) rfsh=true;
			switch(e.type)
			{
				case ATG_EV_RAW:;
					SDL_Event s=e.event.raw;
					switch(s.type)
					{
						case SDL_QUIT:
							do_quit:
							free(state->hist.ents);
							state->hist.ents=NULL;
							state->hist.nents=state->hist.nalloc=0;
							clear_raids(state);
							return(SCRN_MAINMENU);
						break;
						case SDL_MOUSEBUTTONUP:
							routegrab=-1;
						break;
						case SDL_MOUSEMOTION:;
							SDL_MouseMotionEvent me=s.motion;
							if(routegrab>=0)
							{
								int newx=targs[seltarg].route[routegrab][1]+me.xrel;
								clamp(newx, 0, 256);
								targs[seltarg].route[routegrab][1]=newx;
								int newy=targs[seltarg].route[routegrab][0]+me.yrel;
								clamp(newy, 0, 256);
								targs[seltarg].route[routegrab][0]=newy;
							}
							int mapx=me.x-239, mapy=me.y;
							if(mapx>=0&&mapx<256&&mapy>=0&&mapy<256)
							{
								double coords[2], rise, set;
								project_coords(mapy, mapx, coords);
								sun_calc(coords, todays_delta, todays_eqn, &rise, &set);
								harris_time rise_time=convert_ha(rise), set_time=convert_ha(set);
								snprintf(GB_suntimes[0], 6, "%02d:%02d", rise_time.hour, rise_time.minute);
								snprintf(GB_suntimes[1], 6, "%02d:%02d", set_time.hour, set_time.minute);
							}
							else
							{
								snprintf(GB_suntimes[0], 6, "--:--");
								snprintf(GB_suntimes[1], 6, "--:--");
							}
							rfsh=true;
						break;
						case SDL_VIDEORESIZE:
							rfsh=true;
						break;
					}
				break;
				case ATG_EV_CLICK:;
					atg_ev_click c=e.event.click;
					atg_mousebutton b=c.button;
					SDLMod m = SDL_GetModState();
					if(c.e)
					{
						for(unsigned int i=0;i<ntypes;i++)
						{
							if(!datewithin(state->now, types[i].entry, types[i].exit)) continue;
							for(unsigned int n=0;n<NNAVAIDS;n++)
							{
								if(c.e==GB_navbtn[i][n])
								{
									if(b==ATG_MB_LEFT)
									{
										if(m&KMOD_CTRL)
											state->nap[n]=-1;
										else if((m&KMOD_ALT)&&(m&KMOD_SHIFT))
											state->nap[n]=ntypes;
										else if(types[i].nav[n]&&!datebefore(state->now, event[navevent[n]]))
											state->nap[n]=i;
									}
									else if(b==ATG_MB_RIGHT)
										fprintf(stderr, "%ux %s in %ux %s %s\n", types[i].navcount[n], navaids[n], types[i].count, types[i].manu, types[i].name);
									else if(b==ATG_MB_MIDDLE)
										state->nap[n]=-1;
									for(unsigned int j=0;j<ntypes;j++)
										update_navbtn(*state, GB_navbtn, j, n, grey_overlay, yellow_overlay);
								}
							}
							switch(seltarg) {
							case -1:
								break;
							case -2:
								if(c.e==GB_btpic[i]&&!types[i].noarm)
								{
									unsigned int amount, count=0;
									switch(b)
									{
										case ATG_MB_RIGHT:
										case ATG_MB_SCROLLDN:
											amount=1;
										break;
										case ATG_MB_MIDDLE:
											amount=-1;
										break;
										case ATG_MB_LEFT:
											if(m&KMOD_CTRL)
											{
												amount=-1;
												break;
											}
											/* else fallthrough */
										case ATG_MB_SCROLLUP:
										default:
											amount=10;
									}
									for(unsigned int j=0;j<state->nbombers;j++)
									{
										if(state->bombers[j].type!=i) continue;
										if(state->bombers[j].train)
										{
											count++;
											continue;
										}
										if(!amount) continue;
										if(!state->bombers[j].landed) continue;
										if(!filter_apply(state, j)) continue;
										clear_sqn(state, j);
										clear_crew(state, j);
										amount--;
										count++;
										state->bombers[j].train=true;
									}
									if(GB_raidnum[i])
										snprintf(GB_raidnum[i], 32, "%u", count);
									update_btcount(state, i, shownav);
								}
								else if(c.e==GB_rbpic[i])
								{
									unsigned int amount, count=0;
									if(!datebefore(state->now, types[i].train))
										break;
									switch(b)
									{
										case ATG_MB_RIGHT:
										case ATG_MB_SCROLLDN:
											amount=1;
										break;
										case ATG_MB_MIDDLE:
											amount=-1;
										break;
										case ATG_MB_LEFT:
											if(m&KMOD_CTRL)
											{
												amount=-1;
												break;
											}
											/* else fallthrough */
										case ATG_MB_SCROLLUP:
										default:
											amount=10;
									}
									for(unsigned int k=0;k<state->nbombers;k++)
									{
										if(state->bombers[k].type!=i) continue;
										if(!state->bombers[k].train) continue;
										count++;
										if(!amount) continue;
										if(!filter_apply(state, k)) continue;
										clear_sqn(state, k);
										clear_crew(state, k);
										state->bombers[k].train=false;
										amount--;
										count--;
									}
									if(GB_raidnum[i])
										snprintf(GB_raidnum[i], 32, "%u", count);
									update_btcount(state, i, shownav);
								}
								/* Some a/c or crews may have been made available */
								fill_flights(state);
								for(unsigned int k=0;k<state->nbombers;k++)
									if(!state->bombers[k].train && state->bombers[k].squadron>=0)
										ensure_crewed(state, k);
								break;
							default:
								if(c.e==GB_btpic[i])
								{
									unsigned int amount;
									switch(b)
									{
										case ATG_MB_RIGHT:
										case ATG_MB_SCROLLDN:
											amount=1;
										break;
										case ATG_MB_MIDDLE:
											amount=-1;
										break;
										case ATG_MB_LEFT:
											if(m&KMOD_CTRL)
											{
												amount=-1;
												break;
											}
											/* else fallthrough */
										case ATG_MB_SCROLLUP:
										default:
											amount=10;
									}
									memset(shortof, 0, sizeof(shortof));
									for(unsigned int j=0;j<state->nbombers;j++)
									{
										if(state->bombers[j].type!=i) continue;
										if(state->bombers[j].train) continue;
										if(state->bombers[j].squadron<0)
											continue;
										unsigned int s=state->bombers[j].squadron;
										if(state->squads[s].rtime)
											continue;
										unsigned int b=state->squads[s].base;
										if(bases[b].clamped) continue;
										signed int blon=base_lon(bases[b]), blat=base_lat(bases[b]);
										bool unpaved=types[i].heavy&&!bases[b].paved;
										double dist=hypot(blat-(signed)targs[seltarg].lat, blon-(signed)targs[seltarg].lon)*(types[i].ovltank?1.3:1.5);
										if(newstats(types[i]).range*(unpaved?0.8:1.0)<dist)
											continue;
										if(state->bombers[j].failed) continue;
										if(!state->bombers[j].landed) continue;
										if(!filter_apply(state, j)) continue;
										if(!ensure_crewed(state, j)) continue;
										state->bombers[j].landed=false;
										amount--;
										unsigned int n=state->raids[seltarg].nbombers++;
										unsigned int *new=realloc(state->raids[seltarg].bombers, state->raids[seltarg].nbombers*sizeof(unsigned int));
										if(!new)
										{
											perror("realloc");
											state->raids[seltarg].nbombers=n;
											break;
										}
										(state->raids[seltarg].bombers=new)[n]=j;
										if(!amount) break;
									}
									if(GB_raidnum[i])
									{
										unsigned int count=0;
										for(unsigned int j=0;j<state->raids[seltarg].nbombers;j++)
											if(state->bombers[state->raids[seltarg].bombers[j]].type==i) count++;
										snprintf(GB_raidnum[i], 32, "%u", count);
									}
									if(state->raids[seltarg].nbombers&&!datebefore(state->now, event[EVENT_GEE])&&!state->raids[seltarg].routed)
									{
										genroute((unsigned int [2]){0, 0}, seltarg, targs[seltarg].route, state, 10000);
										state->raids[seltarg].routed=true;
									}
									update_raidbox(state, seltarg);
								}
								if(c.e==GB_rbpic[i])
								{
									unsigned int amount;
									switch(b)
									{
										case ATG_MB_RIGHT:
										case ATG_MB_SCROLLDN:
											amount=1;
										break;
										case ATG_MB_MIDDLE:
											amount=-1;
										break;
										case ATG_MB_LEFT:
											if(m&KMOD_CTRL)
											{
												amount=-1;
												break;
											}
											/* else fallthrough */
										case ATG_MB_SCROLLUP:
										default:
											amount=10;
									}
									for(unsigned int l=0;l<state->raids[seltarg].nbombers;l++)
									{
										unsigned int k=state->raids[seltarg].bombers[l];
										if(state->bombers[k].type!=i) continue;
										if(!filter_apply(state, k)) continue;
										state->bombers[k].landed=true;
										amount--;
										state->raids[seltarg].nbombers--;
										for(unsigned int k=l;k<state->raids[seltarg].nbombers;k++)
											state->raids[seltarg].bombers[k]=state->raids[seltarg].bombers[k+1];
										if(!amount) break;
										l--;
									}
									if(GB_raidnum[i])
									{
										unsigned int count=0;
										for(unsigned int l=0;l<state->raids[seltarg].nbombers;l++)
											if(state->bombers[state->raids[seltarg].bombers[l]].type==i) count++;
										snprintf(GB_raidnum[i], 32, "%u", count);
									}
									update_raidbox(state, seltarg);
								}
								break;
							}
							if(c.e==GB_btint[i])
							{
								IB_i=i;
								IB_showmark=types[i].newmark;
								intel_caller=SCRN_CONTROL;
								return(SCRN_INTELBMB);
							}
						}
						if(c.e==GB_ttl)
						{
							seltarg=-1;
							update_raidbox(state, seltarg);
						}
						if(c.e==GB_train)
						{
							seltarg=-2;
							update_raidbox(state, seltarg);
						}
						for(unsigned int i=0;i<ntargs;i++)
						{
							if(!datewithin(state->now, targs[i].entry, targs[i].exit)) continue;
							if(c.e==GB_ttint[i])
							{
								IT_i=i;
								intel_caller=SCRN_CONTROL;
								return(SCRN_INTELTRG);
							}
							if(c.e==GB_ttrow[i])
							{
								seltarg=i;
								update_raidbox(state, seltarg);
							}
						}
						if(c.e==GB_resize)
						{
							mainsizex=default_w;
							mainsizey=default_h;
							atg_resize_canvas(canvas, mainsizex, mainsizey);
						}
						if(c.e==GB_full)
						{
							fullscreen=!fullscreen;
							atg_setopts_canvas(canvas, fullscreen?SDL_FULLSCREEN:SDL_RESIZABLE);
							rfsh=true;
						}
						if(c.e==GB_map&&seltarg>=0&&state->raids[seltarg].routed)
						{
							double mind=26;
							int mins=-1;
							for(unsigned int stage=0;stage<8;stage++)
							{
								int dx=c.pos.x-targs[seltarg].route[stage][1],
								    dy=c.pos.y-targs[seltarg].route[stage][0];
								if(dx*dx+dy*dy<mind)
								{
									mind=dx*dx+dy*dy;
									mins=stage;
								}
							}
							if((routegrab=mins)==4) // don't let them move the A.P.
								routegrab=-1;
						}
						if(c.e==GB_exit)
							goto do_quit;
					}
				break;
				case ATG_EV_TRIGGER:;
					atg_ev_trigger trigger=e.event.trigger;
					if(trigger.e)
					{
						if(trigger.e==GB_go)
						{
							if(GB_go&&GB_go->elemdata&&((atg_button *)GB_go->elemdata)->content)
								((atg_button *)GB_go->elemdata)->content->bgcolour=(atg_colour){55, 55, 55, ATG_ALPHA_OPAQUE};
							atg_flip(canvas);
							return(SCRN_RUNRAID);
						}
						else if(trigger.e==GB_intel[0])
						{
							intel_caller=SCRN_CONTROL;
							return(SCRN_INTELBMB);
						}
						else if(trigger.e==GB_intel[1])
						{
							intel_caller=SCRN_CONTROL;
							return(SCRN_INTELFTR);
						}
						else if(trigger.e==GB_intel[2])
						{
							intel_caller=SCRN_CONTROL;
							return(SCRN_INTELTRG);
						}
						else if(trigger.e==GB_hsquad)
						{
							return(SCRN_SQUADRONS);
						}
						else if(trigger.e==GB_hcrews)
						{
							return(SCRN_HCREWS);
						}
						else if(trigger.e==GB_diff)
						{
							difficulty_show_only=true;
							return(SCRN_SETPDIFF);
						}
						else if(trigger.e==GB_save)
						{
							return(SCRN_SAVEGAME);
						}
						for(unsigned int i=0;i<MAXMSGS;i++)
							if((trigger.e==GB_msgrow[i])&&state->msg[i])
							{
								message_box(canvas, "To the Commander-in-Chief, Bomber Command:", state->msg[i], "Air Chief Marshal C. F. A. Portal, CAS");
							}
					}
				break;
				case ATG_EV_TOGGLE:;
					atg_ev_toggle toggle=e.event.toggle;
					if(toggle.e)
					{
						unsigned int i;
						for(i=0;i<NUM_OVERLAYS;i++)
						{
							if(toggle.e==GB_overlay[i])
							{
								overlays[i].selected=toggle.state;
								break;
							}
						}
						if(i==NUM_OVERLAYS)
						{
							fprintf(stderr, "Clicked on an unknown toggle\n");
						}
					}
				break;
				case ATG_EV_VALUE:;
					atg_ev_value value=e.event.value;
					if(value.e)
					{
						if(value.e==GB_zh)
						{
							if(seltarg<0)
								fprintf(stderr, "Ignored zerohour change (to %d) with bad seltarg %d\n", value.value, seltarg);
							else
								state->raids[seltarg].zerohour=value.value;
						}
					}
				break;
				default:
					fprintf(stderr, "Received unknown event, e.type %d\n", e.type);
				break;
			}
		}
		SDL_Delay(50);
		#if 0 // for testing weathersim code
		rfsh=true;
		for(unsigned int i=0;i<16;i++)
			w_iter(&state->weather, lorw);
		SDL_FreeSurface(map_img->data);
		map_img->data=SDL_ConvertSurface(terrain, terrain->format, terrain->flags);
		SDL_FillRect(map_img->data, &(SDL_Rect){.x=0, .y=0, .w=terrain->w, .h=terrain->h}, SDL_MapRGB(terrain->format, 0, 0, 0));
		SDL_FreeSurface(weather_overlay);
		weather_overlay=render_weather(&state->weather);
		SDL_BlitSurface(weather_overlay, NULL, map_img->data, NULL);
		SDL_FreeSurface(xhair_overlay);
		xhair_overlay=render_xhairs(state);
		SDL_BlitSurface(xhair_overlay, NULL, map_img->data, NULL);
		seltarg_overlay=render_seltarg(seltarg);
		SDL_BlitSurface(seltarg_overlay, NULL, map_img->data, NULL);
		#endif
	}
}

void control_free(void)
{
	SDL_FreeSurface(GB_moonimg);
	atg_free_element(control_box);
}

bool filter_apply(const game *state, unsigned int i)
{
	ac_bomber b=state->bombers[i];
	for(unsigned int n=0;n<NNAVAIDS;n++)
	{
		if(filter_nav[n]>0&&!b.nav[n]) return(false);
		if(filter_nav[n]<0&&b.nav[n]) return(false);
	}
	if(!filter_marks[b.mark])
		for(unsigned int m=0;m<MAX_MARKS;m++)
			if(filter_marks[m])
				return(false);
	if(b.squadron<0)
		return(true);
	bool agroup=false;
	for(unsigned int g=0;g<7;g++)
		if(filter_groups[g])
			agroup=true;
	unsigned int s=b.squadron, g=base_grp(bases[state->squads[s].base])-1;
	if(g>=7)
		g=6;
	if(agroup && !filter_groups[g])
		return(false);
	return(true);
}

void clear_sqn(game *state, unsigned int i)
{
	int s=state->bombers[i].squadron, f=state->bombers[i].flight;
	if(s>=0 && f>=0)
		state->squads[s].nb[f]--;
	state->bombers[i].squadron=-1;
	state->bombers[i].flight=-1;
}

void clear_crew(game *state, unsigned int i)
{
	unsigned int type=state->bombers[i].type;
	for(unsigned int j=0;j<MAX_CREW;j++)
	{
		if(types[type].crew[j]==CCLASS_NONE)
			continue;
		int k=state->bombers[i].crew[j];
		if(k>=0)
		{
			state->crews[k].assignment=-1;
			state->bombers[i].crew[j]=-1;
			int s=state->crews[k].squadron;
			if(s>=0)
				state->squads[s].nc[state->crews[k].class]++;
		}
	}
}

void fill_flights(game *state)
{
	for(unsigned int t=0;t<ntypes;t++)
	{
		unsigned int minf=10;
		for(unsigned int s=0;s<state->nsquads;s++)
		{
			if(state->squads[s].btype!=t)
				continue;
			for(unsigned int f=0;f<(state->squads[s].third_flight?3:2);f++)
				minf=min(minf, state->squads[s].nb[f]);
		}
		if(minf==10) /* none of this type needed */
			continue;
		for(unsigned int i=0;i<state->nbombers;i++)
		{
			if(state->bombers[i].type!=t)
				continue;
			if(state->bombers[i].train)
				continue;
			if(state->bombers[i].squadron>=0)
			{
				if(state->bombers[i].flight<0)
					fprintf(stderr, "Warning: bomber %u has sqn (%d) but no flt (%d)\n", i, state->bombers[i].squadron, state->bombers[i].flight);
				continue;
			}
			for(;minf<10;minf++)
			{
				for(unsigned int s=0;s<state->nsquads;s++)
				{
					if(state->squads[s].btype!=t)
						continue;
					for(unsigned int f=0;f<(state->squads[s].third_flight?3:2);f++)
						if(state->squads[s].nb[f]<=minf)
						{
							state->bombers[i].squadron=s;
							state->bombers[i].flight=f;
							state->squads[s].nb[f]++;
							ensure_crewed(state, i);
							break;
						}
					if(state->bombers[i].squadron>=0)
						break;
				}
				if(state->bombers[i].squadron>=0)
					break;
			}
		}
	}
}

bool fully_crewed(const game *state, unsigned int i)
{
	for(unsigned int j=0;j<MAX_CREW;j++)
	{
		if(types[state->bombers[i].type].crew[j]==CCLASS_NONE)
			continue;
		if(state->bombers[i].crew[j]<0)
			return false;
	}
	return true;
}

void swap_crews(game *state, unsigned int i, unsigned int k)
{
	if(state->bombers[i].type!=state->bombers[k].type)
		/* can't happen */
		fprintf(stderr, "Warning: swapping crews between different types %s (bi %u) and %s (bi %u)\n", types[state->bombers[i].type].name, i, types[state->bombers[k].type].name, k);
	for(unsigned int j=0;j<MAX_CREW;j++)
	{
		int a=state->bombers[i].crew[j];
		int b=state->bombers[k].crew[j];
		state->bombers[i].crew[j]=b;
		if(b>=0)
			state->crews[b].assignment=i;
		state->bombers[k].crew[j]=a;
		if(a>=0)
			state->crews[a].assignment=k;
	}
}

#define CREWOPS(k)	(state->crews[(k)].tour_ops + 30 * state->crews[(k)].full_tours) // TODO second tours should only be 20 ops

bool ensure_crewed(game *state, unsigned int i)
{
	unsigned int type=state->bombers[i].type;
	bool heavy=types[type].heavy;
	bool lanc=types[type].lfs;
	bool ok=true;
	int s=state->bombers[i].squadron, f=state->bombers[i].flight;
	if(s<0 || f<0)
		return false;
	unsigned int grp=base_grp(bases[state->squads[s].base]);
	for(unsigned int j=0;j<MAX_CREW;j++)
	{
		if(types[type].crew[j]==CCLASS_NONE)
			continue;
		int k=state->bombers[i].crew[j];
		// 0. Deassign current crewman if unsuitable
		if(k>=0 && state->crews[k].class!=types[type].crew[j]) // cclass changed under us
		{
			int cs=state->crews[k].squadron;
			if(state->crews[k].assignment!=(int)i) /* can't happen */
				fprintf(stderr, "Warning: crew assignments inconsistent (bomber %u had crew %d assigned %d\n", i, k, state->crews[k].assignment);
			else if(cs>=0)
				state->squads[cs].nc[state->crews[k].class]++;
			state->crews[k].assignment=-1;
			state->bombers[i].crew[j]=-1;
		}
		// 1. Look for an available (unassigned) CREWMAN within the squadron
		if(state->bombers[i].crew[j]<0)
		{
			int best=-1;
			for(unsigned int k=0;k<state->ncrews;k++)
			{
				if(state->crews[k].class!=types[type].crew[j])
					continue;
				if(state->crews[k].status!=CSTATUS_CREWMAN)
					continue;
				if(state->crews[k].squadron!=s)
					continue;
				if(best<0 ||
					   (lanc ? (0.25+state->crews[k].lanc)*(0.25+state->crews[k].heavy) >
						   (0.25+state->crews[best].lanc)*(0.25+state->crews[best].heavy) :
					    heavy ? (state->crews[k].heavy > state->crews[best].heavy ||
						     (state->crews[k].heavy >= 100.0 && state->crews[k].lanc < state->crews[best].lanc)) :
					    state->crews[k].heavy < state->crews[best].heavy))
				{
					if(state->crews[k].assignment<0||(state->bombers[state->crews[k].assignment].failed&&!state->bombers[i].failed&&!fully_crewed(state, state->crews[k].assignment)))
					{
						best=k;
						if(lanc ? (state->crews[k].lanc >= 100.0 && state->crews[k].heavy >= 100.0) :
						   heavy ? (state->crews[k].lanc <= 0.0 && state->crews[k].heavy >= 100.0) :
						   state->crews[k].heavy <= 0.0)
							break; /* won't improve on this */
					}
				}
			}
			if(best>=0)
			{
				if(state->crews[best].assignment<0)
				{
					int cs=state->crews[best].squadron;
					if(cs>=0 && !(state->squads[cs].nc[state->crews[best].class]--)) /* can't happen */
						fprintf(stderr, "Warning: sqn nc went negative for %d.%d\n", cs, state->crews[best].class);
				}
				else
				{
					unsigned int k=state->crews[best].assignment, c;
					for(c=0;c<MAX_CREW;c++)
					{
						if(state->bombers[k].crew[c]==best)
							state->bombers[k].crew[c]=-1;
					}
				}
				state->crews[best].squadron=s;
				state->crews[best].assignment=i;
				state->bombers[i].crew[j]=best;
				if(state->crews[best].group!=grp)
				{
					if (state->crews[best].group) /* can't happen */
						fprintf(stderr, "Warning: crewman %d jumped groups\n", best);
					state->crews[best].group=grp;
				}
			}
			else if(!state->bombers[i].failed)
			{
				// 2. Try to swap entire crew with a failed-but-fully-crewed a/c in the squadron
				for(unsigned int k=0;k<state->nbombers;k++)
				{
					if(k==i)
						continue;
					if(state->bombers[k].squadron!=s)
						continue;
					if(!state->bombers[k].failed)
						continue;
					if(fully_crewed(state, k))
					{
						swap_crews(state, i, k);
						break;
					}
				}
			}
		}
		// 3. Look for an available (unassigned) CREWMAN anywhere in the group or groupless
		if(state->bombers[i].crew[j]<0)
		{
			int best=-1;
			for(unsigned int k=0;k<state->ncrews;k++)
			{
				if(state->crews[k].class!=types[type].crew[j])
					continue;
				if(state->crews[k].status!=CSTATUS_CREWMAN)
					continue;
				// The PFF may steal crews from other groups
				if(state->crews[k].group && state->crews[k].group!=grp && !is_pff(state, i))
					continue;
				if(is_pff(state, i)&&(CREWOPS(k)<(types[type].noarm?30:15)))
					continue;
				if(best<0 ||
					   (lanc ? (0.25+state->crews[k].lanc)*(0.25+state->crews[k].heavy) >
						   (0.25+state->crews[best].lanc)*(0.25+state->crews[best].heavy) :
					    heavy ? (state->crews[k].heavy > state->crews[best].heavy ||
						     (state->crews[k].heavy >= 100.0 && state->crews[k].lanc < state->crews[best].lanc)) :
					    state->crews[k].heavy < state->crews[best].heavy))
				{
					if(state->crews[k].assignment<0)
					{
						best=k;
						if(lanc ? (state->crews[k].lanc >= 100.0 && state->crews[k].heavy >= 100.0) :
						   heavy ? (state->crews[k].lanc <= 0.0 && state->crews[k].heavy >= 100.0) :
						   state->crews[k].heavy <= 0.0)
							break; /* won't improve on this */
					}
				}
			}
			if(best>=0)
			{
				if(state->crews[best].assignment<0)
				{
					int cs=state->crews[best].squadron;
					if(cs>=0 && !(state->squads[cs].nc[state->crews[best].class]--)) /* can't happen */
						fprintf(stderr, "Warning: sqn nc went negative for %d.%d\n", cs, state->crews[best].class);
					state->crews[best].squadron=s;
					state->crews[best].assignment=i;
					state->bombers[i].crew[j]=best;
					if(state->crews[best].group!=grp)
					{
						if (state->crews[best].group && !is_pff(state, i)) /* can't happen */
							fprintf(stderr, "Warning: crewman %d jumped groups\n", best);
						state->crews[best].group=grp;
					}
				}
			}
		}
		if(state->bombers[i].crew[j]<0)
		{
			// 4. Give up, and don't allow assigning this bomber to any raid
			shortof[types[type].crew[j]]=true;
			ok=false;
		}
	}
	return(ok);
}

// Relink crew assignments after a bomber is destroyed, obsoleted or otherwise removed from the list
// This would be so much easier if everything was in linked-lists... ah well
void fixup_crew_assignments(game *state, unsigned int i, bool kill, double wskill)
{
	// While we're here, decrement the flight's nb
	if(state->bombers[i].squadron>=0)
	{
		unsigned int s=state->bombers[i].squadron;
		int f=state->bombers[i].flight;
		if(f<0)
			fprintf(stderr, "Warning: internal flight error\n");
		else if(!state->squads[s].nb[f]--)
			fprintf(stderr, "Warning: nb went negative in %s (%d.%d)\n", __func__, s, f);
	}
	for(unsigned int j=0;j<state->ncrews;j++) {
		if(state->crews[j].status==CSTATUS_CREWMAN)
		{
			if(state->crews[j].assignment>(int)i)
			{
				state->crews[j].assignment--;
			}
			else if(state->crews[j].assignment==(int)i)
			{
				if(kill)
				{
					int x=state->bombers[i].lon, y=state->bombers[i].lat;
					struct region reg=regions[region[x][y]];
					double psurvive=reg.water?0.1+(0.15*wskill/100.0):0.35;
					double pescape[REG_STATUSES]={
						[REGSTAT_FRIENDLY]=1.0,
						[REGSTAT_NEUTRAL]=0.6,
						[REGSTAT_ENEMY]=0.3,};
					if(brandp(psurvive))
					{
						if(brandp(pescape[reg.status]))
						{
							int s=state->crews[j].squadron;
							if (s<0)
							{
								fprintf(stderr, "Warning: internal squadron error\n");
								s=0;
							}
							int bx=base_lon(bases[state->squads[s].base]), by=base_lat(bases[state->squads[s].base]);
							double d=hypot(x-bx,y-by);
							unsigned int rt=ceil(exp(sqrt(d)/3.0));
							state->crews[j].status=CSTATUS_ESCAPEE;
							state->crews[j].assignment=rt;
							ex_append(&state->hist, state->now, (harris_time){11, 00}, state->crews[j].id, state->crews[j].class, rt);
						}
						else
						{
							pw_append(&state->hist, state->now, (harris_time){11, 00}, state->crews[j].id, state->crews[j].class);
							state->ncrews--;
							for(unsigned int k=j;k<state->ncrews;k++)
								state->crews[k]=state->crews[k+1];
							for(unsigned int k=0;k<state->nbombers;k++)
								for(unsigned int l=0;l<MAX_CREW;l++)
									if(state->bombers[k].crew[l]>(int)j)
										state->bombers[k].crew[l]--;
							j--;
						}
					}
					else
					{
						de_append(&state->hist, state->now, (harris_time){11, 00}, state->crews[j].id, state->crews[j].class);
						state->ncrews--;
						for(unsigned int k=j;k<state->ncrews;k++)
							state->crews[k]=state->crews[k+1];
						for(unsigned int k=0;k<state->nbombers;k++)
							for(unsigned int l=0;l<MAX_CREW;l++)
								if(state->bombers[k].crew[l]>(int)j)
									state->bombers[k].crew[l]--;
						j--;
					}
				}
				else
					state->crews[j].assignment=-1;
			}
		}
		else if(state->crews[j].status==CSTATUS_STUDENT)
		{
			if(state->crews[j].assignment>(int)i)
			{
				state->crews[j].assignment--;
			}
			else if(state->crews[j].assignment==(int)i)
			{
				if (kill) /* Currently can't happen */
					fprintf(stderr, "Error: student killed (ignoring)\n");
				state->crews[j].assignment=-1;
			}
		}
	}
}

int update_raidbox(const game *state, int seltarg)
{
	if(GB_raid_label)
	{
		switch(seltarg) {
		case -1:
			snprintf(GB_raid_label, 48, "Select a Target");
			break;
		case -2:
			snprintf(GB_raid_label, 48, "A/c on training units");
			break;
		default:
			snprintf(GB_raid_label, 48, "Raid on %s", targs[seltarg].name);
			break;
		}
	}
	if(GB_zhbox)
	{
		bool stream=!datebefore(state->now, event[EVENT_GEE]);
		if(!(GB_zhbox->hidden=(seltarg<0)||!stream))
			if(GB_zh)
			{
				atg_spinner *s=GB_zh->elemdata;
				s->value=state->raids[seltarg].zerohour;
			}
	}
	bool pffyes[ntypes], pffno[ntypes];
	memset(pffyes, 0, sizeof(pffyes));
	memset(pffno, 0, sizeof(pffno));
	if(seltarg>=0 && targs[seltarg].class==TCLASS_CITY)
		for(unsigned int j=0;j<state->raids[seltarg].nbombers;j++)
		{
			unsigned int k=state->raids[seltarg].bombers[j];
			if (is_pff(state, k))
				pffyes[state->bombers[k].type]=true;
			else
				pffno[state->bombers[k].type]=true;
		}
	for(unsigned int i=0;i<ntypes;i++)
	{
		for(unsigned int j=0;j<2;j++)
		{
			atg_ebox_empty(GB_raidloadbox[i][j]);
			GB_raidload[i][j]=NULL;
		}
		if(seltarg>=0 && targs[seltarg].class==TCLASS_CITY)
		{
			if(!(GB_raidload[i][0]=create_load_selector(&types[i], &state->raids[seltarg].loads[i])))
			{
				fprintf(stderr, "create_load_selector failed\n");
				return(1);
			}
			if(atg_ebox_pack(GB_raidloadbox[i][0], GB_raidload[i][0]))
			{
				perror("atg_ebox_pack");
				return(1);
			}
			GB_raidload[i][0]->hidden=!pffno[i];
			if(!(GB_raidload[i][1]=create_load_selector(&types[i], &state->raids[seltarg].pffloads[i])))
			{
				fprintf(stderr, "create_load_selector failed\n");
				return(1);
			}
			if(atg_ebox_pack(GB_raidloadbox[i][1], GB_raidload[i][1]))
			{
				perror("atg_ebox_pack");
				return(1);
			}
			GB_raidload[i][1]->hidden=!pffyes[i];
		}
	}
	return(update_raidnums(state, seltarg));
}

void update_btcount(const game *state, unsigned int type, bool shownav)
{
	if(GB_btnum[type])
	{
		unsigned int svble=0;
		types[type].count=0;
		for(unsigned int n=0;n<NNAVAIDS;n++)
			types[type].navcount[n]=0;
		for(unsigned int j=0;j<state->nbombers;j++)
			if(state->bombers[j].type==type&&!state->bombers[j].train)
			{
				if(!state->bombers[j].failed) svble++;
				types[type].count++;
				for(unsigned int n=0;n<NNAVAIDS;n++)
					if(state->bombers[j].nav[n]) types[type].navcount[n]++;
			}
		snprintf(GB_btnum[type], 20, "%u/%u/%u I.E.", svble, types[type].count, types[type].nestab);
	}
	GB_navrow[type]->hidden=!shownav;
	for(unsigned int n=0;n<NNAVAIDS;n++)
	{
		update_navbtn(*state, GB_navbtn, type, n, grey_overlay, yellow_overlay);
		if(GB_navgraph[type][n])
			GB_navgraph[type][n]->h=16-(types[type].count?(types[type].navcount[n]*16)/types[type].count:0);
	}
}

int update_raidnums(const game *state, int seltarg)
{
	bool stream=!datebefore(state->now, event[EVENT_GEE]);
	for(unsigned int i=0;i<ntypes;i++)
	{
		unsigned int count=0, tcap=0;
		switch(seltarg) {
		case -1:
			break;
		case -2:
			for(unsigned int k=0;k<state->nbombers;k++)
				if(state->bombers[k].type==i && state->bombers[k].train) count++;
			break;
		default:
			for(unsigned int j=0;j<state->raids[seltarg].nbombers;j++)
			{
				unsigned int k=state->raids[seltarg].bombers[j], type=state->bombers[k].type;
				if(type!=i)
					continue;
				count++;

				int s=state->bombers[k].squadron;
				if (s<0)
				{
					fprintf(stderr, "Warning: internal squadron error\n");
					s=0;
				}
				unsigned int b=state->squads[s].base;
				signed int blon=base_lon(bases[b]), blat=base_lat(bases[b]);
				bool unpaved=types[type].heavy&&!bases[b].paved;
				double dist;
				if(stream)
				{
					dist=hypot(blat-(signed)targs[seltarg].route[0][0], blon-(signed)targs[seltarg].route[0][1]);
					for(unsigned int l=0;l<4;l++)
					{
						double d=hypot((signed)targs[seltarg].route[l+1][0]-(signed)targs[seltarg].route[l][0], (signed)targs[seltarg].route[l+1][1]-(signed)targs[seltarg].route[l][1]);
						dist+=d;
					}
				}
				else
				{
					dist=hypot(blat-(signed)targs[seltarg].lat, blon-(signed)targs[seltarg].lon)*1.07;
				}
				unsigned int cap=bstats(state->bombers[k]).capwt;
				if(unpaved)
					cap-=cap/4;
				unsigned int fuelt=bstats(state->bombers[k]).range*0.6/(bstats(state->bombers[k]).speed/450.0);
				unsigned int estt=dist*1.1/(bstats(state->bombers[k]).speed/450.0)+12;
				if(!stream) estt+=36;
				if(estt>fuelt)
				{
					unsigned int fu=estt-fuelt;
					cap*=120.0/(120.0+fu);
				}
				tcap+=cap;
			}
			break;
		}
		if(GB_raidnum[i])
			snprintf(GB_raidnum[i], 32, "%u", count);
		if(GB_rbrow[i])
			GB_rbrow[i]->hidden=!count||!datewithin(state->now, types[i].entry, types[i].exit)||!state->btypes[i];
		if(seltarg>=0&&count&&GB_estcap[i])
		{
			if(targs[seltarg].class==TCLASS_LEAFLET)
				GB_estcap[i][0]=0;
			else
				snprintf(GB_estcap[i], 24, "%12ulb est. max", 10*(tcap/count/10));
		}
		else
			GB_estcap[i][0]=0;
	}
	return(0);
}

void game_preinit(game *state)
{
	// delete any bombers of deselected types
	unsigned int stripped=0;
	for(unsigned int i=0;i<state->nbombers;i++)
	{
		unsigned int type=state->bombers[i].type;
		if(!state->btypes[type])
		{
			state->nbombers--;
			for(unsigned int j=i;j<state->nbombers;j++)
				state->bombers[j]=state->bombers[j+1];
			fixup_crew_assignments(state, i, false, 0);
			i--;
			stripped++;
			continue;
		}
	}
	if(stripped)
		fprintf(stderr, "preinit: stripped %u bombers (deselected types)\n", stripped);
	// map all currently-existing flaksites
	for(unsigned int i=0;i<nflaks;i++)
		flaks[i].mapped=datewithin(state->now, flaks[i].entry, flaks[i].exit);
}
