#include <fstream>
#include <vector>
#include <map>
#include <ostream>

#include "InstructionPredictor.hh"

using namespace std;


#define WRITE_VALUE 3
#define READ_VALUE -1

InstructionPredictor::InstructionPredictor(int nbAssoc , int nbSet, int nbNVMways, DataArray& SRAMtable, DataArray& NVMtable, HybridCache* cache) : \
	Predictor(nbAssoc, nbSet, nbNVMways, SRAMtable, NVMtable, cache) {
	m_cpt = 1;
}
		
InstructionPredictor::~InstructionPredictor()
{ }

allocDecision
InstructionPredictor::allocateInNVM(uint64_t set, Access element)
{
	//DPRINTF("InstructionPredictor::allocateInNVM\n");
	
	if(element.isInstFetch())
		return ALLOCATE_IN_NVM;

	if(pc_counters.count(element.m_pc) == 0){
		//New PC to track		
		pc_counters.insert(pair<uint64_t,int>(element.m_pc, 1));	

		if(!m_isWarmup)
			stats_PCwrites.insert(pair<uint64_t,pair<int,int> >(element.m_pc, pair<int,int>(0,0) ));
		
		map<uint64_t, pair<int,int> > my_tab = map<uint64_t, pair<int,int> >();
	}

	if(pc_counters[element.m_pc] < 2)
		return ALLOCATE_IN_NVM; 
	else
		return ALLOCATE_IN_SRAM;

}

void
InstructionPredictor::updatePolicy(uint64_t set, uint64_t index, bool inNVM, Access element, bool isWBrequest = false)
{
	//DPRINTF("InstructionPredictor::updatePolicy\n");
	
	CacheEntry* current;

	if(inNVM){
		current = m_tableNVM[set][index];
		m_replacementPolicyNVM_ptr->updatePolicy(set, index , 0);
	}		
	else{
		current = m_tableSRAM[set][index];
		m_replacementPolicySRAM_ptr->updatePolicy(set, index , 0);
	}
		
	current->policyInfo = m_cpt;
	if(element.isWrite()){
		current->cost_value+= WRITE_VALUE;
	}
	else{ 
		current->cost_value += READ_VALUE;	
	} 	
	Predictor::updatePolicy(set, index, inNVM, element , isWBrequest);

	m_cpt++;
	//DPRINTF("InstructionPredictor:: End updatePolicy\n");
}

void InstructionPredictor::insertionPolicy(uint64_t set, uint64_t index, bool inNVM, Access element)
{
	//DPRINTF("InstructionPredictor::insertionPolicy\n");
	
	CacheEntry* current;
	if(inNVM){
		current = m_tableNVM[set][index];
	}		
	else{
		current = m_tableSRAM[set][index];
	}
	
	current->policyInfo = m_cpt;

	if(element.isWrite())
		current->cost_value = WRITE_VALUE;
	else
		current->cost_value = READ_VALUE;
	
	Predictor::insertionPolicy(set , index , inNVM , element);
	m_cpt++;
}

void
InstructionPredictor::printConfig(std::ostream& out, string entete){
	out << entete << ":InstructionPredictor" << std::endl;
	out << entete << ":READ_VALUE\t" << READ_VALUE << std::endl;		
	out << entete << ":WRITE_VALUE\t" << WRITE_VALUE << std::endl;		
	out << entete << ":CostThreshold\t" << simu_parameters.cost_threshold << std::endl;		
	out << entete << ":PCThreshold\t" << PC_THRESHOLD << endl;
	Predictor::printConfig(out, entete);
};


void 
InstructionPredictor::printStats(std::ostream& out, string entete)
{	
	int totalWrite = 0;
	for(auto p : stats_PCwrites)
	{
		pair<int,int> current = p.second;
		totalWrite += current.first;
	}
	
	out << entete << ":InstructionPredictor Parameters:" << endl;
	out << entete << ":Total Write\t" << totalWrite << endl;
	
	/*
	ofstream output_file;
	output_file.open(OUTPUT_PREDICTOR_FILE , std::ofstream::app);
	if(totalWrite > 0){
		output_file << "Write Intensity Per Instruction "<< endl;
		for(auto p : stats_PCwrites)
		{
			pair<int,int> current = p.second;
			output_file << p.first << "\t" << current.first << "\t" << current.second << endl;
		}
	}
	output_file.close();
	*/
	Predictor::printStats(out,entete);
}

int
InstructionPredictor::evictPolicy(int set, bool inNVM)
{	
	//DPRINTF("InstructionPredictor::evictPolicy set %d\n" , set);
	int assoc_victim = -1;
	assert(m_replacementPolicyNVM_ptr != NULL);
	assert(m_replacementPolicySRAM_ptr != NULL);
	

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

	if(current->isValid){
		//DPRINTF("InstructionPredictor::evictPolicy is Valid\n");
		if( current->cost_value >= simu_parameters.cost_threshold)
		{
			pc_counters[current->m_pc]++;
			if(pc_counters[current->m_pc] == 3)
				pc_counters[current->m_pc] = 3;
		
		}
		else
		{
			pc_counters[current->m_pc]--;
			if(pc_counters[current->m_pc] < 0)
				pc_counters[current->m_pc] = 0;
		}	
	}
	
	evictRecording(set , assoc_victim , inNVM);
	
	return assoc_victim;
}

