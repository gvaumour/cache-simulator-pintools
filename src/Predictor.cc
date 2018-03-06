#include <math.h>
#include "Predictor.hh"
#include "ReplacementPolicy.hh"
#include "Cache.hh"

using namespace std;


Predictor::Predictor(int nbAssoc , int nbSet, int nbNVMways, DataArray& SRAMtable , DataArray& NVMtable, HybridCache* cache) :\
	m_tableSRAM(SRAMtable), m_tableNVM(NVMtable)
{
	
	m_nb_set = nbSet;
	m_assoc = nbAssoc;
	m_nbNVMways = nbNVMways;
	m_nbSRAMways = nbAssoc - nbNVMways;
	
	m_replacementPolicyNVM_ptr = new LRUPolicy(m_nbNVMways , m_nb_set , NVMtable);
	m_replacementPolicySRAM_ptr = new LRUPolicy(m_nbSRAMways , m_nb_set, SRAMtable);	
	
	m_cache = cache;

	stats_NVM_errors = vector<int>(1, 0);
	stats_SRAM_errors = vector<int>(1, 0);
	stats_BP_errors = vector<int>(1,0);
	stats_MigrationErrors = vector<int>(1,0);
	
	m_trackError = false;
	stats_COREerrors = 0;
	stats_WBerrors = 0;

	/********************************************/ 
	
	/* Allocation of array of tracking conflict/capacity miss,
	 simulation of a FU cache of the same size */ 
	
	int size_SRAM_FU =  m_nbSRAMways*nbSet;
	int size_NVM_FU =  m_nbNVMways*nbSet;
	
	m_NVM_FU.resize(size_NVM_FU);
	for(int i = 0 ; i < size_NVM_FU; i++)
	{
		m_NVM_FU[i] = new MissingTagEntry();
	}
	m_SRAM_FU.resize(size_SRAM_FU);
	for(int i = 0 ; i < size_SRAM_FU; i++)
	{
		m_SRAM_FU[i] = new MissingTagEntry();
	}
	 
	/********************************************/ 
	m_trackError = false;
	if(m_nbNVMways != 0)
		m_trackError = true;

	if(m_trackError){
	

		/* Allocation of the BP missing tags array*/
		BP_missing_tags.resize(m_nb_set);
		for(int i = 0 ; i < m_nb_set; i++)
		{
			BP_missing_tags[i].resize(m_assoc);
			for(unsigned j = 0 ; j < BP_missing_tags[i].size(); j++)
			{
				BP_missing_tags[i][j] = new MissingTagEntry();
			}
		}

		/* Allocation of the SRAM missing tags array*/
		m_missing_tags.clear();
		m_missing_tags.resize(m_nb_set);
		//int assoc_MT = m_nbNVMways - m_nbSRAMways;
		m_assoc_MT = simu_parameters.sizeMTtags;
		for(int i = 0 ; i < m_nb_set; i++)
		{
			m_missing_tags[i].resize(m_assoc_MT);
			for(int j = 0 ; j < m_assoc_MT; j++)
			{
				m_missing_tags[i][j] = new MissingTagEntry();
			}
		}
	
		m_missingTags_SRAMtracking = true;
		if(simu_parameters.sram_assoc >  simu_parameters.nvm_assoc)
				m_missingTags_SRAMtracking = false;
	}
	
}

Predictor::~Predictor()
{
	if(m_trackError){
		for(unsigned i = 0 ; i < m_missing_tags.size() ; i++)
		{
			for(unsigned j = 0 ; j < m_missing_tags[i].size() ; j++)
			{
				delete m_missing_tags[i][j];
			}
		}
		for(unsigned i = 0 ; i < BP_missing_tags.size() ; i++)
		{
			for(unsigned j = 0 ; j < BP_missing_tags[i].size() ; j++)
			{
				delete BP_missing_tags[i][j];
			}
		}
	}
	delete m_replacementPolicyNVM_ptr;
	delete m_replacementPolicySRAM_ptr;
}

void
Predictor::startWarmup()
{
	m_isWarmup = true;
}

void
Predictor::stopWarmup()
{
	m_isWarmup = false;
}


void  
Predictor::evictRecording(int set, int assoc_victim , bool inNVM)
{
	if(!m_trackError)
		return;

	CacheEntry* current=NULL;
	if(m_missingTags_SRAMtracking)
	{
		if(inNVM)
			return;
		
		current = m_tableSRAM[set][assoc_victim];
	}
	else 
	{	//Missing Tag array track the NVM last tags 
	
		if(!inNVM)
			return;
		
		current = m_tableNVM[set][assoc_victim];	
	}
		
	
	//Choose the oldest victim for the missing tags to replace from the current one 	
	uint64_t cpt_longestime = m_missing_tags[set][0]->last_time_touched;
	uint64_t index_victim = 0;
	
	for(unsigned i = 0 ; i < m_missing_tags[set].size(); i++)
	{
		if(!m_missing_tags[set][i]->isValid)
		{
			index_victim = i;
			break;
		}	
		
		if(cpt_longestime > m_missing_tags[set][i]->last_time_touched){
			cpt_longestime = m_missing_tags[set][i]->last_time_touched; 
			index_victim = i;
		}	
	}

	m_missing_tags[set][index_victim]->addr = current->address;
	m_missing_tags[set][index_victim]->last_time_touched = cpt_time;
	m_missing_tags[set][index_victim]->isValid = current->isValid;


}


void
Predictor::insertionPolicy(uint64_t set, uint64_t index, bool inNVM, Access element)
{

	uint64_t block_addr = bitRemove(element.m_address , 0 , log2(BLOCK_SIZE));
	updateFUcaches(block_addr , inNVM);

	//Update the missing tag as a new cache line is brough into the cache 
	//Has to remove the MT entry if it exists there 
	//DPRINTF("Predictor::insertRecord Begin ");	
	if(!inNVM && m_trackError)
	{
	
		CacheEntry* current = m_tableSRAM[set][index];
		for(unsigned i = 0 ; i < m_missing_tags[set].size(); i++)
		{
			if(m_missing_tags[set][i]->addr == current->address)
			{
				m_missing_tags[set][i]->isValid = false;
			}
		}
	}	
	//DPRINTF("Predictor::insertRecord Begin ");

}

bool
Predictor::recordAllocationDecision(uint64_t set, Access element, allocDecision des)
{	
	if(!m_trackError)
		return false;
	
	bool find = false , isBPerror = false;
	//DPRINTF("Predictor::recordAllocationDecision Set %d, element.m_address = 0x%lx, des = %s \n ", set, element.m_address, allocDecision_str[des]);

	//Look if the element already exists , if yes update it, if no insert it
	for(unsigned i = 0 ; i < BP_missing_tags[set].size() ; i++)
	{	
		if(BP_missing_tags[set][i]->addr == element.m_address)
		{
			find = true;
			BP_missing_tags[set][i]->last_time_touched = cpt_time;
			BP_missing_tags[set][i]->isBypassed = (des == BYPASS_CACHE);

			
			//Bypass an access that would be a hit , BP ERROR ! 
//			if(des == BYPASS_CACHE && BP_missing_tags[set][i]->isBypassed){
			if(des == BYPASS_CACHE){
			
				//DPRINTF("Predictor::recordAllocationDecision BP_error detected\n");

				isBPerror = false;
				
				if(!m_isWarmup)		
					stats_BP_errors[stats_BP_errors.size()-1]++;
			}
	
			break;
		}
	}
	if(!find)
	{
		//DPRINTF("Predictor::recordAllocationDecision Didn't find the entry in the BP Missing Tags\n");
		int index_oldest =0;
		uint64_t oldest = BP_missing_tags[set][0]->last_time_touched;
		for(unsigned i = 0 ; i < BP_missing_tags[set].size() ; i++)
		{
			if(!BP_missing_tags[set][i]->isValid)		
			{
				index_oldest= i;
				break;
			}
		
			if(BP_missing_tags[set][i]->last_time_touched < oldest)
			{
				index_oldest = i;
				oldest = BP_missing_tags[set][i]->last_time_touched;
			}
			
		}
		
		if(BP_missing_tags[set][index_oldest]->isValid)
		{
			//DPRINTF("Predictor::Eviction of entry 0x%lx\n",BP_missing_tags[set][index_oldest]->addr);
		}
		//DPRINTF("Predictor::Insertion of the new entry at assoc %d\n", index_oldest);
		
		BP_missing_tags[set][index_oldest]->last_time_touched = cpt_time;
		BP_missing_tags[set][index_oldest]->addr = element.m_address;
		BP_missing_tags[set][index_oldest]->isValid = true;		
		BP_missing_tags[set][index_oldest]->isBypassed = (des == BYPASS_CACHE);
	}
	return isBPerror;
}


void 
Predictor::updateFUcaches(uint64_t block_addr, bool inNVM)
{

	DPRINTF(DebugFUcache, "Predictor::updateFUcaches update block %#lx\n", block_addr);
	
	if(m_accessedBlocks.count(block_addr) == 0)
	{
		m_accessedBlocks.insert(pair<uint64_t,uint64_t>(block_addr, cpt_time));
		DPRINTF(DebugFUcache, "First Touch of the block\n");
	}

	vector< vector<MissingTagEntry*> > FUcaches;	
	FUcaches.push_back(m_SRAM_FU);
	FUcaches.push_back(m_NVM_FU);
	
	bool find = false;
		
	for(unsigned i = 0 ; i < FUcaches[inNVM].size() && !find ; i++)
	{
		if(FUcaches[inNVM][i]->addr == block_addr)
		{
			DPRINTF(DebugFUcache, "Find at index %d , updatation !!! \n" , i);				
			FUcaches[inNVM][i]->last_time_touched = cpt_time;
			find = true;
		}
	}
	if(!find) // Data not found, insert the access in the FU cache 
	{
		for(unsigned i = 0; i < FUcaches[inNVM].size() && !find; i++)
		{
			if(!FUcaches[inNVM][i]->isValid)
			{
				FUcaches[inNVM][i]->addr = block_addr;
				FUcaches[inNVM][i]->isValid = true;
				FUcaches[inNVM][i]->last_time_touched = cpt_time;	
				DPRINTF(DebugFUcache, "Added in index %d \n" , i);				
				find = true;		
			}
		}
		if(!find){	
			DPRINTF(DebugFUcache, "HERE ? \n");				
			uint64_t min_time = FUcaches[inNVM][0]->last_time_touched, min_index = 0;
			for(unsigned i = 0; i < FUcaches[inNVM].size() ; i++)
			{
				if(FUcaches[inNVM][i]->last_time_touched < min_time)
				{
					min_time = FUcaches[inNVM][i]->last_time_touched;
					min_index = i;
				}
			}
			FUcaches[inNVM][min_index]->addr = block_addr;
			FUcaches[inNVM][min_index]->isValid = true;
			FUcaches[inNVM][min_index]->last_time_touched = cpt_time;					
		}
	}
}


bool
Predictor::reportMiss(uint64_t block_addr , int id_set)
{

	/* Miss classification between cold/conflict/capacity */
	stats_total_miss++;
	DPRINTF(DebugFUcache , "Predictor::reportMiss block %#lx \n" , block_addr);
	if(m_accessedBlocks.count(block_addr) == 0)
	{
		stats_cold_miss++;
		DPRINTF(DebugFUcache , "Predictor::reportMiss block not been accessed \n" );
	}
	else
	{
		bool find = false;
		for(unsigned i = 0; i < m_NVM_FU.size() && !find; i++)
		{
			if(m_NVM_FU[i]->addr == block_addr)
			{
				stats_nvm_conflict_miss++;
				find = true;
				break;
			}
		}
		for(unsigned i = 0; i < m_SRAM_FU.size() && !find; i++)
		{
			if(m_SRAM_FU[i]->addr == block_addr)
			{
				stats_sram_conflict_miss++;
				find = true;
				break;
			}
		}
		if(!find)
			stats_capacity_miss++;
		
	}
	/************************************************************/ 

	/* Check if the block is in the missing tag array */ 
	if(m_trackError && !m_isWarmup){
		//An error in the SRAM part is a block that miss in the SRAM array 
		//but not in the MT array
		for(unsigned i = 0 ; i < m_missing_tags[id_set].size() ; i++)
		{
			if(m_missing_tags[id_set][i]->addr == block_addr && m_missing_tags[id_set][i]->isValid)
			{
				////DPRINTF("BasePredictor::checkMissingTags Found SRAM error as %#lx is present in MT tag %i \n", block_addr ,i);  
				stats_SRAM_errors[stats_SRAM_errors.size()-1]++;
				return true;
			}
		}	
	}
	return false;
}


void
Predictor::openNewTimeFrame()
{
	if(m_isWarmup)
		return;
		
	//DPRINTF("BasePredictor::openNewTimeFrame New Time Frame Start\n");  
	stats_NVM_errors.push_back(0);
	stats_SRAM_errors.push_back(0);
	stats_BP_errors.push_back(0);
	stats_MigrationErrors.push_back(0);
	stats_nbLLCaccessPerFrame = 0;
}

void
Predictor::migrationRecording()
{

	if(!m_isWarmup)
	{
		//stats_NVM_errors[stats_NVM_errors.size()-1]++;
		stats_MigrationErrors[stats_MigrationErrors.size()-1]++;	
	}
}


void
Predictor::updatePolicy(uint64_t set, uint64_t index, bool inNVM, Access element , bool isWBrequest = false)
{	

	uint64_t block_addr = bitRemove(element.m_address , 0 , log2(BLOCK_SIZE));
	updateFUcaches(block_addr , inNVM);

	if(m_isWarmup)
		return;
	
	stats_nbLLCaccessPerFrame++;
	//An error in the NVM side is when handling a write 
	if(inNVM && element.isWrite()){
		stats_NVM_errors[stats_NVM_errors.size()-1]++;

		if(isWBrequest)
			stats_WBerrors++;
		else
			stats_COREerrors++;
	}
}


void 
Predictor::printConfig(std::ostream& out, std::string entete)
{
//	out << entete << ":Predictor" << endl;
	if(m_trackError)
		out << entete << ":Predictor:SizeMTarray:\t" << m_assoc_MT << endl;	
//	else
//		out << entete << ":Predictor:TrackingEnabled\tFALSE" << endl;
}


				 
void 
Predictor::printStats(std::ostream& out, string entete)
{

	uint64_t totalNVMerrors = 0, totalSRAMerrors= 0, totalBPerrors = 0, totalMigration = 0;	
	ofstream output_file;
	output_file.open(PREDICTOR_OUTPUT_FILE);
	for(unsigned i = 0 ; i <  stats_NVM_errors.size(); i++)
	{	
		output_file << stats_NVM_errors[i] << "\t" << stats_SRAM_errors[i] << "\t" << stats_BP_errors[i] << "\t"<< stats_BP_errors[i] << endl;	
		totalNVMerrors += stats_NVM_errors[i];
		totalSRAMerrors += stats_SRAM_errors[i];
		totalBPerrors += stats_BP_errors[i];
		totalMigration += stats_MigrationErrors[i];
	}
	output_file.close();
	out << entete << ":Predictor:TotalMiss:\t" << stats_total_miss << endl;
	out << entete << ":Predictor:NVMConflictMiss:\t" << stats_nvm_conflict_miss << endl;
	out << entete << ":Predictor:SRAMConflictMiss:\t" << stats_sram_conflict_miss << endl;
	out << entete << ":Predictor:ColdMiss:\t" << stats_cold_miss << endl;
	out << entete << ":Predictor:CapacityMiss:\t" << stats_capacity_miss << endl;


	out << entete << ":Predictor:PredictorErrors:" <<endl;
	out << entete << ":Predictor:TotalNVMError\t" << totalNVMerrors << endl;
	out << entete << ":Predictor:NVMError:FromWB\t"  << stats_WBerrors << endl;
	out << entete << ":Predictor:NVMError:FromCore\t" <<  stats_COREerrors << endl;
	out << entete << ":Predictor:TotalMigration\t" <<  totalMigration << endl;
	out << entete << ":Predictor:SRAMError\t" << totalSRAMerrors << endl;
	out << entete << ":Predictor:BPError\t" << totalBPerrors << endl;
}


LRUPredictor::LRUPredictor(int nbAssoc , int nbSet, int nbNVMways, DataArray& SRAMtable, DataArray& NVMtable, HybridCache* cache)\
 : Predictor(nbAssoc, nbSet, nbNVMways, SRAMtable, NVMtable, cache)
{
	/* With LRU policy, the cache is not hybrid, only NVM or only SRAM */ 
//	assert(m_nbNVMways == 0 || m_nbSRAMways == 0);
	
	m_cpt = 1;
}


allocDecision
LRUPredictor::allocateInNVM(uint64_t set, Access element)
{
	// Always allocate on the same data array 
	
	if(m_nbNVMways == 0)
		return ALLOCATE_IN_SRAM;
	else 
		return ALLOCATE_IN_NVM;
}

void
LRUPredictor::updatePolicy(uint64_t set, uint64_t index, bool inNVM, Access element , bool isWBrequest = false)
{
	if(inNVM)
		m_replacementPolicyNVM_ptr->updatePolicy(set, index , 0);
	else
		m_replacementPolicySRAM_ptr->updatePolicy(set, index , 0);

	Predictor::updatePolicy(set , index , inNVM , element);
	m_cpt++;
}

void
LRUPredictor::insertionPolicy(uint64_t set, uint64_t index, bool inNVM, Access element)
{	
	if(inNVM)
		m_replacementPolicyNVM_ptr->insertionPolicy(set, index , 0);
	else
		m_replacementPolicySRAM_ptr->insertionPolicy(set, index , 0);
	
	Predictor::insertionPolicy(set, index , inNVM , element);
	//DPRINTF("END OF INSERTIONPolicy");
	m_cpt++;
}

int
LRUPredictor::evictPolicy(int set, bool inNVM)
{
	int assoc_victim;
	
	if(inNVM)
		assoc_victim = m_replacementPolicyNVM_ptr->evictPolicy(set);
	else
		assoc_victim =  m_replacementPolicySRAM_ptr->evictPolicy(set);
	
	evictRecording(set , assoc_victim , inNVM);
	return assoc_victim;
}


allocDecision
PreemptivePredictor::allocateInNVM(uint64_t set, Access element)
{

	if( m_nbSRAMways == 0 )
		return ALLOCATE_IN_NVM;
	else if(m_nbNVMways == 0)
		return ALLOCATE_IN_SRAM;


	if(element.isWrite())
		return ALLOCATE_IN_SRAM;
	else
		return ALLOCATE_IN_NVM;
}


/********************************************/ 
