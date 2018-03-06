#include <vector>
#include <ostream>

#include "SaturationPredictor.hh"

using namespace std;

/** SaturationCounter Implementation ***********/ 
/*
SaturationCounter::SaturationCounter() : Predictor(){
	m_cpt = 1;
	stats_nbMigrationsFromNVM = vector<int>(2,0);
}*/

SaturationCounter::SaturationCounter(int nbAssoc , int nbSet, int nbNVMways, DataArray& SRAMtable, DataArray& NVMtable, HybridCache* cache) : \
	Predictor(nbAssoc, nbSet, nbNVMways, SRAMtable, NVMtable, cache) {
	m_cpt = 1;
	stats_nbMigrationsFromNVM = vector<int>(2,0);
}
		
SaturationCounter::~SaturationCounter() { }

allocDecision
SaturationCounter::allocateInNVM(uint64_t set, Access element)
{
	if(element.isWrite())
		return ALLOCATE_IN_SRAM;
	else
		return ALLOCATE_IN_NVM;
}

void SaturationCounter::updatePolicy(uint64_t set, uint64_t index, bool inNVM, Access element , bool isWBrequest = false)
{
	//DPRINTF("SaturationCounter::updatePolicy\n");
	
	if(inNVM){
		//Update NVM entry 
		CacheEntry* current = m_tableNVM[set][index];
		current->policyInfo = m_cpt;
		
		if(element.isWrite()){
			current->saturation_counter++;
			if(current->saturation_counter == simu_parameters.saturation_threshold)
			{
				//Trigger Migration
				//Select LRU candidate from SRAM cache
				//DPRINTF("SaturationCounter:: Migration Triggered from NVM\n");

				int id_assoc = evictPolicy(set, false);
				
				CacheEntry* replaced_entry = m_tableSRAM[set][id_assoc];
			
				current->saturation_counter = 0;
				replaced_entry->saturation_counter = 0;
			
				/** Migration incurs one read and one extra write */ 
				replaced_entry->nbWrite++;
				current->nbRead++;

				Predictor::migrationRecording();
				m_cache->triggerMigration(set, id_assoc , index , true);
				if(!m_isWarmup)
					stats_nbMigrationsFromNVM[inNVM]++;

				
			}
		}
		else{
			current->saturation_counter--;
			if(current->saturation_counter < 0)
				current->saturation_counter = 0;					
		}
			
	}
	else{// Update SRAM entry 
		CacheEntry* current = m_tableSRAM[set][index];
		current->policyInfo = m_cpt;
		
		if(element.isWrite())
		{
			current->saturation_counter--;
			if(current->saturation_counter < 0)
				current->saturation_counter = 0;
		}
		else{
			current->saturation_counter++;
			if(current->saturation_counter == simu_parameters.saturation_threshold)
			{
				//Trigger Migration
				//DPRINTF("SaturationCounter:: Migration Triggered from SRAM\n");
				int id_assoc = evictPolicy(set, true);//Select LRU candidate from NVM cache

				CacheEntry* replaced_entry = m_tableNVM[set][id_assoc];
				assert(replaced_entry != NULL);
							
				current->saturation_counter = 0;
				replaced_entry->saturation_counter = 0;
		
				Predictor::migrationRecording();
		
				/** Migration incurs one read and one extra write */ 
				replaced_entry->nbWrite++;
				current->nbRead++;

				m_cache->triggerMigration(set, index, id_assoc, false);
			
				if(!m_isWarmup)
					stats_nbMigrationsFromNVM[inNVM]++;

			}
		}
	}
		
	Predictor::updatePolicy(set, index, inNVM, element , isWBrequest);

	m_cpt++;
}

void SaturationCounter::insertionPolicy(uint64_t set, uint64_t index, bool inNVM, Access element)
{

	if(inNVM){
		m_tableNVM[set][index]->policyInfo = m_cpt;
		m_tableNVM[set][index]->saturation_counter = 0;		
	}
	else{
		m_tableSRAM[set][index]->policyInfo = m_cpt;
		m_tableSRAM[set][index]->saturation_counter = 0;	
	
	}
	
	Predictor::insertionPolicy(set , index , inNVM , element);
	m_cpt++;
}

void
SaturationCounter::printConfig(std::ostream& out, string entete ) {
	out<< entete << ":SaturationThreshold\t" << simu_parameters.saturation_threshold << std::endl;
	Predictor::printConfig(out, entete);
};


void SaturationCounter::printStats(std::ostream& out, string entete)
{	
	
	double migrationNVM = (double) stats_nbMigrationsFromNVM[0];
	double migrationSRAM = (double) stats_nbMigrationsFromNVM[1];
	double total = migrationNVM+migrationSRAM;

	if(total > 0){
		out << entete << ":PredictorStats:" << endl;
		out << entete << ":NBMigration\t" << total << endl;
		out << entete << ":MigrationFromNVM\t" << migrationNVM*100/total << "%" << endl;
		out << entete << ":MigrationFromSRAM\t" << migrationSRAM*100/total << "%" << endl;	
	}
	Predictor::printStats(out, entete);
}

int SaturationCounter::evictPolicy(int set, bool inNVM)
{	
	int assoc_victim;
	if(inNVM)
		assoc_victim = m_replacementPolicyNVM_ptr->evictPolicy(set);
	else
		assoc_victim = m_replacementPolicySRAM_ptr->evictPolicy(set);

	evictRecording(set, assoc_victim, inNVM);
	return assoc_victim;
}

