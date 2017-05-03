#include "Predictor.hh"
#include "ReplacementPolicy.hh"
#include "Cache.hh"

using namespace std;

Predictor::Predictor(): m_tableSRAM(0), m_tableNVM(0), m_nb_set(0), m_assoc(0), m_nbNVMways(0), m_nbSRAMways(0), m_cache(NULL)
{
	m_replacementPolicyNVM_ptr = new LRUPolicy();
	m_replacementPolicySRAM_ptr = new LRUPolicy();	
	stats_beginTimeFrame = 0;
	m_trackError = false;
	
	stats_nbLLCaccessPerFrame = 0;
	stats_WBerrors = 0;
	stats_COREerrors = 0;		
	stats_NVM_errors.clear();
	stats_SRAM_errors.clear();
	missing_tags.clear();
}


Predictor::~Predictor()
{
	if(m_trackError){
		for(int i = 0 ; i < m_nbNVMways - m_nbSRAMways ; i++)
		{
			for(int j = 0 ; j < m_nb_set ; j++)
			{
				delete missing_tags[i][j];
			}
		}
	}
	delete m_replacementPolicyNVM_ptr;
	delete m_replacementPolicySRAM_ptr;
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

	stats_beginTimeFrame = 0;
	stats_NVM_errors = vector<vector<int> >(1, vector<int>(m_nb_set , 0));
	stats_SRAM_errors = vector<vector<int> >(1, vector<int>(m_nb_set , 0));
	
	m_trackError = false;

	if(m_nbNVMways > m_nbSRAMways)
		m_trackError = true;
	
	if(m_trackError){
		missing_tags.resize(m_nbNVMways - m_nbSRAMways);
		
		for(int i = 0 ; i < m_nbNVMways - m_nbSRAMways ; i++)
		{
			missing_tags[i].resize(m_nb_set);
			for(int j = 0 ; j < m_nb_set ; j++)
			{
				missing_tags[i][j] = new MissingTagEntry();
			}
		}
	}


}


void  
Predictor::evictRecording(int set, int assoc_victim , bool inNVM)
{

	if(inNVM || !m_trackError
	)
		return;
		
	CacheEntry* current = m_tableSRAM[set][assoc_victim];
	
	//Choose the oldest victim for the missing tags to replace from the current one 	
	uint64_t cpt_longestime = missing_tags[0][set]->last_time_touched;
	uint64_t index_victim = 0;
	
	for(unsigned i = 0 ; i < missing_tags.size(); i++)
	{
		if(!missing_tags[i][set]->isValid)
		{
			index_victim = i;
			break;
		}	
		
		if(cpt_longestime > missing_tags[i][set]->last_time_touched){
			cpt_longestime = missing_tags[i][set]->last_time_touched; 
			index_victim = i;
		}	
	}

	missing_tags[index_victim][set]->addr = current->address;
	missing_tags[index_victim][set]->last_time_touched = cpt_time;
	missing_tags[index_victim][set]->isValid = current->isValid;

}


void
Predictor::insertRecord(int set, int assoc, bool inNVM)
{
	//Update the missing tag as a new cache line is brough into the cache 
	//Has to remove the MT entry if it exists there 
	
	if(inNVM || !m_trackError)
		return;
	
	
	CacheEntry* current = m_tableSRAM[set][assoc];
	for(unsigned i = 0 ; i < missing_tags.size(); i++)
	{
		if(missing_tags[i][set]->addr == current->address)
		{
			missing_tags[i][set]->isValid = false;
		}
	}
}


void
Predictor::checkMissingTags(uint64_t block_addr , int id_set)
{

	if(m_trackError){
		//An error in the SRAM part is a block that miss in the SRAM array 
		//but not in the MT array
		for(unsigned i = 0 ; i < missing_tags.size(); i++)
		{
			if(missing_tags[i][id_set]->addr == block_addr && missing_tags[i][id_set]->isValid)
			{
				//DPRINTF("BasePredictor::checkMissingTags Found SRAM error as %#lx is present in MT tag %i \n", block_addr ,i);  
				stats_SRAM_errors[stats_SRAM_errors.size()-1][id_set]++;
				return;
			}
		}	
	}
}


void
Predictor::updatePolicy(uint64_t set, uint64_t index, bool inNVM, Access element , bool isWBrequest = false)
{	

	if( (cpt_time - stats_beginTimeFrame) > PREDICTOR_TIME_FRAME)
	{
		stats_NVM_errors.push_back( vector<int>(m_nb_set , 0) );
		stats_SRAM_errors.push_back( vector<int>(m_nb_set , 0) );
		stats_beginTimeFrame = cpt_time;
		DPRINTF("BasePredictor::updatePolicy New Time Frame Start\n");  
		stats_nbLLCaccessPerFrame = 0;
	}

	stats_nbLLCaccessPerFrame++;
	//An error in the NVM side is when handling a write 
	if(inNVM && element.isWrite()){
		stats_NVM_errors[stats_NVM_errors.size()-1][set]++;

		if(isWBrequest)
			stats_WBerrors++;
		else
			stats_COREerrors++;
	}
}
				 
void 
Predictor::printStats(std::ostream& out)
{

	out << "Predictor : NVM Error " << endl;
	out << "\t From WB\t"  << stats_WBerrors << endl;
	out << "\t From Core\t" <<  stats_COREerrors << endl;
	ofstream output_file;
	output_file.open(PREDICTOR_OUTPUT_FILE);
	
	output_file << "NVM Error " << endl;
	for(unsigned i = 0 ; i <  stats_NVM_errors[0].size(); i++)
	{
	
		output_file <<"Set "<< i;
		for(unsigned j = 0 ; j < stats_NVM_errors.size(); j++)
			output_file << "\t" << stats_NVM_errors[j][i ];
		
		output_file << endl;
	}

	output_file << "SRAM Error " << endl;
	for(unsigned i = 0 ; i <  stats_SRAM_errors[0].size(); i++)
	{
		output_file <<"Set "<< i;
		for(unsigned j = 0 ; j <  stats_SRAM_errors.size(); j++)
			output_file << "\t" << stats_SRAM_errors[j][i];
		
		output_file << endl;
	}

	output_file.close();
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
LRUPredictor::updatePolicy(uint64_t set, uint64_t index, bool inNVM, Access element , bool isWBrequest = false)
{
	if(inNVM)
		m_tableNVM[set][index]->policyInfo = m_cpt;
	else
		m_tableSRAM[set][index]->policyInfo = m_cpt;

	Predictor::updatePolicy(set , index , inNVM , element);

	m_cpt++;
}

void
LRUPredictor::insertionPolicy(uint64_t set, uint64_t index, bool inNVM, Access element)
{	
	if(inNVM)
		m_tableNVM[set][index]->policyInfo = m_cpt;
	else
		m_tableSRAM[set][index]->policyInfo = m_cpt;
	
	Predictor::insertRecord(set, index , inNVM);
	m_cpt++;
}

int
LRUPredictor::evictPolicy(int set, bool inNVM)
{
	int assoc_victim;
	
	if(inNVM)
		assoc_victim = m_replacementPolicyNVM_ptr->evictPolicy(set);
	else
		assoc_victim =  m_replacementPolicySRAM_ptr->evictPolicy(set);
	
	evictRecording(set , assoc_victim , inNVM);
	return assoc_victim;
}


bool
PreemptivePredictor::allocateInNVM(uint64_t set, Access element)
{
	return !element.isWrite();
}


/********************************************/ 
