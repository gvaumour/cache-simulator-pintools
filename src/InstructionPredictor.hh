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



#define PC_THRESHOLD 3
//#define UTPUT_PREDICTOR_FILE "InstructionPredictor.out"


class InstructionPredictor : public Predictor {

	public :
//		InstructionPredictor();
		InstructionPredictor(int nbAssoc , int nbSet, int nbNVMways, DataArray& SRAMtable, DataArray& NVMtable, HybridCache* cache);
			
		allocDecision allocateInNVM(uint64_t set, Access element);
		void updatePolicy(uint64_t set, uint64_t index, bool inNVM, Access element , bool isWBrequest );
		void insertionPolicy(uint64_t set, uint64_t index, bool inNVM, Access element);
		int evictPolicy(int set, bool inNVM);
		void evictRecording( int id_set , int id_assoc , bool inNVM) { Predictor::evictRecording(id_set, id_assoc, inNVM);};
		void finishSimu() {};
		void printStats(std::ostream& out, std::string entete);
		void printConfig(std::ostream& out, std::string entete);		
		void openNewTimeFrame() { Predictor::openNewTimeFrame(); };

		~InstructionPredictor();
		
	protected : 
		uint64_t m_cpt;
		int threshold;
		std::map<uint64_t, int> pc_counters;

		std::map<uint64_t, std::pair<int,int> > stats_PCwrites;
		std::map<uint64_t, std::map< uint64_t, std::pair<int,int> > > stats_datasets;
		
};

#endif


