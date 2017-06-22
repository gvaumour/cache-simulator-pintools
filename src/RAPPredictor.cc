#include <fstream>
#include <vector>
#include <map>
#include <ostream>

#include "RAPPredictor.hh"

using namespace std;

/** RAPPredictor Implementation ***********/ 
/*
RAPPredictor::RAPPredictor() : Predictor(){
	m_cpt = 1;
	m_RAPtable.clear();
	stats_ClassErrors.clear();
	
}*/

RAPPredictor::RAPPredictor(int nbAssoc , int nbSet, int nbNVMways, DataArray& SRAMtable, DataArray& NVMtable, HybridCache* cache) : \
	Predictor(nbAssoc, nbSet, nbNVMways, SRAMtable, NVMtable, cache) {
	m_cpt = 1;

	m_RAPtable.clear();
	stats_ClassErrors.clear();
	stats_ClassErrors.push_back(vector<int>(4,0));	
	
	stats_switchDecision.clear();
	stats_switchDecision.push_back(vector<vector<int>>(4 , vector<int>(4,0)));

	m_RAP_assoc = RAP_TABLE_ASSOC;
	m_RAP_sets = RAP_TABLE_SET;

	m_RAPtable.resize(m_RAP_assoc);
	
	for(unsigned i = 0 ; i < m_RAP_assoc ; i++)
	{
		m_RAPtable[i].resize(m_RAP_sets);
		
		
		for(unsigned j = 0 ; j < m_RAP_sets ; j++)
		{
			m_RAPtable[i][j] = new RAPEntry();
			m_RAPtable[i][j]->index = j;
			m_RAPtable[i][j]->assoc = i;
		}
	}

	m_rap_policy = new RAPLRUPolicy(m_RAP_assoc , m_RAP_sets , m_RAPtable);
}


		
RAPPredictor::~RAPPredictor()
{
	for(auto sets : m_RAPtable)
		for(auto entry : sets)
			delete entry;
}

allocDecision
RAPPredictor::allocateInNVM(uint64_t set, Access element)
{

	DPRINTF("RAPPredictor::allocateInNVM set %ld\n" , set);
	
	if(element.isInstFetch())
		return ALLOCATE_IN_NVM;
		
		
	RAPEntry* current = lookup(element.m_pc);
	if(current  == NULL) // Miss in the RAP table
	{
	
		stats_RAP_miss++;
		
		DPRINTF("RAPPredictor::allocateInNVM Eviction and Installation in the RAP table %ld\n" , element.m_pc);
		int index = indexFunction(element.m_pc);
		int assoc = m_rap_policy->evictPolicy(index);
		
		current = m_RAPtable[index][assoc];
		current->initEntry();
		current->m_pc = element.m_pc;
	}
	else	
		stats_RAP_hits++;
	
	m_rap_policy->updatePolicy(current->index , current->assoc);
	
	if(current->des == BYPASS_CACHE)
	{
		current->cptLearning++;
		if(current->cptLearning ==  RAP_LEARNING_THRESHOLD)
			return ALLOCATE_IN_NVM;
		current->cptLearning++;
	}
	
	return current->des;
}

void
RAPPredictor::updatePolicy(uint64_t set, uint64_t index, bool inNVM, Access element, bool isWBrequest = false)
{
	if(inNVM)
		m_replacementPolicyNVM_ptr->updatePolicy(set, index , 0);
	else 
		m_replacementPolicySRAM_ptr->updatePolicy(set, index , 0);

	Predictor::updatePolicy(set , index , inNVM , element , isWBrequest);
}

void RAPPredictor::insertionPolicy(uint64_t set, uint64_t index, bool inNVM, Access element)
{
	
	if(inNVM)
		m_replacementPolicyNVM_ptr->insertionPolicy(set, index , 0);
	else 
		m_replacementPolicySRAM_ptr->insertionPolicy(set, index , 0);

}

void 
RAPPredictor::printStats(std::ostream& out)
{	
	
	ofstream output_file(RAP_OUTPUT_FILE);
	
	output_file << "\tDEAD\tWO\tRO\tRW" << endl;
	for(auto p : stats_ClassErrors)
	{
		output_file << "\t" << p[DEADLINES] << "\t" << p[WOLINES] << "\t" \
			            << p[ROLINES] << "\t" << p[RWLINES] << endl;
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
	
	
	Predictor::printStats(out);
}

int
RAPPredictor::evictPolicy(int set, bool inNVM)
{	
	DPRINTF("RAPPredictor::evictPolicy set %d\n" , set);
		
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
	
	
	RAPEntry* rap_current = lookup(current->m_pc);
	//We didn't create an entry for this dataset, probably a good reason =) (instruction mainly) 
	if(rap_current != NULL)
	{
		int increment = 1;
		if(rap_current->des == BYPASS_CACHE)
			increment = RAP_SATURATION_TH;
		
		
		if(current->nbWrite == 0 && current->nbRead == 0)
		{
			rap_current->cpts[DEADLINES]+= increment;
			if(rap_current->des != BYPASS_CACHE)
				stats_ClassErrors[stats_ClassErrors.size()-1][DEADLINES]++;
		}
		else if(current->nbWrite > 0 && current->nbRead == 0)
		{
			rap_current->cpts[WOLINES]+= increment;
			if(rap_current->des != ALLOCATE_IN_SRAM)
				stats_ClassErrors[stats_ClassErrors.size()-1][WOLINES]++;
		}
		else if(current->nbWrite == 0 && current->nbRead > 0)
		{
			rap_current->cpts[ROLINES]+= increment;
			if(rap_current->des != ALLOCATE_IN_NVM)
				stats_ClassErrors[stats_ClassErrors.size()-1][ROLINES]++;
		}
		else{
			rap_current->cpts[RWLINES]+= increment;		
			if(rap_current->des != ALLOCATE_IN_SRAM)
				stats_ClassErrors[stats_ClassErrors.size()-1][RWLINES]++;
		}
		
		
		if(rap_current->cpts[DEADLINES] == RAP_SATURATION_TH || rap_current->cpts[RWLINES] == RAP_SATURATION_TH || \
			rap_current->cpts[WOLINES] == RAP_SATURATION_TH || rap_current->cpts[ROLINES] == RAP_SATURATION_TH)
		{
			DPRINTF("RAPPredictor:: Saturation of a counter, Update of the decision replacement PC: %ld \n" , rap_current->m_pc);
			allocDecision old = rap_current->des; 
			updateDecision(rap_current);
			if(rap_current->des != old )
			{
			
				stats_switchDecision[stats_switchDecision.size()-1][old][rap_current->des]++;
				DPRINTF("RAPPredictor:: Old Decision : %s \n" , allocDecision_str[rap_current->des]);
				DPRINTF("RAPPredictor:: New Decision : %s \n" , allocDecision_str[rap_current->des]);
			
				
			}	
		}	
	}

	evictRecording(set , assoc_victim , inNVM);	

	return assoc_victim;
}

void 
RAPPredictor::printConfig(std::ostream& out)
{
	out << "\t\tRAP Table Parameters" << endl;
	out << "\t\t\t- Assoc : " << m_RAP_assoc << endl;
	out << "\t\t\t- NB Sets : " << m_RAP_sets << endl;
	out << "\t\t Learning Threshold : " << RAP_LEARNING_THRESHOLD << endl;
	out << "\t\t Saturation Threshold : " << RAP_SATURATION_TH << endl;
}
		

void 
RAPPredictor::openNewTimeFrame()
{ 
	stats_ClassErrors.push_back(vector<int>(4,0));
	stats_switchDecision.push_back(vector<vector<int> >(4,vector<int>(4,0)));

	Predictor::openNewTimeFrame();
}


RAPEntry*
RAPPredictor::lookup(uint64_t pc)
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
RAPPredictor::indexFunction(uint64_t pc)
{
	return pc % m_RAP_assoc;
}

/************* Replacement Policy implementation for the RAP table ********/ 

RAPLRUPolicy::RAPLRUPolicy(int nbAssoc , int nbSet , std::vector<std::vector<RAPEntry*> > rap_entries) :\
		RAPReplacementPolicy(nbAssoc , nbSet, rap_entries) 
{
	m_cpt = 1;
}

void
RAPLRUPolicy::updatePolicy(uint64_t set, uint64_t assoc)
{	
	m_rap_entries[set][assoc]->policyInfo = m_cpt;
	m_cpt++;

}

int
RAPLRUPolicy::evictPolicy(int set)
{
	int smallest_time = m_rap_entries[set][0]->policyInfo , smallest_index = 0;
	
	for(unsigned i = 0 ; i < m_assoc ; i++){
		if(m_rap_entries[set][i]->policyInfo < smallest_time){
			smallest_time =  m_rap_entries[set][i]->policyInfo;
			smallest_index = i;
		}
	}
	return smallest_index;
}



void
updateDecision(RAPEntry* entry)
{
	if(entry->cpts[ROLINES] == RAP_SATURATION_TH)
		entry->des = ALLOCATE_IN_NVM;
	else if(entry->cpts[WOLINES] == RAP_SATURATION_TH)
		entry->des = ALLOCATE_IN_SRAM;
	else if(entry->cpts[DEADLINES] == RAP_SATURATION_TH)
		entry->des = BYPASS_CACHE;
	else{//RW lines
		entry->des = ALLOCATE_IN_SRAM;
	}
	
	entry->cpts = vector<int>(4,0);
}



