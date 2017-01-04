#!/usr/bin/python2
"""prodloss - production and loss figures"""

import sys
import hhist, hdata

def extract_prodloss(ents):
	days = sorted(hhist.group_by_date(ents))
	bentry = [b.get('entry', hhist.date(0, 0, 0)) for b in hdata.Bombers]
	res = {}
	for d in days:
		row = [[0, 0] for b in hdata.Bombers]
		for ent in d[1]:
			if ent['class'] != 'A': continue
			if ent['data']['type']['fb'] != 'B': continue
			typ = ent['data']['type']['ti']
			if ent['data']['etyp'] == 'CT':
				if d[0].next() != bentry[typ]:
					row[typ][0] += 1
			elif ent['data']['etyp'] == 'CR':
				row[typ][1] -= 1
		res[d[0]] = row
	return(res)

if __name__ == '__main__':
	entries = hhist.import_from_save(sys.stdin)
	prodloss = extract_prodloss(entries)
	for d in sorted(prodloss):
		ostr = '%s: %s' % (d, prodloss[d])
		print(ostr)
