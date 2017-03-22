#include <vector>
#include <ostream>

#include "InstructionPredictor.hh"

using namespace std;

/** InstructionPredictor Implementation ***********/ 

InstructionPredictor::InstructionPredictor() : Predictor(){
	m_cpt = 1;
	pc_counters.clear();
}

InstructionPredictor::InstructionPredictor(int nbAssoc , int nbSet, int nbNVMways, DataArray SRAMtable, DataArray NVMtable, HybridCache* cache) : \
	Predictor(nbAssoc, nbSet, nbNVMways, SRAMtable, NVMtable, cache) {
	m_cpt = 1;
	pc_counters.clear();
}
		
InstructionPredictor::~InstructionPredictor()
{ }

bool
InstructionPredictor::allocateInNVM(uint64_t set, Access element)
{
	if(pc_counters.count(element.m_pc) == 0){
		//New PC to track
		pc_counters.insert(pair<uint64_t,int>(element.m_pc, 1));	
		stats_PCwrites.insert(pair<uint64_t,int>(element.m_pc, 0));
	}


	if(element.isInstFetch())
		return true;
	else if(element.isWrite())
		return false;
	else 
		return pc_counters[element.m_pc] < 2;
		//Test if the instruction is cold or not

}

void
InstructionPredictor::updatePolicy(uint64_t set, uint64_t index, bool inNVM, Access element)
{
	DPRINTF("InstructionPredictor::updatePolicy\n");
	

	CacheEntry* current;;

	if(inNVM)
		current = m_tableNVM[set][index];
		
	else 
		current = m_tableSRAM[set][index];

	current->policyInfo = m_cpt;
	if(element.isWrite()){
		if(current->saturation_counter < 2)
		{
			current->saturation_counter++;
			if(current->saturation_counter == 2)
			{
				//If the counter now saturates, update the inst info
				pc_counters[current->m_pc]++;
				if(pc_counters[current->m_pc] > 3)
					pc_counters[current->m_pc] = 3;
				
			}
						
		}
		//We do nothing if the counter is already saturated			
		stats_PCwrites[current->m_pc]++;
	}

	m_cpt++;
}

void InstructionPredictor::insertionPolicy(uint64_t set, uint64_t index, bool inNVM, Access element)
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

void 
InstructionPredictor::printStats(std::ostream& out)
{	
	int totalWrite = 0;
	for(auto p : stats_PCwrites)
	{
		totalWrite += p.second;
	}
	out << "InstructionPredictor Results:" << endl;
	out << "Total Write :" << totalWrite << endl;
}

int
InstructionPredictor::evictPolicy(int set, bool inNVM)
{	
	int assoc_victim;
	if(inNVM)
		assoc_victim = m_replacementPolicyNVM_ptr->evictPolicy(set);
	else
		assoc_victim = m_replacementPolicySRAM_ptr->evictPolicy(set);

	/* Update the status of the instruction */ 
	CacheEntry* current;
	if(inNVM)
		current = m_tableNVM[set][assoc_victim];
	else
		current = m_tableSRAM[set][assoc_victim];

	if(current->saturation_counter < 2)
	{
		pc_counters[current->m_pc]--;
		if(pc_counters[current->m_pc] < 0)
			pc_counters[current->m_pc] = 0;
	}
	
	return assoc_victim;
}

