// ----------------------------------------------------------------------------

// SystemC OpenRISC 1200 Monitor: implementation

// Copyright (C) 2008  Embecosm Limited <info@embecosm.com>

// Contributor Jeremy Bennett <jeremy.bennett@embecosm.com>
// Contributor Julius Baxter <jb@orsoc.se>

// This file is part of the cycle accurate model of the OpenRISC 1000 based
// system-on-chip, ORPSoC, built using Verilator.

// This program is free software: you can redistribute it and/or modify it
// under the terms of the GNU Lesser General Public License as published by
// the Free Software Foundation, either version 3 of the License, or (at your
// option) any later version.

// This program is distributed in the hope that it will be useful, but WITHOUT
// ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
// FITNESS FOR A PARTICULAR PURPOSE.  See the GNU Lesser General Public
// License for more details.

// You should have received a copy of the GNU Lesser General Public License
// along with this program.  If not, see <http://www.gnu.org/licenses/>.

// ----------------------------------------------------------------------------

// $Id: Or1200MonitorSC.cpp 303 2009-02-16 11:20:17Z jeremy $

#include <iostream>
#include <iomanip>
#include <fstream>

using namespace std;

#include "Or1200MonitorSC.h"
#include "OrpsocMain.h"


SC_HAS_PROCESS( Or1200MonitorSC );

//! Constructor for the OpenRISC 1200 monitor

//! @param[in] name  Name of this module, passed to the parent constructor.
//! @param[in] accessor  Accessor class for this Verilated ORPSoC model

Or1200MonitorSC::Or1200MonitorSC (sc_core::sc_module_name   name,
				  OrpsocAccess             *_accessor,
				  MemoryLoad               *_memoryload,
				  int argc, 
				  char *argv[]) :
  sc_module (name),
  accessor (_accessor),
  memoryload(_memoryload)
{

  // If not -log option, then don't log
  
  string logfileDefault("vlt-executed.log");
  string logfileNameString;
  int profiling_enabled = 0; 
  string profileFileName(DEFAULT_PROF_FILE); 
  insn_count=0;
  cycle_count=0;

  exit_perf_summary_enabled = 1; // Simulation exit performance summary is 
                                 // on by default. Turn off with "-q" on the cmd line

  // Parse the command line options
  int cmdline_name_found=0;
  if (argc > 1)
    {
      // Search through the command line parameters for the "-log" option
      for(int i=1; i < argc; i++)
	{
	  if ((strcmp(argv[i], "-l")==0) ||
	      (strcmp(argv[i], "--log")==0))
	    {
	      logfileNameString = (argv[i+1]);
	      cmdline_name_found=1;
	    }
	  else if ((strcmp(argv[i], "-q")==0) ||
		   (strcmp(argv[i], "--quiet")==0))
	    {
	      exit_perf_summary_enabled = 0;
	    }
	  else if ((strcmp(argv[i], "-p")==0) ||
		   (strcmp(argv[i], "--profile")==0))
	    {
	      profiling_enabled = 1;
	      // Check for !end of command line and it's not a command
	      if ((i+1 < argc)){
		if(argv[i+1][0] != '-')
		  profileFileName = (argv[i+1]);
	      }
	    }
	}
    }
  
  
  if (profiling_enabled)
    {
      profileFile.open(profileFileName.c_str(), ios::out); // Open profiling log file
      if(profileFile.is_open())
	{
	  // If the file was opened OK, then enabled logging and print a message.
	  profiling_enabled = 1;
	  cout << "* Execution profiling enabled. Logging to " << profileFileName << endl;
	}
      
      // Setup profiling function
      SC_METHOD (callLog);
      sensitive << clk.pos();
      dont_initialize();
      start = clock();
    }

  
  if(cmdline_name_found==1) // No -log option specified so don't turn on logging
    {      

      logging_enabled = 0; // Default is logging disabled      
      statusFile.open(logfileNameString.c_str(), ios::out ); // open file to write to it

      if(statusFile.is_open())
	{
	  // If we could open the file then turn on logging
	  logging_enabled = 1;
	  cout << "* Processor execution logged to file: " << logfileNameString << endl;
	}
      
    }  
  if (logging_enabled)
    {  
      SC_METHOD (displayState);
      sensitive << clk.pos();
      dont_initialize();
      start = clock();
    }

  
  
  // checkInstruction monitors the bus for special NOP instructionsl
  SC_METHOD (checkInstruction);
  sensitive << clk.pos();
  dont_initialize();
  
  
  
}	// Or1200MonitorSC ()

//! Print command line switches for the options of this module
void 
Or1200MonitorSC::printSwitches()
{
  printf(" [-l <file>] [-q] [-p [<file>]]");
}

//! Print usage for the options of this module
void 
Or1200MonitorSC::printUsage()
{
  printf("  -p, --profile\t\tEnable execution profiling output to file (default "DEFAULT_PROF_FILE")\n");
  printf("  -l, --log\t\tLog processor execution to file\n");
  printf("  -q, --quiet\t\tDisable the performance summary at end of simulation\n");
}

//! Method to handle special instrutions

//! These are l.nop instructions with constant values. At present the
//! following are implemented:

//! - l.nop 1  Terminate the program
//! - l.nop 2  Report the value in R3
//! - l.nop 3  Printf the string with the arguments in R3, etc
//! - l.nop 4  Print a character
extern int SIM_RUNNING;
void
Or1200MonitorSC::checkInstruction()
{
  uint32_t  r3;
  double    ts;
  
  // Check the instruction when the freeze signal is low.
  if (!accessor->getWbFreeze())
    {
      // Do something if we have l.nop
      switch (accessor->getWbInsn())
	{
	case NOP_EXIT:
	  r3 = accessor->getGpr (3);
	  ts = sc_time_stamp().to_seconds() * 1000000000.0;
	  std::cout << std::fixed << std::setprecision (2) << ts;
	  std::cout << " ns: Exiting (" << r3 << ")" << std::endl;
	  perfSummary();
	  if (logging_enabled != 0) statusFile.close();
	  SIM_RUNNING=0;
	  sc_stop();
	  break;

	case NOP_REPORT:
	  ts = sc_time_stamp().to_seconds() * 1000000000.0;
	  r3 = accessor->getGpr (3);
	  std::cout << std::fixed << std::setprecision (2) << ts;
	  std::cout << " ns: report (" << hex << r3 << ")" << std::endl;
	  break;

	case NOP_PRINTF:
	  ts = sc_time_stamp().to_seconds() * 1000000000.0;
	  std::cout << std::fixed << std::setprecision (2) << ts;
	  std::cout << " ns: printf" << std::endl;
	  break;

	case NOP_PUTC:
	  r3 = accessor->getGpr (3);
	  std::cout << (char)r3 << std::flush;
	  break;

	default:
	  break;
	}
    }

}	// checkInstruction()


//! Method to log execution in terms of calls and returns

void
Or1200MonitorSC::callLog()
{
  uint32_t  exinsn, delaypc; 
  uint32_t o_a; // operand a
  uint32_t o_b; // operand b
  struct label_entry *tmp;
  
  cycle_count++;
  // Instructions should be valid when freeze is low and there are no exceptions
  //if (!accessor->getExFreeze())
  if ((!accessor->getWbFreeze()) && (accessor->getExceptType() == 0))
    {
      // Increment instruction counter
      insn_count++;

      //exinsn = accessor->getExInsn();// & 0x3ffffff;
      exinsn = accessor->getWbInsn();
      // Check the instruction
      switch((exinsn >> 26) & 0x3f) { // Check Opcode - top 6 bits
      case 0x1:
	/* Instruction: l.jal */
	o_a = (exinsn >> 0) & 0x3ffffff;
	if(o_a & 0x02000000) o_a |= 0xfe000000;
	
	//delaypc = accessor->getExPC() + (o_a * 4); // PC we're jumping to
	delaypc = accessor->getWbPC() + (o_a * 4); // PC we're jumping to
	// Now we have info about where we're jumping to. Output the info, with label if possible
	// We print the PC we're jumping from + 8 which is the return address
	if ( tmp = memoryload->get_label (delaypc) )
	  profileFile << "+" << std::setfill('0') << hex << std::setw(8) << cycle_count << " " << hex << std::setw(8) << accessor->getWbPC() + 8 << " " << hex << std::setw(8) << delaypc << " " << tmp->name << endl;
	else
	  profileFile << "+" << std::setfill('0') << hex << std::setw(8) << cycle_count << " " << hex << std::setw(8) << accessor->getWbPC() + 8 << " " << hex << std::setw(8) << delaypc << " @" << hex << std::setw(8) << delaypc << endl;
	
	break;
      case 0x11:
	/* Instruction: l.jr */
	// Bits 15-11 contain register number
	o_b = (exinsn >> 11) & 0x1f;
	if (o_b == 9) // l.jr r9 is typical return
	  {
	    // Now get the value in this register
	    delaypc = accessor->getGpr(o_b);
	    // Output this jump
	    profileFile << "-" << std::setfill('0') << hex << std::setw(8) << cycle_count << " "  << hex << std::setw(8) << delaypc << endl;
	  }
	break;
      case 0x12:
	/* Instruction: l.jalr */
	o_b = (exinsn >> 11) & 0x1f;
	// Now get the value in this register
	delaypc = accessor->getGpr(o_b);
	// Now we have info about where we're jumping to. Output the info, with label if possible
	// We print the PC we're jumping from + 8 which is the return address
	if ( tmp = memoryload->get_label (delaypc) )
	  profileFile << "+" << std::setfill('0') << hex << std::setw(8) << cycle_count << " " << hex << std::setw(8) << accessor->getWbPC() + 8 << " " << hex << std::setw(8) << delaypc << " " << tmp->name << endl;
	else
	  profileFile << "+" << std::setfill('0') << hex << std::setw(8) << cycle_count << " " << hex << std::setw(8) << accessor->getWbPC() + 8 << " " << hex << std::setw(8) << delaypc << " @" << hex << std::setw(8) << delaypc << endl;
	
	break;

      }
    }
}	// checkInstruction()


//! Method to output the state of the processor

//! This function will output to a file, if enabled, the status of the processor
//! For now, it's just the PPC and instruction.
#define PRINT_REGS 1
void
Or1200MonitorSC::displayState()
{
  uint32_t  wbinsn;
  
  // Calculate how many instructions we've actually calculated by ignoring cycles where we're frozen, delay slots and flushpipe cycles
  if ((!accessor->getWbFreeze()) && !(accessor->getExceptFlushpipe() && accessor->getExDslot()))

  if (logging_enabled == 0)
	return;	// If we didn't inialise a file, then just return.

  // Output the state if we're not frozen and not flushing during a delay slot
  if ((!accessor->getWbFreeze()) && !(accessor->getExceptFlushpipe() && accessor->getExDslot()))
    {
      // Print PC, instruction
      statusFile << "\nEXECUTED("<< std::setfill(' ') << std::setw(11) << dec << insn_count << "): " << std::setfill('0') << hex << std::setw(8) << accessor->getWbPC() << ": " << hex << accessor->getWbInsn() <<  endl;
#if PRINT_REGS
	// Print general purpose register contents
	for (int i=0; i<32; i++)
	  {
		if ((i%4 == 0)&&(i>0)) statusFile << endl;
		statusFile << std::setfill('0');
		statusFile << "GPR" << dec << std::setw(2) << i << ": " <<  hex << std::setw(8) << (uint32_t) accessor->getGpr(i) << "  ";		
	}
	statusFile << endl;

	statusFile << "SR   : " <<  hex << std::setw(8) << (uint32_t) accessor->getSprSr() << "  ";
	statusFile << "EPCR0: " <<  hex << std::setw(8) << (uint32_t) accessor->getSprEpcr() << "  ";
	statusFile << "EEAR0: " <<  hex << std::setw(8) << (uint32_t) accessor->getSprEear() << "  ";	
	statusFile << "ESR0 : " <<  hex << std::setw(8) << (uint32_t) accessor->getSprEsr() << endl;
#endif

    }

  return;

}	// displayState()

//! Function to calculate the number of instructions performed and the time taken
void 
Or1200MonitorSC::perfSummary()
{
  if (exit_perf_summary_enabled) 
    {
      double ts;
      ts = sc_time_stamp().to_seconds() * 1000000000.0;
      int cycles = ts / (BENCH_CLK_HALFPERIOD*2); // Number of clock cycles we had
      
      clock_t finish = clock();
      double elapsed_time = (double(finish)-double(start))/CLOCKS_PER_SEC;
      // It took elapsed_time seconds to do insn_count instructions. Divide insn_count by the time to get instructions/second.
      double ips = (insn_count/elapsed_time);
      double mips = (insn_count/elapsed_time)/1000000;
      int hertz = (int) ((cycles/elapsed_time)/1000);
      std::cout << "* Or1200Monitor: simulated " << sc_time_stamp() << ",time elapsed: " << elapsed_time << " seconds" << endl;
      std::cout << "* Or1200Monitor: simulated " << dec << cycles << " clock cycles, executed at approx " << hertz << "kHz" << endl;
      std::cout << "* Or1200Monitor: simulated " << insn_count << " instructions, insn/sec. = " << ips << ", mips = " << mips << endl;
    }
  return;
} 	// perfSummary

