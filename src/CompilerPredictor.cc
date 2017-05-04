#include <fstream>
#include <vector>
#include <map>
#include <ostream>

#include "CompilerPredictor.hh"

using namespace std;

CompilerPredictor::CompilerPredictor() : SaturationCounter(){
}

CompilerPredictor::CompilerPredictor(int nbAssoc , int nbSet, int nbNVMways, DataArray SRAMtable, DataArray NVMtable, HybridCache* cache) : \
	SaturationCounter(nbAssoc, nbSet, nbNVMways, SRAMtable, NVMtable, cache) {
}

CompilerPredictor::~CompilerPredictor()
{ }

bool
CompilerPredictor::allocateInNVM(uint64_t set, Access element)
{
	DPRINTF("CompilerPredictor::allocateInNVM\n");
	if(element.m_compilerHints == 1 && !element.isInstFetch())
	{
		DPRINTF("CompilerPredictor:: Not allocate in NVM du to compiler Decision\n");
		return false;	
	}
	else
		return SaturationCounter::allocateInNVM(set, element);
}

void
CompilerPredictor::updatePolicy(uint64_t set, uint64_t index, bool inNVM, Access element, bool isWBrequest = false)
{
	DPRINTF("CompilerPredictor::updatePolicy\n");
	
	//Trigger Migration
	//Select LRU candidate from SRAM cache


	//If the access is tagged as "hot", we need to migrate it
	if(element.m_compilerHints == 1 && inNVM)
	{
		DPRINTF("CompilerPredictor:: Migration Triggered from NVM because of hot spot\n");
		int id_assoc = evictPolicy(set, false);

		CacheEntry* current = m_tableNVM[set][index];
		CacheEntry* replaced_entry = m_tableSRAM[set][id_assoc];
	
		current->saturation_counter = 3;
		replaced_entry->saturation_counter = 3;

		m_cache->triggerMigration(set, id_assoc , index);

		stats_nbMigrationsFromNVM[inNVM]++;
	
	}
	else
		SaturationCounter::updatePolicy(set , index , inNVM , element , isWBrequest);

}

void CompilerPredictor::insertionPolicy(uint64_t set, uint64_t index, bool inNVM, Access element)
{
	SaturationCounter::insertionPolicy(set , index , inNVM , element);
}

void 
CompilerPredictor::printStats(std::ostream& out)
{	
	SaturationCounter::printStats(out);
}

int
CompilerPredictor::evictPolicy(int set, bool inNVM)
{	
	return SaturationCounter::evictPolicy(set, inNVM);
}

