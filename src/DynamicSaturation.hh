#ifndef DYNAMIC_SATURATION_PREDICTOR_HH_
#define DYNAMIC_SATURATION_PREDICTOR_HH_

#include <vector>
#include <ostream>

#include "Predictor.hh"
#include "common.hh"
#include "HybridCache.hh"
#include "Cache.hh"


#define PREDICTOR_DYNAMIC_OUTPUT_FILE "DynamicPredictor.out"


class Predictor;
class HybridCache;


class DynamicSaturation : public Predictor {

	public :
//		DynamicSaturation();
		DynamicSaturation(int nbAssoc , int nbSet, int nbNVMways, DataArray& SRAMtable, DataArray& NVMtable, HybridCache* cache);
			
		allocDecision allocateInNVM(uint64_t set, Access element);
		void updatePolicy(uint64_t set, uint64_t index, bool inNVM, Access element, bool isWBrequest);
		void insertionPolicy(uint64_t set, uint64_t index, bool inNVM, Access element);
		void evictRecording( int id_set , int id_assoc , bool inNVM) { Predictor::evictRecording(id_set, id_assoc, inNVM);};
		int evictPolicy(int set, bool inNVM);
		void finishSimu() {};
		void printConfig(std::ostream& out, std::string entete) { Predictor::printConfig(out, entete); };
		void printStats(std::ostream& out, std::string entete);

		void openNewTimeFrame();
		~DynamicSaturation();
		
	private : 
		int m_cpt;
		int m_thresholdSRAM;
		int m_thresholdNVM;
		std::vector<int> stats_nbMigrationsFromNVM;
		std::vector<int> stats_nbMigrationsFromSRAM;
		std::vector<int> stats_nbCoreError;
		std::vector<int> stats_nbWBError;

		std::vector<int> stats_threshold_SRAM;
		std::vector<int> stats_threshold_NVM;

};

#endif
