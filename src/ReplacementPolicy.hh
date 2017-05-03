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

#ifndef REPLACEMENT_POLICY_HPP
#define REPLACEMENT_POLICY_HPP

#include <vector>
#include <stdint.h>

#include "Cache.hh"

class CacheEntry;

class ReplacementPolicy{
	
	public : 
		ReplacementPolicy() : m_nb_set(0), m_assoc(0) {};
		ReplacementPolicy(int nbAssoc , int nbSet , std::vector<std::vector<CacheEntry*> > cache_entries ) : m_cache_entries(cache_entries),\
											m_nb_set(nbSet) , m_assoc(nbAssoc) {};
		virtual ~ReplacementPolicy();
		virtual void updatePolicy(uint64_t set, uint64_t index, int hints) = 0;
		virtual void insertionPolicy(uint64_t set, uint64_t index, int hints) = 0;
		virtual int evictPolicy(int set) = 0;
		
	protected : 
		std::vector<std::vector<CacheEntry*> > m_cache_entries;
		int m_nb_set;
		int m_assoc;
	
};


class LRUPolicy : public ReplacementPolicy {

	public :
		LRUPolicy() : ReplacementPolicy(){ m_cpt=1;};
		LRUPolicy(int nbAssoc , int nbSet , std::vector<std::vector<CacheEntry*> > cache_entries);
		void updatePolicy(uint64_t set, uint64_t index, int hints);
		void insertionPolicy(uint64_t set, uint64_t index, int hints) { updatePolicy(set,index, hints);}
		int evictPolicy(int set);
		~LRUPolicy() {};
	private : 
		uint64_t m_cpt;
		
};

class RandomPolicy : public ReplacementPolicy {

	public :
		RandomPolicy() :  ReplacementPolicy(){};
		RandomPolicy(int nbAssoc , int nbSet , std::vector<std::vector<CacheEntry*> > cache_entries) : ReplacementPolicy(nbAssoc, nbSet, cache_entries) {
			srand(time(NULL));
		};
		void updatePolicy(uint64_t set, uint64_t index, int hints){};
		void insertionPolicy(uint64_t set, uint64_t index, int hints){};
		int evictPolicy(int set);
		~RandomPolicy(){};
};


#endif
