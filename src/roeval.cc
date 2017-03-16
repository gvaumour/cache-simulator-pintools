#include <iostream>
#include <stdio.h>

#include "pin.H"
#include "Hierarchy.hh"
#include "Cache.hh"

using namespace std;

Hierarchy my_system;
Access element;


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
}

VOID RecordMemWrite(VOID * pc, VOID * addr, int size, int id_thread)
{
//	printf("%p: W %p\n", ip, addr);
	uint64_t convert_pc = reinterpret_cast<uint64_t>(pc);
	uint64_t convert_addr = reinterpret_cast<uint64_t>(addr);
	access(convert_pc , convert_addr , MemCmd::DATA_WRITE , size);
}


VOID Instruction(INS ins, VOID *v)
{

	UINT32 memOperands = INS_MemoryOperandCount(ins);

	for (UINT32 memOp = 0; memOp < memOperands; memOp++)
	{
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

VOID Fini(INT32 code, VOID *v)
{
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
	

	my_system = Hierarchy(1);
	my_system.printResults();
	
	INS_AddInstrumentFunction(Instruction, 0);
	PIN_AddFiniFunction(Fini, 0);
	// Never returns
	PIN_StartProgram();

	return 0;
}
