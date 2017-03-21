#ifndef PREDICTORS_HH_
#define PREDICTORS_HH_

#include <vector>
#include <ostream>
#include "Cache.hh"
#include "common.hh"
#include "ReplacementPolicy.hh"

class CacheEntry;
class Access;	
class ReplacementPolicy;

class Predictor{
	
	public : 
		Predictor();
		Predictor(int nbAssoc , int nbSet, int nbNVMways, DataArray SRAMtable , DataArray NVMtable);

		virtual bool allocateInNVM(uint64_t set, Access element) = 0; // Return true to allocate in NVM
		virtual void updatePolicy(uint64_t set, uint64_t index, bool inNVM, Access element) = 0;
		virtual void insertionPolicy(uint64_t set, uint64_t index, bool inNVM, Access element) = 0;
		virtual int evictPolicy(int set, bool inNVM) = 0;
		virtual void printStats(std::ostream& out) = 0;
		
	protected : 
		DataArray m_tableSRAM;
		DataArray m_tableNVM;
						
		int m_nb_set;
		int m_assoc;
		int m_nbNVMways;
		int m_nbSRAMways;
		ReplacementPolicy* m_replacementPolicyNVM_ptr;
		ReplacementPolicy* m_replacementPolicySRAM_ptr;
};


class LRUPredictor : public Predictor{

	public :
		LRUPredictor() : Predictor(){ m_cpt=0;};
		LRUPredictor(int nbAssoc , int nbSet, int nbNVMways, DataArray SRAMtable, \
			DataArray NVMtable);
			
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
			DataArray NVMtable) : LRUPredictor(nbAssoc, nbSet, nbNVMways, SRAMtable, NVMtable) {};
	
		bool allocateInNVM(uint64_t set, Access element);
};



class SaturationCounter : public Predictor{

	public :
		SaturationCounter();
		SaturationCounter(int nbAssoc , int nbSet, int nbNVMways, DataArray SRAMtable, DataArray NVMtable);
			
		bool allocateInNVM(uint64_t set, Access element);
		void updatePolicy(uint64_t set, uint64_t index, bool inNVM, Access element);
		void insertionPolicy(uint64_t set, uint64_t index, bool inNVM, Access element);
		int evictPolicy(int set, bool inNVM);
		void printStats(std::ostream& out);
		~SaturationCounter();
		
	private : 
		int m_cpt;
		int threshold;
		std::vector<int> stats_nbMigrationsFromNVM;
};


#endif
