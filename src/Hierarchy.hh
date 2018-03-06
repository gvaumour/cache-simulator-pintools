#ifndef HIERARCHY_HH
#define HIERARCHY_HH

#include <vector>
#include <string>
#include <assert.h>
#include <iostream>

#include "Cache.hh"
#include "Level.hh"
#include "Directory.hh"
#include "Prefetcher.hh"

class Level;
class Access;


class Hierarchy
{

	public:
		Hierarchy();
		Hierarchy(const Hierarchy& a);
		Hierarchy(std::string policy, int nbCores);
		~Hierarchy();
		void print(std::ostream& out);
		void handleAccess(Access element);
		void L1sdeallocate(uint64_t addr);
		void signalWB(uint64_t addr, bool isDirty, bool isKept, int idcore);
		void printResults(std::ostream& out);
		void printConfig(std::ostream& out);
		void finishSimu();
		void openNewTimeFrame();
		int convertThreadIDtoCore(int id_thread);			
		void prefetchAddress(Access element);

		void startWarmup();
		void stopWarmup();


		
		/** Accessors functions */
		unsigned getNbLevel() const { return m_nbLevel;};
		std::string getConfigFile() const { return m_configFile;};
		Directory* getDirectory() { return m_directory;};

	protected:
	
		std::vector<Level*> m_private_caches ;
		Level* m_LLC;
		Directory* m_directory;
		Prefetcher* m_prefetcher;

		unsigned int m_nbLevel;
		unsigned int m_nbCores;
		std::string m_configFile;
		
		std::vector<ConfigCache> readConfigFile(std::string configFile);

		int m_start_index;
		uint64_t stats_beginTimeFrame;
		uint64_t stats_cleanWB_MainMem;
		uint64_t stats_dirtyWB_MainMem;
		uint64_t stats_readMainMem;
		uint64_t stats_writeMainMem;

		uint64_t stats_issuedPrefetchs;
		uint64_t stats_hitsPrefetch;
};

//std::ostream& operator<<(std::ostream& out, const Hierarchy& obj);
//std::ostream& operator<<(std::ostream& out, const Level& obj);


#endif
