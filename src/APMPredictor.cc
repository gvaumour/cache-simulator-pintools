#include "APMPredictor.hh"

using namespace std;

APMPredictor::APMPredictor(int nbAssoc , int nbSet, int nbNVMways, DataArray& SRAMtable, DataArray& NVMtable, HybridCache* cache) :
	Predictor(nbAssoc, nbSet, nbNVMways, SRAMtable, NVMtable, cache)
{
	m_cpt = 1;
	stats_nbMigrationsFromNVM = vector<int>(2,0);
}
	
APMPredictor::~APMPredictor(){}

allocDecision
APMPredictor::allocateInNVM(uint64_t set, Access element)
{
	if(element.isWrite())
		return ALLOCATE_IN_SRAM;
	else
		return ALLOCATE_IN_NVM;
}

void APMPredictor::updatePolicy(uint64_t set, uint64_t index, bool inNVM, Access element , bool isWBrequest = false)
{

	if( element.m_type == MemCmd::DIRTY_WRITEBACK ) //Demand Write 
	{
	
	}
	else if(element.m_type == MemCmd::DATA_WRITE) //Core Write 
	{
	
	}
	
	
	Predictor::updatePolicy(set, index, inNVM, element , isWBrequest);
	m_cpt++;
}

void APMPredictor::insertionPolicy(uint64_t set, uint64_t index, bool inNVM, Access element)
{

	if(inNVM){
		m_tableNVM[set][index]->policyInfo = m_cpt;
		m_tableNVM[set][index]->saturation_counter = SATURATION_TH;		
	}
	else{
		m_tableSRAM[set][index]->policyInfo = m_cpt;
		m_tableSRAM[set][index]->saturation_counter = SATURATION_TH;	
	
	}

	Predictor::insertionPolicy(set , index , inNVM , element);
	
	m_cpt++;
}

void
APMPredictor::printConfig(std::ostream& out, string entete) {

	Predictor::printConfig(out , entete);
};


void APMPredictor::printStats(std::ostream& out, string entete)
{	
	
	double migrationNVM = (double) stats_nbMigrationsFromNVM[0];
	double migrationSRAM = (double) stats_nbMigrationsFromNVM[1];
	double total = migrationNVM+migrationSRAM;

	if(total > 0){
		out << entete << ":PredictorStats:" << endl;
		out << entete << ":NBMigration\t" << total << endl;
		out << entete << ":MigrationFromNVM\t" << migrationNVM*100/total << "%" << endl;
		out << entete << ":MigrationFromSRAM\t" << migrationSRAM*100/total << "%" << endl;	
	}
	Predictor::printStats(out, entete);
}

int APMPredictor::evictPolicy(int set, bool inNVM)
{	
	int assoc_victim;
	if(inNVM)
		assoc_victim = m_replacementPolicyNVM_ptr->evictPolicy(set);
	else
		assoc_victim = m_replacementPolicySRAM_ptr->evictPolicy(set);

	evictRecording(set, assoc_victim, inNVM);
	return assoc_victim;
}

