#ifndef RAP_PREDICTOR_HH_
#define RAP_PREDICTOR_HH_

#include <vector>
#include <map>
#include <ostream>

#include "Predictor.hh"
#include "common.hh"
#include "HybridCache.hh"
#include "Cache.hh"


#define SATURATION_TH 2
#define RAP_OUTPUT_FILE "rap_predictor.out"

#define RAP_LEARNING_THRESHOLD 100


#define DEADLINES 0
#define WOLINES 1
#define ROLINES 2
#define RWLINES 3


class Predictor;
class HybridCache;


class RAPEntry
{
	public: 
		RAPEntry() : cpts(std::vector<int>(4,0)), lastWrite(0), pc(0), des(ALLOCATE_IN_SRAM) { };
		std::vector<int> cpts;
		/*int nbRO;
		int nbRW;
		int nbDead;
		int nbWO;*/
		int lastWrite;
	
		uint64_t pc; 
		allocDecision des;
};

class RAPPredictor : public Predictor {

	public :
		RAPPredictor();
		RAPPredictor(int nbAssoc , int nbSet, int nbNVMways, DataArray SRAMtable, DataArray NVMtable, HybridCache* cache);
			
		allocDecision allocateInNVM(uint64_t set, Access element);
		void updatePolicy(uint64_t set, uint64_t index, bool inNVM, Access element , bool isWBrequest );
		void insertionPolicy(uint64_t set, uint64_t index, bool inNVM, Access element);
		int evictPolicy(int set, bool inNVM);
		void evictRecording( int id_set , int id_assoc , bool inNVM) { Predictor::evictRecording(id_set, id_assoc, inNVM);};
		void printStats(std::ostream& out);
		void openNewTimeFrame();
		~RAPPredictor();
		
	protected : 
		uint64_t m_cpt;
		int learningTHcpt;
		std::map<uint64_t, RAPEntry*> m_RAPtable;
		std::vector< std::vector<int> > stats_ClassErrors;
		
};


void updateDecision(RAPEntry* entry);

#endif


