#ifndef PREDICTORS_HH_
#define PREDICTORS_HH_

#include <vector>
#include <ostream>

#include "Cache.hh"
#include "HybridCache.hh"
#include "common.hh"
#include "ReplacementPolicy.hh"

class CacheEntry;
class Access;	
class HybridCache;
class ReplacementPolicy;

class Predictor{
	
	public : 
		Predictor();
		Predictor(int nbAssoc , int nbSet, int nbNVMways, DataArray SRAMtable , DataArray NVMtable, HybridCache* cache);

		virtual bool allocateInNVM(uint64_t set, Access element) = 0; // Return true to allocate in NVM
		virtual void updatePolicy(uint64_t set, uint64_t index, bool inNVM, Access element) = 0;
		virtual void insertionPolicy(uint64_t set, uint64_t index, bool inNVM, Access element) = 0;
		virtual int evictPolicy(int set, bool inNVM);
		virtual void printStats(std::ostream& out);
		
	protected : 		
		DataArray m_tableSRAM;
		DataArray m_tableNVM;
						
		int m_nb_set;
		int m_assoc;
		int m_nbNVMways;
		int m_nbSRAMways;
		ReplacementPolicy* m_replacementPolicyNVM_ptr;
		ReplacementPolicy* m_replacementPolicySRAM_ptr;

		HybridCache* m_cache;
			/*
		std::vector<std::vector<uint64_t> > missing_tags;
		std::vector<int> stats_SRAM_errors;
		std::vector<std::vector<int> > stats_NVM_errors;		
		*/
};


class LRUPredictor : public Predictor{

	public :
		LRUPredictor() : Predictor(){ m_cpt=0;};
		LRUPredictor(int nbAssoc , int nbSet, int nbNVMways, DataArray SRAMtable, DataArray NVMtable, HybridCache* cache);
			
		bool allocateInNVM(uint64_t set, Access element);
		void updatePolicy(uint64_t set, uint64_t index, bool inNVM, Access element);
		void insertionPolicy(uint64_t set, uint64_t index, bool inNVM, Access element);
		int evictPolicy(int set, bool inNVM);
		void printStats(std::ostream& out) {};
		~LRUPredictor() {};
		
	private : 
		int m_cpt;
		
};


class PreemptivePredictor : public LRUPredictor {
	public:
		
		PreemptivePredictor() : LRUPredictor() {};
		PreemptivePredictor(int nbAssoc , int nbSet, int nbNVMways, DataArray SRAMtable, \
			DataArray NVMtable, HybridCache* cache) : LRUPredictor(nbAssoc, nbSet, nbNVMways, SRAMtable, NVMtable, cache) {};
	
		bool allocateInNVM(uint64_t set, Access element);
};




#endif
