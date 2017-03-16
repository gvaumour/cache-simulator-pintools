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

#include "common.hh"
#include "Cache.hh"
#include "ReplacementPolicy.hh"

using namespace std;



Cache::Cache(){
	cout << "Cache: Null Constructor" << endl;
	m_nb_set = 0;
	m_assoc = 0;
}

Cache::Cache(int size , int assoc , int blocksize , string policy, Level* system){

	cout << "Cache: This constructor" << endl;
	cout << size << endl;
	cout << assoc << endl;
	cout << blocksize << endl;
	cout << policy << endl;
	
	nb_double = 0;
	m_assoc = assoc;
	m_cache_size = size;
	m_blocksize = blocksize;
	m_nb_set = size / (assoc * blocksize);
	assert(system != NULL);
	m_system = system;

	assert(isPow2(m_nb_set));
	assert(isPow2(m_blocksize));

	m_policy = policy;
	
	m_table.resize(m_nb_set);
	for(int i = 0  ; i < m_nb_set ; i++){
		m_table[i].resize(m_assoc);
		for(int j = 0 ; j < m_assoc ; j++){
			m_table[i][j] = new CacheEntry();
		}
	}

	if(m_policy == "LRU")
		m_replacementPolicy_ptr = new LRUPolicy(m_assoc,m_nb_set , m_table);	
	else if(m_policy == "Random")
		m_replacementPolicy_ptr = new RandomPolicy(m_assoc,m_nb_set , m_table);
	else {
		assert(false && "Erreur de choix de politique de remplacement ");
	}
			
	m_start_index = log2(blocksize)-1;
	m_end_index = log2(m_blocksize) + log2(m_nb_set);
	
	stats_miss = vector<int>(2 , 0);
	stats_hits = vector<int>(2 , 0);
	stats_cleanWB = 0;
	stats_dirtyWB = 0;
	cout << "Cache: End of this constructor" << endl;
}

Cache::Cache(const Cache& a) : Cache(a.getSize(), a.getAssoc() , a.getBlockSize() , a.getPolicy(), a.getSystem()) 
{
	cout<< "Cache:Copy constructor" << endl;
}

Cache::~Cache(){
	for (int i = 0; i < m_nb_set; i++) {
		for (int j = 0; j < m_assoc; j++) {
		    delete m_table[i][j];
		}
	}
}

bool
Cache::lookup(Access element)
{
	uint64_t address = element.m_address;
	uint64_t line_address_begin = bitRemove(address , 0 , m_start_index+1);

	int id_set = addressToCacheSet(line_address_begin);
	int id_assoc = findTagInSet(id_set , line_address_begin);
	return id_assoc != -1;
}

CacheResponse 
Cache::handleAccess(Access element)
{
	uint64_t address = element.m_address;
	bool isWrite = element.isWrite();
 	int size = element.m_size;
	int hints = 0;
	CacheResponse result;

	assert(size > 0);
	uint64_t line_address_begin = bitRemove(address , 0 , m_start_index+1);
//	uint64_t line_address_end = bitRemove(address + size-1 , 0 , m_start_index+1);

	int stats_index = isWrite ? 1 : 0;

	
	int id_set = addressToCacheSet(line_address_begin);
	int id_assoc = findTagInSet(id_set , line_address_begin);
	if(id_assoc == -1){


		id_assoc = m_replacementPolicy_ptr->evictPolicy(id_set);
		CacheEntry* replaced_entry = m_table[id_set][id_assoc];
		
		if(replaced_entry->isDirty){
			//Data is clean no need to write back the cache line 
			result.m_response = Request::DIRTY_MISS;
			result.m_addr = replaced_entry->address;
			stats_dirtyWB++;
		}
		else{
			//Write Back the data 
			result.m_response = Request::CLEAN_MISS;			
			stats_cleanWB++;
		}
		//if(replaced_entry->isPresentInLowerLevel){
		m_system->signalDeallocate(replaced_entry->address);
			//TO DO: Need to deallocate in lower cache 
		//}
		deallocate(replaced_entry);		

		DPRINTF("It is a Miss ! Allocated Set=%d, Way=%d\n", id_set, id_assoc);

		stats_miss[stats_index]++;
		allocate(line_address_begin , id_set , id_assoc);
		m_replacementPolicy_ptr->insertionPolicy(id_set , id_assoc , hints);

		if( element.isWrite())
			m_table[id_set][id_assoc]->isDirty = true;

	}
	else{
		DPRINTF("It is a hit ! Found Set=%d, Way=%d\n" , id_set, id_assoc);
		
		result.m_response = Request::HIT;
		m_replacementPolicy_ptr->updatePolicy(id_set , id_assoc , hints);
		if( element.isWrite())
			m_table[id_set][id_assoc]->isDirty = true;
		
		stats_hits[stats_index]++;
	}
	
	return result;
}


void 
Cache::deallocate(CacheEntry* replaced_entry)
{
	uint64_t addr = replaced_entry->address;
	map<uint64_t,int>::iterator it = m_tag_index.find(addr);	
	if(it != m_tag_index.end()){
		m_tag_index.erase(it);	
	}

	replaced_entry->initEntry();
}


void
Cache::deallocate(uint64_t addr)
{
	map<uint64_t,int>::iterator it = m_tag_index.find(addr);
	if(it != m_tag_index.end()){
		int id_assoc = it->second;
		int id_set = addressToCacheSet(addr);	

		if(id_assoc > 0)
			m_table[id_set][id_assoc]->initEntry();	
		
		m_tag_index.erase(it);	
	}
}


void 
Cache::allocate(uint64_t address , int id_set , int id_assoc)
{

 	assert(!m_table[id_set][id_assoc]->isValid);

	m_table[id_set][id_assoc]->isValid = true;	
	m_table[id_set][id_assoc]->isPresentInLowerLevel = true;	
	m_table[id_set][id_assoc]->address = address;
	m_table[id_set][id_assoc]->policyInfo = 0;
	
	m_tag_index.insert(pair<uint64_t , int>(address , id_assoc));
}

void 
Cache::isWrittenBack(CacheResponse cacherep)
{
	Request req = cacherep.m_response;
	uint64_t addr = cacherep.m_addr;
	
	int id_set = addressToCacheSet(addr);
	int id_assoc = findTagInSet(id_set , addr);
	
	if(id_assoc > 0)
	{
		m_table[id_set][id_assoc]->isPresentInLowerLevel = false;
		if(req == Request::DIRTY_MISS){
			stats_hits[1]++; // We write the cacheline only if dirty, only update the Cacheline tracking otherwise
			m_table[id_set][id_assoc]->isDirty = true;
		}
	
	}
	else
		cout << "ERROR - Write Back a data that do not exist in higher levels" << endl;
}

int 
Cache::addressToCacheSet(uint64_t address) 
{
	uint64_t a = address;
	a = bitRemove(a , 0, m_start_index);
	a = bitRemove(a , m_end_index,64);
	
	a = a >> (m_start_index+1);
	assert(a < (unsigned int)m_nb_set);
	
	return (int)a;
}


int 
Cache::findTagInSet(int id_set, uint64_t address) 
{
	
	map<uint64_t,int>::iterator p = m_tag_index.find(address);
	
	if (p != m_tag_index.end()){

		if (m_table[id_set][p->second] != NULL)
			return p->second;
	}
	return -1; // Not found
}


double 
Cache::getConsoStatique(){
	assert(false && "Not implemented -- Should not go there");
	return 0.0;
}


double 
Cache::getConsoDynamique(){

	assert(false && "Not implemented -- Should not go there");
	double resultat = 0;
/*	
	int total_misses = stats_miss[0]+stats_miss[1];

	assert(consoDynCache.count(m_cache_size) > 0);
	assert(consoDynCache.count(SIZE_L2) > 0);
	
	int total_read_access = stats_hits[0] + stats_miss[0];
	int total_write_access = stats_hits[1] + stats_miss[1];
	int total_misses = stats_miss[0]+stats_miss[1];
	
	resultat = (total_read_access + total_write_access) * consoDynCache[m_cache_size] + total_misses * consoDynCache[SIZE_L2];
*/	
	return resultat;
}
		

ostream&
operator<<(ostream& out, const Cache& obj)
{
    obj.print(out);
    out << flush;
    return out;
}


void 
Cache::print(ostream& out) const 
{

	int spacePerCase = 20;
	out << endl;
	out << setw(spacePerCase) << "|";
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
	}
	out << endl;
	
}

void 
Cache::printStats(){

		int total_read_access = stats_hits[0] + stats_miss[0];
		int total_write_access = stats_hits[1] + stats_miss[1];

	#ifdef RELEASE
		cout << m_policy << "\t" << (double)(stats_miss[0]+stats_miss[1])*100 / (double)(total_read_access + total_write_access) << "%"<< endl;	

	#else 
		/*cout << "== OUTPUT SIMULATION == " << endl;
		cout << endl;
		*/
		
		cout << "Cache configuration : " << endl;
		cout << "\t- Size : " << m_cache_size << endl;
		cout << "\t- Assoc : " << m_assoc << endl;
		cout << "\t- BlockSize : " << m_blocksize<< " bytes (bits 0 to " << m_start_index << ")" << endl;
		cout << "\t- Sets : " << m_nb_set << " sets (bits " << m_start_index+1 << " to " << m_end_index << ")" << endl;
		cout << "\t- Replacement policy : " << m_policy << endl;
		//cout << endl;
		cout << "************************" << endl;
		cout << "Results : " << endl;
		//cout << endl;
		cout << "\t- Total access : " << total_write_access + total_read_access << endl;
		//cout << "\t- Read Accesses : " << total_read_access << endl;
		//cout << "\t- Write Accesses : " << total_write_access << endl;
		//cout << endl;

		cout << "\t- Total Hits : " << stats_hits[0] + stats_hits[1] << endl;
		//cout << "\t- Read Hits  : " << stats_hits[0]<< endl;
		//cout << "\t- Write Hits : " << stats_hits[1] << endl;
		//cout << endl;
		cout << "\t- Total miss : " << stats_miss[0] + stats_miss[1] << endl;		
		cout << "\t- Miss Rate : " << (double)(stats_miss[0]+stats_miss[1])*100 / (double)(total_read_access + total_write_access) << "%"<< endl;
		//cout << "\t- Read misses  : " << stats_miss[0]<< endl;
		cout << "\t- Clean Write Back : " << stats_cleanWB << endl;
		cout << "\t- Dirty Write Back : " << stats_dirtyWB << endl;
		
		//cout << "\t- Write Misses : " << stats_miss[1] << endl;
		//cout << "\t- Energy : " << getConsoDynamique()<< " nJ "  << endl;

		/*
		ofstream file_reuse("reuse.txt");
		for(auto p : stats_reuse){
			StatsBlock current = p.second;
			file_reuse << std::hex << p.first <<  std::dec << "\t"<< current.nbReuse << "\t" << current.nbEvict << endl; 
		}
		file_reuse.close();*/
	#endif

}
