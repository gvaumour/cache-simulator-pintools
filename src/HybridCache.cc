/** 
Copyright (C) 2016 Gregory Vaumourin

This program is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation; either version 2 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program; if not, write to the Free Software
Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
**/

#include <map>	
#include <iomanip>
#include <string>
#include <vector>
#include <fstream>
#include <iostream>
#include <assert.h>
#include <math.h>

#include "HybridCache.hh"
#include "Cache.hh"
#include "common.hh"

#include "ReplacementPolicy.hh"
#include "SaturationPredictor.hh"
#include "InstructionPredictor.hh"
#include "DynamicSaturation.hh"
#include "CompilerPredictor.hh"
#include "RAPPredictor.hh"
#include "testRAPPredictor.hh"

using namespace std;


HybridCache::HybridCache(){
	m_nb_set = 0;
	m_assoc = 0;
}

HybridCache::HybridCache(int size , int assoc , int blocksize , int nbNVMways, string policy, Level* system){

	////DPRINTF("CACHE::Constructor de HybridCache\n");

	m_assoc = assoc;
	m_cache_size = size;
	m_blocksize = blocksize;
	m_nb_set = size / (assoc * blocksize);
	m_nbNVMways = nbNVMways;	
	m_nbSRAMways = m_assoc - m_nbNVMways;
	m_system = system;

	assert(isPow2(m_nb_set));
	assert(isPow2(m_blocksize));

	m_policy = policy;
	m_printStats = false;
	
	m_tableSRAM.resize(m_nb_set);
	m_tableNVM.resize(m_nb_set);
	for(int i = 0  ; i < m_nb_set ; i++){
		m_tableSRAM[i].resize(m_nbSRAMways);
		for(int j = 0 ; j < m_nbSRAMways ; j++){
			m_tableSRAM[i][j] = new CacheEntry();
		}

		m_tableNVM[i].resize(m_nbNVMways);
		for(int j = 0 ; j < m_nbNVMways ; j++){
			m_tableNVM[i][j] = new CacheEntry();
			m_tableNVM[i][j]->isNVM = true;
		}
	}

	if(m_policy == "preemptive")
		 m_predictor = new PreemptivePredictor(m_assoc, m_nb_set, m_nbNVMways, m_tableSRAM, m_tableNVM,this);	
	else if(m_policy == "LRU")
		 m_predictor = new LRUPredictor(m_assoc, m_nb_set, m_nbNVMways, m_tableSRAM, m_tableNVM, this);	
	else if(m_policy == "Saturation")
		 m_predictor = new SaturationCounter(m_assoc, m_nb_set, m_nbNVMways, m_tableSRAM, m_tableNVM , this);	
	else if(m_policy == "DynamicSaturation")
		 m_predictor = new DynamicSaturation(m_assoc, m_nb_set, m_nbNVMways, m_tableSRAM, m_tableNVM , this);	
	else if(m_policy == "Compiler")
		 m_predictor = new CompilerPredictor(m_assoc, m_nb_set, m_nbNVMways, m_tableSRAM, m_tableNVM , this);	
	else if(m_policy == "InstructionPredictor")
		 m_predictor = new InstructionPredictor(m_assoc, m_nb_set, m_nbNVMways, m_tableSRAM, m_tableNVM , this);	
	else if(m_policy == "RAP")
		 m_predictor = new RAPPredictor(m_assoc, m_nb_set, m_nbNVMways, m_tableSRAM, m_tableNVM , this);	
	else if(m_policy == "testRAP")
		 m_predictor = new testRAPPredictor(m_assoc, m_nb_set, m_nbNVMways, m_tableSRAM, m_tableNVM , this);	
	else {
		assert(false && "Cannot initialize predictor for HybridCache");
	}
			
	m_start_index = log2(blocksize)-1;
	m_end_index = log2(m_blocksize) + log2(m_nb_set);
	
	stats_missSRAM = vector<uint64_t>(2 , 0);
	stats_hitsSRAM = vector<uint64_t>(2 , 0);
	stats_cleanWBSRAM = 0;
	stats_dirtyWBSRAM = 0;

	stats_missNVM = vector<uint64_t>(2 , 0);
	stats_hitsNVM = vector<uint64_t>(2 , 0);
	stats_cleanWBNVM = 0;
	stats_dirtyWBNVM = 0;
	stats_evict = 0;
	
	stats_nbFetchedLines = 0;
	stats_nbLostLine = 0;
	stats_nbROlines = 0;
	stats_nbROaccess = 0;
	stats_nbRWlines = 0;
	stats_nbRWaccess = 0;
	stats_nbWOlines = 0 ;
	stats_nbWOaccess = 0;
	stats_bypass = 0;
	
	stats_histo_ratioRW.clear();
	
	// Record the number of operations issued by the cache 
	stats_operations = vector<uint64_t>(NUM_MEM_CMDS , 0); 

	////DPRINTF("CACHE::End of constructor de HybridCache\n");

}

HybridCache::HybridCache(const HybridCache& a) : HybridCache(a.getSize(), a.getAssoc() , a.getBlockSize(), a.getNVMways(), a.getPolicy(), a.getSystem()) 
{
}

HybridCache::~HybridCache(){
	for (int i = 0; i < m_nb_set; i++) {
		for (int j = 0; j < m_nbSRAMways; j++) {
		    delete m_tableSRAM[i][j];
		}
		for (int j = 0; j < m_nbNVMways; j++) {
		    delete m_tableNVM[i][j];
		}
	}
	delete m_predictor;
}

void 
HybridCache::finishSimu()
{
	for (int i = 0; i < m_nb_set; i++) {
		for (int j = 0; j < m_nbSRAMways; j++) {
		    updateStatsDeallocate(m_tableSRAM[i][j]);
		}
		for (int j = 0; j < m_nbNVMways; j++) {
		    updateStatsDeallocate(m_tableNVM[i][j]);
		}
	}
	m_predictor->finishSimu();
}

bool
HybridCache::lookup(Access element)
{	
	////DPRINTF("CACHE::Lookup of addr %#lx\n" ,  element.m_address);
	return getEntry(element.m_address) != NULL;
}

void  
HybridCache::handleAccess(Access element)
{
	uint64_t address = element.m_address;
	bool isWrite = element.isWrite();
 	int size = element.m_size;
 	
	stats_operations[element.m_type]++;
	
	assert(size > 0);
//	uint64_t block_addr = bitRemove(address , 0 , m_start_index+1);
	int id_set = addressToCacheSet(address);

	int stats_index = isWrite ? 1 : 0;

	CacheEntry* current = getEntry(address);
	
	if(current == NULL){ // The cache line is not in the hybrid cache, Miss !

		//Verify if the cache line is in missing tags 
		m_predictor->checkMissingTags(address , id_set);
		
		CacheEntry* replaced_entry = NULL;
		
		allocDecision des = m_predictor->allocateInNVM(id_set, element);
		
		if(des == BYPASS_CACHE )
		{
			//DPRINTF("CACHE::Bypassing the cache for this \n");
			stats_bypass++;
		}
		else
		{
		
			bool inNVM = (des == ALLOCATE_IN_NVM) ? true : false; 
			int id_assoc = -1;
			
			id_assoc = m_predictor->evictPolicy(id_set, inNVM);			
			
			if(inNVM){//New line allocated in NVM
				replaced_entry = m_tableNVM[id_set][id_assoc];
			}
			else{//Allocated in SRAM 
				replaced_entry = m_tableSRAM[id_set][id_assoc];
			}
				
			//Deallocate the cache line in the lower levels (inclusive system)
			if(replaced_entry->isValid){
//				if(m_printStats)
//					DPRINTF("CACHE::Invalidation of the cache line : %#lx , id_assoc %d\n" , replaced_entry->address, id_assoc);		
		
				m_system->signalDeallocate(replaced_entry->address); 
				//Inform the higher level of the deallocation
				m_system->signalWB(replaced_entry->address , replaced_entry->isDirty);	
				stats_evict++;
			}


			deallocate(replaced_entry);	
			allocate(address , id_set , id_assoc, inNVM, element.m_pc);
			m_predictor->insertionPolicy(id_set , id_assoc , inNVM, element);

			if(inNVM){
				//DPRINTF("CACHE::It is a Miss ! Block[%#lx] is allocated in the NVM cache : Set=%d, Way=%d\n", block_addr , id_set, id_assoc);
				stats_missNVM[stats_index]++;
				if(element.isWrite())
					m_tableNVM[id_set][id_assoc]->isDirty = true;
					
				m_tableNVM[id_set][id_assoc]->m_compilerHints = element.m_compilerHints;
			}
			else{
				////DPRINTF("CACHE::It is a Miss ! Block[%#lx] is allocated in the SRAM cache : Set=%d, Way=%d\n",block_addr, id_set, id_assoc);
				stats_missSRAM[stats_index]++;			
				if(element.isWrite())
					m_tableSRAM[id_set][id_assoc]->isDirty = true;
				
				m_tableSRAM[id_set][id_assoc]->m_compilerHints = element.m_compilerHints;
			}
		}
	}
	else{
		// It is a hit in the cache 		
		int id_assoc = -1;
		map<uint64_t,HybridLocation>::iterator p = m_tag_index.find(current->address);
		id_assoc = p->second.m_way;
		////DPRINTF("CACHE::It is a hit ! Block[%#lx] Found Set=%d, Way=%d\n" , block_addr, id_set, id_assoc);
		
		m_predictor->updatePolicy(id_set , id_assoc, current->isNVM, element , false);
		
		if(element.isWrite()){
			current->isDirty = true;
			current->nbWrite++;		
		}
		else
			current->nbRead++;
	
	
		if(current->isNVM)	
			stats_hitsNVM[stats_index]++;
		else
			stats_hitsSRAM[stats_index]++;
		
		
		current->m_compilerHints = element.m_compilerHints;
		////DPRINTF("CACHE::End of the Handler \n");
	}
}


void 
HybridCache::deallocate(CacheEntry* replaced_entry)
{
	uint64_t addr = replaced_entry->address;
	updateStatsDeallocate(replaced_entry);

	map<uint64_t,HybridLocation>::iterator it = m_tag_index.find(addr);	
	if(it != m_tag_index.end()){
		m_tag_index.erase(it);	
	}	

	replaced_entry->initEntry();
	
	
}

void
HybridCache::updateStatsDeallocate(CacheEntry* current)
{

	if(!current->isValid)
		return;
		
	if(stats_histo_ratioRW.count(current->nbWrite) == 0)
		stats_histo_ratioRW.insert(pair<int,int>(current->nbWrite , 0));
		
	stats_histo_ratioRW[current->nbWrite]++;
	
		
	if(current->nbWrite == 0 && current->nbRead > 0){
		stats_nbROlines++;
		stats_nbROaccess+= current->nbRead; 
	}
	else if(current->nbWrite > 0 && current->nbRead == 0){
		stats_nbWOlines++;
		stats_nbWOaccess+= current->nbWrite; 	
	}
	else if(current->nbWrite == 0 && current->nbRead == 0){	
		stats_nbLostLine++;
	}
	else{
		stats_nbRWlines++;
		stats_nbRWaccess+= current->nbWrite + current->nbRead; 	
	}
}

void
HybridCache::deallocate(uint64_t block_addr)
{
	////DPRINTF("CACHE::DEALLOCATE %#lx\n", block_addr);
	map<uint64_t,HybridLocation>::iterator it = m_tag_index.find(block_addr);	
	
	if(it != m_tag_index.end()){

		int id_set = blockAddressToCacheSet(block_addr);

		HybridLocation loc = it->second;
		CacheEntry* current = NULL; 

		if(loc.m_inNVM)
			current = m_tableNVM[id_set][loc.m_way]; 		
		else
			current = m_tableSRAM[id_set][loc.m_way]; 		

		updateStatsDeallocate(current);
		current->initEntry();
				
		m_tag_index.erase(it);	

	}
}

void 
HybridCache::handleWB(uint64_t block_addr, bool isDirty)
{	
	map<uint64_t,HybridLocation>::iterator it = m_tag_index.find(block_addr);		
	
	if(it != m_tag_index.end() )
	{
	
	
		HybridLocation loc = it->second;
		bool inNVM = loc.m_inNVM;
	
		if(isDirty){
			//Dirty WB updates the state of the cache line		 
			Access element;
			element.m_type = MemCmd::DIRTY_WRITEBACK;
			int id_set = blockAddressToCacheSet(block_addr);
			CacheEntry* current = NULL;

			if(inNVM){
				current = m_tableNVM[id_set][loc.m_way];
				stats_dirtyWBNVM++;
			}
			else {
				current = m_tableSRAM[id_set][loc.m_way];							
				stats_dirtyWBSRAM++;
			}
			current->nbWrite++;
			element.m_compilerHints = current->m_compilerHints;
			
			// The updatePolicy can provoke the migration so we update the nbWrite cpt before
			m_predictor->updatePolicy(id_set , loc.m_way , loc.m_inNVM, element , true);		

		}
		else{
			// Clean WB does not modify the state of the cache line 
			if(inNVM)
				stats_cleanWBNVM++;
			else 
				stats_cleanWBSRAM++;			
		}
	}

}

void 
HybridCache::allocate(uint64_t address , int id_set , int id_assoc, bool inNVM, uint64_t pc)
{

	uint64_t block_addr = bitRemove(address , 0 , m_start_index+1);
	
	if(inNVM){
	 	assert(!m_tableNVM[id_set][id_assoc]->isValid);

		m_tableNVM[id_set][id_assoc]->isValid = true;	
		m_tableNVM[id_set][id_assoc]->address = block_addr;
		m_tableNVM[id_set][id_assoc]->policyInfo = 0;
		m_tableNVM[id_set][id_assoc]->m_pc = pc;
	}
	else
	{
	 	assert(!m_tableSRAM[id_set][id_assoc]->isValid);

		m_tableSRAM[id_set][id_assoc]->isValid = true;	
		m_tableSRAM[id_set][id_assoc]->address = block_addr;
		m_tableSRAM[id_set][id_assoc]->policyInfo = 0;		
		m_tableSRAM[id_set][id_assoc]->m_pc = pc;		
	}
	
	stats_nbFetchedLines++;	
		
	HybridLocation loc(id_assoc, inNVM);
	m_tag_index.insert(pair<uint64_t , HybridLocation>(block_addr , loc));
}


void HybridCache::triggerMigration(int set, int id_assocSRAM, int id_assocNVM)
{
	////DPRINTF("CACHE::TriggerMigration set %d , id_assocSRAM %d , id_assocNVM %d\n" , set , id_assocSRAM , id_assocNVM);
	CacheEntry* sram_line = m_tableSRAM[set][id_assocSRAM];
	CacheEntry* nvm_line = m_tableNVM[set][id_assocNVM];

	uint64_t addrSRAM = sram_line->address;
	uint64_t addrNVM = nvm_line->address;
	
	bool isValidSRAM = sram_line->isValid;
	bool isValidNVM = nvm_line->isValid;

	bool isDirtySRAM = sram_line->isDirty;
	bool isDirtyNVM = nvm_line->isDirty;
		
	sram_line->address = addrNVM;
	sram_line->isNVM = false;
	sram_line->isDirty = isDirtyNVM;
	sram_line->isValid = isValidNVM;

	nvm_line->address = addrSRAM;
	nvm_line->isNVM = true;
	nvm_line->isDirty = isDirtySRAM;
	nvm_line->isValid = isValidSRAM;

	map<uint64_t,HybridLocation>::iterator p = m_tag_index.find(addrSRAM);
	p->second.m_inNVM = true;
	p->second.m_way = id_assocNVM;

	map<uint64_t,HybridLocation>::iterator p1 = m_tag_index.find(addrNVM);
	p1->second.m_inNVM = false;
	p1->second.m_way = id_assocSRAM;
}


int 
HybridCache::addressToCacheSet(uint64_t address) 
{
	uint64_t a = address;
	a = bitRemove(a , 0, m_start_index);
	a = bitRemove(a , m_end_index,64);
	
	a = a >> (m_start_index+1);
	assert(a < (unsigned int)m_nb_set);
	
	return (int)a;
}


int 
HybridCache::blockAddressToCacheSet(uint64_t block_addr) 
{
	uint64_t a =block_addr;
	a = bitRemove(a , m_end_index,64);
	
	a = a >> (m_start_index+1);
	assert(a < (unsigned int)m_nb_set);
	
	return (int)a;
}

CacheEntry*
HybridCache::getEntry(uint64_t addr)
{
	uint64_t block_addr =  bitRemove(addr , 0 , m_start_index+1);

	map<uint64_t,HybridLocation>::iterator p = m_tag_index.find(block_addr);
	
	if (p != m_tag_index.end()){
	
		int id_set = addressToCacheSet(addr);
		HybridLocation loc = p->second;
		if(loc.m_inNVM)
			return m_tableNVM[id_set][loc.m_way];
		else 
			return m_tableSRAM[id_set][loc.m_way];	
	

	}
	else{
		return NULL;
	}
}

void 
HybridCache::openNewTimeFrame()
{
	m_predictor->openNewTimeFrame();
}
	


void 
HybridCache::print(ostream& out) 
{
	printResults(out);	
}


void
HybridCache::printConfig(std::ostream& out) 
{		
		out << "Cache configuration : " << endl;
		out << "\t- Size : " << m_cache_size << endl;
		out << "\t- SRAM ways : " << m_nbSRAMways << endl;
		out << "\t- NVM ways : " << m_nbNVMways << endl;
		out << "\t- BlockSize : " << m_blocksize<< " bytes (bits 0 to " << m_start_index << ")" << endl;
		out << "\t- Sets : " << m_nb_set << " sets (bits " << m_start_index+1 << " to " << m_end_index << ")" << endl;
		out << "\t- Predictor : " << m_policy << endl;
		m_predictor->printConfig(out);
		out << "************************" << endl;
}


void 
HybridCache::printResults(std::ostream& out) 
{
		uint64_t total_missSRAM =  stats_missSRAM[0] + stats_missSRAM[1];
		uint64_t total_missNVM =  stats_missNVM[0] + stats_missNVM[1];
		uint64_t total_miss = total_missNVM + total_missSRAM;
		
		uint64_t total_hitSRAM =  stats_hitsSRAM[0] + stats_hitsSRAM[1];
		uint64_t total_hitNVM =  stats_hitsNVM[0] + stats_hitsNVM[1];
		uint64_t total_hits = total_hitNVM + total_hitSRAM;


		if(total_miss != 0){
		
			out << "Results : " << endl;
			out << "\t- Total access : "<< total_hits+ total_miss << endl;
			out << "\t- Total Hits : " << total_hits << endl;
			out << "\t- Total miss : " << total_miss << endl;		
			out << "\t- Miss Rate : " << (double)(total_miss)*100 / (double)(total_hits+total_miss) << "%"<< endl;
			out << "\t- Clean Write Back : " << stats_cleanWBNVM + stats_cleanWBSRAM << endl;
			out << "\t- Dirty Write Back : " << stats_dirtyWBNVM + stats_dirtyWBSRAM << endl;
			out << "\t- Eviction : " << stats_evict << endl;
			if(stats_bypass > 0)
				out << "\t- Bypass : " << stats_bypass << endl;
			
			
			out << endl;
			
			
			if(m_nbNVMways > 0){
			
				m_predictor->printStats(out);
				
				out << "NVM ways" << endl;
				out << "\t- NB Read : "<< stats_hitsNVM[0]<< endl;
				out << "\t- NB Write : "<< stats_hitsNVM[1] + stats_dirtyWBNVM << endl;		
			}
		
			if(m_nbSRAMways > 0){
				out << "SRAM ways" << endl;
				out << "\t- NB Read : "<< stats_hitsSRAM[0] << endl;
				out << "\t- NB Write : "<< stats_hitsSRAM[1] + stats_dirtyWBSRAM<< endl;	
			}
			//cout << "************************" << endl;
			
			if(m_printStats)
			{
				out << "Cache Line Classification" << endl;
				out << "nbFetchedLines :" << stats_nbFetchedLines << endl;
				out << "\t - NB Lost lines :\t" << stats_nbLostLine << " (" << \
					(double)stats_nbLostLine*100/(double)stats_nbFetchedLines << "%)" << endl;
				out << "\t - NB RO lines :\t" << stats_nbROlines << " (" << \
					(double)stats_nbROlines*100/(double)stats_nbFetchedLines << "%)\t" << stats_nbROaccess << endl;
				out << "\t - NB WO lines :\t" << stats_nbWOlines << " (" << \
					(double)stats_nbWOlines*100/(double)stats_nbFetchedLines << "%)\t" << stats_nbWOaccess << endl;
				out << "\t - NB RW lines :\t" << stats_nbRWlines << " (" << \
					(double)stats_nbRWlines*100/(double)stats_nbFetchedLines << "%)\t" << stats_nbRWaccess << endl;
				/*
				out << "Histogram NB Write" << endl;
				for(auto p : stats_histo_ratioRW){
					out << p.first << "\t" << p.second << endl;
				}*/
			}

			out << "Instruction Distributions" << endl;
	
			for(unsigned i = 0 ; i < stats_operations.size() ; i++){
				if(stats_operations[i] != 0)
					out << "\t" << memCmd_str[i]  << " : " << stats_operations[i] << endl;
			}		
		}


}

//ostream&
//operator<<(ostream& out, const HybridCache& obj)
//{
//    obj.print(out);
//    out << flush;
//    return out;
//}
