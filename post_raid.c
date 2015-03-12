/*
	harris - a strategy game
	Copyright (C) 2012-2014 Edward Cree

	licensed under GPLv3+ - see top of harris.c for details
	
	post_raid: post-raid processing (pretends to be a screen, but isn't really)
*/

#include "post_raid.h"

#include <math.h>
#include "ui.h"
#include "globals.h"
#include "bits.h"
#include "date.h"
#include "history.h"
#include "rand.h"

void produce(int targ, game *state, double amount);

atg_element *post_raid_box;

int post_raid_create(void)
{
	// nothing to do
	return(0);
}

screen_id post_raid_screen(__attribute__((unused)) atg_canvas *canvas, game *state)
{
	for(unsigned int i=0;i<state->nbombers;i++)
	{
		unsigned int type=state->bombers[i].type;
		if(!datewithin(state->now, types[type].entry, types[type].exit))
		{
			ob_append(&state->hist, state->now, (harris_time){11, 10}, state->bombers[i].id, false, type);
			state->nbombers--;
			for(unsigned int j=i;j<state->nbombers;j++)
				state->bombers[j]=state->bombers[j+1];
			i--;
			continue;
		}
		if(state->bombers[i].failed)
		{
			if(brandp((types[type].svp/100.0)/2.0))
			{
				fa_append(&state->hist, state->now, (harris_time){11, 10}, state->bombers[i].id, false, type, 0);
				state->bombers[i].failed=false;
			}
		}
		else
		{
			if(brandp((1-types[type].svp/100.0)/2.4))
			{
				fa_append(&state->hist, state->now, (harris_time){11, 10}, state->bombers[i].id, false, type, 1);
				state->bombers[i].failed=true;
			}
		}
	}
	date tomorrow=nextday(state->now);
	// TODO: if confid or morale too low, SACKED
	state->cshr+=state->confid*2.0;
	state->cshr+=state->morale;
	state->cshr*=.96;
	state->cash+=state->cshr;
	ca_append(&state->hist, state->now, (harris_time){11, 20}, state->cshr, state->cash);
	// Update bomber prodn caps
	for(unsigned int i=0;i<ntypes;i++)
	{
		if(!state->btypes[i]) continue;
		if(!diffdate(tomorrow, types[i].entry)) // when a type is new, get 15 of them free
		{
			for(unsigned int a=0;a<15;a++)
			{
				unsigned int n=state->nbombers++;
				ac_bomber *nb=realloc(state->bombers, state->nbombers*sizeof(ac_bomber));
				if(!nb)
				{
					perror("realloc"); // TODO more visible warning
					state->nbombers=n;
					break;
				}
				(state->bombers=nb)[n]=(ac_bomber){.type=i, .failed=false, .id=rand_acid()};
				for(unsigned int j=0;j<NNAVAIDS;j++)
					nb[n].nav[j]=false;
				if((!datebefore(state->now, event[EVENT_ALLGEE]))&&types[i].nav[NAV_GEE])
					nb[n].nav[NAV_GEE]=true;
				ct_append(&state->hist, state->now, (harris_time){11, 25}, state->bombers[n].id, false, state->bombers[n].type);
			}
		}
		if(!datewithin(state->now, types[i].entry, types[i].exit)) continue;
		types[i].pcbuf=min(types[i].pcbuf, 180000)+types[i].pc;
	}
	// purchase additional planes based on priorities and subject to production capacity constraints
	while(true)
	{
		unsigned int m=0;
		for(unsigned int i=1;i<ntypes;i++)
		{
			if(!state->btypes[i]) continue;
			if(!datewithin(state->now, types[i].entry, types[i].exit)) continue;
			if(types[i].pribuf>types[m].pribuf) m=i;
		}
		if(types[m].pribuf<8)
		{
			bool any=false;
			for(unsigned int i=0;i<ntypes;i++)
			{
				if(!state->btypes[i]) continue;
				if(!datewithin(state->now, types[i].entry, types[i].exit)) continue;
				unsigned int prio=(unsigned int [4]){0, 1, 3, 6}[types[i].prio];
				if(datebefore(state->now, types[i].novelty)) prio=max(prio, 1);
				types[i].pribuf+=prio;
				if(prio) any=true;
			}
			if(!any) break;
			continue;
		}
		if(!state->btypes[m]) // is possible, if m==0
			break;
		unsigned int cost=types[m].cost;
		if(types[m].broughton&&!datebefore(state->now, event[EVENT_BROUGHTON]))
			cost=(cost*2)/3;
		if(cost>state->cash) break;
		if(cost>types[m].pcbuf) break;
		unsigned int n=state->nbombers++;
		ac_bomber *nb=realloc(state->bombers, state->nbombers*sizeof(ac_bomber));
		if(!nb)
		{
			perror("realloc"); // TODO more visible warning
			state->nbombers=n;
			break;
		}
		(state->bombers=nb)[n]=(ac_bomber){.type=m, .failed=false, .id=rand_acid()};
		for(unsigned int j=0;j<NNAVAIDS;j++)
			nb[n].nav[j]=false;
		if((!datebefore(state->now, event[EVENT_ALLGEE]))&&types[m].nav[NAV_GEE])
			nb[n].nav[NAV_GEE]=true;
		state->cash-=cost;
		types[m].pcbuf-=cost;
		types[m].pc+=cost/100;
		types[m].pribuf-=8;
		ct_append(&state->hist, state->now, (harris_time){11, 30}, state->bombers[n].id, false, state->bombers[n].type);
	}
	// install navaids
	for(unsigned int n=0;n<NNAVAIDS;n++)
	{
		if(datebefore(state->now, event[navevent[n]])) continue;
		date x=event[navevent[n]];
		x.month+=2;
		if(x.month>12) {x.month-=12;x.year++;}
		bool notnew=datebefore(x, state->now);
		state->napb[n]+=notnew?25:10;
		int i=state->nap[n];
		if(i>=0&&!datewithin(state->now, types[i].entry, types[i].exit)) continue;
		unsigned int nac=0;
		for(int j=state->nbombers-1;(state->napb[n]>=navprod[n])&&(j>=0);j--)
		{
			if(i<0)
			{
				if(!types[state->bombers[j].type].nav[n]) continue;
			}
			else
			{
				if((int)state->bombers[j].type!=i)	continue;
			}
			if(state->bombers[j].failed) continue;
			if(state->bombers[j].nav[n]) continue;
			state->bombers[j].nav[n]=true;
			na_append(&state->hist, state->now, (harris_time){11, 35}, state->bombers[j].id, false, state->bombers[j].type, n);
			state->napb[n]-=navprod[n];
			if(++nac>=(notnew?10:4)) break;
		}
	}
	// assign PFF
	if(!datebefore(tomorrow, event[EVENT_PFF]))
	{
		for(unsigned int j=0;j<ntypes;j++)
			types[j].count=types[j].pffcount=0;
		for(int i=state->nbombers-1;i>=0;i--)
		{
			unsigned int type=state->bombers[i].type;
			types[type].count++;
			if(state->bombers[i].pff) types[type].pffcount++;
			if(types[type].pff&&types[type].noarm)
			{
				if(!state->bombers[i].pff)
				{
					state->bombers[i].pff=true;
					pf_append(&state->hist, state->now, (harris_time){11, 40}, state->bombers[i].id, false, type);
				}
				continue;
			}
		}
		for(unsigned int j=0;j<ntypes;j++)
		{
			if(!types[j].pff) continue;
			if(types[j].noarm) continue;
			int pffneed=ceil(types[j].count*0.2)-types[j].pffcount;
			for(int i=state->nbombers-1;(pffneed>0)&&(i>=0);i--)
			{
				if(state->bombers[i].type!=j) continue;
				if(state->bombers[i].pff) continue;
				state->bombers[i].pff=true;
				pf_append(&state->hist, state->now, (harris_time){11, 40}, state->bombers[i].id, false, j);
				pffneed--;
			}
		}
	}
	// German production
	unsigned int rcity=GET_DC(state,RCITY),
	             rother=GET_DC(state,ROTHER);
	for(unsigned int i=0;i<ICLASS_MIXED;i++)
		state->dprod[i]=0;
	for(unsigned int i=0;i<ntargs;i++)
	{
		if(targs[i].city<0)
		{
			if(!datebefore(state->now, targs[i].exit)) continue;
		}
		else
		{
			if(!datebefore(state->now, targs[targs[i].city].exit)) continue;
		}
		switch(targs[i].class)
		{
			case TCLASS_CITY:
			{
				state->flam[i]=(state->flam[i]*0.8)+8.0;
				double dflk=(targs[i].flak*state->dmg[i]*.0005)-(state->flk[i]*.05);
				state->flk[i]+=dflk;
				if(dflk)
					tfk_append(&state->hist, state->now, (harris_time){11, 45}, i, dflk, state->flk[i]);
				double ddmg=min(state->dmg[i]*.1/(double)rcity, 100-state->dmg[i]);
				state->dmg[i]+=ddmg;
				if(ddmg)
					tdm_append(&state->hist, state->now, (harris_time){11, 45}, i, ddmg, state->dmg[i]);
				produce(i, state, state->dmg[i]*targs[i].prod*0.8);
			}
			break;
			case TCLASS_LEAFLET:
			{
				double ddmg=min(state->dmg[i]*.2/(double)rother, 100-state->dmg[i]);
				state->dmg[i]+=ddmg;
				if(ddmg)
					tdm_append(&state->hist, state->now, (harris_time){11, 45}, i, ddmg, state->dmg[i]);
				produce(i, state, state->dmg[i]*targs[i].prod/20.0);
			}
			break;
			case TCLASS_SHIPPING:
			case TCLASS_MINING:
			break;
			case TCLASS_AIRFIELD:
			case TCLASS_BRIDGE:
			case TCLASS_ROAD:
				if(!state->dmg[i])
					goto unflak;
			break;
			case TCLASS_INDUSTRY:
				if(state->dmg[i])
				{
					double ddmg=min(state->dmg[i]/(double)rother, 100-state->dmg[i]), cscale=targs[i].city<0?1.0:state->dmg[targs[i].city]/100.0;
					if(cscale==0)
						goto unflak;
					state->dmg[i]+=ddmg;
					if(ddmg)
						tdm_append(&state->hist, state->now, (harris_time){11, 45}, i, ddmg, state->dmg[i]);
					produce(i, state, state->dmg[i]*targs[i].prod*cscale/2.0);
				}
				else
				{
					unflak:
					if(state->flk[i])
					{
						tfk_append(&state->hist, state->now, (harris_time){11, 45}, i, -state->flk[i], 0);
						state->flk[i]=0;
					}
				}
			break;
			default: // shouldn't ever get here
				fprintf(stderr, "Bad targs[%d].class = %d\n", i, targs[i].class);
			break;
		}
	}
	state->gprod[ICLASS_ARM]*=datebefore(state->now, event[EVENT_BARBAROSSA])?0.96:0.94;
	state->gprod[ICLASS_BB]*=0.99;
	state->gprod[ICLASS_RAIL]*=0.99;
	state->gprod[ICLASS_OIL]*=0.984;
	state->gprod[ICLASS_UBOOT]*=0.95; // not actually used for anything
	// German fighters
	unsigned int fcount[nftypes];
	memset(fcount, 0, sizeof(fcount));
	unsigned int maxradpri=0;
	for(unsigned int i=0;i<state->nfighters;i++)
	{
		unsigned int type=state->fighters[i].type;
		if(state->fighters[i].crashed||!datewithin(state->now, ftypes[type].entry, ftypes[type].exit))
		{
			if(!state->fighters[i].crashed&&!datewithin(state->now, ftypes[type].entry, ftypes[type].exit))
				ob_append(&state->hist, state->now, (harris_time){11, 48}, state->fighters[i].id, true, type);
			state->nfighters--;
			for(unsigned int j=i;j<state->nfighters;j++)
				state->fighters[j]=state->fighters[j+1];
			i--;
			continue;
		}
		if((!state->fighters[i].radar)&&(ftypes[type].radpri>maxradpri))
			maxradpri=ftypes[type].radpri;
		fcount[type]++;
		if(brandp(0.1))
		{
			unsigned int base;
			do
				base=state->fighters[i].base=irandu(nfbases);
			while(!datewithin(state->now, fbases[base].entry, fbases[base].exit));
		}
	}
	if(!datebefore(state->now, event[EVENT_L_BC]) && maxradpri)
	{
		unsigned int rcount=4;
		for(int i=state->nfighters-1;i>=0;i--)
		{
			if(state->gprod[ICLASS_RADAR]<5000 || !rcount) break;
			unsigned int type=state->fighters[i].type;
			if((!state->fighters[i].radar)&&(ftypes[type].radpri==maxradpri))
			{
				state->fighters[i].radar=true;
				na_append(&state->hist, state->now, (harris_time){11, 49}, state->fighters[i].id, true, type, 0);
				state->gprod[ICLASS_RADAR]-=5000;
				rcount--;
			}
		}
	}
	unsigned int mfcost=0;
	for(unsigned int i=0;i<nftypes;i++)
	{
		if(!datewithin(state->now, ftypes[i].entry, ftypes[i].exit)) continue;
		mfcost=max(mfcost, ftypes[i].cost);
	}
	while(state->gprod[ICLASS_AC]>=mfcost)
	{
		double p[nftypes], cumu_p=0;
		for(unsigned int j=0;j<nftypes;j++)
		{
			if(!datewithin(state->now, ftypes[j].entry, ftypes[j].exit))
			{
				p[j]=0;
				continue;
			}
			date start=ftypes[j].entry, end=ftypes[j].exit;
			if(start.year<1939) start=(date){1938, 6, 1};
			if(end.year>1945) end=(date){1946, 8, 1};
			int dur=diffdate(end, start), age=diffdate(state->now, start);
			double fr_age=age/(double)dur;
			p[j]=(0.8-fr_age)/sqrt(1+fcount[j]);
			p[j]=max(p[j], 0);
			cumu_p+=p[j];
		}
		double d=drandu(cumu_p);
		unsigned int i=0;
		while(d>=p[i]) d-=p[i++];
		if(!datewithin(state->now, ftypes[i].entry, ftypes[i].exit)) continue; // should be impossible as p[i] == 0
		if(ftypes[i].cost>state->gprod[ICLASS_AC]) break; // should also be impossible as cost <= mfcost <= state->gprod
		unsigned int n=state->nfighters++;
		ac_fighter *newf=realloc(state->fighters, state->nfighters*sizeof(ac_fighter));
		if(!newf)
		{
			perror("realloc"); // TODO more visible warning
			state->nfighters=n;
			break;
		}
		unsigned int base;
		do
			base=irandu(nfbases);
		while(!datewithin(state->now, fbases[base].entry, fbases[base].exit));
		(state->fighters=newf)[n]=(ac_fighter){.type=i, .base=base, .crashed=false, .landed=true, .k=-1, .targ=-1, .damage=0, .id=rand_acid()};
		state->gprod[ICLASS_AC]-=ftypes[i].cost;
		ct_append(&state->hist, state->now, (harris_time){11, 50}, state->fighters[n].id, true, i);
	}
	for(unsigned int i=0;i<ICLASS_MIXED;i++)
		gp_append(&state->hist, state->now, (harris_time){11, 55}, i, state->gprod[i], state->dprod[i]);
	state->now=tomorrow;
	for(unsigned int i=0;i<ntypes;i++)
	{
		if(!state->btypes[i]) continue;
		if(!diffdate(state->now, types[i].entry))
		{
			if(msgadd(canvas, state, state->now, types[i].name, types[i].newtext))
				fprintf(stderr, "failed to msgadd newtype: %s\n", types[i].name);
		}
	}
	for(unsigned int i=0;i<nftypes;i++)
		if(!diffdate(state->now, ftypes[i].entry))
		{
			if(msgadd(canvas, state, state->now, ftypes[i].name, ftypes[i].newtext))
				fprintf(stderr, "failed to msgadd newftype: %s\n", ftypes[i].name);
		}
	for(unsigned int ev=0;ev<NEVENTS;ev++)
	{
		if(!diffdate(state->now, event[ev]))
		{
			if(msgadd(canvas, state, state->now, event_names[ev], evtext[ev]))
				fprintf(stderr, "failed to msgadd event: %s\n", event_names[ev]);
		}
	}
	return(SCRN_CONTROL);
}

void post_raid_free(void)
{
	// nothing to do
}

void produce(int targ, game *state, double amount)
{
	if((targs[targ].iclass!=ICLASS_RAIL)&&(targs[targ].iclass!=ICLASS_MIXED))
		amount=min(amount, state->gprod[ICLASS_RAIL]*8.0);
	switch(targs[targ].iclass)
	{
		case ICLASS_BB:
		case ICLASS_RAIL:
		case ICLASS_STEEL:
		case ICLASS_OIL:
		case ICLASS_UBOOT:
		break;
		case ICLASS_ARM:
			amount=min(amount, state->gprod[ICLASS_STEEL]);
			state->gprod[ICLASS_STEEL]-=amount;
		break;
		case ICLASS_AC:
			amount=min(amount, state->gprod[ICLASS_BB]*2.0);
			state->gprod[ICLASS_BB]-=amount/2.0;
		break;
		case ICLASS_RADAR:
			if(datebefore(state->now, event[EVENT_LICHTENSTEIN]))
				return;
		break;
		case ICLASS_MIXED:
#define ADD(class, qty)	do { state->gprod[class]+=qty; state->dprod[class]+=qty; } while(0)
			ADD(ICLASS_OIL, amount/7.0);
			ADD(ICLASS_RAIL, amount/21.0);
			ADD(ICLASS_ARM, amount/7.0);
			ADD(ICLASS_AC, amount/21.0);
			ADD(ICLASS_BB, amount/14.0);
			ADD(ICLASS_STEEL, amount/35.0);
			if(!datebefore(state->now, event[EVENT_LICHTENSTEIN]))
				ADD(ICLASS_RADAR, amount/200.0);
#undef ADD
			return;
		default:
			fprintf(stderr, "Bad targs[%d].iclass = %d\n", targ, targs[targ].iclass);
		break;
	}
	if(targs[targ].iclass!=ICLASS_RAIL)
		state->gprod[ICLASS_RAIL]-=amount/8.0;
	state->gprod[targs[targ].iclass]+=amount;
	state->dprod[targs[targ].iclass]+=amount;
}
