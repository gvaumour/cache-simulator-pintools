#include "Predictor.hh"
#include "ReplacementPolicy.hh"
#include "Cache.hh"

using namespace std;

Predictor::Predictor(): m_tableSRAM(0), m_tableNVM(0), m_nb_set(0), m_assoc(0), m_nbNVMways(0), m_nbSRAMways(0), m_cache(NULL)
{
	m_replacementPolicyNVM_ptr = new LRUPolicy();
	m_replacementPolicySRAM_ptr = new LRUPolicy();	
}


Predictor::Predictor(int nbAssoc , int nbSet, int nbNVMways, DataArray SRAMtable , DataArray NVMtable, HybridCache* cache) 
{
	m_tableSRAM = SRAMtable;
	m_tableNVM = NVMtable;
	m_nb_set = nbSet;
	m_assoc = nbAssoc;
	m_nbNVMways = nbNVMways;
	m_nbSRAMways = nbAssoc - nbNVMways;
	
	m_replacementPolicyNVM_ptr = new LRUPolicy(m_nbNVMways , m_nb_set , NVMtable);
	m_replacementPolicySRAM_ptr = new LRUPolicy(m_nbSRAMways , m_nb_set, SRAMtable);	
	
	m_cache = cache;
}
				 



LRUPredictor::LRUPredictor(int nbAssoc , int nbSet, int nbNVMways, DataArray SRAMtable, DataArray NVMtable, HybridCache* cache)\
 : Predictor(nbAssoc, nbSet, nbNVMways, SRAMtable, NVMtable, cache)
{
	m_cpt = 1;
}


bool LRUPredictor::allocateInNVM(uint64_t set, Access element)
{
	return false;// Always allocate on SRAM 
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


bool
PreemptivePredictor::allocateInNVM(uint64_t set, Access element)
{
	return !element.isWrite();
}



/********************************************/ 



