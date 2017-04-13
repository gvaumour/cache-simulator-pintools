#ifndef INSTRUCTION_PREDICTOR_HH_
#define INSTRUCTION_PREDICTOR_HH_

#include <vector>
#include <map>
#include <ostream>

#include "Predictor.hh"
#include "common.hh"
#include "HybridCache.hh"
#include "Cache.hh"


class Predictor;
class HybridCache;

/**
	Implementation of the Instruction-based predictor
	"Prediction Hybrid Cache: An Energy-Efficient STT-RAM Cache Architecture"
	J. Ahn, S. Yoo and K. Choi
	IEEE Transactions on Computers 2016
*/

class InstructionPredictor : public Predictor {

	public :
		InstructionPredictor();
		InstructionPredictor(int nbAssoc , int nbSet, int nbNVMways, DataArray SRAMtable, DataArray NVMtable, HybridCache* cache);
			
		bool allocateInNVM(uint64_t set, Access element);
		void updatePolicy(uint64_t set, uint64_t index, bool inNVM, Access element);
		void insertionPolicy(uint64_t set, uint64_t index, bool inNVM, Access element);
		int evictPolicy(int set, bool inNVM);
		void printStats(std::ostream& out);
		~InstructionPredictor();
		
	private : 
		int m_cpt;
		int threshold;
		std::map<uint64_t, int> pc_counters;

		std::map<uint64_t, int> stats_PCwrites;
		
};

#endif


