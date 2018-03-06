#include <vector>
#include <ostream>

#include "DynamicSaturation.hh"

using namespace std;

/*
DynamicSaturation::DynamicSaturation() : Predictor(){
	m_cpt = 1;
	stats_nbMigrationsFromNVM = vector<int>(1,0);
	stats_nbMigrationsFromSRAM = vector<int>(1,0);
	stats_nbCoreError= vector<int>(1,0);
	stats_nbWBError = vector<int>(1,0);
	
	m_thresholdNVM = 3;
	m_thresholdSRAM = 3;
}*/

DynamicSaturation::DynamicSaturation(int nbAssoc , int nbSet, int nbNVMways, DataArray& SRAMtable, DataArray& NVMtable, HybridCache* cache) : \
	Predictor(nbAssoc, nbSet, nbNVMways, SRAMtable, NVMtable, cache) {
	m_cpt = 1;
	stats_nbMigrationsFromNVM = vector<int>(1,0);
	stats_nbMigrationsFromSRAM = vector<int>(1,0);
	stats_nbCoreError= vector<int>(1,0);
	stats_nbWBError = vector<int>(1,0);
	
	m_thresholdNVM = 3;
	m_thresholdSRAM = 3;
}
		
DynamicSaturation::~DynamicSaturation() { }

allocDecision
DynamicSaturation::allocateInNVM(uint64_t set, Access element)
{
	if(element.isWrite())
		return ALLOCATE_IN_SRAM;
	else
		return ALLOCATE_IN_NVM;
}

void DynamicSaturation::updatePolicy(uint64_t set, uint64_t index, bool inNVM, Access element , bool isWBrequest = false)
{
	//DPRINTF("DynamicSaturation::updatePolicy\n");
	
	if(inNVM){
		//Update NVM entry 
		CacheEntry* current = m_tableNVM[set][index];
		current->policyInfo = m_cpt;
		
		if(element.isWrite()){
			current->saturation_counter++;
			
			if(element.m_type == DIRTY_WRITEBACK)
				stats_nbWBError[stats_nbWBError.size()-1]++;
			else
				stats_nbCoreError[stats_nbCoreError.size()-1]++;
				
			
			if(current->saturation_counter == m_thresholdNVM)
			{
				//Trigger Migration
				//Select LRU candidate from SRAM cache
				//DPRINTF("DynamicSaturation:: Migration Triggered from NVM\n");

				int id_assoc = evictPolicy(set, false);
				
				CacheEntry* replaced_entry = m_tableSRAM[set][id_assoc];
				current->saturation_counter = 0;
				replaced_entry->saturation_counter = 0;

				m_cache->triggerMigration(set, id_assoc , index, true);
				stats_nbMigrationsFromNVM[stats_nbMigrationsFromNVM.size()-1]++;
				
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
			if(current->saturation_counter == m_thresholdSRAM)
			{
				//Trigger Migration
				//DPRINTF("DynamicSaturation:: Migration Triggered from SRAM\n");
				int id_assoc = evictPolicy(set, true);//Select LRU candidate from NVM cache

				CacheEntry* replaced_entry = m_tableNVM[set][id_assoc];
				assert(replaced_entry != NULL);
							
				current->saturation_counter = 0;
				replaced_entry->saturation_counter = 0;

				m_cache->triggerMigration(set, index, id_assoc, false);
				
				stats_nbMigrationsFromSRAM[stats_nbMigrationsFromSRAM.size()-1]++;


			}
		}
	}	


	Predictor::updatePolicy(set, index, inNVM, element , isWBrequest);
	m_cpt++;
}


void 
DynamicSaturation::openNewTimeFrame()
{

	/* The beginning of a new frame, Updating the threshold */ 
	int totalNVMerrors = stats_NVM_errors[stats_NVM_errors.size()-1];
	int totalSRAMerrors = stats_SRAM_errors[stats_SRAM_errors.size()-1];
	
	stats_threshold_NVM.push_back(m_thresholdNVM);
	stats_threshold_SRAM.push_back(m_thresholdSRAM);
	
	stats_nbMigrationsFromSRAM.push_back(0);
	stats_nbMigrationsFromNVM.push_back(0);
	
	stats_nbWBError.push_back(0);
	stats_nbCoreError.push_back(0);

	if(stats_nbLLCaccessPerFrame != 0)
	{
		
		if(totalNVMerrors  > 0.01*stats_nbLLCaccessPerFrame){
			m_thresholdNVM--;
			if(m_thresholdNVM < 1)
				m_thresholdNVM = 1;
		}
		else 
		{
			m_thresholdNVM++;
			if(m_thresholdNVM > 20)
				m_thresholdNVM = 20;
		}
	
		if(totalSRAMerrors  > 0.005*stats_nbLLCaccessPerFrame){
			m_thresholdSRAM--;
			if(m_thresholdSRAM < 1)
				m_thresholdSRAM = 1;
		}
		else 
		{
			m_thresholdSRAM++;
			if(m_thresholdSRAM > 20)
				m_thresholdSRAM = 20;
		}
	}
	
	Predictor::openNewTimeFrame();
}

void
DynamicSaturation::insertionPolicy(uint64_t set, uint64_t index, bool inNVM, Access element)
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
DynamicSaturation::printStats(std::ostream& out, string entete)
{	
	/*
	double migrationNVM = (double) stats_nbMigrationsFromNVM[0];
	double migrationSRAM = (double) stats_nbMigrationsFromNVM[1];
	double total = migrationNVM+migrationSRAM;

	if(total > 0){
		out << "Dynamic Saturation Predictor Stats:" << endl;
		out << "NB Migration :" << total << endl;
		out << "\t From NVM : " << migrationNVM*100/total << "%" << endl;
		out << "\t From SRAM : " << migrationSRAM*100/total << "%" << endl;	
	}*/
	
	ofstream output_file(PREDICTOR_DYNAMIC_OUTPUT_FILE);
//	cout << "stats_NVM_errors.size() = " << stats_NVM_errors.size() << endl;


//	cout << "stats_threshold_NVM.size() = " << stats_threshold_NVM.size() << endl;
//	cout << "stats_nbMigrationsFromNVM.size() = " << stats_nbMigrationsFromNVM.size() << endl;
	for(unsigned i = 0; i < stats_threshold_NVM.size() ; i++){
		output_file << stats_threshold_NVM[i] << "\t" << stats_threshold_SRAM[i] << "\t" \
			   << stats_nbMigrationsFromNVM[i] << "\t" << stats_nbMigrationsFromSRAM[i] << "\t" \
			   << stats_nbCoreError[i] << "\t" << stats_nbWBError[i] << "\t" \
			    << endl;	
	}
	
	output_file.close();
	
	Predictor::printStats(out, entete);
}

int DynamicSaturation::evictPolicy(int set, bool inNVM)
{	
	int assoc_victim;
	if(inNVM)
		assoc_victim = m_replacementPolicyNVM_ptr->evictPolicy(set);
	else
		assoc_victim = m_replacementPolicySRAM_ptr->evictPolicy(set);

	evictRecording(set, assoc_victim, inNVM);
	return assoc_victim;
}

