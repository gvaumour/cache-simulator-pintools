#ifndef RAP_PREDICTOR_V2_HH_
#define RAP_PREDICTOR_V2_HH_

#include <vector>
#include <map>
#include <ostream>

#include "Predictor.hh"
#include "common.hh"
#include "HybridCache.hh"
#include "Cache.hh"
#include "RAPPredictor.hh"


class RAPPredictor_opt : public RAPPredictor {

	public :
		RAPPredictor_opt(int nbAssoc , int nbSet, int nbNVMways, DataArray& SRAMtable, DataArray& NVMtable, HybridCache* cache);
		~RAPPredictor_opt();
			
		allocDecision allocateInNVM(uint64_t set, Access element);
		int evictPolicy(int set, bool inNVM);
		
		void dumpDataset(RAPEntry* entry);

	protected : 

};



#endif

