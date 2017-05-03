#include <vector>
#include <ostream>

#include "DynamicSaturation.hh"

using namespace std;


DynamicSaturation::DynamicSaturation() : Predictor(){
	m_cpt = 1;
	stats_nbMigrationsFromNVM = vector<int>(2,0);
	m_thresholdNVM = 3;
	m_thresholdSRAM = 3;
}

DynamicSaturation::DynamicSaturation(int nbAssoc , int nbSet, int nbNVMways, DataArray SRAMtable, DataArray NVMtable, HybridCache* cache) : \
	Predictor(nbAssoc, nbSet, nbNVMways, SRAMtable, NVMtable, cache) {
	m_cpt = 1;
	stats_nbMigrationsFromNVM = vector<int>(2,0);
	m_thresholdNVM = 3;
	m_thresholdSRAM = 3;
}
		
DynamicSaturation::~DynamicSaturation() { }

bool DynamicSaturation::allocateInNVM(uint64_t set, Access element)
{
	return !element.isWrite();
}

void DynamicSaturation::updatePolicy(uint64_t set, uint64_t index, bool inNVM, Access element , bool isWBrequest = false)
{
	DPRINTF("DynamicSaturation::updatePolicy\n");
	
	if(inNVM){
		//Update NVM entry 
		CacheEntry* current = m_tableNVM[set][index];
		current->policyInfo = m_cpt;
		
		if(element.isWrite()){
			current->saturation_counter++;
			if(current->saturation_counter == m_thresholdNVM)
			{
				//Trigger Migration
				//Select LRU candidate from SRAM cache
				DPRINTF("DynamicSaturation:: Migration Triggered from NVM\n");

				int id_assoc = evictPolicy(set, false);
				
				CacheEntry* replaced_entry = m_tableSRAM[set][id_assoc];
				current->saturation_counter = 0;
				replaced_entry->saturation_counter = 0;

				m_cache->triggerMigration(set, id_assoc , index);
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
			if(current->saturation_counter == m_thresholdSRAM)
			{
				//Trigger Migration
				DPRINTF("DynamicSaturation:: Migration Triggered from SRAM\n");
				int id_assoc = evictPolicy(set, true);//Select LRU candidate from NVM cache

				CacheEntry* replaced_entry = m_tableNVM[set][id_assoc];
				assert(replaced_entry != NULL);
							
				current->saturation_counter = 0;
				replaced_entry->saturation_counter = 0;

				m_cache->triggerMigration(set, index, id_assoc);
				
				stats_nbMigrationsFromNVM[inNVM]++;

			}
		}
	}	

	/* The beginning of a new frame, Updating the threshold */ 
	if(  (cpt_time - stats_beginTimeFrame) > PREDICTOR_TIME_FRAME ){

//		cout << "stats_NVM_errors.size()= " << stats_NVM_errors.size() << endl;
		
		vector<int> NVMerrors = stats_NVM_errors[stats_NVM_errors.size()-1];
		vector<int> SRAMerrors = stats_SRAM_errors[stats_SRAM_errors.size()-1];
		uint64_t totalNVMerrors = 0 , totalSRAMerrors = 0;
		
		for (auto n : NVMerrors)
		    totalNVMerrors += n;
		for (auto n : SRAMerrors)
		    totalSRAMerrors += n;
		
		stats_threshold_NVM.push_back(m_thresholdNVM);
		stats_threshold_SRAM.push_back(m_thresholdSRAM);
		
		if(totalNVMerrors  > 0.1*stats_nbLLCaccessPerFrame){
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
		
		if(totalSRAMerrors  > 0.05*stats_nbLLCaccessPerFrame){
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


	Predictor::updatePolicy(set, index, inNVM, element , isWBrequest);
	m_cpt++;
}

void DynamicSaturation::insertionPolicy(uint64_t set, uint64_t index, bool inNVM, Access element)
{

	if(inNVM){
		m_tableNVM[set][index]->policyInfo = m_cpt;
		m_tableNVM[set][index]->saturation_counter = 0;		
	}
	else{
		m_tableSRAM[set][index]->policyInfo = m_cpt;
		m_tableSRAM[set][index]->saturation_counter = 0;	
	
	}
	
	m_cpt++;
}

void DynamicSaturation::printStats(std::ostream& out)
{	
	
	double migrationNVM = (double) stats_nbMigrationsFromNVM[0];
	double migrationSRAM = (double) stats_nbMigrationsFromNVM[1];
	double total = migrationNVM+migrationSRAM;

	if(total > 0){
		out << "Dynamic Saturation Predictor Stats:" << endl;
		out << "NB Migration :" << total << endl;
		out << "\t From NVM : " << migrationNVM*100/total << "%" << endl;
		out << "\t From SRAM : " << migrationSRAM*100/total << "%" << endl;	
	}
	
	ofstream output_file("DynamicPredictor.out");
	for(auto p : stats_threshold_NVM)
		output_file << p << "\t";
	output_file << endl;
	for(auto p : stats_threshold_SRAM)
		output_file << p << "\t";
	output_file << endl;
	output_file.close();
	
	Predictor::printStats(out);
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

