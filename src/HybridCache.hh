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


#ifndef HYBRID_CACHE_HPP
#define HYBRID_CACHE_HPP

#include <map>
#include <stdint.h>
#include <vector>
#include <ostream>

#include "Cache.hh"
#include "Predictor.hh"
#include "Hierarchy.hh"
#include "common.hh"

class Level;
class Predictor;
class Access;
class CacheEntry;


class HybridLocation
{
	public: 
		int m_way;
		bool m_inNVM;
		HybridLocation() : m_way(-1), m_inNVM(false){};
		HybridLocation(int way , bool inNVM) : m_way(way), m_inNVM(inNVM) {}; 
};

class HybridCache {


	public : 
		HybridCache(int size , int assoc , int blocksize , int nbNVMways, std::string policy, Level* system);
		HybridCache();
		HybridCache(const HybridCache& a);
		~HybridCache();

		void handleAccess(Access element);

		void printStats();
		void print(std::ostream& out) const;
		bool lookup(Access element);

		int addressToCacheSet(uint64_t address);
		int findTagInSet(int id_set, uint64_t address); 
		void deallocate(CacheEntry* entry);
		void deallocate(uint64_t addr);
		void allocate(uint64_t address , int id_set , int id_assoc , bool inNVM);		
		CacheEntry* getEntry(uint64_t addr);

		int getSize() const { return m_cache_size;}
		int getBlockSize() const { return m_blocksize;}
		int getAssoc() const { return m_assoc;}
		int getNbSets() const { return m_nb_set;}
		int getStartBit() const { return m_start_index;}
		int getNVMways() const { return m_nbNVMways;}
		std::string getPolicy() const { return m_policy;}
		Level* getSystem() const { return m_system;}
		void setSystem(Level* sys) { m_system = sys;}

		double getConsoDynamique();
		double getConsoStatique();
		
		
	private :
		
		std::vector<int> stats_missSRAM;
		std::vector<int> stats_hitsSRAM;
		int stats_dirtyWBSRAM;
		int stats_cleanWBSRAM;

		std::vector<int> stats_missNVM;
		std::vector<int> stats_hitsNVM;
		int stats_dirtyWBNVM;
		int stats_cleanWBNVM;
		
		std::vector<int> stats_operations;

		std::vector<std::vector<CacheEntry*> > m_tableNVM;
		std::vector<std::vector<CacheEntry*> > m_tableSRAM;

		std::map<uint64_t , HybridLocation> m_tag_index;    		
    
		Predictor *m_predictor;
		
		std::string m_policy;

		int m_start_index;
		int m_end_index;
		int m_cache_size;
		int m_assoc;
		int m_nbNVMways;
		int m_nbSRAMways;
		int m_nb_set;
		int m_blocksize;    
		int nb_double;

		Level* m_system;
};

std::ostream& operator<<(std::ostream& out, const HybridCache& obj);


#endif

