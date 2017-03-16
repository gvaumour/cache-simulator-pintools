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

using namespace std;



HybridCache::HybridCache(){
	m_nb_set = 0;
	m_assoc = 0;
}

HybridCache::HybridCache(int size , int assoc , int blocksize , int nbNVMways, string policy, Level* system){

//	cout << "Constructor de HybridCache" << endl;
	nb_double = 0;
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
		 m_predictor = new PreemptivePredictor(m_assoc, m_nb_set, m_nbNVMways, m_tableSRAM, m_tableNVM);	
	else if(m_policy == "LRU")
		 m_predictor = new LRUPredictor(m_assoc, m_nb_set, m_nbNVMways, m_tableSRAM, m_tableNVM);	
	else {
		assert(false && "Cannot initialize predictor for HybridCache");
	}
			
	m_start_index = log2(blocksize)-1;
	m_end_index = log2(m_blocksize) + log2(m_nb_set);
	
	stats_missSRAM = vector<int>(2 , 0);
	stats_hitsSRAM = vector<int>(2 , 0);
	stats_cleanWBSRAM = 0;
	stats_dirtyWBSRAM = 0;

	stats_missNVM = vector<int>(2 , 0);
	stats_hitsNVM = vector<int>(2 , 0);
	stats_cleanWBNVM = 0;
	stats_dirtyWBNVM = 0;


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
}

bool
HybridCache::lookup(Access element)
{
	return getEntry(element.m_address) != NULL;
}

CacheResponse 
HybridCache::handleAccess(Access element)
{
	uint64_t address = element.m_address;
	bool isWrite = element.isWrite();
 	int size = element.m_size;
	CacheResponse result;

	assert(size > 0);
	uint64_t line_address_begin = bitRemove(address , 0 , m_start_index+1);
	int id_set = addressToCacheSet(line_address_begin);
//	uint64_t line_address_end = bitRemove(address + size-1 , 0 , m_start_index+1);

	int stats_index = isWrite ? 1 : 0;

	CacheEntry* current = getEntry(line_address_begin);
	
	if(current == NULL){ // The cache line is not in the hybrid cache, Miss !


		CacheEntry* replaced_entry = NULL;
		bool inNVM = m_predictor->allocateInNVM(id_set, element);
		int id_assoc = -1;
		if(inNVM){//New line allocated in NVM
			id_assoc = m_predictor->evictPolicy(id_set, inNVM);			
			replaced_entry = m_tableNVM[id_set][id_assoc];
		}
		else{//Allocated in SRAM 
			id_assoc = m_predictor->evictPolicy(id_set, inNVM);
			replaced_entry = m_tableSRAM[id_set][id_assoc];
		}
		
		
		if(replaced_entry->isDirty){
			//Data is clean no need to write back the cache line 
			result.m_response = Request::DIRTY_MISS;
			result.m_addr = replaced_entry->address;

			if(inNVM)
				stats_dirtyWBNVM++;
			else 
				stats_dirtyWBSRAM++;
		}
		else{
			//Write Back the data 
			result.m_response = Request::CLEAN_MISS;
			if(inNVM)
				stats_cleanWBNVM++;
			else 
				stats_cleanWBSRAM++;			
		}

		//Deallocate the cache line in the lower levels (inclusive system)
		m_system->signalDeallocate(replaced_entry->address); 

		deallocate(replaced_entry);	

		allocate(line_address_begin , id_set , id_assoc, inNVM);
		m_predictor->insertionPolicy(id_set , id_assoc , inNVM, element);

		if(inNVM){
			DPRINTF("It is a Miss ! A new line is allocated in the NVM cache : Set=%d, Way=%d\n", id_set, id_assoc);
			stats_missNVM[stats_index]++;
			if(element.isWrite())
				m_tableNVM[id_set][id_assoc]->isDirty = true;
					
		}
		else{
			DPRINTF("It is a Miss ! A new line is allocated in the SRAM cache : Set=%d, Way=%d\n", id_set, id_assoc);
			stats_missSRAM[stats_index]++;			
			if(element.isWrite())
				m_tableSRAM[id_set][id_assoc]->isDirty = true;
		}



	}
	else{
		result.m_response = Request::HIT;
		
		int id_assoc = -1;
		map<uint64_t,HybridLocation>::iterator p = m_tag_index.find(current->address);
		id_assoc = p->second.m_way;
		DPRINTF("It is a hit ! Found Set=%d, Way=%d\n" , id_set, id_assoc);

		m_predictor->updatePolicy(id_set , id_assoc, current->isNVM, element);
		
		if(element.isWrite())
			current->isDirty = true;
	
		if(current->isNVM)	
			stats_hitsNVM[stats_index]++;
		else
			stats_hitsSRAM[stats_index]++;
	}
	
	return result;
}


void 
HybridCache::deallocate(CacheEntry* replaced_entry)
{
	uint64_t addr = replaced_entry->address;
	map<uint64_t,HybridLocation>::iterator it = m_tag_index.find(addr);	
	if(it != m_tag_index.end()){
		m_tag_index.erase(it);	
	}
	
	replaced_entry->initEntry();
}


void
HybridCache::deallocate(uint64_t addr)
{
	map<uint64_t,HybridLocation>::iterator it = m_tag_index.find(addr);	
	
	if(it != m_tag_index.end()){
		int id_set = addressToCacheSet(addr);
		HybridLocation loc = it->second;
		if(loc.m_inNVM){
			m_tableNVM[id_set][loc.m_way]->initEntry();	
			m_tableNVM[id_set][loc.m_way]->isNVM = true;	
		}
		m_tag_index.erase(it);	
	}
}


void 
HybridCache::allocate(uint64_t address , int id_set , int id_assoc, bool inNVM)
{
	if(inNVM){
	 	assert(!m_tableNVM[id_set][id_assoc]->isValid);

		m_tableNVM[id_set][id_assoc]->isValid = true;	
		m_tableNVM[id_set][id_assoc]->isPresentInLowerLevel = true;	
		m_tableNVM[id_set][id_assoc]->address = address;
		m_tableNVM[id_set][id_assoc]->policyInfo = 0;
	}
	else
	{
	 	assert(!m_tableSRAM[id_set][id_assoc]->isValid);

		m_tableSRAM[id_set][id_assoc]->isValid = true;	
		m_tableSRAM[id_set][id_assoc]->isPresentInLowerLevel = true;	
		m_tableSRAM[id_set][id_assoc]->address = address;
		m_tableSRAM[id_set][id_assoc]->policyInfo = 0;		
	}
	
	HybridLocation loc(id_assoc, inNVM);
	m_tag_index.insert(pair<uint64_t , HybridLocation>(address , loc));
}

void 
HybridCache::isWrittenBack(CacheResponse cacherep)
{
	Request req = cacherep.m_response;
	uint64_t addr = cacherep.m_addr;

	CacheEntry* current = getEntry(addr);	
		
	if(current)
	{
		current->isPresentInLowerLevel = false;
		if(req == Request::DIRTY_MISS){
			// We write the cacheline only if dirty, only update the cacheline tracking otherwise
			if(current->isNVM)
				stats_hitsNVM[1]++; 
			else 
				stats_hitsSRAM[1]++; 
								
			current->isDirty = true;
		}
	
	}
	else
		cout << "ERROR - Write Back a data that do not exist in higher levels" << endl;
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


CacheEntry*
HybridCache::getEntry(uint64_t addr)
{

	int id_set = addressToCacheSet(addr);
	map<uint64_t,HybridLocation>::iterator p = m_tag_index.find(addr);
	
	if (p != m_tag_index.end()){
		HybridLocation loc = p->second;
		if(loc.m_inNVM)
			return m_tableNVM[id_set][loc.m_way];
		else 
			return m_tableSRAM[id_set][loc.m_way];	
	}
	else
		return NULL;
}

double 
HybridCache::getConsoStatique(){
	assert(false && "Not implemented -- Should not go there");
	return 0.0;
}


double 
HybridCache::getConsoDynamique(){

	assert(false && "Not implemented -- Should not go there");
	double resultat = 0;
/*	
	int total_misses = stats_miss[0]+stats_miss[1];

	assert(consoDynHybridCache.count(m_cache_size) > 0);
	assert(consoDynHybridCache.count(SIZE_L2) > 0);
	
	int total_read_access = stats_hits[0] + stats_miss[0];
	int total_write_access = stats_hits[1] + stats_miss[1];
	int total_misses = stats_miss[0]+stats_miss[1];
	
	resultat = (total_read_access + total_write_access) * consoDynHybridCache[m_cache_size] + total_misses * consoDynHybridCache[SIZE_L2];
*/	
	return resultat;
}
		

ostream&
operator<<(ostream& out, const HybridCache& obj)
{
    obj.print(out);
    out << flush;
    return out;
}


void 
HybridCache::print(ostream& out) const 
{

	int spacePerCase = 20;
	out << endl;
	out << setw(spacePerCase) << "|";
	/*
//	for(int i = 0 ; i< m_assoc ; i++)
//		out << setw(spacePerCase-1) << string("Way "+ std::stoi(i)) << "|";
//	out << endl;   
	
	string barre="";
	for(int i = 0 ; i < spacePerCase-1 ; i++)
		barre +="-";
	barre +="|";

	for(int i = 0 ; i< m_assoc+1 ; i++)
		out << setw(spacePerCase) << barre;
	out << endl;   
	for(int i = 0 ; i< m_assoc+1 ; i++)
		out << setw(spacePerCase) << barre;
	out << endl;   

	int cpt_set=0;
	for(auto line : m_table){
		
//		out << setw(spacePerCase) << string("Set " + std::to_string(cpt_set) + "|");
		for(auto entry : line){
			if(entry->isValid){
				string word = "0x" + convert_hex(entry->address) ;//+ "(" + to_string(entry->policyInfo) + ")";
				out << setw(spacePerCase-1) << word << "|";
			}
			else
				out << setw(spacePerCase-1) << "INV" << "|";
				
		}
		out << endl;	
		for(int i = 0 ; i< m_assoc+1 ; i++)
			out << setw(spacePerCase) << barre;
		out << endl;   
	
		cpt_set++;
	}*/
	out << endl;
	
}

void 
HybridCache::printStats(){

		int total_missSRAM =  stats_missSRAM[0] + stats_missSRAM[1];
		int total_missNVM =  stats_missNVM[0] + stats_missNVM[1];
		int total_miss = total_missNVM + total_missSRAM;
		
		int total_hitSRAM =  stats_hitsSRAM[0] + stats_hitsSRAM[1];
		int total_hitNVM =  stats_hitsNVM[0] + stats_hitsNVM[1];
		int total_hits = total_hitNVM + total_hitSRAM;
		
		cout << "HybridCache configuration : " << endl;
		cout << "\t- Size : " << m_cache_size << endl;
		cout << "\t- SRAM ways : " << m_nbSRAMways << endl;
		cout << "\t- NVM ways : " << m_nbNVMways << endl;
		cout << "\t- BlockSize : " << m_blocksize<< " bytes (bits 0 to " << m_start_index << ")" << endl;
		cout << "\t- Sets : " << m_nb_set << " sets (bits " << m_start_index+1 << " to " << m_end_index << ")" << endl;
		cout << "\t- Predictor : " << m_policy << endl;
		cout << "************************" << endl;
		cout << "Results : " << endl;
		cout << "\t- Total access : "<< total_hits+ total_miss << endl;
		cout << "\t- Total Hits : " << total_hits << endl;
		cout << "\t- Total miss : " << total_miss << endl;		
		cout << "\t- Miss Rate : " << (double)(total_miss)*100 / (double)(total_hits+total_miss) << "%"<< endl;
		cout << "\t- Clean Write Back : " << stats_cleanWBNVM + stats_cleanWBSRAM << endl;
		cout << "\t- Dirty Write Back : " << stats_dirtyWBNVM + stats_dirtyWBSRAM << endl;

		cout << "NVM ways" << endl;
		cout << "\t- NB Read : "<< stats_hitsNVM[0] << endl;
		cout << "\t- NB Write : "<< stats_hitsNVM[1] << endl;

		cout << "SRAM ways" << endl;
		cout << "\t- NB Read : "<< stats_hitsSRAM[0] << endl;
		cout << "\t- NB Write : "<< stats_hitsSRAM[1] << endl;
}
