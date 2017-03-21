#include "Predictor.hh"
#include "ReplacementPolicy.hh"
#include "Cache.hh"

using namespace std;

Predictor::Predictor(): m_tableSRAM(0), m_tableNVM(0), m_nb_set(0), m_assoc(0), m_nbNVMways(0), m_nbSRAMways(0)
{
	m_replacementPolicyNVM_ptr = new LRUPolicy();
	m_replacementPolicySRAM_ptr = new LRUPolicy();	
}


Predictor::Predictor(int nbAssoc , int nbSet, int nbNVMways, DataArray SRAMtable , DataArray NVMtable) 
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
				 



LRUPredictor::LRUPredictor(int nbAssoc , int nbSet, int nbNVMways, DataArray SRAMtable, DataArray NVMtable)\
 : Predictor(nbAssoc, nbSet, nbNVMways, SRAMtable, NVMtable)
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


/** SaturationCounter Implementation ***********/ 

SaturationCounter::SaturationCounter() : Predictor(){
	m_cpt = 0;
	std::vector<int> stats_nbMigrationsFromNVM = vector<int>(2,0);
}

SaturationCounter::SaturationCounter(int nbAssoc , int nbSet, int nbNVMways, DataArray SRAMtable, DataArray NVMtable) : \
	Predictor(nbAssoc, nbSet, nbNVMways, SRAMtable, NVMtable) {
	m_cpt = 0;
	std::vector<int> stats_nbMigrationsFromNVM = vector<int>(2,0);
}
		
SaturationCounter::~SaturationCounter() { }

bool SaturationCounter::allocateInNVM(uint64_t set, Access element)
{
	return !element.isWrite();
}

void SaturationCounter::updatePolicy(uint64_t set, uint64_t index, bool inNVM, Access element)
{

	DPRINTF("SaturationCounter::updatePolicy\n");
	if(inNVM){
		CacheEntry* current = m_tableNVM[set][index];
		current->policyInfo = m_cpt;
		
		if(element.isWrite()){
			current->saturation_counter--;
			if(current->saturation_counter == 1)
			{
				//Trigger Migration
				//Select LRU candidate from SRAM cache
				DPRINTF("SaturationCounter:: Migration Triggered from NVM\n");

				int id_assoc = evictPolicy(set, false);
				CacheEntry* replaced_entry = m_tableSRAM[set][id_assoc];

				m_tableSRAM[set][id_assoc] = current;
				current->isNVM = false;
				current->saturation_counter = 3;
				
				m_tableNVM[set][id_assoc] = replaced_entry;
				replaced_entry->isNVM = true;
				replaced_entry->saturation_counter = 3;

				stats_nbMigrationsFromNVM[inNVM]++;
				
			}
		}
		else{
			current->saturation_counter++;
			if(current->saturation_counter > 3)
				current->saturation_counter = 3;					
		}
			
	}
	else{
		CacheEntry* current = m_tableSRAM[set][index];
		current->policyInfo = m_cpt;

		if(element.isWrite())
		{
			current->saturation_counter++;
			if(current->saturation_counter > 3)
				current->saturation_counter = 3;
		}
		else{
			current->saturation_counter--;
			if(current->saturation_counter == 1)
			{
				//Trigger Migration
				DPRINTF("SaturationCounter:: Migration Triggered from SRAM\n");
				int id_assoc = evictPolicy(set, true);//Select LRU candidate from NVM cache
				CacheEntry* replaced_entry = m_tableNVM[set][id_assoc];

				m_tableNVM[set][id_assoc] = current;
				current->isNVM = true;
				current->saturation_counter = 3;
				
				m_tableSRAM[set][id_assoc] = replaced_entry;
				replaced_entry->isNVM = false;
				replaced_entry->saturation_counter = 3;

				stats_nbMigrationsFromNVM[inNVM]++;

			}
		}
	}	
	m_cpt++;
}

void SaturationCounter::insertionPolicy(uint64_t set, uint64_t index, bool inNVM, Access element)
{

	if(inNVM){
		m_tableNVM[set][index]->policyInfo = m_cpt;
		m_tableNVM[set][index]->saturation_counter = 3;		
	}
	else{
		m_tableSRAM[set][index]->policyInfo = m_cpt;
		m_tableSRAM[set][index]->saturation_counter = 3;	
	
	}
	
	m_cpt++;
}

void SaturationCounter::printStats(std::ostream& out)
{
	double migrationNVM = stats_nbMigrationsFromNVM[0];
	double migrationSRAM = stats_nbMigrationsFromNVM[1];
	double total = migrationNVM+migrationSRAM;
	
	out << "Predictor Stats:" << endl;
	out << "NB Migration :" << total << endl;
	out << "\t From NVM : " << migrationNVM*100/total << "%" << endl;
	out << "\t From SRAM : " << migrationSRAM*100/total << "%" << endl;
}

int SaturationCounter::evictPolicy(int set, bool inNVM)
{	
	if(inNVM)
		return m_replacementPolicyNVM_ptr->evictPolicy(set);
	else
		return m_replacementPolicySRAM_ptr->evictPolicy(set);

}

/********************************************/ 



