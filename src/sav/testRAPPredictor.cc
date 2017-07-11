#include <fstream>
#include <vector>
#include <map>
#include <ostream>

#include "testRAPPredictor.hh"

using namespace std;


static const char* str_RW_status[] = {"DEAD" , "RO", "WO" , "RW", "RW_NOT_ACCURATE"};
static const char* str_RD_status[] = {"RD_SHORT" , "RD_MEDIUM", "RD_LONG" , "RD_NOT_ACCURATE", "UNKOWN"};


/** testRAPPredictor Implementation ***********/ 
testRAPPredictor::testRAPPredictor(int nbAssoc , int nbSet, int nbNVMways, DataArray& SRAMtable, DataArray& NVMtable, HybridCache* cache) : \
	Predictor(nbAssoc, nbSet, nbNVMways, SRAMtable, NVMtable, cache) {

	m_cpt = 1;

	m_RAPtable.clear();

	m_RAP_assoc = RAP_TABLE_ASSOC;
	m_RAP_sets = RAP_TABLE_SET;

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
	stats_ClassErrors.push_back(vector<int>(4,0));	
	
	stats_switchDecision.clear();
	stats_switchDecision.push_back(vector<vector<int>>(3 , vector<int>(3,0)));	
	
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
		
	testRAPEntry* current = lookup(element.m_pc);
	if(current  == NULL) // Miss in the RAP table
	{
	
		stats_RAP_miss++;
		
		DPRINTF("testRAPPredictor::allocateInNVM Eviction and Installation in the RAP table 0x%lx\n" , element.m_pc);

		int index = indexFunction(element.m_pc);
		int assoc = m_rap_policy->evictPolicy(index);
		
		current = m_RAPtable[index][assoc];
		
		/* Dumping stats before erasing the RAP entry */ 
		if(current->isValid)
			dumpDataset(current);
			
		if(current->nbKeepState + current->nbSwitchState  > 0)
			stats_nbSwitchDst.push_back((double)current->nbSwitchState / (double)current->nbKeepState);
		/******************/ 	

		current->initEntry(element);
		current->m_pc = element.m_pc;
	}
	else
	{	
		stats_RAP_hits++;
		current->nbAccess++;
	}	
	
	m_rap_policy->updatePolicy(current->index , current->assoc);
	
	if(current->des == BYPASS_CACHE)
	{
		current->cptBypassLearning++;
		if(current->cptBypassLearning ==  RAP_LEARNING_THRESHOLD){
			current->cptBypassLearning = 0;
			return ALLOCATE_IN_NVM;		
		}
	}
	
	if(current->des == ALLOCATE_PREEMPTIVELY)
	{
		if(element.isWrite())
			return ALLOCATE_IN_NVM;
		else
			return ALLOCATE_IN_SRAM;
	}
	else
		return current->des;
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
				entry->history.push_back(current);
				dumpDataset(entry);
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
	if(entry->history.size() < 2)
		return;

	ofstream dataset_file(RAP_TEST_OUTPUT_DATASETS, std::ios::app);
	dataset_file << "Dataset\t0x" << std::hex << entry->m_pc << std::dec << endl;
	dataset_file << "\tNB Switch\t" << entry->nbSwitchState <<endl;
	dataset_file << "\tNB Keep State\t" << entry->nbKeepState <<endl;
	dataset_file << "\tHistory" << endl;
	for(auto p : entry->history)
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
	if(rap_current)
	{
		DPRINTF("testRAPPredictor::updatePolicy Reuse distance of the pc %lx, RD = %d\n", current->m_pc , rd );
		rap_current->reuse_distances.push_back(rd);
	}
	
	m_cpt++;
	Predictor::updatePolicy(set , index , inNVM , element , isWBrequest);
}

void testRAPPredictor::insertionPolicy(uint64_t set, uint64_t index, bool inNVM, Access element)
{
	CacheEntry* current = NULL;
	if(inNVM)
		current = m_tableNVM[set][index];
	else 
		current = m_tableSRAM[set][index];
		
	current->policyInfo = m_cpt;

	testRAPEntry* rap_current = lookup(current->m_pc);
	if(rap_current)
	{
		//On the first access the rd is infinite 
		rap_current->reuse_distances.push_back(RD_INFINITE);
	}
	
	m_cpt++;
}


void 
testRAPPredictor::printStats(std::ostream& out)
{	

	
	ofstream output_file(RAP_OUTPUT_FILE);
	
	output_file << "\tDEAD\tWO\tRO\tRW" << endl;
	for(auto p : stats_ClassErrors)
	{
		output_file << "\t" << p[DEAD] << "\t" << p[WO] << "\t" \
			            << p[RO] << "\t" << p[RW] << endl;
	}
	output_file.close();
	

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
	
	
	Predictor::printStats(out);
}


int
testRAPPredictor::evictPolicy(int set, bool inNVM)
{	
	DPRINTF("testRAPPredictor::evictPolicy set %d\n" , set);
		
	int assoc_victim = -1;
	assert(m_replacementPolicyNVM_ptr != NULL);
	assert(m_replacementPolicySRAM_ptr != NULL);

	if(inNVM)
		assoc_victim = m_replacementPolicyNVM_ptr->evictPolicy(set);
	else
		assoc_victim = m_replacementPolicySRAM_ptr->evictPolicy(set);

	CacheEntry* current = NULL;
	if(inNVM)
		current = m_tableNVM[set][assoc_victim];
	else
		current = m_tableSRAM[set][assoc_victim];		
		
	testRAPEntry* rap_current = lookup(current->m_pc);
	//We didn't create an entry for this dataset, probably a good reason =) (instruction mainly) 
	if(rap_current != NULL)
	{

		if(current->nbWrite == 0 && current->nbRead == 0)
		{
			rap_current->cpts[DEAD]++;
			rap_current->reuse_distances.push_back(RD_INFINITE);		
		}
		else if(current->nbWrite > 0 && current->nbRead == 0)
			rap_current->cpts[WO]++;
		else if(current->nbWrite == 0 && current->nbRead > 0)
			rap_current->cpts[RO]++;
		else
			rap_current->cpts[RWLINES]++;		
		
		if(rap_current->des == BYPASS_CACHE) // A learning cache line
		{
			/* If we see reuse on dead dataset, we learn again */ 
			if(current->nbWrite > 0 || current->nbRead > 0)
				rap_current->des = ALLOCATE_PREEMPTIVELY;
		}
		else
		{

			if(rap_current->cptWindow < RAP_WINDOW_SIZE)
				rap_current->cptWindow++;
			else
			{
				RW_TYPE old_rw = rap_current->state_rw;
				RD_TYPE old_rd = rap_current->state_rd;

				determineStatus(rap_current);
			
				if(old_rd != rap_current->state_rd || old_rw != rap_current->state_rw)
				{
					HistoEntry current = {rap_current->state_rw , rap_current->state_rd, rap_current->nbKeepState};
					rap_current->history.push_back(current);
					rap_current->nbSwitchState++;
					rap_current->nbKeepState = 0;
				
					rap_current->des = convertState(rap_current);
				}
				else{
					rap_current->nbKeepState++;
					rap_current->nbKeepCurrentState++;			
				}

				/* Reset the window */			
				rap_current->reuse_distances.clear();
				rap_current->cptWindow = 0;		
			}
		}
	}

	evictRecording(set , assoc_victim , inNVM);	
	return assoc_victim;
}

void 
testRAPPredictor::determineStatus(testRAPEntry* entry)
{
	/* Determination of the rd type */
	int window_rd = entry->reuse_distances.size();
	vector<int> cpts_rd = vector<int>(NUM_RD_TYPE,0);
	
	for(auto rd : entry->reuse_distances)
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
		
	/* Determination of the rw type */
	max = 0 , index_max = -1;
	for(unsigned i = 0 ; i < entry->cpts.size() ; i++)
	{
		if(entry->cpts[i] > max)
		{
			max = entry->cpts[i];
			index_max = i;
		}
	}
	
	if( ((double)max / (double) RAP_WINDOW_SIZE) > RAP_INACURACY_TH)
		entry->state_rw = (RW_TYPE)index_max;
	else
		entry->state_rw = RW_NOT_ACCURATE;

}

allocDecision
testRAPPredictor::convertState(testRAPEntry* rap_current)
{
	RD_TYPE state_rd = rap_current->state_rd;
	RW_TYPE state_rw = rap_current->state_rw;

	if(state_rw == RO)
	{
		if(state_rd == RD_LONG)
			return BYPASS_CACHE;	
		else
			return ALLOCATE_IN_NVM;	
	}
	else if(state_rw == RW || state_rw == WO)
	{
		if(state_rd == RD_SHORT || state_rd == RD_NOT_ACCURATE)
			return ALLOCATE_IN_SRAM;
		else if( (state_rd == RD_LONG && rap_current->des == ALLOCATE_IN_SRAM) || state_rd == RD_MEDIUM)
			return ALLOCATE_IN_NVM;
		else 
			return BYPASS_CACHE;
	}
	else if(state_rw == DEAD)
		return BYPASS_CACHE;
	else
	{ // State RW not accurate 
	
		if(state_rd == RD_SHORT)
			return ALLOCATE_IN_SRAM;
		else if( (state_rd == RD_LONG && rap_current->des == ALLOCATE_IN_SRAM) || state_rd == RD_MEDIUM)
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
	out << "\t\t Learning Threshold : " << RAP_LEARNING_THRESHOLD << endl;
}
		

void 
testRAPPredictor::openNewTimeFrame()
{ 
	stats_ClassErrors.push_back(vector<int>(NUM_RW_TYPE,0));
	stats_switchDecision.push_back(vector<vector<int> >(NUM_ALLOC_DECISION,vector<int>(NUM_ALLOC_DECISION,0)));

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
		return RD_LONG;
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
	int smallest_time = m_rap_entries[set][0]->policyInfo , smallest_index = 0;

	for(unsigned i = 0 ; i < m_assoc ; i++){
		if(!m_rap_entries[set][i]->isValid)
			return i;
	}
	
	for(unsigned i = 0 ; i < m_assoc ; i++){
		if(m_rap_entries[set][i]->policyInfo < smallest_time){
			smallest_time =  m_rap_entries[set][i]->policyInfo;
			smallest_index = i;
		}
	}
	return smallest_index;
}





