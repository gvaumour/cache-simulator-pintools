#ifndef COMPILER_PREDICTOR_HH_
#define COMPILER_PREDICTOR_HH_

#include <vector>
#include <map>
#include <ostream>

#include "SaturationPredictor.hh"
#include "common.hh"
#include "HybridCache.hh"
#include "Cache.hh"


class SaturationPredictor;
class HybridCache;


class CompilerPredictor : public SaturationCounter {

	public :
//		CompilerPredictor();
		CompilerPredictor(int nbAssoc , int nbSet, int nbNVMways, DataArray& SRAMtable, DataArray& NVMtable, HybridCache* cache);
			
		allocDecision allocateInNVM(uint64_t set, Access element);
		void updatePolicy(uint64_t set, uint64_t index, bool inNVM, Access element , bool isWBrequest );
		void insertionPolicy(uint64_t set, uint64_t index, bool inNVM, Access element);
		int evictPolicy(int set, bool inNVM);
		void evictRecording( int id_set , int id_assoc , bool inNVM) { SaturationCounter::evictRecording(id_set, id_assoc, inNVM);};

		void finishSimu() {};
		void printConfig(std::ostream& out, std::string entete);
		void printStats(std::ostream& out, std::string entete);

		~CompilerPredictor();
		
	private : 

};

#endif


