#include <fstream>
#include <vector>
#include <map>
#include <ostream>

#include "RAPPredictor_opt.hh"

using namespace std;


RAPPredictor_opt::RAPPredictor_opt(int nbAssoc , int nbSet, int nbNVMways, DataArray& SRAMtable, DataArray& NVMtable, HybridCache* cache) : \
	RAPPredictor(nbAssoc, nbSet, nbNVMways, SRAMtable, NVMtable, cache) 
{

}	

RAPPredictor_opt::~RAPPredictor_opt()
{
	~RAPPredictor();
}


allocDecision
RAPPredictor_opt::allocateInNVM(uint64_t set, Access element)
{	

}

int
RAPPredictor_opt::evictPolicy(int set, bool inNVM)
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
			/* Update of the rap_current->write_state of the dataset entry */ 
			if(rap_current->cpts[DEADLINES] == RAP_BYPASS_SATURATION_TH && rap_current->write_state != RW_DEAD)
			{
				rap_current->history.push_back(pair<RW_status,int>(rap_current->write_state , rap_current->accessPerPhase));
				rap_current->write_state = RW_DEAD;
				rap_current->accessPerPhase = 0;
			}
			else if(rap_current->cpts[RWLINES] == RAP_SATURATION_TH && rap_current->write_state != RW_RW)
			{
				rap_current->history.push_back(pair<RW_status,int>(rap_current->write_state , rap_current->accessPerPhase));
				rap_current->write_state = RW_RW;
				rap_current->accessPerPhase = 0;
			}
			else if(rap_current->cpts[ROLINES] == RAP_SATURATION_TH && rap_current->write_state != RW_RO)
			{
				rap_current->history.push_back(pair<RW_status,int>(rap_current->write_state , rap_current->accessPerPhase));
				rap_current->write_state = RW_RO;
				rap_current->accessPerPhase = 0;
			}
			else if(rap_current->cpts[WOLINES] == RAP_SATURATION_TH && rap_current->write_state != RW_WO)
			{
				rap_current->history.push_back(pair<RW_status,int>(rap_current->write_state , rap_current->accessPerPhase));
				rap_current->write_state = RW_WO;
				rap_current->accessPerPhase = 0;
			}
			else
				rap_current->accessPerPhase++;
	

			allocDecision old = rap_current->des; 
			updateDecision(rap_current);
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
		}

	}
}
