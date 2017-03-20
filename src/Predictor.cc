#include "Predictor.hh"
#include "ReplacementPolicy.hh"
#include "Cache.hh"


Predictor::Predictor(): m_tableSRAM(0), m_tableNVM(0), m_nb_set(0), m_assoc(0), m_nbNVMways(0), m_nbSRAMways(0)
{
	m_replacementPolicyNVM_ptr = new LRUPolicy();
	m_replacementPolicySRAM_ptr = new LRUPolicy();	
}


Predictor::Predictor(int nbAssoc , int nbSet, int nbNVMways, std::vector<std::vector<CacheEntry*> > SRAMtable , std::vector<std::vector<CacheEntry*> > NVMtable) 
{
	m_tableSRAM = SRAMtable;
	m_tableNVM = NVMtable;
	m_nb_set = nbSet;
	m_assoc = nbAssoc;
	m_nbNVMways = nbNVMways;
	m_nbSRAMways = nbAssoc - nbNVMways;
	
	m_replacementPolicyNVM_ptr = new LRUPolicy(m_nbNVMways , m_nb_set , NVMtable);
	m_replacementPolicySRAM_ptr = new LRUPolicy(m_nbSRAMways , m_nb_set, SRAMtable);	
}
				 

bool
PreemptivePredictor::allocateInNVM(uint64_t set, Access element)
{
	return !element.isWrite();
}

void
PreemptivePredictor::updatePolicy(uint64_t set, uint64_t index, bool inNVM, Access element)
{
	if(inNVM)
		m_replacementPolicyNVM_ptr->updatePolicy(set, index , 0);
	else		
		m_replacementPolicySRAM_ptr->updatePolicy(set, index , 0);
}

void 
PreemptivePredictor::insertionPolicy(uint64_t set, uint64_t index, bool inNVM, Access element)
{
	if(inNVM)
		m_replacementPolicyNVM_ptr->insertionPolicy(set,index,0);
	else
		m_replacementPolicySRAM_ptr->insertionPolicy(set, index,0);
}


int
PreemptivePredictor::evictPolicy(int set, bool inNVM)
{	
	if(inNVM)
		return m_replacementPolicyNVM_ptr->evictPolicy(set);
	else
		return m_replacementPolicySRAM_ptr->evictPolicy(set);
}


LRUPredictor::LRUPredictor(int nbAssoc , int nbSet, int nbNVMways, std::vector<std::vector<CacheEntry*> > SRAMtable, std::vector<std::vector<CacheEntry*> > NVMtable)\
 : Predictor(nbAssoc, nbSet, nbNVMways, SRAMtable, NVMtable)
{
	m_cpt = 1;
}


bool LRUPredictor::allocateInNVM(uint64_t set, Access element)
{
	return false;//!element.isWrite();
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
	if(inNVM)
		return m_replacementPolicyNVM_ptr->evictPolicy(set);
	else
		return m_replacementPolicySRAM_ptr->evictPolicy(set);
}
