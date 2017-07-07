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
#include <vector>
#include <ostream>

enum MemCmd{
	INST_READ,
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

enum allocDecision
{	
	ALLOCATE_IN_SRAM = 0,
	ALLOCATE_IN_NVM,
	BYPASS_CACHE,
	ALLOCATE_PREEMPTIVELY,
	NUM_ALLOC_DECISION
};

enum RD_TYPE
{
	RD_SHORT = 0,
	RD_MEDIUM,
	RD_LONG,
	RD_NOT_ACCURATE,
	NUM_RD_TYPE
};

enum RW_TYPE
{
	DEAD = 0,
	WO,
	RO,
	RW,
	RW_NOT_ACCURATE,
	NUM_RW_TYPE
};



enum Request
{
	HIT,
	CLEAN_MISS,
	DIRTY_MISS,
	NUM_REQUESTS
};

class Access{
	
	public : 
		Access() : m_address(0), m_size(0), m_pc(0), m_type(DATA_READ), m_idthread(0),isSRAMerror(false) {};
		Access(uint64_t address, int size, uint64_t pc , MemCmd type, int id_thread) : m_address(address), m_size(size), \
				m_pc(pc), m_hints(0), m_type(type) , m_idthread(id_thread), isSRAMerror(false) {};

		bool isWrite() { return m_type == MemCmd::DATA_WRITE || m_type == MemCmd::DIRTY_WRITEBACK;}
		bool isInstFetch() { return m_type == MemCmd::INST_READ || m_type == MemCmd::INST_PREFETCH;}
		
		void print(std::ostream& out) const;

		uint64_t m_address;
		int m_size;
		uint64_t m_pc;
		int m_hints;
		MemCmd m_type;		
		unsigned m_idthread;
		int m_compilerHints;
		bool isSRAMerror;
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
			lastWrite = false;
			isLearning = false;
			address = 0;
			policyInfo = 0; 
			saturation_counter = 0;
			m_pc = 0;
			nbRead = 0;
			nbWrite = 0;
			m_compilerHints = 0;
		}
		bool isValid;
		bool isDirty;
		uint64_t address;
		uint64_t m_pc;
		int policyInfo;
		int m_compilerHints;
		bool isNVM;
		
		int nbWrite;
		int nbRead;
		bool lastWrite;
		
		//field used only by the SaturationCounter Predictor
		int saturation_counter; 
		//field used only by the RAP predictor
		bool isLearning;
};

typedef std::vector<std::vector<CacheEntry*> > DataArray;

#endif

