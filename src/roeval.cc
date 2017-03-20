#include <fstream>
#include <iostream>
#include <stdio.h>

#include "pin.H"
#include "Hierarchy.hh"
#include "Cache.hh"
#include "common.hh"
using namespace std;

Hierarchy my_system;
Access element;
uint64_t cpt_time;
ofstream log_file;

VOID access(uint64_t pc , uint64_t addr, MemCmd type, int size)
{
	my_system.handleAccess(Access(addr, size, pc , type));	
}


VOID RecordMemRead(VOID* pc , VOID* addr, int size, int id_thread)
{
//	printf("%p: R %p\n", ip, addr);
	uint64_t convert_pc = reinterpret_cast<uint64_t>(pc);
	uint64_t convert_addr = reinterpret_cast<uint64_t>(addr);
	access(convert_pc , convert_addr , MemCmd::DATA_READ , size);
	cpt_time++;
}

VOID RecordMemWrite(VOID * pc, VOID * addr, int size, int id_thread)
{
//	printf("%p: W %p\n", ip, addr);
	uint64_t convert_pc = reinterpret_cast<uint64_t>(pc);
	uint64_t convert_addr = reinterpret_cast<uint64_t>(addr);
	access(convert_pc , convert_addr , MemCmd::DATA_WRITE , size);
	cpt_time++;
}


VOID Routine(RTN rtn, VOID *v)
{           
	RTN_Open(rtn);
	
	string image_name = StripPath(IMG_Name(SEC_Img(RTN_Sec(rtn))).c_str());
	log_file << "Image : "<< image_name << " Fonction " <<  RTN_Name(rtn) << endl;
		
	for (INS ins = RTN_InsHead(rtn); INS_Valid(ins); ins = INS_Next(ins)){

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
	cout << "Execution finished" << endl;
	cout << "Printing results : " << endl;
	my_system.printResults();
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
	my_system = Hierarchy(1);
//	my_system.printResults();
	log_file.open("log.txt");
	
	RTN_AddInstrumentFunction(Routine, 0);
	PIN_AddFiniFunction(Fini, 0);
	// Never returns
	PIN_StartProgram();

	return 0;
}
