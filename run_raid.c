/*
	harris - a strategy game
	Copyright (C) 2012-2014 Edward Cree

	licensed under GPLv3+ - see top of harris.c for details
	
	run_raid: the raid in-progress screen
*/

#include "run_raid.h"

#include <math.h>
#include "ui.h"
#include "globals.h"
#include "date.h"
#include "history.h"
#include "render.h"
#include "routing.h"
#include "rand.h"
#include "weather.h"
#include "geom.h"

atg_element *run_raid_box;
char *RB_time_label;
atg_element *RB_map;
atg_element *RB_kill_box[10];
unsigned int fill_kill_box;
unsigned int totalraids;
int **dij, **nij, **tij, **lij;
unsigned int *heat;
bool *canscore;
double bridge, cidam;

int clear_raids(game *state)
{
	if(!state) return(1);
	for(unsigned int i=0;i<ntargs;i++)
	{
		state->raids[i].nbombers=0;
		free(state->raids[i].bombers);
		state->raids[i].bombers=NULL;
		state->raids[i].zerohour=RRT(1,0);
		state->raids[i].routed=false;
	}
	return(0);
}

/* The string returned by this function should not be free()d,
 * and may be overwritten by subsequent calls */
const char *describe_location(int x, int y)
{
	static char cbuf[80];
	int mi=-1, md=50;
	for(unsigned int i=0;i<nlocs;i++)
	{
		int d=(locs[i].lon-x)*(locs[i].lon-x)+(locs[i].lat-y)*(locs[i].lat-y);
		if(d<md)
		{
			mi=i;
			md=d;
		}
	}
	if(mi<0)
	{
		snprintf(cbuf, 80, "at (%u,%u)", y, x);
	}
	else if(md==0)
	{
		snprintf(cbuf, 80, "at %s", locs[mi].name);
	}
	else
	{
		int dx=x-locs[mi].lon,
			dy=y-locs[mi].lat,
			th=floor(atan2(dy, dx)*4/M_PI+4.5),
			d=sqrt(md)*3;
		const char *dir=(const char *[8]){"W", "NW", "N", "NE", "E", "SE", "S", "SW"}[th%8];
		snprintf(cbuf, 80, "%umi %s of %s", d, dir, locs[mi].name);
	}
	return(cbuf);
}

void scroll_kill_box(void)
{
	for(unsigned int i=0;i<5;i++)
	{
		atg_element tmp=*RB_kill_box[i];
		*RB_kill_box[i]=*RB_kill_box[i+5];
		*RB_kill_box[i+5]=tmp;
		atg_ebox_empty(RB_kill_box[i+5]);
		RB_kill_box[i+5]->hidden=true;
	}
	fill_kill_box=5;
}

void describe_cr_helper(atg_colour colour, SDL_Surface *pic, const char *loc, const char *reason, SDL_Surface *kpic)
{
	if(fill_kill_box>9)
		scroll_kill_box();
	atg_element *kb=RB_kill_box[fill_kill_box++];
	kb->hidden=false;
	atg_element *tpic=atg_create_element_image(pic);
	if(!tpic)
	{
		fprintf(stderr, "atg_create_element_image failed\n");
	}
	else
	{
		tpic->w=38;
		if(atg_ebox_pack(kb, tpic))
		{
			perror("atg_ebox_pack");
			atg_free_element(tpic);
		}
	}
	atg_element *text_box=atg_create_element_box(ATG_BOX_PACK_VERTICAL, (atg_colour){23, 27, 35, ATG_ALPHA_OPAQUE}), *text_line=NULL;
	if(text_box)
	{
		if(atg_ebox_pack(kb, text_box))
		{
			perror("atg_ebox_pack");
			atg_free_element(text_box);
		}
		else
		{
			atg_element *shim=atg_create_element_box(ATG_BOX_PACK_HORIZONTAL, (atg_colour){23, 27, 35, ATG_ALPHA_OPAQUE});
			if(shim)
			{
				shim->h=11;
				if(atg_ebox_pack(text_box, shim))
				{
					perror("atg_ebox_pack");
					atg_free_element(shim);
				}
				else
				{
					text_line=atg_create_element_box(ATG_BOX_PACK_HORIZONTAL, (atg_colour){23, 27, 35, ATG_ALPHA_OPAQUE});
					if(text_line)
					{
						if(atg_ebox_pack(text_box, text_line))
						{
							perror("atg_ebox_pack");
							atg_free_element(text_line);
							text_line=NULL;
						}
					}
				}
			}
		}
	}
	if(!text_line)
		text_line=kb;
	atg_element *crl=atg_create_element_label(" crashed ", 14, colour);
	if(!crl)
	{
		fprintf(stderr, "atg_create_element_label failed\n");
	}
	else
	{
		if(atg_ebox_pack(text_line, crl))
		{
			perror("atg_ebox_pack");
			atg_free_element(crl);
		}
	}
	atg_element *locl=atg_create_element_label(loc, 14, colour);
	if(!locl)
	{
		fprintf(stderr, "atg_create_element_label failed\n");
	}
	else
	{
		if(atg_ebox_pack(text_line, locl))
		{
			perror("atg_ebox_pack");
			atg_free_element(locl);
		}
	}
	if(kpic)
	{
		atg_element *rl=atg_create_element_label(" - killer: ", 14, colour);
		if(!rl)
		{
			fprintf(stderr, "atg_create_element_label failed\n");
		}
		else
		{
			if(atg_ebox_pack(text_line, rl))
			{
				perror("atg_ebox_pack");
				atg_free_element(rl);
			}
		}
		atg_element *ri=atg_create_element_image(kpic);
		if(!ri)
		{
			fprintf(stderr, "atg_create_element_image failed\n");
		}
		else
		{
			if(atg_ebox_pack(kb, ri))
			{
				perror("atg_ebox_pack");
				atg_free_element(ri);
			}
		}
	}
	else
	{
		atg_element *rl=atg_create_element_label(reason, 14, colour);
		if(!rl)
		{
			fprintf(stderr, "atg_create_element_label failed\n");
		}
		else
		{
			if(atg_ebox_pack(text_line, rl))
			{
				perror("atg_ebox_pack");
				atg_free_element(rl);
			}
		}
	}
}

void describe_crb(const game *state, unsigned int k)
{
	ac_bomber b=state->bombers[k];
	int x=b.lon, y=b.lat;
	char reason[80]="";
	SDL_Surface *kpic=NULL;
	switch(b.ld.ds)
	{
		case DS_NONE: /* should never happen */
			snprintf(reason, 80, " with no prior damage");
		break;
		case DS_MECH:
			snprintf(reason, 80, " - killer: mechanical failure");
		break;
		case DS_FLAK:;
			flaksite fs=flaks[b.ld.idx];
			snprintf(reason, 80, " - killer: flak %s", describe_location(fs.lon, fs.lat));
		break;
		case DS_TFLK:;
			target t=targs[b.ld.idx];
			snprintf(reason, 80, " - killer: flak at %s", t.name);
		break;
		case DS_FIGHTER:;
			ac_fighter f=state->fighters[b.ld.idx];
			snprintf(reason, 80, " - killer: %s %s", ftypes[f.type].manu, ftypes[f.type].name);
			kpic=ftypes[f.type].picture;
		break;
		default:
			snprintf(reason, 80, " - killer: a bug (ds=%d, idx=%u)", b.ld.ds, b.ld.idx);
		break;
	}
	const char *loc=describe_location(x, y);
	describe_cr_helper((atg_colour){255, 127, 0, ATG_ALPHA_OPAQUE}, types[b.type].picture, loc, reason, kpic);
	fprintf(stderr, "%s %s crashed %s%s\n", types[b.type].manu, types[b.type].name, loc, reason);
}

void describe_crf(const game *state, unsigned int j)
{
	ac_fighter f=state->fighters[j];
	int x=f.lon, y=f.lat;
	char reason[80]="";
	SDL_Surface *kpic=NULL;
	switch(f.ld.ds)
	{
		case DS_NONE: /* should never happen */
			snprintf(reason, 80, " with no prior damage");
		break;
		case DS_FUEL:
			snprintf(reason, 80, " - killer: fuel exhaustion");
		break;
		case DS_BOMBER:;
			ac_bomber b=state->bombers[f.ld.idx];
			snprintf(reason, 80, " - killer: %s %s", types[b.type].manu, types[b.type].name);
			kpic=types[b.type].picture;
		break;
		default:
			snprintf(reason, 80, " - killer: a bug (ds=%d, idx=%u)", f.ld.ds, f.ld.idx);
		break;
	}
	const char *loc=describe_location(x, y);
	describe_cr_helper((atg_colour){127, 255, 127, ATG_ALPHA_OPAQUE}, ftypes[f.type].picture, loc, reason, kpic);
	fprintf(stderr, "%s %s crashed %s%s\n", ftypes[f.type].manu, ftypes[f.type].name, loc, reason);
}

int run_raid_create(void)
{
	run_raid_box=atg_create_element_box(ATG_BOX_PACK_VERTICAL, GAME_BG_COLOUR);
	if(!run_raid_box)
	{
		fprintf(stderr, "atg_create_element_box failed\n");
		return(1);
	}
	atg_element *RB_hbox=atg_create_element_box(ATG_BOX_PACK_HORIZONTAL, GAME_BG_COLOUR);
	if(!RB_hbox)
	{
		fprintf(stderr, "atg_create_element_box failed\n");
		return(1);
	}
	if(atg_ebox_pack(run_raid_box, RB_hbox))
	{
		perror("atg_ebox_pack");
		return(1);
	}
	if(!(RB_time_label=malloc(6)))
	{
		perror("malloc");
		return(1);
	}
	snprintf(RB_time_label, 6, "--:--");
	atg_element *RB_time=atg_create_element_label_nocopy(RB_time_label, 12, (atg_colour){175, 199, 255, ATG_ALPHA_OPAQUE});
	if(!RB_time)
	{
		fprintf(stderr, "atg_create_element_label failed\n");
		return(1);
	}
	RB_time->w=239;
	if(atg_ebox_pack(RB_hbox, RB_time))
	{
		perror("atg_ebox_pack");
		return(1);
	}
	RB_map=atg_create_element_image(terrain);
	if(!RB_map)
	{
		fprintf(stderr, "atg_create_element_image failed\n");
		return(1);
	}
	RB_map->h=terrain->h+2;
	if(atg_ebox_pack(RB_hbox, RB_map))
	{
		perror("atg_ebox_pack");
		return(1);
	}
	atg_element *kills_box=atg_create_element_box(ATG_BOX_PACK_VERTICAL, GAME_BG_COLOUR);
	if(!kills_box)
	{
		fprintf(stderr, "atg_create_element_box failed\n");
		return(1);
	}
	kills_box->w=800;
	kills_box->h=440;
	if(atg_ebox_pack(run_raid_box, kills_box))
	{
		perror("atg_ebox_pack");
		return(1);
	}
	for(unsigned int i=0;i<10;i++)
	{
		if(!(RB_kill_box[i]=atg_create_element_box(ATG_BOX_PACK_HORIZONTAL, (atg_colour){23, 27, 35, ATG_ALPHA_OPAQUE})))
		{
			fprintf(stderr, "atg_create_element_box failed\n");
			return(1);
		}
		RB_kill_box[i]->h=40;
		RB_kill_box[i]->w=800;
		RB_kill_box[i]->hidden=true;
		if(atg_ebox_pack(kills_box, RB_kill_box[i]))
		{
			perror("atg_ebox_pack");
			return(1);
		}
		atg_element *shim=atg_create_element_box(ATG_BOX_PACK_HORIZONTAL, GAME_BG_COLOUR);
		if(!shim)
		{
			fprintf(stderr, "atg_create_element_box failed\n");
			return(1);
		}
		shim->h=4;
		if(atg_ebox_pack(kills_box, shim))
		{
			perror("atg_ebox_pack");
			return(1);
		}
	}
	return(0);
}

screen_id run_raid_screen(atg_canvas *canvas, game *state)
{
	state->roe.idtar=datebefore(state->now, event[EVENT_CIV]);
	double moonphase=pom(state->now);
	double moonillum=foldpom(moonphase);
	double flakscale=state->gprod[ICLASS_ARM]/(GET_DC(state,FLAK)*10000.0);
	unsigned int rcity=GET_DC(state,RCITY),
	             rindus=GET_DC(state,RINDUS),
	             rship=GET_DC(state,RSHIP),
	             rleaf=GET_DC(state,RLEAF),
	             rother=GET_DC(state,ROTHER);
	double d_fsr=GET_DC(state,FSR)/10.0;
	unsigned int fightersleft;
	totalraids=0;
	fightersleft=state->nfighters;
	for(unsigned int i=0;i<ntargs;i++)
	{
		totalraids+=state->raids[i].nbombers;
		targs[i].threat=0;
		targs[i].nfighters=0;
		targs[i].fires=0;
		targs[i].skym=-1;
	}
	for(unsigned int i=0;i<nflaks;i++)
		flaks[i].ftr=-1;
	for(unsigned int i=0;i<state->nfighters;i++)
	{
		state->fighters[i].targ=-1;
		state->fighters[i].k=-1;
		state->fighters[i].hflak=-1;
		state->fighters[i].damage=0;
		state->fighters[i].ld.ds=DS_NONE;
		state->fighters[i].landed=true;
		state->fighters[i].lat=fbases[state->fighters[i].base].lat;
		state->fighters[i].lon=fbases[state->fighters[i].base].lon;
	}
	if(totalraids)
	{
		if(RB_time_label) snprintf(RB_time_label, 6, "12:00");
		bool stream=!datebefore(state->now, event[EVENT_GEE]),
		     moonshine=!datebefore(state->now, event[EVENT_MOONSHINE]),
		     window=!datebefore(state->now, event[EVENT_WINDOW]),
		     wairad= datewithin(state->now, event[EVENT_WINDOW], event[EVENT_L_SN]);
		unsigned int it=0, startt=9999;
		for(unsigned int i=0;i<ntargs;i++)
		{
			unsigned int halfhalf=0; // count for halfandhalf bombloads
			for(unsigned int j=0;j<state->raids[i].nbombers;j++)
			{
				unsigned int k=state->raids[i].bombers[j], type=state->bombers[k].type;
				state->bombers[k].targ=i;
				state->bombers[k].lat=types[type].blat;
				state->bombers[k].lon=types[type].blon;
				state->bombers[k].routestage=0;
				if(stream)
					for(unsigned int l=0;l<8;l++)
					{
						state->bombers[k].route[l][0]=targs[i].route[l][0];
						state->bombers[k].route[l][1]=targs[i].route[l][1];
					}
				else
					genroute((unsigned int [2]){types[type].blat, types[type].blon}, i, state->bombers[k].route, state, 100);
				double dist=hypot((signed)types[type].blat-(signed)state->bombers[k].route[0][0], (signed)types[type].blon-(signed)state->bombers[k].route[0][1]), outward=dist;
				for(unsigned int l=0;l<7;l++)
				{
					double d=hypot((signed)state->bombers[k].route[l+1][0]-(signed)state->bombers[k].route[l][0], (signed)state->bombers[k].route[l+1][1]-(signed)state->bombers[k].route[l][1]);
					dist+=d;
					if(l<4) outward+=d;
				}
				unsigned int cap=types[type].cap;
				state->bombers[k].bombed=false;
				state->bombers[k].crashed=false;
				state->bombers[k].landed=false;
				state->bombers[k].idtar=false;
				state->bombers[k].ld.ds=DS_NONE;
				state->bombers[k].lat+=rand()*3.0/RAND_MAX-1;
				state->bombers[k].lon+=rand()*3.0/RAND_MAX-1;
				state->bombers[k].navlat=0;
				state->bombers[k].navlon=0;
				state->bombers[k].driftlat=0;
				state->bombers[k].driftlon=0;
				state->bombers[k].speed=types[type].speed/450.0;
				if(stream)
				{
					// aim for Zero Hour 01:00 plus up to 10 minutes
					// PFF should arrive at Zero minus 6, and be finished by Zero minus 2
					// Zero Hour is t=840, and a minute is two t-steps
					int tt=state->bombers[k].pff?(state->raids[i].zerohour-12+irandu(8)):(state->raids[i].zerohour+irandu(20));
					int st=tt-(outward/state->bombers[k].speed)-3;
					if(state->bombers[k].pff) st-=3;
					if(st<0)
					{
						tt-=st;
						st=0;
					}
					state->bombers[k].startt=st;
				}
				else
					state->bombers[k].startt=(state->raids[i].zerohour-480)+irandu(90); // 2100 to 2145
				startt=min(startt, state->bombers[k].startt);
				ra_append(&state->hist, state->now, maketime(startt), state->bombers[k].id, false, state->bombers[k].type, i);
				state->bombers[k].fuelt=state->bombers[k].startt+types[type].range*0.6/(double)state->bombers[k].speed;
				unsigned int eta=state->bombers[k].startt+outward*1.1/(double)state->bombers[k].speed+12;
				if(!stream) eta+=36;
				if(eta>state->bombers[k].fuelt)
				{
					unsigned int fu=eta-state->bombers[k].fuelt;
					state->bombers[k].fuelt+=fu;
					cap*=120.0/(120.0+fu);
				}
				else
					state->bombers[k].fuelt=eta;
				state->bombers[k].b_hc=0;
				state->bombers[k].b_gp=0;
				state->bombers[k].b_in=0;
				state->bombers[k].b_ti=0;
				state->bombers[k].b_le=0;
				unsigned int bulk=types[type].cap;
				bool inext=true;
				switch(targs[i].class)
				{
					case TCLASS_LEAFLET:
						state->bombers[k].b_le=min(types[type].cap*3, cap*20);
					break;
					case TCLASS_CITY:
						switch(state->bombers[k].pff?state->raids[i].pffloads[type]:state->raids[i].loads[type])
						{
							case BL_PLUMDUFF:
								if(cap>=4000) // cookie + gp+in mix
								{
									transfer(4000, cap, state->bombers[k].b_hc);
									while(cap&&(bulk=types[type].cap-loadbulk(state->bombers[k])))
									{
										if(inext)
											transfer(min(bulk/1.5, 800), cap, state->bombers[k].b_in);
										else
											transfer(min(bulk, 500), cap, state->bombers[k].b_gp);
										inext=!inext;
									}
								}
								else // can't take a cookie, so just gp+in mix (but more gp than BL_USUAL)
								{
									while(cap&&(bulk=types[type].cap-loadbulk(state->bombers[k])))
									{
										if(inext)
											transfer(min(bulk/1.5, 300), cap, state->bombers[k].b_in);
										else
											transfer(min(bulk, 1000), cap, state->bombers[k].b_gp);
										inext=!inext;
									}
								}
							break;
							case BL_USUAL:
								if(types[type].load[BL_PLUMDUFF]&&types[type].cap>=10000&&cap>4000) // cookie + incendiaries
								{
									transfer(4000, cap, state->bombers[k].b_hc);
									bulk=types[type].cap-loadbulk(state->bombers[k]);
									state->bombers[k].b_in=min(cap, bulk/1.5);
								}
								else // gp+in mix
								{
									while(cap&&(bulk=types[type].cap-loadbulk(state->bombers[k])))
									{
										if(inext)
											transfer(min(bulk/1.5, 800), cap, state->bombers[k].b_in);
										else
											transfer(min(bulk, types[type].inc?250:500), cap, state->bombers[k].b_gp);
										inext=!inext;
									}
								}
							break;
							case BL_ARSON:
								state->bombers[k].b_in=min(cap, bulk/1.5);
							break;
							case BL_HALFHALF:
								if(cap>=4000&&(halfhalf++%2))
								{
									state->bombers[k].b_hc=(cap/4000)*4000;
									break;
								}
								// else fallthrough to BL_ILLUM
							case BL_ILLUM:
								while(cap&&(bulk=types[type].cap-loadbulk(state->bombers[k])))
								{
									if(inext)
										transfer(min(bulk/2.0, 250), cap, state->bombers[k].b_ti);
									else
										transfer(min(bulk, 500), cap, state->bombers[k].b_gp);
									inext=!inext;
								}
							break;
							case BL_PPLUS: // LanX, up to 12,000lb cookie; LanI, up to 8,000lb cookie
								transfer(min(types[type].cap/5333, cap/4000)*4000, cap, state->bombers[k].b_hc);
								state->bombers[k].b_gp=cap;
							break;
							case BL_PONLY:
								if(cap>=4000)
								{
									state->bombers[k].b_hc=(cap/4000)*4000;
									break;
								}
								// else fallthrough to BL_ABNORMAL
							case BL_ABNORMAL:
							default:
								state->bombers[k].b_gp=cap;
							break;
						}
					break;
					default: // all other targets use all-GP loads
						state->bombers[k].b_gp=cap;
					break;
				}
				#define b_roundto(what, n)	state->bombers[k].b_##what=(state->bombers[k].b_##what/n)*n
				b_roundto(hc, 4000); // should never affect anything
				b_roundto(gp, 50);
				b_roundto(in, 10);
				b_roundto(ti, 50);
				b_roundto(le, 1000);
				//fprintf(stderr, "%s: %ulb hc + %u lb gp + %ulb in + %ulb ti = %ulb\n", types[type].name, state->bombers[k].b_hc, state->bombers[k].b_gp, state->bombers[k].b_in, state->bombers[k].b_ti, loadweight(state->bombers[k]));
				#undef b_roundto
			}
		}
		oboe.k=-1;
		atg_image *map_img=RB_map->elemdata;
		SDL_FreeSurface(map_img->data);
		SDL_Surface *with_flak_and_target, *with_weather; /* with_weather also has route */
		map_img->data=SDL_ConvertSurface(terrain, terrain->format, terrain->flags);
		with_flak_and_target=SDL_ConvertSurface(terrain, terrain->format, terrain->flags);
		SDL_FreeSurface(flak_overlay);
		flak_overlay=render_flak(state->now);
		SDL_BlitSurface(flak_overlay, NULL, with_flak_and_target, NULL);
		SDL_FreeSurface(target_overlay);
		target_overlay=render_targets(state->now);
		SDL_BlitSurface(target_overlay, NULL, with_flak_and_target, NULL);
		with_weather=SDL_ConvertSurface(with_flak_and_target, with_flak_and_target->format, with_flak_and_target->flags);
		SDL_FreeSurface(weather_overlay);
		weather_overlay=render_weather(state->weather);
		SDL_BlitSurface(weather_overlay, NULL, with_weather, NULL);
		SDL_FreeSurface(route_overlay);
		route_overlay=render_routes(state);
		SDL_BlitSurface(route_overlay, NULL, with_weather, NULL);
		SDL_Surface *with_ac=SDL_ConvertSurface(with_weather, with_weather->format, with_weather->flags);
		SDL_Surface *ac_overlay=render_ac(state);
		SDL_BlitSurface(ac_overlay, NULL, with_ac, NULL);
		SDL_BlitSurface(with_ac, NULL, map_img->data, NULL);
		for(unsigned int i=0;i<10;i++)
		{
			atg_ebox_empty(RB_kill_box[i]);
			RB_kill_box[i]->hidden=true;
		}
		fill_kill_box=0;
		unsigned int inair=totalraids, t=0;
		cidam=0;
		bridge=0;
		// Tame Boar raid tracking
		bool tameboar=!datebefore(state->now, event[EVENT_TAMEBOAR]);
		unsigned int boxes[16][16]; // 10x10 boxes starting at (89,40)
		int topx=-1, topy=-1; // co-ords of fullest box
		unsigned int sumx, sumy; // sum of offsets within box
		double velx, vely; // sum of velocity components ditto
		while(t<startt)
		{
			t++;
			if((!(t&3))&&(it<720))
			{
				w_iter(&state->weather, lorw);
				it++;
			}
		}
		SDL_FreeSurface(weather_overlay);
		weather_overlay=render_weather(state->weather);
		SDL_BlitSurface(with_flak_and_target, NULL, with_weather, NULL);
		SDL_BlitSurface(weather_overlay, NULL, with_weather, NULL);
		while(inair)
		{
			t++;
			harris_time now = maketime(t);
			if(RB_time_label) snprintf(RB_time_label, 6, "%02u:%02u", now.hour, now.minute);
			if((!(t&3))&&(it<720))
			{
				w_iter(&state->weather, lorw);
				SDL_FreeSurface(weather_overlay);
				weather_overlay=render_weather(state->weather);
				SDL_BlitSurface(with_flak_and_target, NULL, with_weather, NULL);
				SDL_BlitSurface(weather_overlay, NULL, with_weather, NULL);
				SDL_BlitSurface(route_overlay, NULL, with_weather, NULL);
				it++;
			}
			if(tameboar)
			{
				memset(boxes, 0, sizeof(boxes));
				sumx=sumy=0;
				velx=vely=0;
			}
			for(unsigned int i=0;i<ntargs;i++)
				targs[i].shots=0;
			for(unsigned int i=0;i<nflaks;i++)
			{
				flaks[i].shots=0;
				flaks[i].heat=0;
			}
			for(unsigned int i=0;i<ntargs;i++)
				for(unsigned int j=0;j<state->raids[i].nbombers;j++)
				{
					unsigned int k=state->raids[i].bombers[j], type=state->bombers[k].type;
					if(t<state->bombers[k].startt) continue;
					if(state->bombers[k].crashed||state->bombers[k].landed) continue;
					if(state->bombers[k].damage>=100)
					{
						cr_append(&state->hist, state->now, now, state->bombers[k].id, false, state->bombers[k].type);
						state->bombers[k].crashed=true;
						describe_crb(state, k);
						inair--;
						continue;
					}
					if(brandp(types[type].fail/(50.0*min(240.0, 48.0+t-state->bombers[k].startt))+state->bombers[k].damage/2400.0))
					{
						if(state->bombers[k].ld.ds==DS_NONE)
							state->bombers[k].ld.ds=DS_MECH;
						fa_append(&state->hist, state->now, now, state->bombers[k].id, false, state->bombers[k].type, 1);
						state->bombers[k].failed=true;
						if(brandp((1.0+state->bombers[k].damage/50.0)/(240.0-types[type].fail*5.0)))
						{
							cr_append(&state->hist, state->now, now, state->bombers[k].id, false, state->bombers[k].type);
							state->bombers[k].crashed=true;
							describe_crb(state, k);
							inair--;
							continue;
						}
					}
					unsigned int stage=state->bombers[k].routestage;
					while((stage<8)&&!(state->bombers[k].route[stage][0]||state->bombers[k].route[stage][1]))
						stage=++state->bombers[k].routestage;
					bool home=(state->bombers[k].failed&&!state->bombers[k].bombed)||(stage>=8);
					if((stage==4)&&state->bombers[k].nav[NAV_OBOE]&&xyr(state->bombers[k].lon-oboe.lon, state->bombers[k].lat-oboe.lat, 50+types[type].alt*.3)) // OBOE
					{
						if(oboe.k==-1)
							oboe.k=k;
						if(oboe.k==(int)k)
						{
							state->bombers[k].navlon=state->bombers[k].navlat=0;
							state->bombers[k].fix=true;
						}
						// the rest of these involve taking priority over an existing user and so they grab the k but don't get to use this turn
						if(!state->bombers[oboe.k].b_ti) // PFF priority and priority for bigger loads
						{
							if(state->bombers[k].b_ti||((state->bombers[k].b_hc+state->bombers[k].b_gp)>(state->bombers[oboe.k].b_hc+state->bombers[oboe.k].b_gp)))
								oboe.k=k;
						}
					}
					else if(oboe.k==(int)k)
						oboe.k=-1;
					int destx=home?types[type].blon:state->bombers[k].route[stage][1],
						desty=home?types[type].blat:state->bombers[k].route[stage][0],
						prevx=stage?state->bombers[k].route[stage-1][1]:types[type].blon,
						prevy=stage?state->bombers[k].route[stage-1][0]:types[type].blat;
					double thinklon=state->bombers[k].lon+state->bombers[k].navlon,
						thinklat=state->bombers[k].lat+state->bombers[k].navlat;
					clamp(thinklon, 0, 256);
					clamp(thinklat, 0, 256);
					double cx=destx-thinklon, cy=desty-thinklat;
					int px=destx-prevx, py=desty-prevy;
					double d=hypot(cx, cy);
					if(home)
					{
						if(d<0.7)
						{
							if(state->bombers[k].lon<63)
							{
								state->bombers[k].landed=true;
								inair--;
							}
							else
							{
								state->bombers[k].navlon=0;
							}
						}
						else if(t>RRT(9,0))
						{
							state->bombers[k].navlon=state->bombers[k].navlat=0;
							state->bombers[k].lon=min(state->bombers[k].lon, 127);
						}
					}
					else if(stage==4)
					{
						bool fuel=(t>=state->bombers[k].fuelt);
						bool damaged=(state->bombers[k].damage>=8);
						bool roeok=state->bombers[k].idtar||(!state->roe.idtar&&brandp(0.2))||brandp(0.005);
						bool leaf=state->bombers[k].b_le;
						bool pffstop=stream&&(t<state->raids[i].zerohour-(state->bombers[k].pff?16:0)); // PFF start bombing at Zero minus 8; Main Force at Zero Hour
						double cr=1.2;
						if(oboe.k==(int)k) cr=0.3;
						unsigned int dm=0; // target crew believes is nearest
						double mind=1e6;
						for(unsigned int i=0;i<ntargs;i++)
						{
							double dx=state->bombers[k].lon+state->bombers[k].navlon-targs[i].lon, dy=state->bombers[k].lat+state->bombers[k].navlat-targs[i].lat, dd=dx*dx+dy*dy;
							if(dd<mind)
							{
								mind=dd;
								dm=i;
							}
						}
						if(((fabs(cx)<cr)&&(fabs(cy)<cr)&&roeok&&!pffstop&&(dm==state->bombers[k].targ))||((fuel||damaged)&&(roeok||leaf)))
						{
							state->bombers[k].bmblon=state->bombers[k].lon;
							state->bombers[k].bmblat=state->bombers[k].lat;
							state->bombers[k].navlon=targs[dm].lon-state->bombers[k].lon;
							state->bombers[k].navlat=targs[dm].lat-state->bombers[k].lat;
							state->bombers[k].bt=t;
							state->bombers[k].bombed=true;
							if(!leaf)
							{
								for(unsigned int ta=0;ta<ntargs;ta++)
								{
									if(targs[ta].class!=TCLASS_CITY) continue;
									if(!datewithin(state->now, targs[ta].entry, targs[ta].exit)) continue;
									int dx=floor(state->bombers[k].bmblon+.5)-targs[ta].lon, dy=floor(state->bombers[k].bmblat+.5)-targs[ta].lat;
									int hx=targs[ta].picture->w/2;
									int hy=targs[ta].picture->h/2;
									if((abs(dx)<=hx)&&(abs(dy)<=hy)&&(pget(targs[ta].picture, dx+hx, dy+hy).a==ATG_ALPHA_OPAQUE))
									{
										unsigned int he=state->bombers[k].b_hc+state->bombers[k].b_gp;
										hi_append(&state->hist, state->now, maketime(t), state->bombers[k].id, false, type, ta, he+state->bombers[k].b_in+state->bombers[k].b_ti);
										state->flam[ta]=min(state->flam[ta]+state->bombers[k].b_hc/5000.0, 100); // HC (cookies) increase target flammability
										double maybe_dmg=(he*1.2+state->bombers[k].b_in*(targs[ta].flammable?2.4:1.5)*state->flam[ta]/40.0)/(targs[ta].psiz*rcity*1000.0);
										double dmg=min(state->dmg[ta], maybe_dmg);
										cidam+=dmg*(targs[ta].berlin?2.0:1.0);
										state->dmg[ta]-=dmg;
										tdm_append(&state->hist, state->now, maketime(t), ta, dmg, state->dmg[ta]);
										targs[ta].fires+=state->bombers[k].b_in*(state->flam[ta]/40.0)/1500+state->bombers[k].b_ti/30;
										break;
									}
								}
							}
							stage=++state->bombers[k].routestage;
						}
						else if(fuel)
							stage=++state->bombers[k].routestage;
					}
					else
					{
						double s=(cx*px)+(cy*py); // dot product
						if(s<0)
							stage=++state->bombers[k].routestage;
					}
					state->bombers[k].idtar=false;
					double dx=cx*state->bombers[k].speed/d,
						dy=cy*state->bombers[k].speed/d;
					if(d==0) dx=dy=-1;
					state->bombers[k].lon+=dx;
					state->bombers[k].lat+=dy;
					for(unsigned int i=0;i<ntargs;i++)
					{
						if(!datewithin(state->now, targs[i].entry, targs[i].exit)) continue;
						double dx=state->bombers[k].lon-targs[i].lon, dy=state->bombers[k].lat-targs[i].lat, dd=dx*dx+dy*dy;
						double range;
						switch(targs[i].class)
						{
							case TCLASS_CITY:
							case TCLASS_LEAFLET:
								range=3.6;
							break;
							case TCLASS_SHIPPING:
								range=2.2;
							break;
							case TCLASS_AIRFIELD:
							case TCLASS_ROAD:
								range=1.6;
							break;
							case TCLASS_MINING:
								range=2.0;
							break;
							case TCLASS_BRIDGE:
							case TCLASS_INDUSTRY:
								range=1.1;
							break;
							default:
								fprintf(stderr, "Bad targs[%d].class = %d\n", i, targs[i].class);
								range=0.6;
							break;
						}
						if(dd<range*range)
						{
							unsigned int x=targs[i].lon/2, y=targs[i].lat/2;
							double wea=((x<128)&&(y<128))?state->weather.p[x][y]-1000:0;
							double preccap=100.0/(5.0*(moonillum+.3)/(double)(8+max(4-wea, 0)));
							if(brandp(state->flk[i]*flakscale/min((9+targs[i].shots++)*40.0, preccap)))
							{
								double ddmg;
								if(brandp(types[type].defn/400.0))
									ddmg=100;
								else
									ddmg=irandu(types[type].defn)/2.5;
								state->bombers[k].damage+=ddmg;
								if(ddmg)
								{
									dmtf_append(&state->hist, state->now, now, state->bombers[k].id, false, state->bombers[k].type, ddmg, state->bombers[k].damage, i);
									state->bombers[k].ld=(dmgsrc){.ds=DS_TFLK, .idx=i};
								}
							}
						}
						bool water=false;
						int x=state->bombers[k].lon/2;
						int y=state->bombers[k].lat/2;
						if((x>=0)&&(y>=0)&&(x<128)&&(y<128))
							water=lorw[x][y];
						if(dd<(water?8*8:30*30))
							targs[i].threat+=sqrt(targs[i].prod*types[type].cap/5000/(6.0+max(dd, 0.25)));
					}
					for(unsigned int i=0;i<nflaks;i++)
					{
						if(!datewithin(state->now, flaks[i].entry, flaks[i].exit)) continue;
						bool rad=!datebefore(state->now, flaks[i].radar);
						double dx=state->bombers[k].lon-flaks[i].lon,
						       dy=state->bombers[k].lat-flaks[i].lat;
						if(xyr(dx, dy, 3.0))
						{
							unsigned int x=flaks[i].lon/2, y=flaks[i].lat/2;
							double wea=((x<128)&&(y<128))?state->weather.p[x][y]-1000:0;
							double preccap=160.0/(5.0*(moonillum+.3)/(double)(8+max(4-wea, 0)));
							if(rad) preccap=min(preccap, window?960.0:480.0);
							if(brandp(flaks[i].strength*flakscale/min((12+flaks[i].shots++)*40.0, preccap)))
							{
								double ddmg;
								if(brandp(types[type].defn/500.0))
									ddmg=100;
								else
									ddmg=irandu(types[type].defn)/5.0;
								state->bombers[k].damage+=ddmg;
								if(ddmg)
								{
									dmfk_append(&state->hist, state->now, now, state->bombers[k].id, false, state->bombers[k].type, ddmg, state->bombers[k].damage, i);
									state->bombers[k].ld=(dmgsrc){.ds=DS_FLAK, .idx=i};
								}
							}
						}
						if(rad&&xyr(dx, dy, 12)) // 36 miles range for Würzburg radar (Wikipedia gives range as "up to 43mi")
						{
							flaks[i].heat++;
							if(brandp(0.1))
							{
								if(flaks[i].ftr>=0)
								{
									unsigned int ftr=flaks[i].ftr;
									if((xyr(state->fighters[ftr].lon-(signed)flaks[i].lon, state->fighters[ftr].lat-(signed)flaks[i].lat, 10))&&(state->fighters[ftr].k<0))
									{
										state->fighters[ftr].k=k;
										//fprintf(stderr, "ftr%u k%u\n", ftr, k);
									}
								}
							}
						}
					}
					for(unsigned int j=0;j<state->nfighters;j++)
					{
						if(state->fighters[j].landed) continue;
						if(state->fighters[j].k>=0) continue;
						if(state->fighters[j].hflak>=0) continue;
						bool airad=state->fighters[j].radar;
						unsigned int x=state->fighters[j].lon/2, y=state->fighters[j].lat/2;
						double wea=((x<128)&&(y<128))?state->weather.p[x][y]-1000:0;
						bool heavy=types[state->bombers[k].type].heavy;
						double seerange=(airad?(heavy?1.9:1.2):((heavy?5.0:2.1)*(moonillum+.3)/(double)(8+max(4-wea, 0))))*d_fsr;
						if(xyr(state->bombers[k].lon-state->fighters[j].lon, state->bombers[k].lat-state->fighters[j].lat, seerange))
						{
							double findp=airad?0.7:0.8/(double)(8+max(4-wea, 0));
							if(brandp(findp))
								state->fighters[j].k=k;
						}
					}
					// TODO: navigation affected by navaids
					double navacc=3.0/types[type].accu;
					double ex=drandu(navacc)-(navacc/2), ey=drandu(navacc)-(navacc/2);
					state->bombers[k].driftlon=state->bombers[k].driftlon*.98+ex;
					state->bombers[k].driftlat=state->bombers[k].driftlat*.98+ey;
					state->bombers[k].lon+=state->bombers[k].driftlon;
					state->bombers[k].lat+=state->bombers[k].driftlat;
					clamp(state->bombers[k].lon, 0, 256);
					clamp(state->bombers[k].lat, 0, 256);
					state->bombers[k].navlon-=state->bombers[k].driftlon;
					state->bombers[k].navlat-=state->bombers[k].driftlat;
					if(state->bombers[k].nav[NAV_GEE]&&xyr(state->bombers[k].lon-gee.lon, state->bombers[k].lat-gee.lat, datebefore(state->now, event[EVENT_GEEJAM])?56+(types[type].alt*.3):gee.jrange))
					{
						state->bombers[k].navlon=(state->bombers[k].navlon>0?1:-1)*min(fabs(state->bombers[k].navlon), 0.4);
						state->bombers[k].navlat=(state->bombers[k].navlat>0?1:-1)*min(fabs(state->bombers[k].navlat), 2.4);
					}
					unsigned int x=state->bombers[k].lon/2, y=state->bombers[k].lat/2;
					double wea=((x<128)&&(y<128))?state->weather.p[x][y]-1000:0;
					if(state->bombers[k].nav[NAV_H2S]) // TODO: restrict usage after NAXOS
					{
						unsigned char h=((x<128)&&(y<128))?tnav[x][y]:0;
						wea=max(wea, h*0.08-12);
					}
					double navp=types[type].accu*0.05*(sqrt(moonillum)*.8+.5)/(double)(8+max(16-wea, 8));
					if(home&&(state->bombers[k].lon<64)) navp=1;
					bool b=brandp(navp);
					state->bombers[k].fix=b;
					if(b)
					{
						unsigned char h=((x<128)&&(y<128))?tnav[x][y]:0;
						double cf=(700.0+state->bombers[k].lon-h*0.6)/1e3;
						state->bombers[k].navlon*=cf;
						state->bombers[k].navlat*=cf;
						state->bombers[k].driftlon*=cf;
						state->bombers[k].driftlat*=cf;
					}
					unsigned int dtm=0; // target nearest to bomber
					double mind=1e6;
					for(unsigned int i=0;i<ntargs;i++)
					{
						double dx=state->bombers[k].lon-targs[i].lon, dy=state->bombers[k].lat-targs[i].lat, dd=dx*dx+dy*dy;
						if(dd<mind)
						{
							mind=dd;
							dtm=i;
						}
					}
					unsigned int sage=t-targs[dtm].skym;
					bool sk=targs[dtm].skym>=0?brandp(10/(5.0+sage)-0.6):false;
					bool c=brandp(targs[dtm].fires/2e3);
					if(b||c||sk)
					{
						unsigned int dm=0; // target crew believes is nearest
						double mind=1e6;
						for(unsigned int i=0;i<ntargs;i++)
						{
							double dx=state->bombers[k].lon+state->bombers[k].navlon-targs[i].lon, dy=state->bombers[k].lat+state->bombers[k].navlat-targs[i].lat, dd=dx*dx+dy*dy;
							if(dd<mind)
							{
								mind=dd;
								dm=i;
							}
						}
						int bx=state->bombers[k].lon+0.5,
							by=state->bombers[k].lat+0.5;
						switch(targs[i].class)
						{
							case TCLASS_INDUSTRY:
								if(mind<0.9)
									state->bombers[k].idtar=true;
							break;
							case TCLASS_CITY:
							case TCLASS_LEAFLET:
							case TCLASS_SHIPPING:
							case TCLASS_AIRFIELD:
							case TCLASS_ROAD:
							case TCLASS_BRIDGE:
								if(pget(target_overlay, bx, by).a==ATG_ALPHA_OPAQUE) // XXX this might behave oddly as we have all cities on target_overlay
								{
									state->bombers[k].idtar=true;
								}
								else if((c||sk)&&(targs[dm].class==TCLASS_CITY)&&(targs[dtm].class==TCLASS_CITY))
								{
									state->bombers[k].navlon=(signed)targs[dtm].lon-(signed)targs[dm].lon;
									state->bombers[k].navlat=(signed)targs[dtm].lat-(signed)targs[dm].lat;
								}
							break;
							case TCLASS_MINING:;
								int x=state->bombers[k].lon/2;
								int y=state->bombers[k].lat/2;
								if((x>=0)&&(y>=0)&&(x<128)&&(y<128)&&lorw[x][y])
									state->bombers[k].idtar=true;
							break;
							default: // shouldn't ever get here
								fprintf(stderr, "Bad targs[%d].class = %d\n", i, targs[i].class);
							break;
						}
					}
					if(tameboar)
					{
						int boxx=floor((state->bombers[k].lon-89)/10.0),
							boxy=floor((state->bombers[k].lat-40)/10.0);
						if(0<=boxx&&boxx<16&&0<=boxy&&boxy<16)
						{
							unsigned int type=state->bombers[k].type;
							unsigned int size=types[type].cap/1000; // how big a signal on Freya ground radar
							// TODO: extra WINDOW (will need UI support in raid planning)
							size*=irandu(3);
							boxes[boxx][boxy]+=size;
							if(boxx==topx&&boxy==topy)
							{
								sumx+=(state->bombers[k].lon-89-boxx*10)*size;
								sumy+=(state->bombers[k].lat-40-boxy*10)*size;
								velx+=dx*size;
								vely+=dy*size;
							}
						}
					}
				}
			bool boar_up=false;
			double boarx,boary,boarvx,boarvy;
			if(tameboar)
			{
				int ox=topx;
				int oy=topy;
				topx=-1;
				topy=-1;
				unsigned int topval=0;
				for(int findx=0;findx<16;findx++)
					for(int findy=0;findy<16;findy++)
					{
						// once raid is identified, prefer to move contiguously
						unsigned int val=boxes[findx][findy];
						if(ox>=0&&oy>=0)
						{
							if(abs(findx-ox)+abs(findy-oy)<2)
								val*=3;
						}
						if(val>topval)
						{
							topx=findx;
							topy=findy;
							topval=val;
						}
					}
				if(topx>=0&&topy>=0)
				{
					boar_up=true;
					boarx=sumx/(double)topval+89+topx*10;
					boary=sumy/(double)topval+40+topy*10;
					boarvx=velx/(double)topval;
					boarvy=vely/(double)topval;
				}
			}
			for(unsigned int j=0;j<state->nfighters;j++)
			{
				if(state->fighters[j].landed)
				{
					if(tameboar) goto boarable;
					continue;
				}
				if(state->fighters[j].crashed) continue;
				if((state->fighters[j].damage>=100)||brandp(state->fighters[j].damage/4000.0))
				{
					cr_append(&state->hist, state->now, now, state->fighters[j].id, true, state->fighters[j].type);
					describe_crf(state, j);
					state->fighters[j].crashed=true;
					int t=state->fighters[j].targ;
					if(t>=0)
						targs[t].nfighters--;
					int f=state->fighters[j].hflak;
					if(f>=0)
						flaks[f].ftr=-1;
					continue;
				}
				unsigned int type=state->fighters[j].type;
				if(t>state->fighters[j].fuelt)
				{
					int ta=state->fighters[j].targ;
					int fl=state->fighters[j].hflak;
					if(ta>=0)
						targs[ta].nfighters--;
					if(fl>=0)
						flaks[fl].ftr=-1;
					if((ta>=0)||(fl>=0))
						fightersleft++;
					state->fighters[j].targ=-1;
					state->fighters[j].hflak=-1;
					state->fighters[j].k=-1;
					if(t>state->fighters[j].fuelt+(ftypes[type].night?96:58))
					{
						cr_append(&state->hist, state->now, now, state->fighters[j].id, true, state->fighters[j].type);
						if(!state->fighters[j].ld.ds)
							state->fighters[j].ld.ds=DS_FUEL;
						describe_crf(state, j);
						state->fighters[j].crashed=true;
						fightersleft--;
					}
				}
				unsigned int x=state->fighters[j].lon/2, y=state->fighters[j].lat/2;
				double wea=((x<128)&&(y<128))?state->weather.p[x][y]-1000:0;
				if(wea<6)
				{
					double nav=log2(6-wea)/(ftypes[type].night?6.0:2.0);
					double dx=drandu(nav)-nav/2.0,
					       dy=drandu(nav)-nav/2.0;
					state->fighters[j].lon+=dx;
					state->fighters[j].lat+=dy;
				}
				int k=state->fighters[j].k;
				if(k>=0)
				{
					unsigned int ft=state->fighters[j].type, bt=state->bombers[k].type;
					bool radcon=false; // radar controlled
					if(state->fighters[j].hflak>=0)
					{
						unsigned int f=state->fighters[j].hflak;
						radcon=xyr((signed)flaks[f].lat-state->fighters[j].lat, (signed)flaks[f].lon-state->fighters[j].lon, 12);
					}
					if(window) radcon=false;
					bool airad=state->fighters[j].radar;
					if(airad&&wairad) airad=brandp(0.8);
					unsigned int x=state->fighters[j].lon/2, y=state->fighters[j].lat/2;
					double wea=((x<128)&&(y<128))?state->weather.p[x][y]-1000:0;
					double seerange=airad?2.0:7.0/(double)(8+max(4-wea, 0));
					if(state->bombers[k].crashed||!xyr(state->bombers[k].lon-state->fighters[j].lon, state->bombers[k].lat-state->fighters[j].lat, (radcon?12.0:seerange))||brandp(airad?0.03:0.12))
						state->fighters[j].k=-1;
					else
					{
						double x=state->fighters[j].lon,
							y=state->fighters[j].lat;
						double tx=state->bombers[k].lon,
							ty=state->bombers[k].lat;
						double d=hypot(x-tx, y-ty);
						if(d)
						{
							double spd=ftr_speed(state->fighters[j]);
							double cx=(tx-x)/d,
								cy=(ty-y)/d;
							state->fighters[j].lon+=cx*spd;
							state->fighters[j].lat+=cy*spd;
						}
						if(d<(airad?0.4:radcon?0.34:0.25)*(.7*moonillum+.6))
						{
							if(brandp(ftypes[ft].mnv*(2.7+loadness(state->bombers[k]))/400.0))
							{
								unsigned int dmg=irandu(ftypes[ft].arm)*types[bt].defn/15.0;
								state->bombers[k].damage+=dmg;
								if(dmg)
								{
									dmac_append(&state->hist, state->now, now, state->bombers[k].id, false, state->bombers[k].type, dmg, state->bombers[k].damage, state->fighters[j].id);
									state->bombers[k].ld=(dmgsrc){.ds=DS_FIGHTER, .idx=j};
								}
							}
							if(brandp(0.35))
								state->fighters[j].k=-1;
						}
						if(d<(ftypes[ft].night?0.3:0.2)*(.8*moonillum+.6)) // easier to spot nightfighters as they're bigger
						{
							if(!types[bt].noarm&&(brandp(0.2/types[bt].defn)))
							{
								unsigned int dmg=irandu(20);
								state->fighters[j].damage+=dmg;
								if(dmg)
								{
									dmac_append(&state->hist, state->now, now, state->fighters[j].id, true, state->fighters[j].type, dmg, state->fighters[j].damage, state->bombers[k].id);
									state->fighters[j].ld=(dmgsrc){.ds=DS_BOMBER, .idx=k};
								}
								if(brandp(0.6)) // fighter breaks off to avoid return fire, but 40% chance to maintain contact
									state->fighters[j].k=-1;
							}
						}
					}
				}
				else if(state->fighters[j].targ>=0)
				{
					double x=state->fighters[j].lon+drandu(3.0)-1.5,
						y=state->fighters[j].lat+drandu(3.0)-1.5;
					unsigned int t=state->fighters[j].targ;
					int tx=targs[t].lon,
						ty=targs[t].lat;
					double d=hypot(x-tx, y-ty);
					double spd=ftr_speed(state->fighters[j]);
					if(d>0.2)
					{
						double cx=(tx-x)/d,
							cy=(ty-y)/d;
						state->fighters[j].lon+=cx*spd;
						state->fighters[j].lat+=cy*spd;
					}
					else
					{
						double theta=drandu(2*M_PI);
						double cx=cos(theta), cy=sin(theta);
						state->fighters[j].lon+=cx*spd;
						state->fighters[j].lat+=cy*spd;
					}
				}
				else if(state->fighters[j].hflak>=0)
				{
					double x=state->fighters[j].lon+drandu(3.0)-1.5,
						y=state->fighters[j].lat+drandu(3.0)-1.5;
					unsigned int f=state->fighters[j].hflak;
					int fx=flaks[f].lon,
						fy=flaks[f].lat;
					double d=hypot(x-fx, y-fy);
					double spd=ftr_speed(state->fighters[j]);
					if(d>0.2)
					{
						double cx=(fx-x)/d,
							cy=(fy-y)/d;
						state->fighters[j].lon+=cx*spd;
						state->fighters[j].lat+=cy*spd;
					}
					else
					{
						double theta=drandu(2*M_PI);
						double cx=cos(theta), cy=sin(theta);
						state->fighters[j].lon+=cx*spd;
						state->fighters[j].lat+=cy*spd;
					}
				}
				else
				{
					boarable:;
					unsigned int type=state->fighters[j].type;
					double spd=ftr_speed(state->fighters[j]);
					bool boared=false;
					if(tameboar&&ftypes[type].night&&boar_up&&(state->fighters[j].landed||t<state->fighters[j].fuelt))
					{
						// d = b - f
						double dx=boarx-state->fighters[j].lon;
						double dy=boary-state->fighters[j].lat;
						// vector u is boarv, velocity of the bomber stream
						// vector v is the fighter's velocity - only its modulus is known
						// then T = (u.d + sqrt((u.d)^2 + d^2 (v^2 - u^2))) / (v^2 - u^2) is the time to intercept
						// and v = d/t + u is the heading (and speed) to follow
						double vv = spd*spd;
						double ud = boarvx*dx+boarvy*dy;
						double uu = boarvx*boarvx+boarvy*boarvy;
						double dd = dx*dx+dy*dy;
						if(uu<vv)
						{
							double T = (ud + sqrt(ud*ud + dd*(vv-uu))) / (vv-uu);
							double fuelt=state->fighters[j].landed?40:state->fighters[j].fuelt-t-40;
							if(state->gprod[ICLASS_OIL]<(state->nfighters-fightersleft)*100) fuelt=-1;
							if(T<fuelt)
							{
								boared=true;
								if(state->fighters[j].landed)
								{
									state->fighters[j].landed=false;
									int famount=72+irandu(28);
									state->fighters[j].fuelt=t+famount;
									state->gprod[ICLASS_OIL]-=famount*0.2;
								}
								double vx = dx/T + boarvx;
								double vy = dy/T + boarvy;
								state->fighters[j].lon+=vx;
								state->fighters[j].lat+=vy;
							}
						}
					}
					if(!boared&&!state->fighters[j].landed)
					{
						double mrr=1e6;
						unsigned int mb=0;
						for(unsigned int b=0;b<nfbases;b++)
						{
							if(!datewithin(state->now, fbases[b].entry, fbases[b].exit)) continue;
							int bx=fbases[b].lon,
								by=fbases[b].lat;
							double dx=state->fighters[j].lon-bx,
								dy=state->fighters[j].lat-by;
							double rr=dx*dx+dy*dy;
							if(rr<mrr)
							{
								mb=b;
								mrr=rr;
							}
						}
						state->fighters[j].base=mb;
						double x=state->fighters[j].lon,
							y=state->fighters[j].lat;
						int bx=fbases[mb].lon,
							by=fbases[mb].lat;
						double d=hypot(x-bx, y-by);
						if(d>0.8)
						{
							double cx=(bx-x)/d,
								cy=(by-y)/d;
							state->fighters[j].lon+=cx*spd;
							state->fighters[j].lat+=cy*spd;
						}
						else
						{
							state->fighters[j].landed=true;
							if(state->fighters[j].fuelt>t) // return unused fuel
								state->gprod[ICLASS_OIL]+=(state->fighters[j].fuelt-t)*(ftypes[type].night?0.2:0.1);
							state->fighters[j].lon=bx;
							state->fighters[j].lat=by;
						}
					}
				}
			}
			if(datebefore(state->now, event[EVENT_TAMEBOAR])&&!datebefore(state->now, event[EVENT_WURZBURG]))
			{
				for(unsigned int i=0;i<nflaks;i++)
				{
					if(flaks[i].ftr>=0)
					{
						unsigned int ftr=flaks[i].ftr;
						if(state->fighters[ftr].k>=0)
						{
							unsigned int k=state->fighters[ftr].k;
							if(xyr((signed)flaks[i].lat-state->bombers[k].lat, (signed)flaks[i].lon-state->bombers[k].lon, 12))
								state->fighters[ftr].k=-1;
						}
						continue;
					}
					if(!datewithin(state->now, flaks[i].radar, flaks[i].exit)) continue;
					if(!flaks[i].heat) continue;
					if(fightersleft)
					{
						unsigned int mind=1000000;
						int minj=-1;
						unsigned int oilme=(state->nfighters-fightersleft)*3000/max(flaks[i].heat, 15); // don't use up the last of your oil just to chase one bomber...
						for(unsigned int j=0;j<state->nfighters;j++)
						{
							if(state->fighters[j].damage>=1) continue;
							unsigned int type=state->fighters[j].type;
							unsigned int range=(ftypes[type].night?100:50)*ftr_speed(state->fighters[j]);
							if(state->fighters[j].landed)
							{
								if(state->gprod[ICLASS_OIL]>=oilme)
								{
									const unsigned int base=state->fighters[j].base;
									signed int dx=(signed)flaks[i].lat-(signed)fbases[base].lat, dy=(signed)flaks[i].lon-(signed)fbases[base].lon;
									unsigned int dd=dx*dx+dy*dy;
									if(dd<mind&&dd<range*range)
									{
										mind=dd;
										minj=j;
									}
								}
							}
							else if(ftr_free(state->fighters[j]))
							{
								signed int dx=(signed)flaks[i].lat-state->fighters[j].lat, dy=(signed)flaks[i].lon-state->fighters[j].lon;
								unsigned int dd=dx*dx+dy*dy;
								if(dd<mind&&dd<range*range)
								{
									mind=dd;
									minj=j;
								}
							}
						}
						if(minj>=0)
						{
							unsigned int ft=state->fighters[minj].type;
							if(state->fighters[minj].landed)
							{
								int famount=(ftypes[ft].night?(72+irandu(28)):(35+irandu(15)));
								state->gprod[ICLASS_OIL]-=famount*(ftypes[ft].night?0.2:0.1);
								state->fighters[minj].fuelt=t+famount;
							}
							state->fighters[minj].landed=false;
							state->fighters[minj].hflak=i;
							fightersleft--;
							flaks[i].ftr=minj;
						}
					}
				}
			}
			for(unsigned int i=0;i<ntargs;i++)
			{
				if(targs[i].class==TCLASS_MINING) continue;
				targs[i].fires*=.92;
				if(!datewithin(state->now, targs[i].entry, targs[i].exit)) continue;
				double thresh=3e3*targs[i].nfighters/(double)fightersleft;
				if(targs[i].threat*(moonshine?0.7:1.0)+state->heat[i]*(moonshine?12.0:10.0)>thresh)
				{
					unsigned int mind=1000000;
					int minj=-1;
					for(unsigned int j=0;j<state->nfighters;j++)
					{
						if(state->fighters[j].damage>=1) continue;
						unsigned int type=state->fighters[j].type;
						unsigned int range=(ftypes[type].night?100:50)*ftr_speed(state->fighters[j]);
						if(state->fighters[j].landed)
						{
							if(state->gprod[ICLASS_OIL]>=(state->nfighters-fightersleft)*100)
							{
								const unsigned int base=state->fighters[j].base;
								signed int dx=(signed)targs[i].lat-(signed)fbases[base].lat, dy=(signed)targs[i].lon-(signed)fbases[base].lon;
								unsigned int dd=dx*dx+dy*dy;
								if(dd<mind&&dd<range*range)
								{
									mind=dd;
									minj=j;
								}
							}
						}
						else if(ftr_free(state->fighters[j]))
						{
							signed int dx=(signed)targs[i].lat-state->fighters[j].lat, dy=(signed)targs[i].lon-state->fighters[j].lon;
							unsigned int dd=dx*dx+dy*dy;
							if(dd<mind&&dd<range*range)
							{
								mind=dd;
								minj=j;
							}
						}
					}
					if(minj>=0)
					{
						unsigned int ft=state->fighters[minj].type;
						if(state->fighters[minj].landed)
						{
							int famount=(ftypes[ft].night?(72+irandu(28)):(35+irandu(15)));
							state->gprod[ICLASS_OIL]-=famount*(ftypes[ft].night?0.2:0.1);
							state->fighters[minj].fuelt=t+famount;
						}
						state->fighters[minj].landed=false;
						state->fighters[minj].targ=i;
						targs[i].threat-=(thresh*0.6);
						fightersleft--;
						targs[i].nfighters++;
					}
				}
				targs[i].threat*=.98;
				targs[i].threat-=targs[i].nfighters*0.01;
				if(targs[i].nfighters&&(targs[i].threat<thresh/(2.0+max(fightersleft, 2))))
				{
					for(unsigned int j=0;j<state->nfighters;j++)
						if(state->fighters[j].targ==(int)i)
						{
							targs[i].nfighters--;
							fightersleft++;
							state->fighters[j].targ=-1;
							break;
						}
				}
			}
			SDL_FreeSurface(ac_overlay);
			ac_overlay=render_ac(state);
			SDL_BlitSurface(with_weather, NULL, with_ac, NULL);
			SDL_BlitSurface(ac_overlay, NULL, with_ac, NULL);
			SDL_BlitSurface(with_ac, NULL, map_img->data, NULL);
			atg_flip(canvas);
		}
		SDL_FreeSurface(with_flak_and_target);
		SDL_FreeSurface(with_weather);
		SDL_FreeSurface(ac_overlay);
		SDL_FreeSurface(with_ac);
		// incorporate the results, and clear the raids ready for next cycle
		for(unsigned int i=0;i<ntargs;i++)
		{
			heat[i]=0;
			canscore[i]=(state->dmg[i]>0.1);
			for(unsigned int j=0;j<ntypes;j++)
				dij[i][j]=nij[i][j]=tij[i][j]=lij[i][j]=0;
		}
		for(unsigned int i=0;i<ntargs;i++)
		{
			for(unsigned int j=0;j<state->raids[i].nbombers;j++)
			{
				unsigned int k=state->raids[i].bombers[j], type=state->bombers[k].type;
				dij[i][type]++;
				bool leaf=state->bombers[k].b_le;
				bool mine=(targs[i].class==TCLASS_MINING);
				for(unsigned int l=0;l<ntargs;l++)
				{
					if(!state->bombers[k].bombed) break;
					if(!datewithin(state->now, targs[l].entry, targs[l].exit)) continue;
					int dx=floor(state->bombers[k].bmblon+.5)-targs[l].lon, dy=floor(state->bombers[k].bmblat+.5)-targs[l].lat;
					int hx=targs[l].picture->w/2;
					int hy=targs[l].picture->h/2;
					switch(targs[l].class)
					{
						case TCLASS_CITY:
							if(leaf) continue;
							if((abs(dx)<=hx)&&(abs(dy)<=hy))
							{
								if(pget(targs[l].picture, dx+hx, dy+hy).a==ATG_ALPHA_OPAQUE)
								{
									// most of it was already handled when the bombs were dropped
									heat[l]++;
									nij[l][type]++;
									tij[l][type]+=state->bombers[k].b_hc+state->bombers[k].b_gp+state->bombers[k].b_in+state->bombers[k].b_ti;
									state->bombers[k].bombed=false;
								}
							}
						break;
						case TCLASS_INDUSTRY:
							if(leaf) continue;
							if((abs(dx)<=1)&&(abs(dy)<=1))
							{
								heat[l]++;
								if(brandp(targs[l].esiz/30.0))
								{
									unsigned int he=state->bombers[k].b_hc+state->bombers[k].b_gp;
									hi_append(&state->hist, state->now, maketime(state->bombers[k].bt), state->bombers[k].id, false, type, l, he);
									double dmg=min(he/(rindus*600.0), state->dmg[l]);
									cidam+=dmg*(targs[l].berlin?2.0:1.0);
									state->dmg[l]-=dmg;
									tdm_append(&state->hist, state->now, maketime(state->bombers[k].bt), l, dmg, state->dmg[l]);
									nij[l][type]++;
									tij[l][type]+=he;
								}
								state->bombers[k].bombed=false;
							}
						break;
						case TCLASS_AIRFIELD:
						case TCLASS_ROAD:
							if(leaf) continue;
							if((abs(dx)<=hx)&&(abs(dy)<=hy))
							{
								if(pget(targs[l].picture, dx+hx, dy+hy).a==ATG_ALPHA_OPAQUE)
								{
									heat[l]++;
									if(brandp(targs[l].esiz/30.0))
									{
										unsigned int he=state->bombers[k].b_hc+state->bombers[k].b_gp;
										hi_append(&state->hist, state->now, maketime(state->bombers[k].bt), state->bombers[k].id, false, type, l, he);
										double dmg=min(he/(rother*100.0), state->dmg[l]);
										cidam+=dmg*(targs[l].berlin?2.0:1.0);
										state->dmg[l]-=dmg;
										tdm_append(&state->hist, state->now, maketime(state->bombers[k].bt), l, dmg, state->dmg[l]);
										nij[l][type]++;
										tij[l][type]+=he;
									}
									state->bombers[k].bombed=false;
								}
							}
						break;
						case TCLASS_BRIDGE:
							if(leaf) continue;
							if((abs(dx)<=hx)&&(abs(dy)<=hy))
							{
								if(pget(targs[l].picture, dx+hx, dy+hy).a==ATG_ALPHA_OPAQUE)
								{
									heat[l]++;
									if(brandp(targs[l].esiz/30.0))
									{
										unsigned int he=state->bombers[k].b_hc+state->bombers[k].b_gp;
										hi_append(&state->hist, state->now, maketime(state->bombers[k].bt), state->bombers[k].id, false, type, l, he);
										if(brandp(log2(he/(rother*1.25))/200.0))
										{
											cidam+=state->dmg[l];
											bridge+=state->dmg[l];
											tdm_append(&state->hist, state->now, maketime(state->bombers[k].bt), l, state->dmg[l], 0);
											state->dmg[l]=0;
										}
										nij[l][type]++;
										tij[l][type]+=he;
									}
									state->bombers[k].bombed=false;
								}
							}
						break;
						case TCLASS_LEAFLET:
							if(!leaf) continue;
							if((abs(dx)<=hx)&&(abs(dy)<=hy))
							{
								if(pget(targs[l].picture, dx+hx, dy+hy).a==ATG_ALPHA_OPAQUE)
								{
									hi_append(&state->hist, state->now, maketime(state->bombers[k].bt), state->bombers[k].id, false, type, l, state->bombers[k].b_le);
									double dmg=min(state->bombers[k].b_le/(targs[l].psiz*rleaf*600.0), state->dmg[l]);
									state->dmg[l]-=dmg;
									tdm_append(&state->hist, state->now, maketime(state->bombers[k].bt), l, dmg, state->dmg[l]);
									nij[l][type]++;
									tij[l][type]+=state->bombers[k].b_le;
									state->bombers[k].bombed=false;
								}
							}
						break;
						case TCLASS_SHIPPING:
							if(leaf) continue;
							if((abs(dx)<=2)&&(abs(dy)<=1))
							{
								heat[l]++;
								if(brandp(targs[l].esiz/100.0))
								{
									unsigned int he=state->bombers[k].b_hc+state->bombers[k].b_gp;
									nij[l][type]++;
									hi_append(&state->hist, state->now, maketime(state->bombers[k].bt), state->bombers[k].id, false, type, l, he);
									if(brandp(max(log2(he/(rship*100.0))/8.0, 0.05)))
									{
										tij[l][type]++;
										tsh_append(&state->hist, state->now, maketime(state->bombers[k].bt), l);
									}
								}
								state->bombers[k].bombed=false;
							}
						break;
						case TCLASS_MINING:
							if(leaf||!mine) continue;
							int x=state->bombers[k].bmblon/2;
							int y=state->bombers[k].bmblat/2;
							bool water=(x>=0)&&(y>=0)&&(x<128)&&(y<128)&&lorw[x][y];
							if((abs(dx)<=8)&&(abs(dy)<=8)&&water)
							{
								unsigned int he=state->bombers[k].b_hc+state->bombers[k].b_gp;
								hi_append(&state->hist, state->now, maketime(state->bombers[k].bt), state->bombers[k].id, false, type, l, he);
								double dmg=min(he/(rother*20000.0), state->dmg[l]);
								state->dmg[l]-=dmg;
								tdm_append(&state->hist, state->now, maketime(state->bombers[k].bt), l, dmg, state->dmg[l]);
								nij[l][type]++;
								tij[l][type]+=he;
								state->bombers[k].bombed=false;
							}
						break;
						default: // shouldn't ever get here
							fprintf(stderr, "Bad targs[%d].class = %d\n", l, targs[l].class);
						break;
					}
				}
			}
		}
		clear_raids(state);
		for(unsigned int i=0;i<state->nbombers;i++)
		{
			unsigned int type=state->bombers[i].type;
			if(state->bombers[i].crashed)
			{
				lij[state->bombers[i].targ][state->bombers[i].type]++;
				state->nbombers--;
				for(unsigned int j=i;j<state->nbombers;j++)
					state->bombers[j]=state->bombers[j+1];
				i--;
				continue;
			}
			else if(state->bombers[i].damage>50)
			{
				fa_append(&state->hist, state->now, (harris_time){11, 00}, state->bombers[i].id, false, type, 1);
				state->bombers[i].failed=true; // mark as u/s
			}
		}
		// finish the weather
		for(; it<720;it++)
			w_iter(&state->weather, lorw);
	}
	return(SCRN_RRESULTS);
}

void run_raid_free(void)
{
	atg_free_element(run_raid_box);
}
