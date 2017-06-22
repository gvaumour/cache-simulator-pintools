#ifndef PREDICTORS_HH_
#define PREDICTORS_HH_

#include <vector>
#include <ostream>
#include <fstream>

#include "Cache.hh"
#include "HybridCache.hh"
#include "common.hh"
#include "ReplacementPolicy.hh"

#define PREDICTOR_OUTPUT_FILE "predictor.out"


class CacheEntry;
class Access;	
class HybridCache;
class ReplacementPolicy;

class MissingTagEntry{

	public : 
		uint64_t addr;
		uint64_t last_time_touched;
		bool isValid;

		MissingTagEntry() : addr(0) , last_time_touched(0), isValid(false) { };
		MissingTagEntry(uint64_t a , uint64_t t, bool v) : addr(a) , last_time_touched(t), isValid(v) {};
	
};

class Predictor{
	
	public : 
//		Predictor();
		Predictor(int nbAssoc , int nbSet, int nbNVMways, DataArray& SRAMtable , DataArray& NVMtable, HybridCache* cache);
		virtual ~Predictor();

		virtual allocDecision allocateInNVM(uint64_t set, Access element) = 0; // Return true to allocate in NVM
		virtual void updatePolicy(uint64_t set, uint64_t index, bool inNVM, Access element, bool isWBrequest) = 0;
		virtual void insertionPolicy(uint64_t set, uint64_t index, bool inNVM, Access element) = 0;
		virtual int evictPolicy(int set, bool inNVM) =0;
		virtual void printConfig(std::ostream& out) = 0;
		virtual void printStats(std::ostream& out);
	
		void insertRecord(int set, int assoc, bool inNVM);
		void checkMissingTags(uint64_t block_addr , int id_set);
		void evictRecording(int id_set , int id_assoc , bool inNVM);
		virtual void openNewTimeFrame();
		
	protected : 		
		DataArray& m_tableSRAM;
		DataArray& m_tableNVM;
						
		int m_nb_set;
		int m_assoc;
		int m_nbNVMways;
		int m_nbSRAMways;
		ReplacementPolicy* m_replacementPolicyNVM_ptr;
		ReplacementPolicy* m_replacementPolicySRAM_ptr;

		HybridCache* m_cache;
		 
		uint64_t stats_nbLLCaccessPerFrame;
		
		bool m_trackError;
		std::vector<std::vector<MissingTagEntry*> > missing_tags;
		std::vector<int> stats_NVM_errors;
		std::vector<int> stats_SRAM_errors;		
		int stats_WBerrors;
		int stats_COREerrors;

};


class LRUPredictor : public Predictor{

	public :
//		LRUPredictor() : Predictor(){ m_cpt=0;};
		LRUPredictor(int nbAssoc , int nbSet, int nbNVMways, DataArray& SRAMtable, DataArray& NVMtable, HybridCache* cache);
			
		allocDecision allocateInNVM(uint64_t set, Access element);
		void updatePolicy(uint64_t set, uint64_t index, bool inNVM, Access element, bool isWBrequest);
		void insertionPolicy(uint64_t set, uint64_t index, bool inNVM, Access element);
		int evictPolicy(int set, bool inNVM);
		void evictRecording( int id_set , int id_assoc , bool inNVM) { Predictor::evictRecording(id_set, id_assoc, inNVM);};
		void openNewTimeFrame() { };

		void printStats(std::ostream& out) { Predictor::printStats(out);};
		void printConfig(std::ostream& out) { };
		~LRUPredictor() {};
		
	private : 
		uint64_t m_cpt;
		
};


class PreemptivePredictor : public LRUPredictor {
	public:
		
//		PreemptivePredictor() : LRUPredictor() {};
		PreemptivePredictor(int nbAssoc , int nbSet, int nbNVMways, DataArray& SRAMtable, \
			DataArray& NVMtable, HybridCache* cache) : LRUPredictor(nbAssoc, nbSet, nbNVMways, SRAMtable, NVMtable, cache) {};
	
		allocDecision allocateInNVM(uint64_t set, Access element);

};




#endif
