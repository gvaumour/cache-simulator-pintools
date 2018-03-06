#ifndef _LEVEL_HPP
#define _LEVEL_HPP

#include <vector>
#include <string>
#include <assert.h>
#include <iostream>

#include "Cache.hh"
#include "HybridCache.hh"
#include "Hierarchy.hh"

class HybridCache;
class Hierarchy;
class Access;

class Level{

	public:
//		Level();
		Level(int id_core, std::vector<ConfigCache> configs, Hierarchy* system);
		~Level();
		void handleAccess(Access element);
		bool lookup(Access element);
		void deallocate(uint64_t addr);
		void signalDeallocate(uint64_t addr);
		void signalWB(uint64_t addr, bool isDirty, bool isKept);

		void sendInvalidation(uint64_t addr, bool toInstCache);
		bool receiveInvalidation(uint64_t addr);
		
		void handleWB(uint64_t addr, bool isDirty);
		void print(std::ostream& out);
		void printResults(std::ostream& out);
		void printConfig(std::ostream& out);		
		void finishSimu();
		void openNewTimeFrame();
		
		void startWarmup();
		void stopWarmup();

		void resetPrefetchFlag(uint64_t block_addr);
		bool isPrefetchBlock(uint64_t block_addr);
		
		CacheEntry* getEntry(uint64_t addr);
		
	protected:
		HybridCache* m_dcache;
		HybridCache* m_icache;
		int m_IDcore;
		Hierarchy* m_system;
		bool m_isUnified;
		bool m_printStats;
};

#endif
