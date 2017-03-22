#include <vector>
#include <ostream>

#include "SaturationPredictor.hh"

using namespace std;

/** SaturationCounter Implementation ***********/ 

SaturationCounter::SaturationCounter() : Predictor(){
	m_cpt = 1;
	stats_nbMigrationsFromNVM = vector<int>(2,0);
}

SaturationCounter::SaturationCounter(int nbAssoc , int nbSet, int nbNVMways, DataArray SRAMtable, DataArray NVMtable, HybridCache* cache) : \
	Predictor(nbAssoc, nbSet, nbNVMways, SRAMtable, NVMtable, cache) {
	m_cpt = 1;
	stats_nbMigrationsFromNVM = vector<int>(2,0);
}
		
SaturationCounter::~SaturationCounter() { }

bool SaturationCounter::allocateInNVM(uint64_t set, Access element)
{
	return !element.isWrite();
}

void SaturationCounter::updatePolicy(uint64_t set, uint64_t index, bool inNVM, Access element)
{
	DPRINTF("SaturationCounter::updatePolicy\n");
	
	if(inNVM){
		//Update NVM entry 
		CacheEntry* current = m_tableNVM[set][index];
		current->policyInfo = m_cpt;
		
		if(element.isWrite()){
			current->saturation_counter--;
			if(current->saturation_counter == 1)
			{
				//Trigger Migration
				//Select LRU candidate from SRAM cache
				DPRINTF("SaturationCounter:: Migration Triggered from NVM\n");

				int id_assoc = evictPolicy(set, false);
				DPRINTF("id_assoc = %d , set %d\n" , id_assoc, set);
				
				CacheEntry* replaced_entry = m_tableSRAM[set][id_assoc];
				current->saturation_counter = 3;
				replaced_entry->saturation_counter = 3;

				m_cache->triggerMigration(set, id_assoc , index);
				stats_nbMigrationsFromNVM[inNVM]++;
				
			}
		}
		else{
			current->saturation_counter++;
			if(current->saturation_counter > 3)
				current->saturation_counter = 3;					
		}
			
	}
	else{// Update SRAM entry 
		CacheEntry* current = m_tableSRAM[set][index];
		current->policyInfo = m_cpt;
		
		if(element.isWrite())
		{
			current->saturation_counter++;
			if(current->saturation_counter > 3)
				current->saturation_counter = 3;
		}
		else{
			current->saturation_counter--;
			if(current->saturation_counter == 1)
			{
				//Trigger Migration
				DPRINTF("SaturationCounter:: Migration Triggered from SRAM\n");
				int id_assoc = evictPolicy(set, true);//Select LRU candidate from NVM cache

				CacheEntry* replaced_entry = m_tableNVM[set][id_assoc];
				assert(replaced_entry != NULL);
							
				current->saturation_counter = 3;
				replaced_entry->saturation_counter = 3;

				m_cache->triggerMigration(set, index, id_assoc);
				
				stats_nbMigrationsFromNVM[inNVM]++;

			}
		}
	}	
	m_cpt++;
}

void SaturationCounter::insertionPolicy(uint64_t set, uint64_t index, bool inNVM, Access element)
{

	if(inNVM){
		m_tableNVM[set][index]->policyInfo = m_cpt;
		m_tableNVM[set][index]->saturation_counter = 3;		
	}
	else{
		m_tableSRAM[set][index]->policyInfo = m_cpt;
		m_tableSRAM[set][index]->saturation_counter = 3;	
	
	}
	
	m_cpt++;
}

void SaturationCounter::printStats(std::ostream& out)
{	
	
	double migrationNVM = (double) stats_nbMigrationsFromNVM[0];
	double migrationSRAM = (double) stats_nbMigrationsFromNVM[1];
	double total = migrationNVM+migrationSRAM;

	if(total > 0){
		out << "Predictor Stats:" << endl;
		out << "NB Migration :" << total << endl;
		out << "\t From NVM : " << migrationNVM*100/total << "%" << endl;
		out << "\t From SRAM : " << migrationSRAM*100/total << "%" << endl;	
	}	
}

int SaturationCounter::evictPolicy(int set, bool inNVM)
{	
	if(inNVM)
		return m_replacementPolicyNVM_ptr->evictPolicy(set);
	else
		return m_replacementPolicySRAM_ptr->evictPolicy(set);
}

