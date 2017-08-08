#include <fstream>
#include <vector>
#include <map>
#include <ostream>

#include "testRAPPredictor.hh"

using namespace std;

static const char* str_RW_status[] = {"DEAD" , "WO", "RO" , "RW", "RW_NOTACC"};
static const char* str_RD_status[] = {"RD_SHORT" , "RD_MEDIUM", "RD_NOTACC", "UNKOWN"};


/** testRAPPredictor Implementation ***********/ 
testRAPPredictor::testRAPPredictor(int nbAssoc , int nbSet, int nbNVMways, DataArray& SRAMtable, DataArray& NVMtable, HybridCache* cache) : \
	Predictor(nbAssoc, nbSet, nbNVMways, SRAMtable, NVMtable, cache) {

	m_cpt = 1;

	m_RAPtable.clear();

	m_RAP_assoc = TEST_RAP_TABLE_ASSOC;
	m_RAP_sets = TEST_RAP_TABLE_SET;

	m_RAPtable.resize(m_RAP_sets);
	
	for(unsigned i = 0 ; i < m_RAP_sets ; i++)
	{
		m_RAPtable[i].resize(m_RAP_assoc);
		
		
		for(unsigned j = 0 ; j < m_RAP_assoc ; j++)
		{
			m_RAPtable[i][j] = new testRAPEntry();
			m_RAPtable[i][j]->index = i;
			m_RAPtable[i][j]->assoc = j;
		}
	}
	
	DPRINTF("RAPPredictor::Constructor m_RAPtable.size() = %d , m_RAPtable[0].size() = %d\n" , m_RAPtable.size() , m_RAPtable[0].size());

	m_rap_policy = new testRAPLRUPolicy(m_RAP_assoc , m_RAP_sets , m_RAPtable);

	/* Stats init*/ 
	stats_nbSwitchDst.clear();
	stats_ClassErrors.clear();
	stats_ClassErrors = vector< vector<int> >(NUM_RW_TYPE*NUM_RD_TYPE, vector<int>(NUM_RD_TYPE*NUM_RW_TYPE, 0));	
	
	/* Reseting the dataset_file */ 
	ofstream dataset_file(RAP_TEST_OUTPUT_DATASETS);
	dataset_file.close();


	stats_nbMigrationsFromNVM = vector<int>(2,0);
//	stats_switchDecision.clear();
//	stats_switchDecision.push_back(vector<vector<int>>(3 , vector<int>(3,0)));	
	
}


		
testRAPPredictor::~testRAPPredictor()
{
	for(auto sets : m_RAPtable)
		for(auto entry : sets)
			delete entry;
}

allocDecision
testRAPPredictor::allocateInNVM(uint64_t set, Access element)
{

	DPRINTF("testRAPPredictor::allocateInNVM set %ld\n" , set);
	
	if(element.isInstFetch())
		return ALLOCATE_IN_NVM;
		
	
	testRAPEntry* rap_current = lookup(element.m_pc);
	if(rap_current  == NULL) // Miss in the RAP table
	{
		stats_RAP_miss++;
		
		DPRINTF("testRAPPredictor::allocateInNVM Eviction and Installation in the RAP table 0x%lx\n" , element.m_pc);

		int index = indexFunction(element.m_pc);
		int assoc = m_rap_policy->evictPolicy(index);
		
		rap_current = m_RAPtable[index][assoc];
		
		/* Dumping stats before erasing the RAP entry */ 
		if(rap_current->isValid)
		{
			dumpDataset(rap_current);
			
			if(rap_current->nbKeepState + rap_current->nbSwitchState  > 0)
				stats_nbSwitchDst.push_back((double)rap_current->nbSwitchState / \
					 (double)(rap_current->nbKeepState+rap_current->nbSwitchState) );		
		}
		/******************/ 	

		rap_current->initEntry(element);
		rap_current->m_pc = element.m_pc;
	}
	else
	{	
		stats_RAP_hits++;
		rap_current->nbAccess++;
	}	
	
	m_rap_policy->updatePolicy(rap_current->index , rap_current->assoc);
	
	if(rap_current->des == BYPASS_CACHE)
	{
		rap_current->cptBypassLearning++;
//		updateWindow(rap_current);		
//		rap_current->cptWindow++;
		
		if(rap_current->cptBypassLearning ==  RAP_LEARNING_THRESHOLD){
			rap_current->cptBypassLearning = 0;
			return ALLOCATE_IN_NVM;		
		}
	}
	
	if(rap_current->des == ALLOCATE_PREEMPTIVELY)
	{
		if(element.isWrite())
			return ALLOCATE_IN_NVM;
		else
			return ALLOCATE_IN_SRAM;
	}
	else
		return rap_current->des;
}

void 
testRAPPredictor::finishSimu()
{
	DPRINTF("RAPPredictor::FINISH SIMU\n");

	for(auto lines : m_RAPtable)
	{
		for(auto entry : lines)
		{		
			if(entry->isValid)
			{
				HistoEntry current = {entry->state_rw , entry->state_rd , entry->nbKeepState};
				entry->stats_history.push_back(current);
				dumpDataset(entry);

				if(entry->nbKeepState + entry->nbSwitchState  > 0)
					stats_nbSwitchDst.push_back((double)entry->nbSwitchState / \
					  (double) (entry->nbKeepState + entry->nbSwitchState) );
			}
		}
	}
}


int
testRAPPredictor::computeRd(uint64_t set, uint64_t  index , bool inNVM)
{
	vector<CacheEntry*> line;
	int ref_rd;
	
	if(inNVM){
		line = m_tableNVM[set];
		ref_rd = m_tableNVM[set][index]->policyInfo;
	}
	else{
		line = m_tableSRAM[set];
		ref_rd = m_tableSRAM[set][index]->policyInfo;
	}
	
	int position = 0;
	
	/* Determine the position of the cachel line in the LRU stack, the policyInfo is not enough */
	for(unsigned i = 0 ; i < line.size() ; i ++)
	{
		if(line[i]->policyInfo > ref_rd)
			position++;
	}	
	return position;
}


void
testRAPPredictor::dumpDataset(testRAPEntry* entry)
{
	if(entry->stats_history.size() < 2)
		return;

	ofstream dataset_file(RAP_TEST_OUTPUT_DATASETS, std::ios::app);
	dataset_file << "Dataset\t0x" << std::hex << entry->m_pc << std::dec << endl;
	dataset_file << "\tNB Switch\t" << entry->nbSwitchState <<endl;
	dataset_file << "\tNB Keep State\t" << entry->nbKeepState <<endl;
	dataset_file << "\tHistory" << endl;
	for(auto p : entry->stats_history)
	{
		dataset_file << "\t\t" << str_RW_status[p.state_rw]<< "\t" << str_RD_status[p.state_rd] << "\t" << p.nbKeepState << endl;
	}
	dataset_file.close();
}


void
testRAPPredictor::updatePolicy(uint64_t set, uint64_t index, bool inNVM, Access element, bool isWBrequest = false)
{
	CacheEntry* current = NULL;
	if(inNVM)
		current = m_tableNVM[set][index];
	else 
		current = m_tableSRAM[set][index];
	
	int rd = computeRd(set, index , inNVM);

	current->policyInfo = m_cpt;
	testRAPEntry* rap_current = lookup(current->m_pc);
	
	if(current->m_pc !=0)
	{	
		if(rap_current)
		{

			DPRINTF("testRAPPredictor::updatePolicy Reuse distance of the pc %lx, RD = %d\n", current->m_pc , rd );
			rap_current->rd_history.push_back(rd);
			rap_current->rw_history.push_back(element.isWrite());		
			
			updateWindow(rap_current);
			
			if(ENABLE_LAZY_MIGRATION)
			{
				checkLazyMigration(rap_current , current , set , inNVM , index);
			}
		}	
	}
	
	m_cpt++;
	Predictor::updatePolicy(set , index , inNVM , element , isWBrequest);
}


void
testRAPPredictor::checkLazyMigration(testRAPEntry* rap_current , CacheEntry* current ,uint64_t set,bool inNVM, uint64_t index)
{
	if(rap_current->des == ALLOCATE_IN_NVM && inNVM == false)
	{
		//Trigger Migration
		DPRINTF("testRAPPredictor:: Migration Triggered from SRAM\n");
		int id_assoc = evictPolicy(set, true);//Select LRU candidate from NVM cache

		CacheEntry* replaced_entry = m_tableNVM[set][id_assoc];
					
		/** Migration incurs one read and one extra write */ 
		replaced_entry->nbWrite++;
		current->nbRead++;

		m_cache->triggerMigration(set, index , id_assoc , false);
		
		stats_nbMigrationsFromNVM[inNVM]++;
	}
	else if( rap_current->des == ALLOCATE_IN_SRAM && inNVM == true)
	{
	
		DPRINTF("testRAPPredictor:: Migration Triggered from NVM\n");

		int id_assoc = evictPolicy(set, false);
		
		CacheEntry* replaced_entry = m_tableSRAM[set][id_assoc];

		/** Migration incurs one read and one extra write */ 
		replaced_entry->nbWrite++;
		current->nbRead++;
	
		m_cache->triggerMigration(set, id_assoc , index , true);
		
		/* Record the write error migration */ 
		Predictor::migrationRecording();
		stats_nbMigrationsFromNVM[inNVM]++;

	}
//	cout <<" FINISH Lazy Migration "<< endl;
}

void
testRAPPredictor::insertionPolicy(uint64_t set, uint64_t index, bool inNVM, Access element)
{
	CacheEntry* current = NULL;
	if(inNVM)
		current = m_tableNVM[set][index];
	else 
		current = m_tableSRAM[set][index];
		
	current->policyInfo = m_cpt;
	
	testRAPEntry* rap_current;
	if( (rap_current= lookup(current->m_pc) ) != NULL )
	{
		/* Set the flag of learning cache line when bypassing */
		if(rap_current->des == BYPASS_CACHE)
			current->isLearning = true;
	}
	
	m_cpt++;
}


void 
testRAPPredictor::printStats(std::ostream& out)
{	

	
	ofstream output_file(RAP_OUTPUT_FILE);
	
	for(int i = 0 ; i < NUM_RW_TYPE ;i++)
	{
		for(int j = 0 ; j < NUM_RD_TYPE ; j++)
			output_file << "\t[" << str_RW_status[i] << ","<< str_RD_status[j] << "]";
	}
	output_file << endl;
	
	for(unsigned i = 0 ; i < stats_ClassErrors.size() ; i++)
	{
		output_file << "[" << str_RW_status[i/NUM_RD_TYPE] << ","<< str_RD_status[i%NUM_RD_TYPE] << "]\t";
		for(unsigned j = 0 ; j < stats_ClassErrors[i].size() ; j++)
		{
			output_file << stats_ClassErrors[i][j] << "\t";
		}
		output_file << endl;
	}
	output_file.close();
	
	/*
	ofstream output_file1(RAP_OUTPUT_FILE1);	
	output_file1 << "\tDEAD->DEAD\tDEAD->WO\tDEAD->RO\tDEAD->RW\tWO->DEAD\tWO->WO\tWO->RO\tWO->RW";
	output_file1 << "\tRO->DEAD\tRO->WO\tRO->RO\tRO->RW\tRW->DEAD\tRW->WO\tRW->RO\tRW->RW";
	output_file1 << endl;
	for(auto p : stats_switchDecision)
	{
		for(auto a : p){
			for(auto b : a){
				output_file1 << "\t" << b;
			}
		}
		
		output_file1 << endl;
	}
	output_file1.close();
	*/

	out << "\tRAP Table:" << endl;
	out << "\t\t NB Hits: " << stats_RAP_hits << endl;
	out << "\t\t NB Miss: " << stats_RAP_miss << endl;
	out << "\t\t Miss Ratio: " << stats_RAP_miss*100.0/(double)(stats_RAP_hits+stats_RAP_miss)<< "%" << endl;
	
	if(stats_nbSwitchDst.size() > 0)
	{
		double sum=0;
		for(auto d : stats_nbSwitchDst)
			sum+= d;
		out << "\t Average Switch\t" << sum / (double) stats_nbSwitchDst.size() << endl;	
	}
	
	if(ENABLE_LAZY_MIGRATION)
	{
		double migrationNVM = (double) stats_nbMigrationsFromNVM[0];
		double migrationSRAM = (double) stats_nbMigrationsFromNVM[1];
		double total = migrationNVM+migrationSRAM;

		if(total > 0){
			out << "Predictor Migration:" << endl;
			out << "NB Migration :" << total << endl;
			out << "\t From NVM : " << migrationNVM*100/total << "%" << endl;
			out << "\t From SRAM : " << migrationSRAM*100/total << "%" << endl;	
		}

	
	}
	
	Predictor::printStats(out);
}


void
testRAPPredictor::updateWindow(testRAPEntry* rap_current)
{
	if(rap_current->des == BYPASS_CACHE) 
	//if(false)
	{
		//A learning cache line from a bypassed dataset
		//We update dataset status immediately if we see reuse
		//determineStatus(rap_current);
		
		return;
	}
	else {
		if(rap_current->cptWindow < RAP_WINDOW_SIZE)
			rap_current->cptWindow++;
		else
		{
			RW_TYPE old_rw = rap_current->state_rw;
			RD_TYPE old_rd = rap_current->state_rd;

			determineStatus(rap_current);
	
			stats_ClassErrors[old_rd + NUM_RD_TYPE*old_rw][rap_current->state_rd + NUM_RD_TYPE*rap_current->state_rw]++;

			if(old_rd != rap_current->state_rd || old_rw != rap_current->state_rw)
			{
				HistoEntry current = {rap_current->state_rw , rap_current->state_rd, rap_current->nbKeepState};
				rap_current->stats_history.push_back(current);
				rap_current->nbSwitchState++;
				rap_current->nbKeepState = 0;					
	
				rap_current->des = convertState(rap_current);
			}
			else{
				rap_current->nbKeepState++;
				rap_current->nbKeepCurrentState++;			
			}

			/* Reset the window */			
			rap_current->rd_history.clear();
			rap_current->rw_history.clear();
			rap_current->cptWindow = 0;		
		}
	}	
}


int
testRAPPredictor::evictPolicy(int set, bool inNVM)
{	

		
	int assoc_victim = -1;
	assert(m_replacementPolicyNVM_ptr != NULL);
	assert(m_replacementPolicySRAM_ptr != NULL);

	CacheEntry* current = NULL;
	if(inNVM)
	{	
		assoc_victim = m_replacementPolicyNVM_ptr->evictPolicy(set);
		current = m_tableNVM[set][assoc_victim];
	}
	else
	{
		assoc_victim = m_replacementPolicySRAM_ptr->evictPolicy(set);
		current = m_tableSRAM[set][assoc_victim];		
	}
		
	DPRINTF("testRAPPredictor::evictPolicy set %d n assoc_victim %d \n" , set , assoc_victim);
	testRAPEntry* rap_current = lookup(current->m_pc);
	//We didn't create an entry for this dataset, probably a good reason =) (instruction mainly) 
	if(rap_current != NULL)
	{
	
		if(rap_current->des == BYPASS_CACHE)
		{
			// A learning cache line on dead dataset goes here
			if( !(current->nbWrite == 0 && current->nbWrite == 0) )
			{
				// Reset the window 
				RW_TYPE old_rw = rap_current->state_rw;
				RD_TYPE old_rd = rap_current->state_rd;
				
				rap_current->des = ALLOCATE_PREEMPTIVELY;
				rap_current->state_rd = RD_NOT_ACCURATE;
				rap_current->state_rw = RW_NOT_ACCURATE;
				rap_current->dead_counter = 0;
			
				stats_ClassErrors[old_rd + NUM_RD_TYPE*old_rw][rap_current->state_rd + NUM_RD_TYPE*rap_current->state_rw]++;
			}
			//No reuse on learning line , we stay on bypass mode		
		}
		else
		{
			// A learning cache line on dead dataset goes here
			if(current->nbWrite == 0 && current->nbWrite == 0)
				rap_current->dead_counter--;
			else
				rap_current->dead_counter++;
		
			if(rap_current->dead_counter > RAP_DEAD_COUNTER_SATURATION)
				rap_current->dead_counter = RAP_DEAD_COUNTER_SATURATION;
			else if(rap_current->dead_counter < -RAP_DEAD_COUNTER_SATURATION)
				rap_current->dead_counter = -RAP_DEAD_COUNTER_SATURATION;

			if(rap_current->dead_counter == -RAP_DEAD_COUNTER_SATURATION && ENABLE_BYPASS)
			{
				/* Switch the state of the dataset to dead */ 
				rap_current->des = BYPASS_CACHE;			

				// Reset the window 
				RW_TYPE old_rw = rap_current->state_rw;
				RD_TYPE old_rd = rap_current->state_rd;

				rap_current->state_rd = RD_NOT_ACCURATE;
				rap_current->state_rw = DEAD;
				rap_current->rd_history.clear();
				rap_current->rw_history.clear();
				rap_current->cptWindow = 0;	
				rap_current->dead_counter = 0;
				// Monitor state switching 
				stats_ClassErrors[old_rd + NUM_RD_TYPE*old_rw][rap_current->state_rd + NUM_RD_TYPE*rap_current->state_rw]++;
			
			}
		}
	}
	
	evictRecording(set , assoc_victim , inNVM);	
	return assoc_victim;
}

void 
testRAPPredictor::determineStatus(testRAPEntry* entry)
{
	if(entry->des == BYPASS_CACHE)
	{
		//All the info gathered here are from learning cache line
		//If we see any any reuse we learn again
		if(entry->rw_history.size() != 0)
		{
			entry->des = ALLOCATE_PREEMPTIVELY;
			entry->state_rd = RD_NOT_ACCURATE;
			entry->state_rw = RW_NOT_ACCURATE;
			entry->dead_counter = 0;
		}
		return;		
	}


	/* Count the number of read and write during window */
	vector<int> cpts_rw = vector<int>(2,0);
	for(auto isWrite : entry->rw_history)
	{
		if(isWrite)
			cpts_rw[INDEX_WRITE]++;
		else
			cpts_rw[INDEX_READ]++;
	}

	if(cpts_rw[INDEX_READ] == 0 && cpts_rw[INDEX_WRITE] > 0)
		entry->state_rw = WO;
	else if(cpts_rw[INDEX_READ] > 0 && cpts_rw[INDEX_WRITE] == 0)
		entry->state_rw = RO;
	else if(cpts_rw[INDEX_READ] > 0 && cpts_rw[INDEX_WRITE] > 0)
		entry->state_rw = RW;
	else
		entry->state_rw = DEAD;
		
	if(entry->state_rw == DEAD)
	{
		entry->state_rd = RD_NOT_ACCURATE;
		return;	
	}

	
	/* Determination of the rd type */
	int window_rd = entry->rd_history.size();

	vector<int> cpts_rd = vector<int>(NUM_RD_TYPE,0);
	
	for(auto rd : entry->rd_history)
		cpts_rd[convertRD(rd)]++;	
	
	int max = 0, index_max = -1;
	for(unsigned i = 0 ; i < cpts_rd.size() ; i++)
	{
		if(cpts_rd[i] > max)
		{
			max = cpts_rd[i];
			index_max = i;
		}
	}
	if( ((double)max / (double)window_rd) > RAP_INACURACY_TH)
		entry->state_rd = (RD_TYPE) index_max;
	else
		entry->state_rd = RD_NOT_ACCURATE;
			
//	/** Debugging purpose */	
//	if(entry->state_rd == RD_NOT_ACCURATE && entry->state_rw == DEAD)
//	{
//		cout << "RD history\t";
//		for(auto p : entry->rd_history)
//			cout << p << "\t";
//		cout << endl;
//		cout << "RW history\tNBWrite=" << cpts_rw[INDEX_WRITE] << "\tNBRead=" << cpts_rw[INDEX_READ] << endl;
//		cout << "New state is " << str_RD_status[entry->state_rd] << "\t" << str_RW_status[entry->state_rw] << endl;	
//	}


	/*
	if( ((double)max / (double) RAP_WINDOW_SIZE) > RAP_INACURACY_TH)
		entry->state_rw = (RW_TYPE)index_max;
	else
		entry->state_rw = RW_NOT_ACCURATE;
	*/
}

allocDecision
testRAPPredictor::convertState(testRAPEntry* rap_current)
{
	RD_TYPE state_rd = rap_current->state_rd;
	RW_TYPE state_rw = rap_current->state_rw;

	if(state_rw == RO)
	{
		return ALLOCATE_IN_NVM;	
	}
	else if(state_rw == RW || state_rw == WO)
	{
		if(state_rd == RD_SHORT || state_rd == RD_NOT_ACCURATE)
			return ALLOCATE_IN_SRAM;
		else if(state_rd == RD_MEDIUM)
			return ALLOCATE_IN_NVM;
		else 
			return BYPASS_CACHE;
	}
	else if(state_rw == DEAD)
	{
		// When in dead state, rd state is always innacurate, no infos 
		if(rap_current->des == ALLOCATE_IN_SRAM)
			return ALLOCATE_IN_NVM;
		else 
			return BYPASS_CACHE;	
	}
	else
	{ // State RW not accurate 
	
		if(state_rd == RD_SHORT)
			return ALLOCATE_IN_SRAM;
		else if( state_rd == RD_MEDIUM)
			return ALLOCATE_IN_NVM;
		else if(state_rd == RD_NOT_ACCURATE) // We don't know anything for sure, go preemptive
			return ALLOCATE_PREEMPTIVELY;
		else 
			return BYPASS_CACHE;
	}
}


void 
testRAPPredictor::printConfig(std::ostream& out)
{
	out << "\t\tRAP Table Parameters" << endl;
	out << "\t\t\t- Assoc : " << m_RAP_assoc << endl;
	out << "\t\t\t- NB Sets : " << m_RAP_sets << endl;
	out << "\t\t Window size : " << RAP_WINDOW_SIZE << endl;
	out << "\t\t Inacurracy Threshold : " << RAP_INACURACY_TH << endl;

	string a =  ENABLE_BYPASS ? "TRUE" : "FALSE"; 
	out << "\t\t Bypass Enable : " << a << endl;
	out << "\t\t Bypass DEAD COUNTER Saturation : " << RAP_DEAD_COUNTER_SATURATION << endl;
	out << "\t\t Bypass Learning Threshold : " << RAP_LEARNING_THRESHOLD << endl;
	a =  ENABLE_LAZY_MIGRATION ? "TRUE" : "FALSE"; 
	out << "\t\t Lazy Migration Enable : " << a << endl;
}
		

void 
testRAPPredictor::openNewTimeFrame()
{ 
//	stats_switchDecision.push_back(vector<vector<int> >(NUM_ALLOC_DECISION,vector<int>(NUM_ALLOC_DECISION,0)));

	Predictor::openNewTimeFrame();
}


testRAPEntry*
testRAPPredictor::lookup(uint64_t pc)
{
	uint64_t set = indexFunction(pc);
	for(unsigned i = 0 ; i < m_RAP_assoc ; i++)
	{
		if(m_RAPtable[set][i]->m_pc == pc)
			return m_RAPtable[set][i];
	}
	return NULL;
}

uint64_t 
testRAPPredictor::indexFunction(uint64_t pc)
{
	return pc % m_RAP_sets;
}



RD_TYPE
testRAPPredictor::evaluateRd(vector<int> reuse_distances)
{
	if(reuse_distances.size() == 0)
		return RD_NOT_ACCURATE;
	
	return convertRD(reuse_distances.back());
}

RD_TYPE
testRAPPredictor::convertRD(int rd)
{
	if(rd < RAP_SRAM_ASSOC)
		return RD_SHORT;
	else if(rd < RAP_NVM_ASSOC)
		return RD_MEDIUM;
	else 
		return RD_NOT_ACCURATE;
}

/************* Replacement Policy implementation for the test RAP table ********/ 

testRAPLRUPolicy::testRAPLRUPolicy(int nbAssoc , int nbSet , std::vector<std::vector<testRAPEntry*> >& rap_entries) :\
		testRAPReplacementPolicy(nbAssoc , nbSet, rap_entries) 
{
	m_cpt = 1;
}

void
testRAPLRUPolicy::updatePolicy(uint64_t set, uint64_t assoc)
{	

	
	m_rap_entries[set][assoc]->policyInfo = m_cpt;
	m_cpt++;

}

int
testRAPLRUPolicy::evictPolicy(int set)
{
	for(unsigned i = 0 ; i < m_assoc ; i++){
		if(!m_rap_entries[set][i]->isValid)
			return i;
	}
	
	int smallest_time = m_rap_entries[set][0]->policyInfo , smallest_index = 0;
	for(unsigned i = 0 ; i < m_assoc ; i++){
		if(m_rap_entries[set][i]->policyInfo < smallest_time){
			smallest_time =  m_rap_entries[set][i]->policyInfo;
			smallest_index = i;
		}
	}
	return smallest_index;
}





