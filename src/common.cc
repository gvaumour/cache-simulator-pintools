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

#include <stdint.h>
#include <assert.h>
#include <stdlib.h> 
#include <string.h>
#include <map>
#include <set>
#include <vector>
#include <iostream>
#include <string>
#include <sstream>

#include "common.hh"


using namespace std;


uint64_t cpt_time;

const char* memCmd_str[] = { "INST_READ", "INST_PREFETCH", "DATA_READ", "DATA_WRITE", "DATA_PREFETCH", "CLEAN_WRITEBACK", \
	"DIRTY_WRITEBACK", "SILENT_WRITEBACK", "INSERT", "EVICTION", "ACE"};

const char* allocDecision_str[] = {"ALLOCATE_IN_SRAM", "ALLOCATE_IN_NVM" , "BYPASS_CACHE", "ALLOCATE_PREEMPTIVELY"};


const char* directory_state_str[] = {"SHARED_L1" , "MODIFIED_L1", "EXCLUSIVE_L1", "CLEAN_LLC", "DIRTY_LLC" , "NOT_PRESENT"};

const char* simulation_debugflags[] = {"DebugCache", "DebugDBAMB", "DebugHierarchy", "DebugFUcache"};


SimuParameters simu_parameters;


vector<string> 
split(string s, char delimiter){
  vector<string> result;
  std::istringstream ss( s );
  while (!ss.eof())
  {
		string field;
		getline( ss, field, delimiter );
		if (field.empty()) continue;
		result.push_back( field );
  }
  return result;
}


uint64_t
bitRemove(uint64_t address , unsigned int small, unsigned int big){
    uint64_t mask;
    assert(big >= small);

	if (small == 0) {
		mask = (uint64_t)~0 << big;
		return (address & mask);
	} 
	else {
		mask = ~((uint64_t)~0 << small);
		uint64_t lower_bits = address & mask;
		mask = (uint64_t)~0 << (big + 1);
		uint64_t higher_bits = address & mask;

		higher_bits = higher_bits >> (big - small + 1);
		return (higher_bits | lower_bits);
	}
}

/** Fonction qui convertit une adresse en hexa de type "0x0774" en uint64_t */ 
uint64_t
hexToInt(string adresse_hex){
	char * p;
	uint64_t n = strtoul( adresse_hex.c_str(), & p, 16 ); 

	if ( * p != 0 ) {  
		cout << "not a number" << endl;
		return 0;
	}    
	else   
		return n;
}

static char * mydummy_var;

uint64_t
hexToInt1(const char* adresse_hex){
	uint64_t n = strtoul( adresse_hex, &mydummy_var, 16 ); 

	if ( *mydummy_var != 0 ) {  
		cout << "not a number" << endl;
		return 0;
	}    
	else   
		return n;
}


bool
readInputArgs(int argc , char* argv[] , int& sizeCache , int& assoc , int& blocksize, std::string& filename, std::string& policy)
{
	
	if(argc == 1){
		sizeCache = 512;
		assoc = 4;
		blocksize = 64;
		filename = "test.txt";		
		policy = "LRU";

	}
	else if(argc == 3){
		sizeCache = 16384;
		assoc = 4;
		blocksize = 64;
		filename = argv[1];	
		policy = argv[2];	
	}
	else if(argc == 6){
		sizeCache = atoi(argv[1]);
		assoc = atoi(argv[2]);
		blocksize = atoi(argv[3]);
		policy = argv[4];
		filename = argv[5];		
	}
	else{
		return false;
	}		
	return true;
}

bool			
isPow2(int x)
{ 
	return (x & (x-1)) == 0;
}


std::string
convert_hex(int n)
{
   std::stringstream ss;
   ss << std::hex << n;
   return ss.str();
}


const char *
StripPath(const char * path)
{
    const char * file = strrchr(path,'/');
    if (file)
        return file+1;
    else
        return path;
}


void
init_default_parameters()
{
	simu_parameters.enableBP = false;
	simu_parameters.enableMigration= false;
	simu_parameters.flagTest = true;	
	simu_parameters.printDebug = false;
	
	simu_parameters.deadSaturationCouter = 3;
	simu_parameters.window_size = 20; 
	simu_parameters.rap_innacuracy_th = 0.9;
	simu_parameters.learningTH = 20;
	
//	simu_parameters.sram_assoc = 4;
//	simu_parameters.nvm_assoc = 12;
	simu_parameters.sram_assoc = 16;
	simu_parameters.nvm_assoc = 0;
	simu_parameters.nb_sets = 1024;

	simu_parameters.rap_assoc = 128;
	simu_parameters.rap_sets = 128;
	
	simu_parameters.prefetchDegree = 2;
	simu_parameters.prefetchStreams = 16; 
	simu_parameters.enablePrefetch = false;

	simu_parameters.enableReuseErrorComputation = false;
	simu_parameters.enablePCHistoryTracking = false;
	
	simu_parameters.nbCores = 1;
	simu_parameters.policy = "DBAMB";

	simu_parameters.DBAMP_optTarget = "energy";
	
	simu_parameters.saturation_threshold = 2;
	
	simu_parameters.cost_threshold = 50;
	
	simu_parameters.nb_bits = 64;

	simu_parameters.sizeMTtags = 4;//simu_parameters.nvm_assoc - simu_parameters.sram_assoc;
}


