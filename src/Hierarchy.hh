#ifndef HIERARCHY_HH
#define HIERARCHY_HH

#include <vector>
#include <string>
#include <assert.h>

#include "Cache.hh"
#include "HybridCache.hh"

class HybridCache;
class Hierarchy;
class Access;

class ConfigCache{
	public : 
		ConfigCache(int size , int assoc , int blocksize, std::string policy, int nbNVMways) : \
		m_size(size), m_assoc(assoc), m_blocksize(blocksize), m_policy(policy), m_nbNVMways(nbNVMways) {};
		
		ConfigCache(const ConfigCache& a): m_size(a.m_size), m_assoc(a.m_assoc), m_blocksize(a.m_blocksize),\
					 m_policy(a.m_policy), m_nbNVMways(a.m_nbNVMways) {};		

		ConfigCache(): m_size(0), m_assoc(0), m_blocksize(0), m_policy(""), m_nbNVMways(0) {};

		int m_size;
		int m_assoc;
		int m_blocksize;
		std::string m_policy;	
		int m_nbNVMways;	
};

class Level{

	public:
		Level() : m_caches(0), m_level(-1), m_system(NULL) { };
		Level(int level, std::vector<ConfigCache> configs, Hierarchy* system);
		Level(const Level& a); 

		void handleAccess(Access element);
		bool lookup(Access element);
		void deallocate(uint64_t addr);
		void signalDeallocate(uint64_t addr);
		void print(std::ostream& out) const;
		void printResults();
		std::vector<HybridCache> getCaches() const {return m_caches;};
		int getLevel() const { return m_level;};
		Hierarchy* getSystem() const { return m_system;};
		

	protected:
		std::vector<HybridCache> m_caches;
		int m_level;
		Hierarchy* m_system;
};

class Hierarchy
{

	public:
		Hierarchy(): m_levels(0), m_nbLevel(0), m_configFile("") {};
		Hierarchy(int nbLevel);
		void print(std::ostream& out) const;
		void handleAccess(Access element);
		void deallocateFromLevel(uint64_t addr , int level);
		void printResults();

	protected:
	
		std::vector<Level> m_levels;
		unsigned int m_nbLevel;
		std::string m_configFile;
		
		std::vector<ConfigCache> readConfigFile(std::string configFile);

};

std::ostream& operator<<(std::ostream& out, const Hierarchy& obj);
std::ostream& operator<<(std::ostream& out, const Level& obj);


#endif
