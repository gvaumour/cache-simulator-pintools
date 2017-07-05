#include <fstream>
#include <vector>
#include <map>
#include <ostream>

#include "testRAPPredictor.hh"

using namespace std;

/** testRAPPredictor Implementation ***********/ 

testRAPPredictor::testRAPPredictor(int nbAssoc , int nbSet, int nbNVMways, DataArray& SRAMtable, DataArray& NVMtable, HybridCache* cache) : \
	Predictor(nbAssoc, nbSet, nbNVMways, SRAMtable, NVMtable, cache) {
	m_cpt = 1;

	m_RAPtable.clear();

	m_RAP_assoc = RAP_TABLE_ASSOC;
	m_RAP_sets = RAP_TABLE_SET;

	m_RAPtable.resize(m_RAP_assoc);
	
	for(unsigned i = 0 ; i < m_RAP_assoc ; i++)
	{
		m_RAPtable[i].resize(m_RAP_sets);
		
		
		for(unsigned j = 0 ; j < m_RAP_sets ; j++)
		{
			m_RAPtable[i][j] = new testRAPEntry();
			m_RAPtable[i][j]->index = j;
			m_RAPtable[i][j]->assoc = i;
		}
	}
	m_rap_policy = new testRAPLRUPolicy(m_RAP_assoc , m_RAP_sets , m_RAPtable);

	/* Stats init*/ 
	stats_nbSwitchDst.clear();
	stats_ClassErrors.clear();
	stats_ClassErrors.push_back(vector<int>(NUM_RW_TYPE,0));	
	
	stats_switchDecision.clear();
	stats_switchDecision.push_back(vector<vector<int>>(NUM_ALLOC_DECISION , vector<int>(NUM_ALLOC_DECISION,0)));
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
		/*if(current->m_pc != 0)
		{	
					
			DPRINTF("NB Access of the evicted line\t%d\n" , current->nbAccess); 
			DPRINTF("Produced RD of PC: %ld " , current->m_pc); 
			for(auto p : current->reuse_distances)
			{			
				DPRINTF("%d " , p );
			}
				
			DPRINTF("\n"); 		
			
		}*/

		if(current->nbUpdate > 0)
			stats_nbSwitchDst.push_back((double)current->nbSwitch / (double)current->nbUpdate);
	
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
	
	return current->des;
}


void 
testRAPPredictor::finishSimu()
{
	DPRINTF("testRAPPredictor::FINISH SIMU\n");
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
/*
void 
testRAPPredictor::dumpRAPtable(testRAPEntry* entry)
{
	cerr << "Table " << endl;
	
	for(auto lines : m_RAPtable)
	{
		for(auto entry : lines)
		{
			if(!entry->isValid)
				continue;
			
			cerr << "\tNB Access\t" << entry->nbAccess << endl;
			cerr << "\t RDs recorded:\t"; 
			for(auto rd : reuse_distances)
			cerr << "\t" << rd;
			cerr << endl;
		}
	}
	
}*/

void 
testRAPPredictor::printStats(std::ostream& out)
{	

	
	ofstream output_file(RAP_OUTPUT_FILE);
	
	output_file << "\tDEAD\tWO\tRO\tRW" << endl;
	for(auto p : stats_ClassErrors)
	{
		output_file << "\t" << p[DEAD] << "\t" << p[WO] << "\t" \
			            << p[RO] << "\t" << p[RW] << "\t" << p[ALMOST_RO] << endl;
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
		rap_current->nbUpdate++;
		rap_current->nbReuse = current->nbWrite + current->nbRead;
		 
		// Write Status of the cache line 
		if(current->nbWrite == 0 && current->nbRead == 0)
		{
			rap_current->cpts[DEAD]++;
			if(rap_current->des != BYPASS_CACHE)
				stats_ClassErrors[stats_ClassErrors.size()-1][DEAD]++;
		}
		else if(current->nbWrite > 0 && current->nbRead == 0)
		{
			rap_current->cpts[WO]++;
			if(rap_current->des != ALLOCATE_IN_SRAM)
				stats_ClassErrors[stats_ClassErrors.size()-1][WO]++;
		}
		else if(current->nbWrite == 0 && current->nbRead > 0)
		{
			rap_current->cpts[RO]++;
			if(rap_current->des != ALLOCATE_IN_NVM)
				stats_ClassErrors[stats_ClassErrors.size()-1][RO]++;
		}
		else if(current->nbWrite == 1 && current->nbRead > 0)
		{
			rap_current->cpts[ALMOST_RO]++;		
			if(rap_current->des != ALLOCATE_IN_SRAM)
				stats_ClassErrors[stats_ClassErrors.size()-1][ALMOST_RO]++;
		}	
		else
		{
			rap_current->cpts[RWLINES]++;		
			if(rap_current->des != ALLOCATE_IN_SRAM)
				stats_ClassErrors[stats_ClassErrors.size()-1][RW]++;
		}
		/*
		if(rap_current->cpts[DEAD] == RAP_SATURATION_TH || rap_current->cpts[] == RAP_SATURATION_TH || \
			rap_current->cpts[WOLINES] == RAP_SATURATION_TH || rap_current->cpts[ROLINES] == RAP_SATURATION_TH)
		{*/

		allocDecision old = rap_current->des; 
		updateDecision(rap_current);/*
		if(rap_current->des != old )
		{
			//printf("testRAPPredictor:: Saturation of a counter, Update of the decision replacement PC: %ld \n" , rap_current->m_pc);
			stats_switchDecision[stats_switchDecision.size()-1][old][rap_current->des]++;
			rap_current->nbSwitch++;
		
			if(rap_current->cpts[DEAD] == RAP_SATURATION_TH)
				state= DEAD;
			else if(rap_current->cpts[RO] == RAP_SATURATION_TH)
				state= RO;
			else if(rap_current->cpts[WO] == RAP_SATURATION_TH)
				state= WO;
			else 
				state= RW;
				
			//printf("testRAPPredictor:: Old Decision : %s \n" , allocDecision_str[old]);
			//printf("testRAPPredictor:: New Decision : %s \n" , allocDecision_str[rap_current->des]);	
			cptLearning = 0;
		}
		else
		{	
			cptLearning++;
			if(cptLearning == RAP_LEARNING_THRESHOLD1)
			{
				cptLearning = 0
				if(state == RO)
				{
					
				}
			}
		}	*/
		//}	
	}

	evictRecording(set , assoc_victim , inNVM);	
	
	return assoc_victim;
}

void 
testRAPPredictor::printConfig(std::ostream& out)
{
	out << "\t\tRAP Table Parameters" << endl;
	out << "\t\t\t- Assoc : " << m_RAP_assoc << endl;
	out << "\t\t\t- NB Sets : " << m_RAP_sets << endl;
	out << "\t\t Learning Threshold : " << RAP_LEARNING_THRESHOLD << endl;
	out << "\t\t Saturation Threshold : " << RAP_SATURATION_TH << endl;
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
	return pc % m_RAP_assoc;
}



void
testRAPPredictor::updateDecision(testRAPEntry* entry)
{
/*	string state = "Dead";
	if(entry->cpts[ROLINES] == RAP_SATURATION_TH)
		state = "RO";
	else if(entry->cpts[WOLINES] == RAP_SATURATION_TH)
		state ="WO";
	else if(entry->cpts[RWLINES] == RAP_SATURATION_TH)
		state = "RW";

	cerr << "Update Decision: " << endl;
	cerr << "\tSaturated cpt\t"<< state << endl;
	cerr << "\tPC\t"<< entry->m_pc << endl;
	cerr << "\tAlloc Des\t"<< allocDecision_str[entry->des] << endl;
	cerr << "\tNB Access\t" << entry->nbAccess << endl;
	cerr << "\tRDs recorded:\t"; 
	for(int i = entry->reuse_distances.size()-1 ; i >= 0 && i >= entry->reuse_distances.size()-10 ; i--)
		cerr << "\t" << entry->reuse_distances[i];
	cerr << endl;
*/

	if(entry->cpts[ROLINES] == RAP_SATURATION_TH)
	{
		entry->des = ALLOCATE_IN_NVM;
	
	}
	else if(entry->cpts[WOLINES] == RAP_SATURATION_TH || entry->cpts[RWLINES] == RAP_SATURATION_TH)
	{
		/*
		RD_TYPE rd = evaluateRd(entry->reuse_distances);		
		
		if( entry->des == ALLOCATE_IN_SRAM)
		{
			if(rd == RD_MEDIUM || rd == RD_LONG)
				entry->des = ALLOCATE_IN_NVM;
			else
				entry->des = ALLOCATE_IN_SRAM;
		}
		else if( entry->des == ALLOCATE_IN_NVM)
		{
			if(rd == RD_LONG)
				entry->des = BYPASS_CACHE;
			//If we don't know rd, better go to SRAM for WO and RW datasets
			else if(rd == RD_SHORT || rd == RD_NOT_ACCURATE) 
				entry->des =ALLOCATE_IN_SRAM;
			else 
				entry->des =ALLOCATE_IN_NVM;
		}
		else 
		{
			//Learning Cache line for bypassed dataset
			//Allocated in NVM so we can use rd info
			rd = convertRD(entry->reuse_distances.back());
			
			if(rd == RD_SHORT)
				entry->des = ALLOCATE_IN_SRAM;
			else if(rd == RD_MEDIUM)
				entry->des = ALLOCATE_IN_NVM;
			else
				entry->des = BYPASS_CACHE;
		}*/
		entry->des = ALLOCATE_IN_SRAM;
	}
	else{//No Reuse 
		entry->des = BYPASS_CACHE;
	}

	entry->cpts = vector<int>(NUM_RW_TYPE,0);
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





