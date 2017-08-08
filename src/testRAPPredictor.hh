#ifndef TEST_RAP_PREDICTOR_HH_
#define TEST_RAP_PREDICTOR_HH_

#include <vector>
#include <map>
#include <ostream>

#include "Predictor.hh"
#include "common.hh"
#include "HybridCache.hh"
#include "Cache.hh"
#include "../RAP_config.hh"

#define RAP_OUTPUT_FILE "rap_predictor.out"
#define RAP_OUTPUT_FILE1 "rap_predictor1.out"
#define RAP_TEST_OUTPUT_DATASETS "rap_test_dataset.out"

#ifndef ENABLE_LAZY_MIGRATION
	#define ENABLE_LAZY_MIGRATION true
#endif

//#define ENABLE_BYPASS true
//#define RAP_DEAD_COUNTER_SATURATION 3
//#define RAP_LEARNING_THRESHOLD 20
//#define RAP_WINDOW_SIZE 20
//#define RAP_INACURACY_TH 0.7

#define TEST_RAP_TABLE_ASSOC 128
#define TEST_RAP_TABLE_SET 128

#define RAP_SRAM_ASSOC 4
#define RAP_NVM_ASSOC 12 

#define INDEX_READ 0
#define INDEX_WRITE 1

class Predictor;
class HybridCache;


struct HistoEntry
{
	RW_TYPE state_rw;
	RD_TYPE state_rd;
	int nbKeepState;
};


class testRAPEntry
{
	public: 
		testRAPEntry() { initEntry( Access()); isValid = false; };
		
		void initEntry(Access element) {
		 	cpts =  std::vector<int>(NUM_RW_TYPE,0);
		 	lastWrite = 0;
		 	m_pc = -1;
		 	
		 	des = ALLOCATE_PREEMPTIVELY;
		 					
		 	policyInfo = 0;
			cptBypassLearning = 0;
			rd_history.clear();
			rw_history.clear();
			nbAccess = 0;
			isValid = true;
			
			state_rw = RW_NOT_ACCURATE;
			state_rd = RD_NOT_ACCURATE;
			
			nbKeepCurrentState = 0;
			nbKeepState = 0;
			nbSwitchState = 0;
			cptWindow = 0;
			
			dead_counter = 0;
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

		int cptBypassLearning;
		
		std::vector<int> rd_history;
		std::vector<bool> rw_history;
		
		int nbAccess;
		int cptWindow;
		
		bool isValid;
		
		int nbKeepState;
		int nbSwitchState;
		int nbKeepCurrentState;
		
		RW_TYPE state_rw;
		RD_TYPE state_rd;

		std::vector<HistoEntry> stats_history;
		
		int dead_counter;
};


class testRAPReplacementPolicy{
	
	public : 
		testRAPReplacementPolicy(int nbAssoc , int nbSet , std::vector<std::vector<testRAPEntry*> >& rap_entries ) : m_rap_entries(rap_entries),\
											m_nb_set(nbSet) , m_assoc(nbAssoc) {};
		virtual void updatePolicy(uint64_t set, uint64_t index) = 0;
		virtual void insertionPolicy(uint64_t set, uint64_t index) = 0;
		virtual int evictPolicy(int set) = 0;
		
	protected : 
		std::vector<std::vector<testRAPEntry*> >& m_rap_entries;
		int m_cpt;
		unsigned m_nb_set;
		unsigned m_assoc;
};


class testRAPLRUPolicy : public testRAPReplacementPolicy {

	public :
		testRAPLRUPolicy(int nbAssoc , int nbSet , std::vector<std::vector<testRAPEntry*> >& rap_entries);
		void updatePolicy(uint64_t set, uint64_t index);
		void insertionPolicy(uint64_t set, uint64_t index) { updatePolicy(set,index);}
		int evictPolicy(int set);
	private : 
		uint64_t m_cpt;
		
};



class testRAPPredictor : public Predictor {

	public :
		testRAPPredictor(int nbAssoc , int nbSet, int nbNVMways, DataArray& SRAMtable, DataArray& NVMtable, HybridCache* cache);
		~testRAPPredictor();
			
		allocDecision allocateInNVM(uint64_t set, Access element);
		void updatePolicy(uint64_t set, uint64_t index, bool inNVM, Access element , bool isWBrequest );
		void insertionPolicy(uint64_t set, uint64_t index, bool inNVM, Access element);
		int evictPolicy(int set, bool inNVM);
		void evictRecording( int id_set , int id_assoc , bool inNVM) { Predictor::evictRecording(id_set, id_assoc, inNVM);};
		void printStats(std::ostream& out);
		void printConfig(std::ostream& out);
		void openNewTimeFrame();
		void finishSimu();
		testRAPEntry* lookup(uint64_t pc);
		uint64_t indexFunction(uint64_t pc);

		int computeRd(uint64_t set, uint64_t index, bool inNVM);
		RD_TYPE convertRD(int rd);
		RD_TYPE evaluateRd(std::vector<int> reuse_distances);
		
		void determineStatus(testRAPEntry* entry);
		allocDecision convertState(testRAPEntry* rap_current);
		void dumpDataset(testRAPEntry* entry);		

		void updateWindow(testRAPEntry* rap_current);
		
		void checkLazyMigration(testRAPEntry* rap_current , CacheEntry* current ,uint64_t set,bool inNVM , uint64_t index);

	protected : 
		uint64_t m_cpt;
		int learningTHcpt;

		/* RAP Table Handlers	*/
		std::vector< std::vector<testRAPEntry*> > m_RAPtable;
		testRAPReplacementPolicy* m_rap_policy;
		unsigned m_RAP_assoc;
		unsigned m_RAP_sets; 
		
		unsigned stats_RAP_miss;
		unsigned stats_RAP_hits;
				
		/* Stats */
//		std::vector< std::vector< std::vector<int> > > stats_switchDecision;		
		std::vector<double> stats_nbSwitchDst;
		std::vector< std::vector<int> > stats_ClassErrors;

		/* Lazy Migration opt */ 
		std::vector<int> stats_nbMigrationsFromNVM;
};



#endif

