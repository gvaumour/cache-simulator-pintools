#ifndef SATURATION_PREDICTOR_HH_
#define SATURATION_PREDICTOR_HH_

#include <vector>
#include <ostream>

#include "Predictor.hh"
#include "common.hh"
#include "HybridCache.hh"
#include "Cache.hh"

class Predictor;
class HybridCache;

/*
	Implementation of the saturation-based predictor 
	"Power and performance of read-write aware Hybrid Caches with non-volatile memories,"
	Xiaoxia Wu, Jian Li, Lixin Zhang, E. Speight and Yuan Xie,
	DATE 2009
*/

class SaturationCounter : public Predictor {

	public :
		SaturationCounter();
		SaturationCounter(int nbAssoc , int nbSet, int nbNVMways, DataArray SRAMtable, DataArray NVMtable, HybridCache* cache);
			
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
