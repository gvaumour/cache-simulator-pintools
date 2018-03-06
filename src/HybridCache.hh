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
class SaturationCounter;
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
		HybridCache(int id, bool isInstructionCache, int size , int assoc , int blocksize , int nbNVMways, std::string policy, Level* system);
		HybridCache(const HybridCache& a);
		~HybridCache();

		void handleAccess(Access element);

		void printResults(std::ostream& out);
		void printConfig(std::ostream& out);
		void print(std::ostream& out);

		bool lookup(Access element);
		int addressToCacheSet(uint64_t address);
		int blockAddressToCacheSet(uint64_t block_add); 
		int findTagInSet(int id_set, uint64_t address); 
		void deallocate(CacheEntry* entry);
		void deallocate(uint64_t addr);
		void allocate(uint64_t address , int id_set , int id_assoc , bool inNVM, uint64_t pc, bool isPrefetch);		
		CacheEntry* getEntry(uint64_t addr);
		void handleWB(uint64_t addr, bool isDirty);
		void signalWB(uint64_t block_addr, bool isKept);
		bool receiveInvalidation(uint64_t addr);
		
		void triggerMigration(int set, int id_assocSRAM, int id_assocNVM , bool fromNVM);
		void updateStatsDeallocate(CacheEntry* current);
		void finishSimu();
		void openNewTimeFrame();
		
		void startWarmup();
		void stopWarmup();
		
		/** Accessors */
		int getSize() const { return m_cache_size;}
		int getBlockSize() const { return m_blocksize;}
		int getAssoc() const { return m_assoc;}
		int getNbSets() const { return m_nb_set;}
		int getStartBit() const { return m_start_index;}
		int getNVMways() const { return m_nbNVMways;}
		std::string getPolicy() const { return m_policy;}
		Level* getSystem() const { return m_system;}
		void setSystem(Level* sys) { m_system = sys;}		
		void setPrintState(bool printStats) { m_printStats = printStats;};
		int getID() const {return m_ID;};
		bool isInstCache() const {return m_isInstructionCache;};
		bool isPrefetchBlock(uint64_t block_addr);
		void resetPrefetchFlag(uint64_t block_addr);

	private :
	
		int m_ID;
		bool m_isInstructionCache;
		
		std::vector<std::vector<CacheEntry*> > m_tableNVM;
		std::vector<std::vector<CacheEntry*> > m_tableSRAM;

		std::map<uint64_t , HybridLocation> m_tag_index;    		
   		Predictor *m_predictor;		
		std::string m_policy;
		bool m_printStats;
		bool m_isWarmup; //Stats are not recorded during warmup phase
		bool m_Deallocating;
		
		int m_start_index;
		int m_end_index;
		int m_cache_size;
		int m_assoc;
		int m_nbNVMways;
		int m_nbSRAMways;
		int m_nb_set;
		int m_blocksize;    
		Level* m_system;

		/* Stats */ 		
		std::vector<uint64_t> stats_missSRAM;
		std::vector<uint64_t> stats_hitsSRAM;
		uint64_t stats_dirtyWBSRAM;
		uint64_t stats_cleanWBSRAM;
		uint64_t stats_evict;
		uint64_t stats_bypass;
		
		std::vector<uint64_t> stats_missNVM;
		std::vector<uint64_t> stats_hitsNVM;
		uint64_t stats_dirtyWBNVM;
		uint64_t stats_cleanWBNVM;
		
		std::vector<uint64_t> stats_migration;
		
		std::vector<uint64_t> stats_operations;
		uint64_t stats_nbFetchedLines;
		uint64_t stats_nbLostLine;

		uint64_t stats_nbROlines;
		uint64_t stats_nbROaccess;

		uint64_t stats_nbRWlines;
		uint64_t stats_nbRWaccess;

		uint64_t stats_nbWOlines;
		uint64_t stats_nbWOaccess;

		uint64_t stats_nbAlmostROlines;
		uint64_t stats_nbAlmostROaccess;
		std::map<uint64_t,uint64_t> stats_histo_ratioRW;
		
		uint64_t stats_nbDeadMigration;
		uint64_t stats_nbPingMigration;
				
		/********/
		void entete_debug();

};

//std::ostream& operator<<(std::ostream& out, const HybridCache& obj);


#endif

