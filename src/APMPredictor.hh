#ifndef APM_PREDICTOR_HH_
#define APM_PREDICTOR_HH_

#include <vector>
#include <ostream>

#include "Predictor.hh"
#include "common.hh"
#include "HybridCache.hh"
#include "Cache.hh"

class Predictor;
class HybridCache;

#define SATURATION_TH 3

/*
	Implementation of the APM predictor 
	Adaptive placement and migration policy for an STT-RAM-based hybrid cache
	Xiaoxia Wu, Jian Li, Lixin Zhang, E. Speight and Yuan Xie,
	http://ieeexplore.ieee.org/document/6835933/
	HPCA 2014
*/

class APMPredictor : public Predictor {

	public :
		APMPredictor(int nbAssoc , int nbSet, int nbNVMways, DataArray& SRAMtable, DataArray& NVMtable, HybridCache* cache);
			
		allocDecision allocateInNVM(uint64_t set, Access element);
		void updatePolicy(uint64_t set, uint64_t index, bool inNVM, Access element, bool isWBrequest);
		void insertionPolicy(uint64_t set, uint64_t index, bool inNVM, Access element);
		void evictRecording( int id_set , int id_assoc , bool inNVM) { Predictor::evictRecording(id_set, id_assoc, inNVM);};
		int evictPolicy(int set, bool inNVM);
		void printStats(std::ostream& out, std::string entete);
		void printConfig(std::ostream& out, std::string entete);
		void openNewTimeFrame() { };
		void finishSimu() {};

		~APMPredictor();
		
	protected : 
		int m_cpt;
		int threshold;
		std::vector<int> stats_nbMigrationsFromNVM;
};

#endif
