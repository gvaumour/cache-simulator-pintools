#include <fstream>
#include <vector>
#include <map>
#include <ostream>

#include "RAPPredictor.hh"

using namespace std;

/** RAPPredictor Implementation ***********/ 

RAPPredictor::RAPPredictor() : Predictor(){
	m_cpt = 1;
	m_RAPtable.clear();
	stats_ClassErrors.clear();
	
}

RAPPredictor::RAPPredictor(int nbAssoc , int nbSet, int nbNVMways, DataArray SRAMtable, DataArray NVMtable, HybridCache* cache) : \
	Predictor(nbAssoc, nbSet, nbNVMways, SRAMtable, NVMtable, cache) {
	m_cpt = 1;

	m_RAPtable.clear();
	stats_ClassErrors.clear();
	stats_ClassErrors.push_back(vector<int>(4,0));
}
		
RAPPredictor::~RAPPredictor()
{
	for(auto p : m_RAPtable)
		delete p.second;
}

allocDecision
RAPPredictor::allocateInNVM(uint64_t set, Access element)
{

	DPRINTF("RAPPredictor::allocateInNVM set %ld\n" , set);
	
	if(element.isInstFetch())
		return ALLOCATE_IN_NVM;
		
		
	if( m_RAPtable.count(element.m_pc) == 0)
	{
		RAPEntry* current = new RAPEntry();
		current->pc = element.m_pc;
		current->des = element.isWrite() ? ALLOCATE_IN_SRAM : ALLOCATE_IN_NVM;
		m_RAPtable.insert(pair<uint64_t, RAPEntry*>(element.m_pc, current));
	}
	
	
	if(learningTHcpt == RAP_LEARNING_THRESHOLD && m_RAPtable[element.m_pc]->des == BYPASS_CACHE)
	{		
		learningTHcpt = 0;
		return ALLOCATE_IN_NVM;
	}
	
	return m_RAPtable[element.m_pc]->des;
}

void
RAPPredictor::updatePolicy(uint64_t set, uint64_t index, bool inNVM, Access element, bool isWBrequest = false)
{

	Predictor::updatePolicy(set, index, inNVM, element , isWBrequest);
}

void RAPPredictor::insertionPolicy(uint64_t set, uint64_t index, bool inNVM, Access element)
{
	
	learningTHcpt++;	
	m_cpt++;
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
	
	//We didn't create for this dataset, probably a good reason =) (instruction mainly) 
	if(m_RAPtable.count(current->m_pc) > 0)
	{
		RAPEntry* entry = m_RAPtable[current->m_pc];
		
		if(current->nbWrite == 0 && current->nbRead == 0)
		{
			entry->cpts[DEADLINES]++;
			if(entry->des != BYPASS_CACHE)
				stats_ClassErrors[stats_ClassErrors.size()-1][DEADLINES]++;
		}
		else if(current->nbWrite > 0 && current->nbRead == 0)
		{
			entry->cpts[WOLINES]++;
			if(entry->des != ALLOCATE_IN_SRAM)
				stats_ClassErrors[stats_ClassErrors.size()-1][WOLINES]++;
		}
		else if(current->nbWrite == 0 && current->nbRead > 0)
		{
			entry->cpts[ROLINES]++;
			if(entry->des != ALLOCATE_IN_NVM)
				stats_ClassErrors[stats_ClassErrors.size()-1][ROLINES]++;
		}
		else{
			entry->cpts[RWLINES]++;		
			if(entry->des != ALLOCATE_IN_SRAM)
				stats_ClassErrors[stats_ClassErrors.size()-1][RWLINES]++;
		}
		

		
		
		if(entry->cpts[DEADLINES] == SATURATION_TH || entry->cpts[RWLINES] == SATURATION_TH || \
			entry->cpts[WOLINES] == SATURATION_TH || entry->cpts[ROLINES] == SATURATION_TH)
		{
			updateDecision(entry);	
		}
			
	}

	evictRecording(set , assoc_victim , inNVM);	

	return assoc_victim;
}


void 
RAPPredictor::openNewTimeFrame()
{ 
	stats_ClassErrors.push_back(vector<int>(4,0));
	Predictor::openNewTimeFrame();
}

void
updateDecision(RAPEntry* entry)
{
	if(entry->cpts[ROLINES] == SATURATION_TH)
		entry->des = ALLOCATE_IN_NVM;
	else if(entry->cpts[WOLINES] == SATURATION_TH)
		entry->des = ALLOCATE_IN_SRAM;
	else if(entry->cpts[DEADLINES] == SATURATION_TH)
		entry->des = BYPASS_CACHE;
	else{
		entry->des = ALLOCATE_IN_SRAM;
	}
	
	entry->cpts = vector<int>(4,0);
}
