#ifndef PREDICTORS_HH_
#define PREDICTORS_HH_

#include <vector>
#include <queue>
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
		bool isBypassed;
	
		MissingTagEntry() : addr(0) , last_time_touched(0), isValid(false), isBypassed(false) { };
		MissingTagEntry(uint64_t a , uint64_t t, bool v) : addr(a) , last_time_touched(t), isValid(v), isBypassed(false) {};
	
};

class Predictor{
	
	public : 
//		Predictor();
		Predictor(int nbAssoc , int nbSet, int nbNVMways, DataArray& SRAMtable , DataArray& NVMtable, HybridCache* cache);
		virtual ~Predictor();

		virtual allocDecision allocateInNVM(uint64_t set, Access element) = 0; // Return true to allocate in NVM
		virtual void updatePolicy(uint64_t set, uint64_t index, bool inNVM, Access element, bool isWBrequest);
		virtual void insertionPolicy(uint64_t set, uint64_t index, bool inNVM, Access element);
		bool recordAllocationDecision(uint64_t set, Access element, allocDecision des);

		virtual int evictPolicy(int set, bool inNVM) =0;
		virtual void printConfig(std::ostream& out, std::string entete) ;
		virtual void finishSimu() = 0;
		virtual void printStats(std::ostream& out, std::string entete);
		virtual void openNewTimeFrame();

		virtual void startWarmup();
		virtual void stopWarmup();
	
//		void insertRecord(int set, int assoc, bool inNVM);
		bool reportMiss(uint64_t block_addr , int id_set);
		bool checkBPerror(uint64_t block_addr , int id_set);
		void migrationRecording();
		void evictRecording(int id_set , int id_assoc , bool inNVM);
		void updateFUcaches(uint64_t block_addr, bool inNVM);

	protected : 		
		DataArray& m_tableSRAM;
		DataArray& m_tableNVM;
						
		int m_nb_set;
		int m_assoc;
		int m_nbNVMways;
		int m_nbSRAMways;
		int m_assoc_MT;
		
		ReplacementPolicy* m_replacementPolicyNVM_ptr;
		ReplacementPolicy* m_replacementPolicySRAM_ptr;

		HybridCache* m_cache;
		 
		uint64_t stats_nbLLCaccessPerFrame;
		
		bool m_trackError;
		bool m_isWarmup;
		bool m_missingTags_SRAMtracking;
		
//		std::vector<std::vector<MissingTagEntry*> > SRAM_missing_tags;
		std::vector< std::vector<MissingTagEntry*> > m_missing_tags;
		std::vector< std::vector<MissingTagEntry*> >BP_missing_tags;

		std::vector<MissingTagEntry*> m_NVM_FU;
		std::vector<MissingTagEntry*> m_SRAM_FU;
		std::map<uint64_t,uint64_t> m_accessedBlocks;
		uint64_t stats_capacity_miss;
		uint64_t stats_nvm_conflict_miss;
		uint64_t stats_sram_conflict_miss;
		uint64_t stats_cold_miss;
		uint64_t stats_total_miss;
		

		std::vector<int> stats_NVM_errors;
		std::vector<int> stats_SRAM_errors;		
		std::vector<int> stats_BP_errors;		
		std::vector<int> stats_MigrationErrors;
		uint64_t stats_WBerrors;
		uint64_t stats_COREerrors;
};


class LRUPredictor : public Predictor{

	public :
//		LRUPredictor() : Predictor(){ m_cpt=0;};
		LRUPredictor(int nbAssoc , int nbSet, int nbNVMways, DataArray& SRAMtable, DataArray& NVMtable, HybridCache* cache);
			
		allocDecision allocateInNVM(uint64_t set, Access element);
		void updatePolicy(uint64_t set, uint64_t index, bool inNVM, Access element, bool isWBrequest);
		void insertionPolicy(uint64_t set, uint64_t index, bool inNVM, Access element);
		int evictPolicy(int set, bool inNVM);
		void finishSimu() { };
		void evictRecording( int id_set , int id_assoc , bool inNVM) { Predictor::evictRecording(id_set, id_assoc, inNVM);};
		void openNewTimeFrame() { };

		void printStats(std::ostream& out, std::string entete) { Predictor::printStats(out, entete);};
		void printConfig(std::ostream& out, std::string entete) { Predictor::printConfig(out, entete);};
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
