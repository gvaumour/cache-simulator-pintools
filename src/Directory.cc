#include <iostream>
#include <assert.h>
#include <map>
#include <vector>
#include <set>
#include <math.h> //For log2 function

#include "Directory.hh"
#include "common.hh"

using namespace std;


void
DirectoryEntry::print()
{

	#ifdef TEST
	
		string type = isInst ? "INST" : "DATA";
		
		cout << "DIRECTORYENTRY::Addr=0x" << std::hex << block_addr;
		cout << std::dec << ", State=" << directory_state_str[coherence_state];
		cout << ", Type=" << type; 
		cout << ", Tracker=";
	
		for(auto p : nodeTrackers)		
			cout << p << ",";			
		cout << std::endl;
	#endif
}

Directory::Directory()
{
	m_nb_set = DIRECTORY_NB_SETS;
	m_assoc = DIRECTORY_ASSOC;

	//DPRINTF("DIRECTORY::Constructor , assoc=%d , m_nb_set=%d\n", m_assoc , m_nb_set);

	m_table.resize(m_nb_set);
	for(int i = 0  ; i < m_nb_set ; i++){
//		m_table[i].resize(m_assoc);
		m_table[i].clear();
		/*
		for(int j = 0 ; j < m_assoc ; j++){
			m_table[i][j] = new DirectoryEntry();
		}*/
	}
		
	m_policy = new DirectoryReplacementPolicy(m_assoc , m_nb_set , m_table);
	m_start_index = log2(BLOCK_SIZE)-1;
	m_end_index = log2(BLOCK_SIZE) + log2(m_nb_set);
}

Directory::~Directory()
{
	for(auto sets : m_table)
	{
		for(auto entry : sets)
			delete entry;
	}
	delete m_policy;
}


DirectoryEntry*
Directory::addEntry(uint64_t addr , bool isInst)
{
	if(getEntry(addr) != NULL)
		return getEntry(addr);	
	
	uint64_t id_set = indexFunction(addr);
//	int assoc_victim = m_policy->evictPolicy(id_set);
		
	DirectoryEntry* entry = new DirectoryEntry();
	
	/*if(entry->isValid)
	{
	
//		cout << "ID SET =" << id_set << endl;
		//DPRINTF("DIRECTORY::Eviction of dir entry, Block_addr[%#lx] \n", entry->block_addr);
		cout << "Assoc victim =" << assoc_victim << endl;
//		entry->print();	
//		for(int i  = 0 ; i < m_table[id_set].size(); i++)
//		{
//			cout << "\t Way " << i << endl;
//			m_table[id_set][i]->print();
//		}
	}*/
	
	entry->initEntry();
	entry->block_addr = addr;
	entry->isValid = true;
	entry->isInst = isInst;
	m_policy->insertionPolicy(entry);

	m_table[id_set].push_back(entry);

	return entry;
}

bool
Directory::lookup(uint64_t addr)
{
	
	uint64_t id_set = indexFunction(addr);
	for(unsigned i = 0 ; i < m_table[id_set].size() ; i++)
	{
		if(m_table[id_set][i]->block_addr == addr)
			return true;
	}	
	return false;
}

void 
Directory::setTrackerToEntry(uint64_t addr , int node)
{
	DirectoryEntry* entry = getEntry(addr);
	assert(entry != NULL);

	entry->nodeTrackers.clear();
	entry->nodeTrackers.insert(node);

//	m_policy->updatePolicy(entry);
}



std::set<int>
Directory::getTrackers(uint64_t addr)
{
	DirectoryEntry* entry = getEntry(addr);
	assert(entry != NULL);
	return entry->nodeTrackers;
}
		

void 
Directory::removeTracker(uint64_t addr, int node)
{
	DirectoryEntry* entry = getEntry(addr);
	assert(entry != NULL);

	//DPRINTF("DIRECTORY::removeTracker Addr %#lx, Node %d\n", addr, node );	

	set<int>::iterator it = entry->nodeTrackers.find(node);
	if(it != entry->nodeTrackers.end())
		entry->nodeTrackers.erase(it);

	//if the cl is no longer in a L1 cache
	if(entry->nodeTrackers.empty())
		entry->coherence_state = CLEAN_LLC;
}

void
Directory::setCoherenceState(uint64_t addr, DirectoryState dir_state)
{

	DirectoryEntry* entry = getEntry(addr);
	assert(entry != NULL);
	entry->coherence_state = dir_state;
	entry->print();
//	m_policy->updatePolicy(entry);
}


void
Directory::resetTrackersToEntry(uint64_t addr)
{
	//DPRINTF("DIRECTORY::resetTrackersToEntry Addr %#lx\n", addr );

	DirectoryEntry* entry = getEntry(addr);
	assert(entry != NULL);

	entry->resetTrackers();	
//	m_policy->updatePolicy(entry);
}


DirectoryEntry*
Directory::getEntry(uint64_t addr)
{
	int id_set = indexFunction(addr);
	for(auto entry : m_table[id_set])
	{
		if(entry->block_addr == addr && entry->isValid)
			return entry;
	}
	return NULL;
}

void
Directory::removeEntry(uint64_t addr)
{
	DirectoryEntry* entry = getEntry(addr);
	assert(entry != NULL);
	//DPRINTF("DIRECTORY::removeEntry Addr %#lx\n" , addr);
	int id_set = indexFunction(addr);
	for(unsigned i = 0 ; i < m_table[id_set].size(); i++)
	{
		if(addr == m_table[id_set][i]->block_addr && m_table[id_set][i]->isValid){
			m_table[id_set].erase(m_table[id_set].begin() + i);
			break;
		}
	}
}


void 
Directory::addTrackerToEntry(uint64_t addr, int node)
{
	//DPRINTF("DIRECTORY::addTrackerToEntry Addr %#lx, node %d\n" , addr, node );

	DirectoryEntry* entry = getEntry(addr);
	assert(entry != NULL);	
	entry->nodeTrackers.insert(node);
//	m_policy->updatePolicy(entry);
}

void
Directory::printConfig()
{
	cout << "Directory Configuration" << endl;
	cout << "\tAssociativity\t" << m_assoc << endl;
	cout << "\tNB Sets\t" << m_nb_set << endl;
}


void
Directory::printStats()
{
	cout << "Directory Stats" << endl;
	cout << "********************" << endl;
}


uint64_t 
Directory::indexFunction(uint64_t block_addr)
{
	uint64_t a =block_addr;
	a = bitRemove(a , m_end_index,64);	
	a = a >> (m_start_index+1);
	assert(a < (unsigned int)m_nb_set);
	
	return (int)a;
}

void
Directory::updateEntry(uint64_t addr)
{
	DirectoryEntry* entry = getEntry(addr);
	assert(entry != NULL);	
	m_policy->updatePolicy(entry);
	
}



/******** Implementation of the replacement policy of the Directory */ 

DirectoryReplacementPolicy::DirectoryReplacementPolicy(int nbAssoc , int nbSet, 
			 std::vector<std::vector<DirectoryEntry*> >& dir_entries) : \
			 m_assoc(nbAssoc) , m_nb_set(nbSet), m_directory_entries(dir_entries)
{
	m_cpt = 1;
}

void
DirectoryReplacementPolicy::updatePolicy(DirectoryEntry* entry)
{
	m_cpt++;
	entry->policyInfo = m_cpt;
}

int 
DirectoryReplacementPolicy::evictPolicy(int set)
{
	/*
	int smallest_time = m_directory_entries[set][0]->policyInfo , smallest_index = 0;

	for(unsigned i = 0 ; i < m_directory_entries[set].size() ; i++){
		if(!m_directory_entries[set][i]->isValid) 
			return i;
	}
	
	for(unsigned i = 0 ; i < m_directory_entries[set].size() ; i++){
		if(m_directory_entries[set][i]->policyInfo < smallest_time){
			smallest_time =  m_directory_entries[set][i]->policyInfo;
			smallest_index = i;
		}
	}*/
	return 0;
}


