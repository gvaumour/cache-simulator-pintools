#ifndef PREDICTORS_HH_
#define PREDICTORS_HH_

#include <vector>

#include "Cache.hh"

class CacheEntry;
class Access;	

class Predictor{
	
	public : 
		Predictor(): m_tableSRAM(0), m_tableNVM(0), m_nb_set(0), m_assoc(0), m_nbNVMways(0), m_nbSRAMways(0) {};
		Predictor(int nbAssoc , int nbSet, int nbNVMways, std::vector<std::vector<CacheEntry*> > SRAMtable , std::vector<std::vector<CacheEntry*> > NVMtable) : \
				 m_tableSRAM(SRAMtable),\
				 m_tableNVM(NVMtable),\
				 m_nb_set(nbSet),\
  				 m_assoc(nbAssoc),\
				 m_nbNVMways(nbNVMways),\
				 m_nbSRAMways(nbAssoc - nbNVMways){};

		virtual bool allocateInNVM(uint64_t set, Access element) = 0; // Return true to allocate in NVM
		virtual void updatePolicy(uint64_t set, uint64_t index, bool inNVM, Access element) = 0;
		virtual void insertionPolicy(uint64_t set, uint64_t index, bool inNVM, Access element) = 0;
		virtual int evictPolicy(int set, bool inNVM) = 0;
		
	protected : 
		std::vector<std::vector<CacheEntry*> > m_tableSRAM;
		std::vector<std::vector<CacheEntry*> > m_tableNVM;
		int m_nb_set;
		int m_assoc;
		int m_nbNVMways;
		int m_nbSRAMways;
	
};


class PreemptivePredictor : public Predictor {
	public:
		
		PreemptivePredictor() : Predictor() {};
		PreemptivePredictor(int nbAssoc , int nbSet, int nbNVMways, std::vector<std::vector<CacheEntry*> > SRAMtable, \
			std::vector<std::vector<CacheEntry*> > NVMtable) : Predictor(nbAssoc, nbSet, nbNVMways, SRAMtable, NVMtable) {};
	
		bool allocateInNVM(uint64_t set, Access element);
		void updatePolicy(uint64_t set, uint64_t index, bool inNVM, Access element);
		void insertionPolicy(uint64_t set, uint64_t index, bool inNVM, Access element);
		int evictPolicy(int set, bool inNVM);		
};

class LRUPredictor : public Predictor{

	public :
		LRUPredictor() : Predictor(){ m_cpt=0;};
		LRUPredictor(int nbAssoc , int nbSet, int nbNVMways, std::vector<std::vector<CacheEntry*> > SRAMtable, \
			std::vector<std::vector<CacheEntry*> > NVMtable);
			
		bool allocateInNVM(uint64_t set, Access element);
		void updatePolicy(uint64_t set, uint64_t index, bool inNVM, Access element);
		void insertionPolicy(uint64_t set, uint64_t index, bool inNVM, Access element);
		int evictPolicy(int set, bool inNVM);
		~LRUPredictor() {};
		
	private : 
		int m_cpt;
		
};


#endif
