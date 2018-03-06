#ifndef __DIRECTORY__HH__
#define __DIRECTORY__HH__

#include <set>
#include <vector>


#include <stdint.h>

#include "Cache.hh"
#include "common.hh"
#define DIRECTORY_ASSOC 64
#define DIRECTORY_NB_SETS 2048


class DirectoryEntry
{
	public:
		DirectoryEntry()
		{
			isValid = false;	
			initEntry();
		};

		void initEntry()
		{
			block_addr = 0;
			isInst = false;
			coherence_state = NOT_PRESENT;
			nodeTrackers.clear();
		};
		void resetTrackers()
		{
			nodeTrackers.clear();
		};
		bool isInL1()
		{	
			return isValid && ( (coherence_state == SHARED_L1) || \
			(coherence_state == EXCLUSIVE_L1) || \
			(coherence_state == MODIFIED_L1) );
		};
		
		void print();
				
		std::set<int> nodeTrackers;
		uint64_t block_addr;
		DirectoryState coherence_state;
		bool isValid;
		bool isInst;
		
		int policyInfo;
		
};

class DirectoryReplacementPolicy{

	public :
		DirectoryReplacementPolicy(int nbAssoc , int nbSet , std::vector<std::vector<DirectoryEntry*> >& dir_entries);
		void updatePolicy(DirectoryEntry* entry);
		void insertionPolicy(DirectoryEntry* entry) { updatePolicy(entry);}
		int evictPolicy(int set);

	private : 	
		uint64_t m_cpt;	
		unsigned m_assoc;
		unsigned m_nb_set;
		std::vector<std::vector<DirectoryEntry*> >& m_directory_entries;
};



class Directory
{
	public:
		Directory();
		~Directory();
	
		void addTrackerToEntry(uint64_t addr , int node);
		void setTrackerToEntry(uint64_t addr , int node);
		void resetTrackersToEntry(uint64_t addr);
		void removeTracker(uint64_t addr, int node);
		void removeEntry(uint64_t addr);
		void updateEntry(uint64_t addr);

		std::set<int> getTrackers(uint64_t addr);
		void setCoherenceState(uint64_t addr, DirectoryState dir_state);

		DirectoryEntry* getEntry(uint64_t addr);
		DirectoryEntry* addEntry(uint64_t addr, bool isInst);
		
		uint64_t indexFunction(uint64_t addr);
		bool lookup(uint64_t addr);

		void printConfig();
		void printStats();
		
	
	private:
		std::vector< std::vector<DirectoryEntry*> > m_table;
		DirectoryReplacementPolicy* m_policy;
	
		int m_nb_set;
		int m_assoc;
		
		int m_end_index;
		int m_start_index;
};




#endif
