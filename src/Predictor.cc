#include "Predictor.hh"
#include "Cache.hh"


bool
PreemptivePredictor::allocateInNVM(uint64_t set, Access element)
{
	return !element.isWrite();
}

void
PreemptivePredictor::updatePolicy(uint64_t set, uint64_t index, bool inNVM, Access element)
{

}

void 
PreemptivePredictor::insertionPolicy(uint64_t set, uint64_t index, bool inNVM, Access element)
{

}


int
PreemptivePredictor::evictPolicy(int set, bool inNVM)
{
	return 0;
}


LRUPredictor::LRUPredictor(int nbAssoc , int nbSet, int nbNVMways, std::vector<std::vector<CacheEntry*> > SRAMtable, std::vector<std::vector<CacheEntry*> > NVMtable)\
 : Predictor(nbAssoc, nbSet, nbNVMways, SRAMtable, NVMtable)
{
	m_cpt = 1;
}


bool LRUPredictor::allocateInNVM(uint64_t set, Access element)
{
	return !element.isWrite();
}

void
LRUPredictor::updatePolicy(uint64_t set, uint64_t index, bool inNVM, Access element)
{
	if(inNVM)
		m_tableNVM[set][index]->policyInfo = m_cpt;
	else
		m_tableSRAM[set][index]->policyInfo = m_cpt;
	
	m_cpt++;	
}

void
LRUPredictor::insertionPolicy(uint64_t set, uint64_t index, bool inNVM, Access element)
{	
	if(inNVM)
		m_tableNVM[set][index]->policyInfo = m_cpt;
	else
		m_tableSRAM[set][index]->policyInfo = m_cpt;
	
	m_cpt++;
}

int
LRUPredictor::evictPolicy(int set, bool inNVM)
{

	if(inNVM){

		int smallest_time = m_tableNVM[set][0]->policyInfo , smallest_index = 0;

		for(int i = 0 ; i < m_assoc ; i++){
			if(m_tableNVM[set][i]->policyInfo < smallest_time){
				smallest_time =  m_tableNVM[set][i]->policyInfo;
				smallest_index = i;
			}
		}
		return smallest_index;
	}
	else{
		int smallest_time = m_tableSRAM[set][0]->policyInfo , smallest_index = 0;

		for(int i = 0 ; i < m_assoc ; i++){
			if(m_tableSRAM[set][i]->policyInfo < smallest_time){
				smallest_time =  m_tableSRAM[set][i]->policyInfo;
				smallest_index = i;
			}
		}
		return smallest_index;

	}
}
