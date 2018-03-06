#include <set>
#include <fstream>
#include <vector>
#include <map>
#include <ostream>

#include "DBAMBPredictor.hh"

using namespace std;

static const char* str_RW_status[] = {"DEAD" , "WO", "RO" , "RW", "RW_NOTACC"};
static const char* str_RD_status[] = {"RD_SHORT" , "RD_MEDIUM", "RD_NOTACC", "UNKOWN"};


ofstream dataset_file;
int id_DATASET = 0;

/** DBAMBPredictor Implementation ***********/ 
DBAMBPredictor::DBAMBPredictor(int nbAssoc , int nbSet, int nbNVMways, DataArray& SRAMtable, DataArray& NVMtable, HybridCache* cache) : \
	Predictor(nbAssoc, nbSet, nbNVMways, SRAMtable, NVMtable, cache) {

	m_cpt = 1;
	m_RAP_assoc = simu_parameters.rap_assoc;
	m_RAP_sets = simu_parameters.rap_sets;

	m_DHPTable.clear();
	m_DHPTable.resize(m_RAP_sets);
	
	for(unsigned i = 0 ; i < m_RAP_sets ; i++)
	{
		m_DHPTable[i].resize(m_RAP_assoc);
		
		
		for(unsigned j = 0 ; j < m_RAP_assoc ; j++)
		{
			m_DHPTable[i][j] = new DHPEntry();
			m_DHPTable[i][j]->index = i;
			m_DHPTable[i][j]->assoc = j;
		}
	}
	/*
	if(simu_parameters.enableReuseErrorComputation)
		stats_history_SRAM.resize(nbSet);
	*/
	
	//DPRINTF("RAPPredictor::Constructor m_DHPTable.size() = %lu , m_DHPTable[0].size() = %lu\n" , m_DHPTable.size() , m_DHPTable[0].size());

	m_rap_policy = new DBAMBLRUPolicy(m_RAP_assoc , m_RAP_sets , m_DHPTable);

	/* Stats init*/ 
	stats_nbSwitchDst.clear();
	stats_ClassErrors.clear();
	stats_ClassErrors = vector< vector<int> >(NUM_RW_TYPE*NUM_RD_TYPE, vector<int>(NUM_RD_TYPE*NUM_RW_TYPE, 0));	
	

	if(simu_parameters.printDebug)
		dataset_file.open(RAP_TEST_OUTPUT_DATASETS);


	stats_nbMigrationsFromNVM.push_back(vector<int>(2,0));
//	stats_switchDecision.clear();
//	stats_switchDecision.push_back(vector<vector<int>>(3 , vector<int>(3,0)));	
	
//	myFile.open("dead_block_dump.out");

	m_optTarget = simu_parameters.DBAMP_optTarget;
	assert((m_optTarget == "energy" || m_optTarget == "perf") && "Wrong optimization parameter for the function cost" );
	if(m_optTarget == "energy")
		m_costParameters = EnergyParameters();
	else if(m_optTarget == "perf")
		m_costParameters = PerfParameters();
		
	stats_error_wrongallocation = 0;
	stats_error_learning = 0;
	stats_error_wrongpolicy = 0;	

	stats_error_SRAMwrongallocation = 0;
	stats_error_SRAMlearning = 0;
	stats_error_SRAMwrongpolicy = 0;

}


		
DBAMBPredictor::~DBAMBPredictor()
{
//	myFile.close();
	
	for(auto sets : m_DHPTable)
		for(auto entry : sets)
			delete entry;
	if(simu_parameters.printDebug)
		dataset_file.close();
}

allocDecision
DBAMBPredictor::allocateInNVM(uint64_t set, Access element)
{

	//DPRINTF("DBAMBPredictor::allocateInNVM set %ld\n" , set);
	
	if(element.isInstFetch())
		return ALLOCATE_IN_NVM;
		
	
	DHPEntry* rap_current = lookup(element.m_pc);
	if(rap_current  == NULL) // Miss in the RAP table
	{
		if(!m_isWarmup)
			stats_RAP_miss++;	
		
		//DPRINTF("DBAMBPredictor::allocateInNVM Eviction and Installation in the RAP table 0x%lx\n" , element.m_pc);

		int index = indexFunction(element.m_pc);
		int assoc = m_rap_policy->evictPolicy(index);
		
		rap_current = m_DHPTable[index][assoc];
		
		/* Dumping stats before erasing the RAP entry */ 
		if(rap_current->isValid)
		{
			dumpDataset(rap_current);
					
			if(rap_current->nbKeepState > 0 && rap_current->nbSwitchState  > 0 && !m_isWarmup)
				stats_nbSwitchDst.push_back((double)rap_current->nbSwitchState / \
					 (double)(rap_current->nbKeepState+rap_current->nbSwitchState) );		
		}
		/******************/

		rap_current->initEntry(element);
		rap_current->id = id_DATASET++;
		rap_current->m_pc = element.m_pc;
		dataset_file << "ID Dataset:" << rap_current->id << "\t" << element.m_pc << endl;
	}
	else
	{	
		if(!m_isWarmup)
			stats_RAP_hits++;
		rap_current->nbAccess++;
	}	
	
	m_rap_policy->updatePolicy(rap_current->index , rap_current->assoc);
	
	if(rap_current->des == BYPASS_CACHE)
	{
		rap_current->cptBypassLearning++;
		if(simu_parameters.printDebug)
		{
			dataset_file << "Dataset n°" << rap_current->id << ": BYPASS, Address: " << std::hex << \
			element.m_address << std::dec << "; Threshold=" << rap_current->cptBypassLearning << endl;		
		}
			
		if(rap_current->cptBypassLearning ==  simu_parameters.learningTH){
			rap_current->cptBypassLearning = 0;

			if(simu_parameters.printDebug)
			{
				dataset_file << "Dataset n°" << rap_current->id << ": Insert Learning Cl, " \
				<< std::hex << element.m_address << std::dec  << endl;			
			}
			return ALLOCATE_IN_NVM;		
		}
	}
	
	if(rap_current->des == ALLOCATE_PREEMPTIVELY)
	{
		if(element.isWrite())
			return ALLOCATE_IN_SRAM;
		else
			return ALLOCATE_IN_NVM;
	}
	else
		return rap_current->des;
}

void 
DBAMBPredictor::finishSimu()
{
	//DPRINTF("RAPPredictor::FINISH SIMU\n");


	if(m_isWarmup)
		return;
		
	for(auto lines : m_DHPTable)
	{
		for(auto entry : lines)
		{		
			if(entry->isValid)
			{
				HistoEntry current = {entry->state_rw , entry->state_rd , entry->nbKeepState};
				entry->stats_history.push_back(current);
				dumpDataset(entry);

				if(entry->nbKeepState + entry->nbSwitchState  > 0)
					stats_nbSwitchDst.push_back((double)entry->nbSwitchState / \
					  (double) (entry->nbKeepState + entry->nbSwitchState) );
			}
		}
	}
}


int
DBAMBPredictor::computeRd(uint64_t set, uint64_t  index , bool inNVM)
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
	
	/* Determine the position of the cachel line in the LRU stack, the policyInfo is not enough */
	for(unsigned i = 0 ; i < line.size() ; i ++)
	{
		if(line[i]->policyInfo > ref_rd)
			position++;
	}	
	return position;
}


void
DBAMBPredictor::dumpDataset(DHPEntry* entry)
{	/*
	if(entry->stats_history.size() < 2)
		return;

	if(m_isWarmup)
		return;

	ofstream //dataset_file(RAP_TEST_OUTPUT_DATASETS, std::ios::app);
	//dataset_file << "Dataset\t0x" << std::hex << entry->m_pc << std::dec << endl;
	//dataset_file << "\tNB Switch\t" << entry->nbSwitchState <<endl;
	//dataset_file << "\tNB Keep State\t" << entry->nbKeepState <<endl;
	//dataset_file << "\tHistory" << endl;
	for(auto p : entry->stats_history)
	{
		//dataset_file << "\t\t" << str_RW_status[p.state_rw]<< "\t" << str_RD_status[p.state_rd] << "\t" << p.nbKeepState << endl;
	}
	//dataset_file.close();*/
}


void
DBAMBPredictor::updatePolicy(uint64_t set, uint64_t index, bool inNVM, Access element, bool isWBrequest = false)
{

	//DPRINTF("DBAMBPredictor::updatePolicy set %ld , index %ld\n" , set, index);

	CacheEntry* current = NULL;
	if(inNVM)
		current = m_tableNVM[set][index];
	else 
		current = m_tableSRAM[set][index];
	
	int rd = computeRd(set, index , inNVM);

	/*
	if(inNVM && simu_parameters.enableReuseErrorComputation){
		RD_TYPE reuse_class = convertRD(rd);
		if(reuse_class == RD_MEDIUM)
		{
			stats_NVM_medium_pred++;
			cout <<"DBAMBPredictor::set=" << set << " RD MEDIUM DETECTED cpt_time="<< m_cpt << " address=" << current->address << endl; 
			cerr <<"DBAMBPredictor::set=" << set << " RD MEDIUM DETECTED cpt_time="<< m_cpt << " address=" << current->address << endl; 

			if(hitInSRAM(set , current->policyInfo))
			{
				stats_NVM_medium_pred_errors++;			
				cout <<"DBAMBPredictor::set=" << set << " stats_NVM_medium_pred_errors happens cpt_time="<< m_cpt << " address=" << current->address << endl; 
				
			}
		}

		cout <<"DBAMBPredictor::Record history cpt_time=" << m_cpt << " address=" << current->address << " set=" << set << endl; 
		stats_history_SRAM[set].push_front(pair<uint64_t, uint64_t>(m_cpt, current->address));
	}*/
	

	current->policyInfo = m_cpt;
	DHPEntry* rap_current = lookup(current->m_pc);


	if(current->m_pc !=0)
	{	
		if(rap_current)
		{

//			//DPRINTF("DBAMBPredictor::updatePolicy Reuse distance of the pc %lx, RD = %d\n", current->m_pc , rd );
			rap_current->rd_history.push_back(convertRD(rd));
			rap_current->rw_history.push_back(element.isWrite());		
			
			//A learning cl shows some reuse so we quit dead state 
			if(current->isLearning)
			{
				// Reset the window 
				RW_TYPE old_rw = rap_current->state_rw;
				RD_TYPE old_rd = rap_current->state_rd;
				
				rap_current->des = ALLOCATE_PREEMPTIVELY;
				rap_current->state_rd = RD_NOT_ACCURATE;
				rap_current->state_rw = RW_NOT_ACCURATE;
				rap_current->dead_counter = 0;
	
				if(!m_isWarmup)
					stats_ClassErrors[old_rd + NUM_RD_TYPE*old_rw][rap_current->state_rd + NUM_RD_TYPE*rap_current->state_rw]++;
				
				dataset_file << "Dataset n°" << rap_current->id << ": Reuse detected on a learning Cl, switching to learning state" << endl;
			}
			else
			{
				updateWindow(rap_current);
			
				reportAccess(rap_current , element, current, current->isNVM, "UPDATE", str_RD_status[convertRD(rd)]);
				if(simu_parameters.enableMigration && element.enableMigration )
					current = checkLazyMigration(rap_current , current , set , inNVM , index, element.isWrite());
			}
			
		}	
	}
	
	m_cpt++;
	Predictor::updatePolicy(set , index , inNVM , element , isWBrequest);
}


void
DBAMBPredictor::reportAccess(DHPEntry* rap_current, Access element, CacheEntry* current, bool inNVM, string entete, string reuse_class)
{
	string cl_location = inNVM ? "NVM" : "SRAM";
	string is_learning = current->isLearning ? "Learning" : "Regular";
	string access_type = element.isWrite() ? "WRITE" : "READ";
	
	string is_error= "";
	if(element.isSRAMerror)
		is_error = ", is an SRAM error";
	
	if(simu_parameters.printDebug)
	{
		dataset_file << entete << ":Dataset n°" << rap_current->id << ": [" \
		<< str_RW_status[rap_current->state_rw] << ","  << str_RD_status[rap_current->state_rd] << "]," \
		<< access_type << " on " << is_learning << " Cl 0x" << std::hex << current->address \
		<< std::dec << " allocated in " << cl_location<< ", Reuse=" << reuse_class \
		<< " , " << is_error << ", " << endl;	
	}
	
	
	if(inNVM && element.isWrite()){
		if(rap_current->des == ALLOCATE_IN_NVM)
			stats_error_wrongpolicy++;
		else if(rap_current->des == ALLOCATE_PREEMPTIVELY)
			stats_error_learning++;
		else if(rap_current->des == ALLOCATE_IN_SRAM && inNVM)
			stats_error_wrongallocation++;	
	}
	if(element.isSRAMerror){
		if(rap_current->des == ALLOCATE_IN_SRAM)
			stats_error_SRAMwrongpolicy++;
		else if(rap_current->des == ALLOCATE_PREEMPTIVELY)
			stats_error_SRAMlearning++;
		else if(rap_current->des == ALLOCATE_IN_NVM && !inNVM)
			stats_error_SRAMwrongallocation++;
	}
	if(simu_parameters.printDebug)
	{
		if(inNVM && element.isWrite())
		{
			dataset_file << "NVM Error : ";
			if(rap_current->des == ALLOCATE_IN_NVM)
				dataset_file << " Policy Error";		
			else if(rap_current->des == ALLOCATE_PREEMPTIVELY)
				dataset_file << " Learning Error";
			else if(rap_current->des == ALLOCATE_IN_SRAM && inNVM)
				dataset_file << "Wrongly allocated cl";
			else
				dataset_file << " Des: " << allocDecision_str[rap_current->des] << "UNKNOW Error";
			dataset_file << endl;
		}
		
		if(element.isSRAMerror)
		{
			dataset_file << "SRAM Error : ";
		
			if(rap_current->des == ALLOCATE_IN_SRAM)
				dataset_file << "Wrong policy";
			else if(rap_current->des == ALLOCATE_PREEMPTIVELY)
				dataset_file << " Learning Error";
			else if(rap_current->des == ALLOCATE_IN_NVM && !inNVM)
				dataset_file << "Wrongly allocated cl";
		
			dataset_file << endl;		
		}
	
	}
}

void
DBAMBPredictor::reportMigration(DHPEntry* rap_current, CacheEntry* current, bool fromNVM)
{
	string cl_location = fromNVM ? "from NVM" : "from SRAM";
	
	if(simu_parameters.printDebug)
	{
		dataset_file << "Migration:Dataset n°" << rap_current->id << ": [" << \
		str_RW_status[rap_current->state_rw] << ","  << str_RD_status[rap_current->state_rd] << "], Migration " << cl_location << \
		" Cl 0x" << std::hex << current->address << std::dec << " allocated in " << cl_location << endl;	
	}
	
}

CacheEntry*
DBAMBPredictor::checkLazyMigration(DHPEntry* rap_current , CacheEntry* current ,uint64_t set,bool inNVM, uint64_t index, bool isWrite)
{
	if(rap_current->des == ALLOCATE_IN_NVM && inNVM == false && (simu_parameters.flagTest && !isWrite) )
	{
		//Trigger Migration
		int id_assoc = evictPolicy(set, true);//Select LRU candidate from NVM cache
		//DPRINTF("DBAMBPredictor:: Migration Triggered from SRAM, index %ld, id_assoc %d \n" , index, id_assoc);

		CacheEntry* replaced_entry = m_tableNVM[set][id_assoc];
					
		/** Migration incurs one read and one extra write */ 
		replaced_entry->nbWrite++;
		current->nbRead++;
		
		/* Record the write error migration */ 
		Predictor::migrationRecording();
		
		reportMigration(rap_current, current, false);
		
		m_cache->triggerMigration(set, index , id_assoc , false);
		if(!m_isWarmup)
			stats_nbMigrationsFromNVM.back()[FROMSRAM]++;


		current = replaced_entry;		
	}
	else if( rap_current->des == ALLOCATE_IN_SRAM && inNVM == true)
	{
	
		//DPRINTF("DBAMBPredictor::Migration Triggered from NVM\n");

		int id_assoc = evictPolicy(set, false);
		
		CacheEntry* replaced_entry = m_tableSRAM[set][id_assoc];

		/** Migration incurs one read and one extra write */ 
		replaced_entry->nbWrite++;
		current->nbRead++;
		reportMigration(rap_current, current, true);
	
		m_cache->triggerMigration(set, id_assoc , index , true);
		
		if(!m_isWarmup)
			stats_nbMigrationsFromNVM.back()[FROMNVM]++;

		current = replaced_entry;		
	}
//	cout <<" FINISH Lazy Migration "<< endl;
	return current;
}

bool
DBAMBPredictor::hitInSRAM(int set, uint64_t old_time)
{
	/*
	cout <<"old_time=" << old_time << endl;
	if(old_time == 0)
		return false;
	
	std::set<uint64_t> adresses_access;
	
	for(auto item : stats_history_SRAM[set])
	{
		if(item.first < old_time)
			return true;
		
		if(adresses_access.count(item.second) == 0)
		{
			cout <<"\tRD++"<<endl;		
			adresses_access.insert(item.second);
		}

		if(adresses_access.size() >= simu_parameters.sram_assoc)
			return false;
	}
	*/
	return false;
}

void
DBAMBPredictor::insertionPolicy(uint64_t set, uint64_t index, bool inNVM, Access element)
{
	CacheEntry* current = NULL;
	if(inNVM)
		current = m_tableNVM[set][index];
	else 
		current = m_tableSRAM[set][index];
		
	current->policyInfo = m_cpt;
	
	
	RD_TYPE reuse_class = RD_NOT_ACCURATE;
	DHPEntry* rap_current;
	/*
	if(simu_parameters.enableReuseErrorComputation)
	{
		if(inNVM)
		{
			cout <<"DBAMBPredictor::Record history cpt_time=" << m_cpt << " address=" << current->address << " set=" << set << endl; 
			stats_history_SRAM[set].push_front(pair<uint64_t, uint64_t>(m_cpt, current->address));	
		}			
	}*/
		
	if( (rap_current= lookup(current->m_pc) ) != NULL )
	{

		/* Set the flag of learning cache line when bypassing */
		if(rap_current->des == BYPASS_CACHE)
		{
			current->isLearning = true;
		}
		else
		{
		
			if(element.isSRAMerror)
				reuse_class = RD_MEDIUM; 

			rap_current->rd_history.push_back(reuse_class); 
			rap_current->rw_history.push_back(element.isWrite());

			updateWindow(rap_current);		
		}
	
		reportAccess(rap_current, element, current, inNVM, "INSERTION",  str_RD_status[reuse_class]);
	}
	
	Predictor::insertionPolicy(set , index , inNVM , element);
	m_cpt++;
}




void
DBAMBPredictor::updateWindow(DHPEntry* rap_current)
{

	if(rap_current->cptWindow < simu_parameters.window_size)
		rap_current->cptWindow++;
	else
	{
		RW_TYPE old_rw = rap_current->state_rw;
		RD_TYPE old_rd = rap_current->state_rd;

		determineStatus(rap_current);
		
		dataset_file << "Window:Dataset n°" << rap_current->id << ": NewWindow [" << str_RW_status[rap_current->state_rw] << "," \
		  	     << str_RD_status[rap_current->state_rd] << "]"  << endl;	
	
	
		if(!m_isWarmup)
			stats_ClassErrors[old_rd + NUM_RD_TYPE*old_rw][rap_current->state_rd + NUM_RD_TYPE*rap_current->state_rw]++;

		if(old_rd != rap_current->state_rd || old_rw != rap_current->state_rw)
		{
			HistoEntry current = {rap_current->state_rw , rap_current->state_rd, rap_current->nbKeepState};

			if(!m_isWarmup)
				rap_current->stats_history.push_back(current);

			rap_current->nbSwitchState++;
			rap_current->nbKeepState = 0;					

			rap_current->des = convertState(rap_current);
		}
		else{
			rap_current->nbKeepState++;
			rap_current->nbKeepCurrentState++;			
		}

		/* Reset the window */			
		rap_current->rd_history.clear();
		rap_current->rw_history.clear();
		rap_current->cptWindow = 0;		
	}	
}


int
DBAMBPredictor::evictPolicy(int set, bool inNVM)
{	

		
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
		
	//DPRINTF("DBAMBPredictor::evictPolicy set %d n assoc_victim %d \n" , set , assoc_victim);
	DHPEntry* rap_current = lookup(current->m_pc);
	//We didn't create an entry for this dataset, probably a good reason =) (instruction mainly) 
	if(rap_current != NULL)
	{
		
		if(rap_current->des == BYPASS_CACHE)
		{
			// A learning cache line on dead dataset goes here
			
			/*if( !(current->nbWrite == 0 && current->nbRead == 0) )
			{
				// Reset the window 
				RW_TYPE old_rw = rap_current->state_rw;
				RD_TYPE old_rd = rap_current->state_rd;
				
				rap_current->des = ALLOCATE_PREEMPTIVELY;
				rap_current->state_rd = RD_NOT_ACCURATE;
				rap_current->state_rw = RW_NOT_ACCURATE;
				rap_current->dead_counter = 0;
	
				if(!m_isWarmup)
					stats_ClassErrors[old_rd + NUM_RD_TYPE*old_rw][rap_current->state_rd + NUM_RD_TYPE*rap_current->state_rw]++;
			}*/
			//No reuse on learning line , we stay on bypass mode		
		}
		else
		{
			// A learning cache line on dead dataset goes here
			if( current->nbWrite == 0 && current->nbRead == 0)
				rap_current->dead_counter--;
			else
				rap_current->dead_counter++;
		
			if(rap_current->dead_counter > simu_parameters.deadSaturationCouter)
				rap_current->dead_counter = simu_parameters.deadSaturationCouter;
			else if(rap_current->dead_counter < -simu_parameters.deadSaturationCouter)
				rap_current->dead_counter = -simu_parameters.deadSaturationCouter;


			string a =  current->nbWrite == 0 && current->nbRead == 0 ? "DEAD" : "LIVELY";
			dataset_file << "Dataset n°" << rap_current->id << ": Eviction of a " << a << " CL, Address :" << \
				std::hex << current->address << std::dec << " DeadCounter=" << rap_current->dead_counter << endl;
				
			if(rap_current->dead_counter == -simu_parameters.deadSaturationCouter && simu_parameters.enableBP)
			{
				if(rap_current->des == ALLOCATE_IN_SRAM && simu_parameters.flagTest)
				{
					/* Switch the state of the dataset to NVM */ 
					rap_current->des = ALLOCATE_IN_NVM;			
					dataset_file << "Dataset n°" << rap_current->id << ": Going into NVM MEDIUM mode " << endl;
					// Reset the window 
					RW_TYPE old_rw = rap_current->state_rw;
					RD_TYPE old_rd = rap_current->state_rd;

					rap_current->state_rd = RD_MEDIUM;
					rap_current->rd_history.clear();
					rap_current->rw_history.clear();
					rap_current->cptWindow = 0;	
					rap_current->dead_counter = 0;
	
					// Monitor state switching 
					if(!m_isWarmup)
						stats_ClassErrors[old_rd + NUM_RD_TYPE*old_rw][rap_current->state_rd + NUM_RD_TYPE*rap_current->state_rw]++;
					
				}
				else			
				{
					/* Switch the state of the dataset to dead */ 
					rap_current->des = BYPASS_CACHE;			
					dataset_file << "Dataset n°" << rap_current->id << ": Going into BYPASS mode " << endl;
					// Reset the window 
					RW_TYPE old_rw = rap_current->state_rw;
					RD_TYPE old_rd = rap_current->state_rd;

					rap_current->state_rd = RD_NOT_ACCURATE;
					rap_current->state_rw = DEAD;
					rap_current->rd_history.clear();
					rap_current->rw_history.clear();
					rap_current->cptWindow = 0;	
					rap_current->dead_counter = 0;
	
					// Monitor state switching 
					if(!m_isWarmup)
						stats_ClassErrors[old_rd + NUM_RD_TYPE*old_rw][rap_current->state_rd + NUM_RD_TYPE*rap_current->state_rw]++;
			
				}
			}
		}
	}
	
	evictRecording(set , assoc_victim , inNVM);	
	return assoc_victim;
}

void 
DBAMBPredictor::determineStatus(DHPEntry* entry)
{
	if(entry->des == BYPASS_CACHE)
	{
		//All the info gathered here are from learning cache line
		//If we see any any reuse we learn again
		/*if(entry->rw_history.size() != 0)
		{
			entry->des = ALLOCATE_PREEMPTIVELY;
			entry->state_rd = RD_NOT_ACCURATE;
			entry->state_rw = RW_NOT_ACCURATE;
			entry->dead_counter = 0;
		}*/
		return;		
	}


	/* Count the number of read and write during window */
	vector<int> cpts_rw = vector<int>(2,0);
	for(auto isWrite : entry->rw_history)
	{
		if(isWrite)
			cpts_rw[INDEX_WRITE]++;
		else
			cpts_rw[INDEX_READ]++;
	}

	if(cpts_rw[INDEX_READ] == 0 && cpts_rw[INDEX_WRITE] > 0)
		entry->state_rw = WO;
	else if(cpts_rw[INDEX_READ] > 0 && cpts_rw[INDEX_WRITE] == 0)
		entry->state_rw = RO;
	else if(cpts_rw[INDEX_READ] > 0 && cpts_rw[INDEX_WRITE] > 0)
		entry->state_rw = RW;
	else
		entry->state_rw = DEAD;
		
	if(entry->state_rw == DEAD)
	{
		entry->state_rd = RD_NOT_ACCURATE;
		return;	
	}

	
	/* Determination of the rd type */
//	int window_rd = entry->rd_history.size();

	vector<int> cpts_rd = vector<int>(NUM_RD_TYPE,0);
	
	for(auto rd : entry->rd_history)
		cpts_rd[rd]++;
		

	
//	int max = 0, index_max = -1;
//	for(unsigned i = 0 ; i < cpts_rd.size() ; i++)
//	{
//		if(cpts_rd[i] > max)
//		{
//			max = cpts_rd[i];
//			index_max = i;
//		}
//	}

	double costSRAM = 0;
	double costNVM =0;
	for(unsigned i = 0 ; i < entry->rd_history.size() ; i++)
	{
		bool isWrite = entry->rw_history[i];
		if(entry->rd_history[i] == RD_NOT_ACCURATE)
		{			
				costSRAM += m_costParameters.costDRAM[isWrite] + m_costParameters.costSRAM[isWrite]; 
				costNVM += m_costParameters.costDRAM[isWrite] + m_costParameters.costNVM[isWrite];		
		}
		else if(entry->rd_history[i] == RD_SHORT)
		{
			costSRAM += m_costParameters.costSRAM[isWrite]; 
			costNVM += m_costParameters.costNVM[isWrite];
		}
		else
		{
			costSRAM += m_costParameters.costDRAM[isWrite]; 
			costNVM += m_costParameters.costNVM[isWrite];	
		}
	}

	if(costSRAM > costNVM)
		entry->state_rd = (RD_TYPE) RD_MEDIUM;
	else
		entry->state_rd = (RD_TYPE) RD_SHORT;
			
	
//	if( ((double)max / (double)window_rd) > RAP_INACURACY_TH)
//		entry->state_rd = (RD_TYPE) index_max;
//	else
//		entry->state_rd = RD_NOT_ACCURATE;

	

			
//	/** Debugging purpose */	
//	if(entry->state_rd == RD_NOT_ACCURATE && entry->state_rw == DEAD)
//	{
//		cout <<"RD history\t";
//		for(auto p : entry->rd_history)
//			cout <<p << "\t";
//		cout <<endl;
//		cout <<"RW history\tNBWrite=" << cpts_rw[INDEX_WRITE] << "\tNBRead=" << cpts_rw[INDEX_READ] << endl;
//		cout <<"New state is " << str_RD_status[entry->state_rd] << "\t" << str_RW_status[entry->state_rw] << endl;	
//	}


	/*
	if( ((double)max / (double) RAP_WINDOW_SIZE) > RAP_INACURACY_TH)
		entry->state_rw = (RW_TYPE)index_max;
	else
		entry->state_rw = RW_NOT_ACCURATE;
	*/
}

allocDecision
DBAMBPredictor::convertState(DHPEntry* rap_current)
{
	RD_TYPE state_rd = rap_current->state_rd;
	RW_TYPE state_rw = rap_current->state_rw;

	if(state_rw == RO)
	{
		if(simu_parameters.sram_assoc <= simu_parameters.nvm_assoc)
		{
			return ALLOCATE_IN_NVM;	
		}
		else
		{
			if(state_rd == RD_SHORT || state_rd == RD_NOT_ACCURATE)
				return ALLOCATE_IN_NVM;
			else if(state_rd == RD_MEDIUM)
				return ALLOCATE_IN_SRAM;
			else 
				return BYPASS_CACHE;		
		}
		
	}
	else if(state_rw == RW || state_rw == WO)
	{
	
		if(simu_parameters.sram_assoc <= simu_parameters.nvm_assoc)
		{
			if(state_rd == RD_SHORT || state_rd == RD_NOT_ACCURATE)
				return ALLOCATE_IN_SRAM;
			else if(state_rd == RD_MEDIUM)
				return ALLOCATE_IN_NVM;
			else 
				return BYPASS_CACHE;		
		}
		else
		{
			return ALLOCATE_IN_SRAM;		
		}
	}
	else if(state_rw == DEAD)
	{
		// When in dead state, rd state is always innacurate, no infos 
		if(rap_current->des == ALLOCATE_IN_SRAM)
			return ALLOCATE_IN_NVM;
		else 
			return BYPASS_CACHE;	
	}
	else
	{ // State RW not accurate 
	
		if(state_rd == RD_SHORT)
			return ALLOCATE_IN_SRAM;
		else if( state_rd == RD_MEDIUM)
			return ALLOCATE_IN_NVM;
		else if(state_rd == RD_NOT_ACCURATE) // We don't know anything for sure, go preemptive
			return ALLOCATE_PREEMPTIVELY;
		else 
			return BYPASS_CACHE;
	}
}


void 
DBAMBPredictor::printConfig(std::ostream& out, string entete)
{
	out << entete << ":DBAMBPredictor:RAPTableAssoc\t" << m_RAP_assoc << endl;
	out << entete << ":DBAMBPredictor:RAPTableNBSets\t" << m_RAP_sets << endl;
	out << entete << ":DBAMBPredictor:Windowsize\t" << simu_parameters.window_size << endl;
	out << entete << ":DBAMBPredictor:InacurracyThreshold\t" << simu_parameters.rap_innacuracy_th << endl;

	string a =  simu_parameters.enableBP ? "TRUE" : "FALSE"; 
	out << entete << ":DBAMBPredictor:BypassEnable\t" << a << endl;
	out << entete << ":DBAMBPredictor:DEADCOUNTERSaturation\t" << simu_parameters.deadSaturationCouter << endl;
	out << entete << ":DBAMBPredictor:LearningThreshold\t" << simu_parameters.learningTH << endl;
	a =  simu_parameters.enableMigration ? "TRUE" : "FALSE"; 
	out << entete << ":DBAMBPredictor:MigrationEnable\t" << a << endl;
	out << entete << ":DBAMBPredictor:OptTarget\t" << m_optTarget << endl;
	
	Predictor::printConfig(out, entete);
}

void 
DBAMBPredictor::printStats(std::ostream& out, string entete)
{	

	
	ofstream output_file(RAP_OUTPUT_FILE);
	
	for(int i = 0 ; i < NUM_RW_TYPE ;i++)
	{
		for(int j = 0 ; j < NUM_RD_TYPE ; j++)
			output_file << "\t[" << str_RW_status[i] << ","<< str_RD_status[j] << "]";
	}
	output_file << endl;
	
	for(unsigned i = 0 ; i < stats_ClassErrors.size() ; i++)
	{
		output_file << "[" << str_RW_status[i/NUM_RD_TYPE] << ","<< str_RD_status[i%NUM_RD_TYPE] << "]\t";
		for(unsigned j = 0 ; j < stats_ClassErrors[i].size() ; j++)
		{
			output_file << stats_ClassErrors[i][j] << "\t";
		}
		output_file << endl;
	}
	output_file.close();


	double migrationNVM=0, migrationSRAM = 0;

	if(simu_parameters.enableMigration)
	{

		ofstream file_migration_stats(RAP_MIGRATION_STATS);
		
		for(unsigned i = 0 ; i < stats_nbMigrationsFromNVM.size() ; i++)
		{
			migrationNVM += stats_nbMigrationsFromNVM[i][FROMNVM];
			migrationSRAM += stats_nbMigrationsFromNVM[i][FROMSRAM];			
			
			file_migration_stats << stats_nbMigrationsFromNVM[i][FROMNVM] << "\t" << stats_nbMigrationsFromNVM[i][FROMSRAM] << endl;	
		}
		file_migration_stats.close();
	}
	/*
	out << "\tRAP Table:" << endl;
	out << "\t\t NB Hits\t" << stats_RAP_hits << endl;
	out << "\t\t NB Miss\t" << stats_RAP_miss << endl;
	out << "\t\t Miss Ratio\t" << stats_RAP_miss*100.0/(double)(stats_RAP_hits+stats_RAP_miss)<< "%" << endl;
	*/
	
	/*out << "\tSource of Error, NVM error" << endl;
	out << "\t\tWrong Policy\t" << stats_error_wrongpolicy << endl;
	out << "\t\tWrong Allocation\t" << stats_error_wrongallocation << endl;
	out << "\t\tLearning\t" << stats_error_learning << endl;		
	*/
	/*
	if(simu_parameters.enableMigration)
		out << "\t\tMigration Error\t" << migrationSRAM << endl;		
	*/
	out << entete << ":DBAMBPredictor:NVM_medium_pred\t" << stats_NVM_medium_pred << endl;
	out << entete << ":DBAMBPredictor:NVM_medium_pred_errors\t" << stats_NVM_medium_pred_errors << endl;

//	out << "\tSource of Error, SRAM error" << endl;
//	out << "\t\tWrong Policy\t" << stats_error_SRAMwrongpolicy << endl;
//	out << "\t\tWrong Allocation\t" << stats_error_SRAMwrongallocation << endl;
//	out << "\t\tLearning\t" << stats_error_SRAMlearning << endl;		
	
	
	/*
	if(stats_nbSwitchDst.size() > 0)
	{
		double sum=0;
		for(auto d : stats_nbSwitchDst)
			sum+= d;
		out << "\t Average Switch\t" << sum / (double) stats_nbSwitchDst.size() << endl;	
	}*/
	


	double total = migrationNVM+migrationSRAM;

	if(total > 0){
		out << entete << ":DBAMBPredictor:NBMigration\t" << total << endl;
		out << entete << ":DBAMBPredictor:MigrationFromNVM\t" << migrationNVM << "\t" << migrationNVM*100/total << "%" << endl;
		out << entete << ":DBAMBPredictor:MigrationFromSRAM\t" << migrationSRAM << "\t" << migrationSRAM*100/total << "%" << endl;
	}
	
	Predictor::printStats(out, entete);
}

		

void 
DBAMBPredictor::openNewTimeFrame()
{ 

	if(m_isWarmup)
		return;

//	stats_switchDecision.push_back(vector<vector<int> >(NUM_ALLOC_DECISION,vector<int>(NUM_ALLOC_DECISION,0)));
	stats_nbMigrationsFromNVM.push_back(vector<int>(2,0));
	dataset_file << "TimeFrame" << endl;
	Predictor::openNewTimeFrame();
}


DHPEntry*
DBAMBPredictor::lookup(uint64_t pc)
{
	uint64_t set = indexFunction(pc);
	for(unsigned i = 0 ; i < m_RAP_assoc ; i++)
	{
		if(m_DHPTable[set][i]->m_pc == pc)
			return m_DHPTable[set][i];
	}
	return NULL;
}

uint64_t 
DBAMBPredictor::indexFunction(uint64_t pc)
{
	return pc % m_RAP_sets;
}



RD_TYPE
DBAMBPredictor::evaluateRd(vector<int> reuse_distances)
{
	if(reuse_distances.size() == 0)
		return RD_NOT_ACCURATE;
	
	return convertRD(reuse_distances.back());
}

RD_TYPE
DBAMBPredictor::convertRD(int rd)
{

	if(simu_parameters.sram_assoc <= simu_parameters.nvm_assoc)
	{	
		if(rd < simu_parameters.sram_assoc)
			return RD_SHORT;
		else if(rd < simu_parameters.nvm_assoc)
			return RD_MEDIUM;
		else 
			return RD_NOT_ACCURATE;
	}
	else
	{
		if(rd < simu_parameters.nvm_assoc)
			return RD_SHORT;
		else if(rd < simu_parameters.sram_assoc)
			return RD_MEDIUM;
		else 
			return RD_NOT_ACCURATE;
	
	}
}

/************* Replacement Policy implementation for the test RAP table ********/ 

DBAMBLRUPolicy::DBAMBLRUPolicy(int nbAssoc , int nbSet , std::vector<std::vector<DHPEntry*> >& rap_entries) :\
		DBAMBReplacementPolicy(nbAssoc , nbSet, rap_entries) 
{
	m_cpt = 1;
}

void
DBAMBLRUPolicy::updatePolicy(uint64_t set, uint64_t assoc)
{	

	
	m_rap_entries[set][assoc]->policyInfo = m_cpt;
	m_cpt++;

}

int
DBAMBLRUPolicy::evictPolicy(int set)
{
	for(unsigned i = 0 ; i < m_assoc ; i++){
		if(!m_rap_entries[set][i]->isValid)
			return i;
	}
	
	int smallest_time = m_rap_entries[set][0]->policyInfo , smallest_index = 0;
	for(unsigned i = 0 ; i < m_assoc ; i++){
		if(m_rap_entries[set][i]->policyInfo < smallest_time){
			smallest_time =  m_rap_entries[set][i]->policyInfo;
			smallest_index = i;
		}
	}
	return smallest_index;
}





