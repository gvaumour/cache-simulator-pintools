#ifndef RAP_PREDICTOR_HH_
#define RAP_PREDICTOR_HH_

#include <vector>
#include <map>
#include <ostream>

#include "Predictor.hh"
#include "common.hh"
#include "HybridCache.hh"
#include "Cache.hh"


#define RAP_SATURATION_TH 1
#define RAP_OUTPUT_FILE "rap_predictor.out"
#define RAP_OUTPUT_FILE1 "rap_predictor1.out"

#define RAP_LEARNING_THRESHOLD 20

#define RAP_TABLE_ASSOC 2
#define RAP_TABLE_SET 128

#define RAP_SRAM_ASSOC 4
#define RAP_NVM_ASSOC 12 

#define DEADLINES 0
#define WOLINES 1
#define ROLINES 2
#define RWLINES 3

#define RD_SHORT 0
#define RD_MEDIUM 1
#define RD_LONG 2 


#define RD_INFINITE 1E9

class Predictor;
class HybridCache;


class RAPEntry
{
	public: 
		RAPEntry() { initEntry(); isValid = false; };
		void initEntry() {
		 	cpts =  std::vector<int>(4,0);
		 	lastWrite = 0;
		 	m_pc = -1;
		 	des = ALLOCATE_IN_SRAM; 
		 	policyInfo = 0;
			cptLearning = 0;
			reuse_distances.clear();
			nbAccess = 0;
			isValid = true;
			nbSwitch = 0;
			nbUpdate = 0;
//		 	rd_state = RD_UNKNOWN;
//			write_state = WRITE_UNKNOWN;
		 };
		/* Saturation counters for the classes of cl*/ 
		std::vector<int> cpts;

		int lastWrite;
		
		/* PC of the dataset */
		uint64_t m_pc; 
		
		allocDecision des;

		/* Replacement info bits */ 
		int policyInfo;

		/* Those parameters are static, they do not change during exec */ 
		int index,assoc;

		/* Those parameters are static, they do not change during exec */ 
		int cptLearning;
		
//		std::vector<allocDecision> stats_historyDecision;
		std::vector<int> reuse_distances;
		
		int nbAccess;
		
		bool isValid;
		
		int nbSwitch;
		int nbUpdate;
		
//		Write_Status write_state;
//		RD_Status rd_state;

};


class RAPReplacementPolicy{
	
	public : 
		RAPReplacementPolicy(int nbAssoc , int nbSet , std::vector<std::vector<RAPEntry*> >& rap_entries ) : m_rap_entries(rap_entries),\
											m_nb_set(nbSet) , m_assoc(nbAssoc) {};
		virtual void updatePolicy(uint64_t set, uint64_t index) = 0;
		virtual void insertionPolicy(uint64_t set, uint64_t index) = 0;
		virtual int evictPolicy(int set) = 0;
		
	protected : 
		std::vector<std::vector<RAPEntry*> >& m_rap_entries;
		int m_cpt;
		unsigned m_nb_set;
		unsigned m_assoc;
};


class RAPLRUPolicy : public RAPReplacementPolicy {

	public :
		RAPLRUPolicy(int nbAssoc , int nbSet , std::vector<std::vector<RAPEntry*> >& rap_entries);
		void updatePolicy(uint64_t set, uint64_t index);
		void insertionPolicy(uint64_t set, uint64_t index) { updatePolicy(set,index);}
		int evictPolicy(int set);
	private : 
		uint64_t m_cpt;
		
};



class RAPPredictor : public Predictor {

	public :
//		RAPPredictor();
		RAPPredictor(int nbAssoc , int nbSet, int nbNVMways, DataArray& SRAMtable, DataArray& NVMtable, HybridCache* cache);
		~RAPPredictor();
			
		allocDecision allocateInNVM(uint64_t set, Access element);
		void updatePolicy(uint64_t set, uint64_t index, bool inNVM, Access element , bool isWBrequest );
		void insertionPolicy(uint64_t set, uint64_t index, bool inNVM, Access element);
		int evictPolicy(int set, bool inNVM);
		void evictRecording( int id_set , int id_assoc , bool inNVM) { Predictor::evictRecording(id_set, id_assoc, inNVM);};
		void printStats(std::ostream& out);
		void printConfig(std::ostream& out);
		void openNewTimeFrame();
		void finishSimu();
		RAPEntry* lookup(uint64_t pc);
		uint64_t indexFunction(uint64_t pc);
		int computeRd(uint64_t set, uint64_t index, bool inNVM);

	protected : 
		uint64_t m_cpt;
		int learningTHcpt;

		/* RAP Table Handlers	*/
		std::vector< std::vector<RAPEntry*> > m_RAPtable;
		RAPReplacementPolicy* m_rap_policy;
		unsigned m_RAP_assoc;
		unsigned m_RAP_sets; 
		
		unsigned stats_RAP_miss;
		unsigned stats_RAP_hits;
				
		/* Stats */
		std::vector< std::vector< std::vector<int> > > stats_switchDecision;		
		std::vector< std::vector<int> > stats_ClassErrors;
		std::vector<double> stats_nbSwitchDst;
};


void updateDecision(RAPEntry* entry);

#endif

