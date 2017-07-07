#ifndef RAP_PREDICTOR_HH_
#define RAP_PREDICTOR_HH_

#include <vector>
#include <map>
#include <ostream>

#include "Predictor.hh"
#include "common.hh"
#include "HybridCache.hh"
#include "Cache.hh"


#define RAP_SATURATION_TH 2
#define RAP_BYPASS_SATURATION_TH 3

#define RD_EVAL "simple"

#define RAP_OUTPUT_FILE "rap_predictor.out"
#define RAP_OUTPUT_FILE1 "rap_predictor1.out"
#define RAP_OUTPUT_FILE_DATASETS "rap_datasets.out"

#define RAP_LEARNING_THRESHOLD 20

#define RAP_TABLE_ASSOC 2
#define RAP_TABLE_SET 128

#define RAP_SRAM_ASSOC 4
#define RAP_NVM_ASSOC 12 


#define RD_INFINITE 1E9



class Predictor;
class HybridCache;


class RAPEntry
{
	public: 
		RAPEntry() { initEntry(Access()); isValid = false; };
		void initEntry(Access el) {
		 	cpts =  std::vector<int>(4,0);
		 	lastWrite = 0;
		 	m_pc = -1;

		 	if(el.isWrite())
			 	des = ALLOCATE_IN_SRAM; 
			else
				des = ALLOCATE_IN_NVM;
				
		 	policyInfo = 0;
			cptLearning = 0;
			reuse_distances.clear();
			nbAccess = 0;
			isValid = true;
			nbSwitch = 0;
			nbUpdate = 0;
			write_state = RW_NOT_ACCURATE;
			history.clear();
			size = 0;
			accessPerPhase = 0;
			sramErrors = 0;
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
		
		RW_TYPE write_state;
		std::vector<std::pair<RW_TYPE,int> > history;

		int size;
		int accessPerPhase;
//		RD_Status rd_state;
		int sramErrors;

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

		RD_TYPE complexRdEvaluation(std::vector<int> reuse_distances);
		RD_TYPE simpleRdEvaluation(std::vector<int> reuse_distances);
		void updateDecision(RAPEntry* entry);
		RD_TYPE convertRD(int rd);

		void dumpDataset(RAPEntry* entry);

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



#endif

