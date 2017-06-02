#ifndef HIERARCHY_HH
#define HIERARCHY_HH

#include <vector>
#include <string>
#include <assert.h>
#include <iostream>

#include "Cache.hh"
#include "HybridCache.hh"

class HybridCache;
class Hierarchy;
class Access;

class ConfigCache{
	public : 
		ConfigCache(int size , int assoc , int blocksize, std::string policy, int nbNVMways) : \
		m_size(size), m_assoc(assoc), m_blocksize(blocksize), m_policy(policy), m_nbNVMways(nbNVMways), m_printStats(false) {};
		
		ConfigCache(const ConfigCache& a): m_size(a.m_size), m_assoc(a.m_assoc), m_blocksize(a.m_blocksize),\
					 m_policy(a.m_policy), m_nbNVMways(a.m_nbNVMways), m_printStats(a.m_printStats) {};		

		ConfigCache(): m_size(0), m_assoc(0), m_blocksize(0), m_policy(""), m_nbNVMways(0), m_printStats(false) {};

		int m_size;
		int m_assoc;
		int m_blocksize;
		std::string m_policy;	
		int m_nbNVMways;
		bool m_printStats;
};

class Level{

	public:
		Level();
		Level(int level, std::vector<ConfigCache> configs, Hierarchy* system);
		~Level();
		void handleAccess(Access element);
		bool lookup(Access element);
		void deallocate(uint64_t addr);
		void signalDeallocate(uint64_t addr);
		void signalWB(uint64_t addr, bool isDirty);
		void handleWB(uint64_t addr, bool isDirty);
		void print(std::ostream& out);
		void printResults(std::ostream& out);
		void printConfig(std::ostream& out);		
		void finishSimu();
		void openNewTimeFrame();
		
		CacheEntry* getEntry(uint64_t addr);
		
	protected:
		HybridCache* m_dcache;
		HybridCache* m_icache;
		unsigned m_level;
		Hierarchy* m_system;
		bool m_isUnified;
		bool m_printStats;
};

class Hierarchy
{

	public:
		Hierarchy();
		~Hierarchy();
		void print(std::ostream& out);
		void handleAccess(Access element);
		void deallocateFromLevel(uint64_t addr , unsigned level);
		void signalWB(uint64_t addr, bool isDirty, unsigned fromLevel);
		void printResults(std::ostream& out);
		void printConfig(std::ostream& out);
		void finishSimu();
		void openNewTimeFrame();
				
		/** Accessors functions */
		unsigned getNbLevel() const { return m_nbLevel;}
		std::string getConfigFile() const { return m_configFile;}

	protected:
	
		std::vector<std::vector<Level*> > m_levels;

		unsigned int m_nbLevel;
		unsigned int m_nbCores;
		std::string m_configFile;
		
		std::vector<ConfigCache> readConfigFile(std::string configFile);

		uint64_t stats_beginTimeFrame;
};

//std::ostream& operator<<(std::ostream& out, const Hierarchy& obj);
//std::ostream& operator<<(std::ostream& out, const Level& obj);


#endif
