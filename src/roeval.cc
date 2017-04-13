#include <fstream>
#include <iostream>
#include <stdio.h>

#include "pin.H"
#include "Hierarchy.hh"
#include "Cache.hh"
#include "common.hh"
using namespace std;

Hierarchy* my_system;
Access element;
uint64_t cpt_time;
int start_debug;
ofstream log_file;
ofstream output_file;
ofstream config_file;


VOID access(uint64_t pc , uint64_t addr, MemCmd type, int size, int id_thread)
{
	my_system->handleAccess(Access(addr, size, pc , type , id_thread));		
}


/* Record Instruction Fetch */
VOID RecordMemInst(VOID* pc, int size, int id_thread)
{
	uint64_t convert_pc = reinterpret_cast<uint64_t>(pc);	
	access(convert_pc , convert_pc , MemCmd::INST_READ , size , id_thread);
	cpt_time++;
}


/* Record Data Read Fetch */
VOID RecordMemRead(VOID* pc , VOID* addr, int size, int id_thread)
{
	uint64_t convert_pc = reinterpret_cast<uint64_t>(pc);
	uint64_t convert_addr = reinterpret_cast<uint64_t>(addr);

	access(convert_pc , convert_addr , MemCmd::DATA_READ , size , id_thread);
	cpt_time++;
}


/* Record Data Write Fetch */
VOID RecordMemWrite(VOID * pc, VOID * addr, int size, int id_thread)
{
	uint64_t convert_pc = reinterpret_cast<uint64_t>(pc);
	uint64_t convert_addr = reinterpret_cast<uint64_t>(addr);

	access(convert_pc , convert_addr , MemCmd::DATA_WRITE , size , id_thread);
	cpt_time++;
}


VOID Routine(RTN rtn, VOID *v)
{           
	RTN_Open(rtn);
	
	string image_name = StripPath(IMG_Name(SEC_Img(RTN_Sec(rtn))).c_str());
	log_file << "Image : "<< image_name << "\tFonction " <<  RTN_Name(rtn) << endl;
	string name = image_name+":"+RTN_Name(rtn);
		
	for (INS ins = RTN_InsHead(rtn); INS_Valid(ins); ins = INS_Next(ins)){

		INS_InsertCall(ins, IPOINT_BEFORE, (AFUNPTR)RecordMemInst,
		    IARG_INST_PTR,
		    IARG_UINT32,
		    INS_Size(ins),
		    IARG_THREAD_ID,
		    IARG_END);

    		UINT32 memOperands = INS_MemoryOperandCount(ins);	    						
	
		for (UINT32 memOp = 0; memOp < memOperands; memOp++){
			if (INS_MemoryOperandIsRead(ins, memOp))
			{
				INS_InsertPredicatedCall(
				ins, IPOINT_BEFORE, (AFUNPTR)RecordMemRead,
				IARG_INST_PTR,
				IARG_MEMORYOP_EA, memOp,
				IARG_MEMORYREAD_SIZE,
				IARG_THREAD_ID,
				IARG_END);
			}

			if (INS_MemoryOperandIsWritten(ins, memOp))
			{
				INS_InsertPredicatedCall(
				ins, IPOINT_BEFORE, (AFUNPTR)RecordMemWrite,
				IARG_INST_PTR,
				IARG_MEMORYOP_EA, memOp,
				IARG_MEMORYREAD_SIZE,
				IARG_THREAD_ID,
				IARG_END);
			}
		}
	}
	RTN_Close(rtn);
}


VOID Fini(INT32 code, VOID *v)
{
	log_file.close();
	my_system->finishSimu();
	output_file << "Execution finished" << endl;
	output_file << "Printing results : " << endl;
	my_system->printResults(output_file);
	output_file.close();
}

/* ===================================================================== */
/* Print Help Message                                                    */
/* ===================================================================== */
   
INT32 Usage()
{
	PIN_ERROR( "This Pin tool evaluate the RO detection on a LLC \n" 
	      + KNOB_BASE::StringKnobSummary() + "\n");
	return -1;
}

/* ===================================================================== */
/* Main                                                                  */
/* ===================================================================== */

int main(int argc, char *argv[])
{
	if (PIN_Init(argc, argv)) return Usage();
	
	cpt_time = 0;
	start_debug = 0;
	
	my_system = new Hierarchy();
	
	config_file.open(CONFIG_FILE);
	my_system->printConfig(config_file);
	config_file.close();

	log_file.open(LOG_FILE);
	output_file.open(OUTPUT_FILE);
	
	RTN_AddInstrumentFunction(Routine, 0);
	PIN_AddFiniFunction(Fini, 0);
	// Never returns
	PIN_StartProgram();

	return 0;
}
