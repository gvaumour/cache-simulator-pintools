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


#ifndef CACHE_HPP
#define CACHE_HPP

#include <map>
#include <stdint.h>
#include <vector>
#include <ostream>

#include "Hierarchy.hh"
#include "ReplacementPolicy.hh"

class ReplacementPolicy;
class Level;


enum MemCmd{
	INST_WRITE,
	INST_PREFETCH,
	DATA_READ,
	DATA_WRITE,
	DATA_PREFETCH,
	CLEAN_WRITEBACK,
	DIRTY_WRITEBACK,
	SILENT_WRITEBACK,
	INSERT,
	EVICTION,
	ACE,
	NUM_MEM_CMDS
};

enum Request
{
	HIT,
	CLEAN_MISS,
	DIRTY_MISS,
	NUM_REQUESTS
};
	
struct CacheResponse{

	public:
		Request m_response;
		uint64_t m_addr;
		CacheResponse() : m_response(Request::HIT), m_addr(0) {};
		CacheResponse(Request req ,  uint64_t addr) : m_response(req), m_addr(addr) {};
		bool isWB() {return m_response == Request::CLEAN_MISS || m_response == Request::DIRTY_MISS;}
};

class Access{
	
	public : 
		Access() : m_address(0), m_size(0), m_pc(0){};
		Access(uint64_t address, int size, uint64_t pc , MemCmd type) : m_address(address), m_size(size), m_pc(pc), m_hints(0), m_type(type) {};

		bool isWrite() const
		{
			return m_type == MemCmd::DATA_WRITE || m_type == MemCmd::INST_WRITE || m_type == MemCmd::DIRTY_WRITEBACK;		
		}

		void print(std::ostream& out) const;

		uint64_t m_address;
		int m_size;
		int m_pc;
		int m_hints;
		MemCmd m_type;		
};

class StatsBlock{
//	std::list<int> nbReuse;
	public :
		StatsBlock() : nbReuse(0),nbEvict(0) {};
		int nbReuse;
		int nbEvict;
};



class CacheEntry{
	public :
		CacheEntry() { 
			isNVM = false;
			initEntry();
		};

		void initEntry() {
			isValid = false; 
			isDirty = false;
			address = 0;
			isPresentInLowerLevel = false;
			policyInfo = 0; 
		}
		bool isPresentInLowerLevel;
		bool isValid;
		bool isDirty;
		uint64_t address;
		int policyInfo;
		bool isNVM;
};

class Cache{


	public : 
		Cache(int size , int assoc , int blocksize , std::string policy, Level* system);
		Cache();
		Cache(const Cache& a);
		~Cache();

		CacheResponse handleAccess(Access element);

		void printStats();
		void print(std::ostream& out) const;
		void isWrittenBack(CacheResponse cacherep);
		bool lookup(Access element);

		int addressToCacheSet(uint64_t address);
		int findTagInSet(int id_set, uint64_t address); 
		void deallocate(CacheEntry* entry);
		void deallocate(uint64_t addr);
		void allocate(uint64_t address , int id_set , int id_assoc);		

		int getSize() const { return m_cache_size;}
		int getBlockSize() const { return m_blocksize;}
		int getAssoc() const { return m_assoc;}
		int getNbSets() const { return m_nb_set;}
		int getStartBit() const { return m_start_index;}
		std::string getPolicy() const { return m_policy;}
		Level* getSystem() const { return m_system;}
		void setSystem(Level* sys) { m_system = sys;}
		
		double getConsoDynamique();
		double getConsoStatique();
		
		
	private :
		
		std::vector<int> stats_miss;
		std::vector<int> stats_hits;
		int stats_dirtyWB;
		int stats_cleanWB;
		
		std::map<uint64_t,StatsBlock> stats_reuse;

		std::vector<std::vector<CacheEntry*> > m_table;
		std::map<uint64_t , int> m_tag_index;    		
    
		ReplacementPolicy *m_replacementPolicy_ptr;
		std::string m_policy;
		int m_start_index;
		int m_end_index;
		int m_cache_size;
		int m_assoc;
		int m_nb_set;
		int m_blocksize;    
		int nb_double;

		Level* m_system;
};

std::ostream& operator<<(std::ostream& out, const Cache& obj);


#endif

