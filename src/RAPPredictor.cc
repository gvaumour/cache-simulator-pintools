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
	
	/* Create/Reset  the dataset file */
	ofstream dataset_file(RAP_OUTPUT_FILE_DATASETS);
	dataset_file.close();

	m_RAPtable.clear();

	m_RAP_assoc = RAP_TABLE_ASSOC;
	m_RAP_sets = RAP_TABLE_SET;

	m_RAPtable.resize(m_RAP_sets);
	
	for(unsigned i = 0 ; i < m_RAP_sets ; i++)
	{
		m_RAPtable[i].resize(m_RAP_assoc);
		
		
		for(unsigned j = 0 ; j < m_RAP_assoc ; j++)
		{
			m_RAPtable[i][j] = new RAPEntry();
			m_RAPtable[i][j]->index = i;
			m_RAPtable[i][j]->assoc = j;
		}
	}
	
	DPRINTF("RAPPredictor::Constructor m_RAPtable.size() = %d , m_RAPtable[0].size() = %d\n" , m_RAPtable.size() , m_RAPtable[0].size());

	m_rap_policy = new RAPLRUPolicy(m_RAP_assoc , m_RAP_sets , m_RAPtable);

	/* Stats init*/ 
	stats_nbSwitchDst.clear();
	stats_ClassErrors.clear();
	stats_ClassErrors.push_back(vector<int>(4,0));	
	
	stats_switchDecision.clear();
	stats_switchDecision.push_back(vector<vector<int>>(3 , vector<int>(3,0)));
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
		

		int index = indexFunction(element.m_pc);
		int assoc = m_rap_policy->evictPolicy(index);
		DPRINTF("RAPPredictor::allocateInNVM Eviction and Installation in the RAP table 0x%lx, index %d assoc %d\n" , element.m_pc , index , assoc);
		
		current = m_RAPtable[index][assoc];
		
		
		if(current->isValid)
			dumpDataset(current);

		if(current->nbUpdate > 0)
			stats_nbSwitchDst.push_back((double)current->nbSwitch / (double)current->nbUpdate);
	
		current->initEntry(element);
		current->m_pc = element.m_pc;
	}
	else
	{	
		stats_RAP_hits++;
		current->size++;
	}	

	if(element.isSRAMerror)
		current->sramErrors++;

	m_rap_policy->updatePolicy(current->index , current->assoc);

	if(current->des == BYPASS_CACHE)
	{
		current->cptLearning++;
		if(current->cptLearning ==  RAP_LEARNING_THRESHOLD){
			current->cptLearning = 0;
			return ALLOCATE_IN_NVM;		
		}
	}
	
	return current->des;
}


void 
RAPPredictor::finishSimu()
{
	DPRINTF("RAPPredictor::FINISH SIMU\n");

	
	for(auto lines : m_RAPtable)
	{
		for(auto entry : lines)
		{		
			if(entry->isValid)
			{
				entry->history.push_back(pair<RW_TYPE,int>(entry->write_state , entry->accessPerPhase));
				dumpDataset(entry);
			}
				
			DPRINTF("NB Access of the evicted line\t%d\n" , entry->nbAccess); 
		}
	}
}

int
RAPPredictor::computeRd(uint64_t set, uint64_t  index , bool inNVM)
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
RAPPredictor::updatePolicy(uint64_t set, uint64_t index, bool inNVM, Access element, bool isWBrequest = false)
{
	CacheEntry* current = NULL;
	if(inNVM)
		current = m_tableNVM[set][index];
	else 
		current = m_tableSRAM[set][index];
	
	int rd = computeRd(set, index , inNVM);

	current->policyInfo = m_cpt;
	RAPEntry* rap_current = lookup(current->m_pc);
	if(rap_current)
	{
		DPRINTF("RAPPredictor::updatePolicy Reuse distance of the pc %lx, RD = %d\n", current->m_pc , rd );
		rap_current->reuse_distances.push_back(rd);
	}
	
	m_cpt++;
	Predictor::updatePolicy(set , index , inNVM , element , isWBrequest);
}

void RAPPredictor::insertionPolicy(uint64_t set, uint64_t index, bool inNVM, Access element)
{
	CacheEntry* current = NULL;
	if(inNVM)
		current = m_tableNVM[set][index];
	else 
		current = m_tableSRAM[set][index];
		
	current->policyInfo = m_cpt;

	/*RAPEntry* rap_current = lookup(current->m_pc);
	if(rap_current)
	{
		//On the first access the rd is infinite 
		rap_current->reuse_distances.push_back(RD_INFINITE);
	}*/
	
	m_cpt++;
}
/*
void 
RAPPredictor::dumpRAPtable(RAPEntry* entry)
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
RAPPredictor::printStats(std::ostream& out)
{	

	/*
	ofstream output_file(RAP_OUTPUT_FILE);
	
	output_file << "\tDEAD\tWO\tRO\tRW" << endl;
	for(auto p : stats_ClassErrors)
	{
		output_file << "\t" << p[DEADLINES] << "\t" << p[WOLINES] << "\t" \
			            << p[ROLINES] << "\t" << p[RWLINES] << endl;
	}
	output_file.close();
	*/

	ofstream output_file1(RAP_OUTPUT_FILE1);
	

	output_file1 << "\tSRAM->SRAM\tSRAM->NVM\tSRAM->BYPASS\tNVM->SRAM\tNVM->NVM\tNVM->BYPASS";
	output_file1 << "\tBYPASS->SRAM\tBYPASS->NVM\tBYPASS->BYPASS";
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
RAPPredictor::evictPolicy(int set, bool inNVM)
{	
	DPRINTF("RAPPredictor::evictPolicy set %d\n" , set);
		
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
		
	RAPEntry* rap_current = lookup(current->m_pc);
	//We didn't create an entry for this dataset, probably a good reason =) (instruction mainly) 
	if(rap_current != NULL)
	{
		rap_current->nbUpdate++;
		/*
		if(current->nbWrite == 0 && current->nbRead == 0)
			rap_current->reuse_distances.push_back(RD_INFINITE);

		// Write Status of the cache line 
		if(current->nbWrite == 0 && current->nbRead == 0)
		{
			rap_current->cpts[DEADLINES]++;
			if(rap_current->des != BYPASS_CACHE)
				stats_ClassErrors[stats_ClassErrors.size()-1][DEADLINES]++;
		}
		else if(current->nbWrite > 0 && current->nbRead == 0)
		{
			rap_current->cpts[WOLINES]++;
			if(rap_current->des != ALLOCATE_IN_SRAM)
				stats_ClassErrors[stats_ClassErrors.size()-1][WOLINES]++;
		}
		else if(current->nbWrite == 0 && current->nbRead > 0)
		{
			rap_current->cpts[ROLINES]++;
			if(rap_current->des != ALLOCATE_IN_NVM)
				stats_ClassErrors[stats_ClassErrors.size()-1][ROLINES]++;
		}
		else{
			rap_current->cpts[RWLINES]++;		
			if(rap_current->des != ALLOCATE_IN_SRAM)
				stats_ClassErrors[stats_ClassErrors.size()-1][RWLINES]++;
		}
		
		if(rap_current->cpts[DEADLINES] == RAP_BYPASS_SATURATION_TH || rap_current->cpts[RWLINES] == RAP_SATURATION_TH || \
			rap_current->cpts[WOLINES] == RAP_SATURATION_TH || rap_current->cpts[ROLINES] == RAP_SATURATION_TH)
		{
			if(rap_current->cpts[DEADLINES] == RAP_BYPASS_SATURATION_TH && rap_current->write_state != RW_DEAD)
			{
				rap_current->history.push_back(pair<RW_TYPE,int>(rap_current->write_state , rap_current->accessPerPhase));
				rap_current->write_state = RW_DEAD;
				rap_current->accessPerPhase = 0;
			}
			else if(rap_current->cpts[RWLINES] == RAP_SATURATION_TH && rap_current->write_state != RW_RW)
			{
				rap_current->history.push_back(pair<RW_TYPE,int>(rap_current->write_state , rap_current->accessPerPhase));
				rap_current->write_state = RW_RW;
				rap_current->accessPerPhase = 0;
			}
			else if(rap_current->cpts[ROLINES] == RAP_SATURATION_TH && rap_current->write_state != RW_RO)
			{
				rap_current->history.push_back(pair<RW_TYPE,int>(rap_current->write_state , rap_current->accessPerPhase));
				rap_current->write_state = RW_RO;
				rap_current->accessPerPhase = 0;
			}
			else if(rap_current->cpts[WOLINES] == RAP_SATURATION_TH && rap_current->write_state != RW_WO)
			{
				rap_current->history.push_back(pair<RW_TYPE,int>(rap_current->write_state , rap_current->accessPerPhase));
				rap_current->write_state = RW_WO;
				rap_current->accessPerPhase = 0;
			}
			else
				rap_current->accessPerPhase++;
	

			allocDecision old = rap_current->des; 
			mappingStateAllocDes(rap_current);
			if(rap_current->des != old )
			{
				stats_switchDecision[stats_switchDecision.size()-1][old][rap_current->des]++;
				rap_current->nbSwitch++;

				//printf("RAPPredictor:: Saturation of a counter, Update of the decision replacement PC: %ld \n" , rap_current->m_pc);
				//printf("RAPPredictor:: Old Decision : %s \n" , allocDecision_str[old]);
				//printf("RAPPredictor:: New Decision : %s \n" , allocDecision_str[rap_current->des]);
			}
		}
		else
		{
			rap_current->accessPerPhase++;
		}*/

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
	out << "\t\t Bypass Saturation Threshold : " << RAP_BYPASS_SATURATION_TH << endl;
	out << "\t\t RD Eval strategy : " << RD_EVAL << endl;

}
		

void 
RAPPredictor::openNewTimeFrame()
{ 
	stats_ClassErrors.push_back(vector<int>(4,0));
	stats_switchDecision.push_back(vector<vector<int> >(3,vector<int>(3,0)));

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
	return pc % m_RAP_sets;
}



void
RAPPredictor::mappingStateAllocDes(RAPEntry* entry)
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

	if(entry->cpts[RO] == RAP_SATURATION_TH)
	{
		entry->des = ALLOCATE_IN_NVM;
	
	}
	else if(entry->cpts[WO] == RAP_SATURATION_TH || entry->cpts[RW] == RAP_SATURATION_TH)
	{
		RD_TYPE rd = RD_NOT_ACCURATE;
		if( string(RD_EVAL) == "complex")
			rd = complexRdEvaluation(entry->reuse_distances);
		else if(string(RD_EVAL) == "simple")
			rd = simpleRdEvaluation(entry->reuse_distances);
		
		
		if( entry->des == ALLOCATE_IN_SRAM)
		{
			if(rd == RD_MEDIUM)
				entry->des = ALLOCATE_IN_NVM;
			else
				entry->des = ALLOCATE_IN_SRAM;
		}
		else if( entry->des == ALLOCATE_IN_NVM)
		{
			//If we don't know rd, better go to SRAM for WO and RW datasets
			if(rd == RD_SHORT || rd == RD_NOT_ACCURATE) 
				entry->des =ALLOCATE_IN_SRAM;
			else 
				entry->des =ALLOCATE_IN_NVM;
		}
		else 
		{
			//Learning Cache line for bypassed dataset
			//Allocated in NVM so we can use rd info
			rd = convertRD(entry->reuse_distances.back());
			
			if(rd == RD_SHORT || rd == RD_NOT_ACCURATE)
				entry->des = ALLOCATE_IN_SRAM;
			else if(rd == RD_MEDIUM)
				entry->des = ALLOCATE_IN_NVM;
			else
				entry->des = BYPASS_CACHE;
		}
	}
	else{//No Reuse 
		entry->des = BYPASS_CACHE;
	}

	entry->cpts = vector<int>(4,0);
}


RD_TYPE
RAPPredictor::simpleRdEvaluation(vector<int> reuse_distances)
{
	if(reuse_distances.size() == 0)
		return RD_NOT_ACCURATE;
	
	return convertRD(reuse_distances.back());
}

RD_TYPE
RAPPredictor::complexRdEvaluation(vector<int> reuse_distances)
{

	if(reuse_distances.size() == 0)
		return RD_NOT_ACCURATE;
	
	int index = reuse_distances.size()-5; 
	if(index < 0)
		index = 0;			
		
	vector<int> dummy = vector<int>(NUM_RD_TYPE , 0);

	for(int i = reuse_distances.size()-1; i > index ; i--)
		dummy[convertRD(reuse_distances[i])]++;

	if(dummy[RD_SHORT] > 2)
		return RD_SHORT;
	else if(dummy[RD_MEDIUM] > 2)
		return RD_MEDIUM;
	else
	{
		return RD_NOT_ACCURATE;
	}	
}


RD_TYPE 
RAPPredictor::convertRD(int rd)
{
	if(rd < RAP_SRAM_ASSOC)
		return RD_SHORT;
	else if(rd < RAP_NVM_ASSOC)
		return RD_MEDIUM;
	else 
		return RD_NOT_ACCURATE;
}


static const char* str_RW_TYPE[] = {"DEAD" , "RO", "WO" , "RW", "UNKOWN"};

void
RAPPredictor::dumpDataset(RAPEntry* entry)
{
	if(entry->history.size() < 10)
		return;

	ofstream dataset_file(RAP_OUTPUT_FILE_DATASETS, std::ios::app);
	dataset_file << "Dataset\t0x" << std::hex << entry->m_pc << std::dec << endl;
	dataset_file << "\tSize\t" << entry->size <<endl;
	dataset_file << "\tNB Update\t" << entry->nbUpdate <<endl;
	dataset_file << "\tNB Access\t" << entry->nbSwitch <<endl;
	dataset_file << "\tNB SRAM error\t" << entry->sramErrors << endl;
	dataset_file << "\tHistory" << endl;
	for(auto p : entry->history)
		dataset_file << "\t\t" << str_RW_TYPE[p.first] << "\t" << p.second << endl;
	dataset_file.close();
}

/************* Replacement Policy implementation for the RAP table ********/ 

RAPLRUPolicy::RAPLRUPolicy(int nbAssoc , int nbSet , std::vector<std::vector<RAPEntry*> >& rap_entries) :\
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





