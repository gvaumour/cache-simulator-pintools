/** 
Copyright (C) 2016 Gregory Vaumourin

This program is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation; either version 2 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program; if not, write to the Free Software
Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
**/

#include <vector>
#include <iostream>
#include <map>
#include <climits>

#include "common.hh"
#include "ReplacementPolicy.hh"


using namespace std;


ReplacementPolicy::~ReplacementPolicy()
{

}

LRUPolicy::LRUPolicy(int nbAssoc , int nbSet , std::vector<std::vector<CacheEntry*> > cache_entries) : ReplacementPolicy(nbAssoc , nbSet, cache_entries) 
{
	m_cpt = 1;
}

void
LRUPolicy::updatePolicy(uint64_t set, uint64_t index, int hints = 0)
{	
	m_cache_entries[set][index]->policyInfo = m_cpt;
	m_cpt++;
}

int
LRUPolicy::evictPolicy(int set)
{
	int smallest_time = m_cache_entries[set][0]->policyInfo , smallest_index = 0;

	for(int i = 0 ; i < m_assoc ; i++){
		if(!m_cache_entries[set][i]->isValid) 
			return i;
	}
	
	for(int i = 0 ; i < m_assoc ; i++){
		if(m_cache_entries[set][i]->policyInfo < smallest_time){
			smallest_time =  m_cache_entries[set][i]->policyInfo;
			smallest_index = i;
		}
	}
	return smallest_index;
}

int 
RandomPolicy::evictPolicy(int set)
{
	return rand()%m_assoc;
}

