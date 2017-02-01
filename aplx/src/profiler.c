/**** termometer.c
*
* SUMMARY
*  1. read the temperature sensor of the chip every (ca.) 0.5s and send to the host as SDP
*
* AUTHOR
*  Indar Sugiarto - indar.sugiarto@manchester.ac.uk
*
* DETAILS
*  Created on       : 3 Sept 2015
*  Version          : $Revision: 1 $
*  Last modified by : $Author: indi $
*
* COPYRIGHT
*  Copyright (c) The University of Manchester, 2011. All rights reserved.
*  SpiNNaker Project
*  Advanced Processor Technologies Group
*  School of Computer Science
*
*******/

#include <math.h>
#include "profiler.h"

/* in readTemp(), read the sensors and put the result in val[] */
#define MY_CODE			1
#define PATRICK_CODE	2
#define READING_VERSION	MY_CODE	// 1 = mine, 2 = patrick's
void readTemp(uint ticks, uint arg2)
{
#if (READING_VERSION==1)
	uint i, done, S[] = {SC_TS0, SC_TS1, SC_TS2};

	for(i=0; i<3; i++) {
		done = 0;
		// set S-flag to 1 and wait until F-flag change to 1
		sc[S[i]] = 0x80000000;
		do {
			done = sc[S[i]] & 0x01000000;
		} while(!done);
		// turnoff S-flag and read the value
		sc[S[i]] = sc[S[i]] & 0x0FFFFFFF;
		tempVal[i] = sc[S[i]] & 0x00FFFFFF;
	}

#elif (READING_VERSION==2)
	uint k, temp1, temp2, temp3;

	// Start tempearture measurement
	sc[SC_TS0] = 0x80000000;
	// Wait for measurement TS0 to finish
	k = 0;
	while(!(sc[SC_TS0] & (1<<24))) k++;
	// Get value
	temp1 = sc[SC_TS0] & 0x00ffffff;
	// Stop measurement
	sc[SC_TS0] = 0<<31;
	//io_printf(IO_BUF, "k(T1):%d\n", k);

	// Start tempearture measurement
	sc[SC_TS1] = 0x80000000;
	// Wait for measurement TS1 to finish
	k=0;
	while(!(sc[SC_TS1] & (1<<24))) k++;
	// Get value
	temp2 = sc[SC_TS1] & 0x00ffffff;
	// Stop measurement
	sc[SC_TS1] = 0<<31;
	//io_printf(IO_BUF, "k(T2):%d\n", k);

	// Start tempearture measurement
	sc[SC_TS2] = 0x80000000;
	// Wait for measurement TS2 to finish
	k=0;
	while(!(sc[SC_TS2] & (1<<24))) k++;
	// Get value
	temp3 = sc[SC_TS2] & 0x00ffffff;
	// Stop measurement
	sc[SC_TS2] = 0<<31;
	//io_printf(IO_BUF, "k(T3):%d\n\n", k);
	tempVal[0] = temp1;
	tempVal[1] = temp2;
	tempVal[2] = temp3;
#endif
}

void getFreqParams(uint f, uint *ms, uint *ns)
{
	uint i;
	for(i=0; i<lnMemTable; i++)
		if(f == (uint)memTable[i][0]) {
			*ms = (uint)memTable[i][1];
			*ns = (uint)memTable[i][2];
			break;
		}
}

void idle(uint arg0, uint arg1)
{
	r25 = sc[SC_SLEEP];
	for(_idleCntr=0; _idleCntr<18; _idleCntr++)
		cpuIdleCntr[_idleCntr] += (r25 >> _idleCntr) & 1;
	myOwnIdleCntr++; //NOTE: it doesn't make any sense, since we're busy!!! Rasanya ini salah!!!
	spin1_schedule_callback(idle, 0, 0, IDLE_PRIORITY_VAL);
}

void changeFreq(uint f, uint null)
{
	r20 = sc[SC_PLL1];		// clk sources for cores should always from PLL1

	uint ns, ms;
	getFreqParams(f, &ms, &ns);
	r20 &= 0xFFFFC0C0;			// apply masking at MS and NS
	r20 |= ns;					// apply NS
	r20 |= (ms << 8);			// apply MS
	sc[SC_PLL1] = r20;			// change the value of r20 with the new parameters

	_freq = f;

#if (DEBUG_LEVEL>0)
	io_printf(IO_BUF, "\nSelected ms = %u, ns = %u\n", ms, ns);
	io_printf(IO_BUF, "The cpu clock is set to %uMHz\n\n", f);
#endif
	sendReply(HOST_SET_FREQ_VALUE,myChipID);
}

/* NOTE: readSpinFreqVal() assumes that the value of _dv_ is always 2 */
uint readSpinFreqVal()
{
	uint i, MS1, NS1, f = sv->cpu_clk;
	r20 = sc[SC_PLL1];

	MS1 = (r20 >> 8) & 0x3F;
	NS1 = r20 & 0x3F;

	for(i=0; i<lnMemTable; i++) {
		if(memTable[i][1]==MS1 && memTable[i][2]==NS1) {
			f = memTable[i][0];
			break;
		}
	}
	return f;
}

/* It is better if the host interpret the value of r20, r21 and r24, not the spinnaker program */
void readSpinFreqReg(uint arg0, uint arg1)
{
	r20 = sc[SC_PLL1];
	r21 = sc[SC_PLL2];
	r24 = sc[SC_CLKMUX];

	replyMsg.cmd_rc = HOST_REQ_FREQ_VALUE;
	replyMsg.seq = (ushort)myChipID;
	replyMsg.arg1 = r20;
	replyMsg.arg2 = r21;
	replyMsg.arg3 = r24;
	replyMsg.length = sizeof(sdp_hdr_t) + sizeof(cmd_hdr_t);

	spin1_send_sdp_msg(&replyMsg, DEF_TIMEOUT);
}

void readSCP(uint mailbox, uint port)
{
	sdp_msg_t *msg = (sdp_msg_t *) mailbox;
	// use MY_SDP_PORT for communication with the host via eth
#if (DEBUG_LEVEL>0)
	io_printf(IO_BUF, "Receiving SCP with cmd=0x%x, seq=0x%x, arg1=0x%x, arg2=0x%x, arg3=0x%x\n", msg->cmd_rc, msg->seq, msg->arg1, msg->arg2, msg->arg3);
#endif
	if(port==DEF_SPINN_SDP_PORT) {
		switch(msg->cmd_rc) {
		case HOST_SEND_TERMINATE:
			spin1_schedule_callback(terminateProfiler, 0, 0, NON_CRITICAL_PRIORITY_VAL);
		case HOST_SET_CHANGE_PLL:
			spin1_schedule_callback(changePLL, msg->seq, HOST_SET_CHANGE_PLL, NON_CRITICAL_PRIORITY_VAL);
			//changePLL(1);
			break;
			/*
		case HOST_REQ_REVERT_PLL:
			spin1_schedule_callback(changePLL, PLL_ORIGINAL_TAG, HOST_REQ_REVERT_PLL, NON_CRITICAL_PRIORITY_VAL);
			//changePLL(0);
			break;
			*/
		case HOST_SET_FREQ_VALUE:
			spin1_schedule_callback(changeFreq, msg->seq, HOST_SET_FREQ_VALUE, NON_CRITICAL_PRIORITY_VAL);
			// then update timer to have, approximately, constant period
			spin1_schedule_callback(reset_timer2, timer2_tick, 0, NON_CRITICAL_PRIORITY_VAL);
			//reset_timer2(timer2_tick);

			//changeFreq((uchar)msg->seq);
			break;
		case HOST_REQ_FREQ_VALUE:
			spin1_schedule_callback(readSpinFreqReg, 0, 0, NON_CRITICAL_PRIORITY_VAL);
			break;
		case HOST_SEND_START_REPORT:
			bRptStreamEnable = 1;
			break;
		case HOST_SEND_STOP_REPORT:
			bRptStreamEnable = 0;
			break;
		case HOST_REQ_CPU_MAP:
			replyMsg.cmd_rc = HOST_REQ_CPU_MAP;
			// NOTE: here is the way to get virtual cpu from physical one:
			// sark.virt_cpu = sv->p2v_map[sark.phys_cpu];
			sark_mem_cpy((void *)replyMsg.data, (void *)sv->p2v_map, 20); // send physical to virtual core map
			replyMsg.length = szHeaderOnly+20;
			spin1_send_sdp_msg(&replyMsg, DEF_TIMEOUT);
			break;
		case HOST_REQ_DISABLE_CPU:
			spin1_schedule_callback(disableCPU, msg->seq, 0, NON_CRITICAL_PRIORITY_VAL);
			break;
		case HOST_REQ_ENABLE_CPU:
			spin1_schedule_callback(enableCPU, msg->seq, 0, NON_CRITICAL_PRIORITY_VAL);
			break;
		case HOST_REQ_ACTIVE_CORES:
			replyMsg.cmd_rc = HOST_REQ_ACTIVE_CORES;
			//replyMsg.arg1 = sc[SC_CPU_OK];
			replyMsg.arg1 = sc[SC_CPU_DIS];
			replyMsg.length = szHeaderOnly;
			spin1_send_sdp_msg(&replyMsg, DEF_TIMEOUT);
			break;
		case HOST_REQ_TO_DEACTIVATE_CORE:
			spin1_schedule_callback(disableCPU, msg->seq, 0, NON_CRITICAL_PRIORITY_VAL);
			break;
		case HOST_REQ_TO_ACTIVATE_CORE:
			spin1_schedule_callback(enableCPU, msg->seq, 0, NON_CRITICAL_PRIORITY_VAL);
			break;
		}
	}

	spin1_msg_free (msg);
}

void disableCPU(uint virtCoreID, uint none)
{
	uchar pCore = sv->v2p_map[virtCoreID];
	uint code = sc[SC_CPU_DIS];
	code &= 0xFFFFF;               // make sure the security field is cleared
	code |= 0x5EC00000;            // set the security field
	code |= (1 << pCore);          // set DISABLED_CORE to '1'
	sc[SC_CPU_DIS] = code;         // write to the register

	// Then reset via r9 to be in a low-power state
	code = sc[SC_SOFT_RST_P];
	code &= 0xFFFFF;               // make sure the security field is cleared
	code |= 0x5EC00000;            // set the security field
	code |= (1 << pCore);          // send pulse to the DISABLED_CORE to '1'
	sc[SC_SOFT_RST_P] = code;
	//sc[SC_HARD_RST_P] = code;		// kok sepertinya ndak ada bedanya dengan sc[SC_SOFT_RST_P]
	sark_delay_us(1000);
	//testing aja (mungkin bisa dihapus nanti...):
	//sc[SC_SOFT_RST_P] &= ~(1 << pCore);

	// Finally, indicate that core is not functional
	sc[SC_CLR_OK] |= (1 << pCore);
	io_printf(IO_STD, "Core-%u is disabled!\n", virtCoreID); sark_delay_us(1000);
}

void enableCPU(uint virtCoreID, uint none)
{
	uchar pCore = sv->v2p_map[virtCoreID];
	uint code = ~(1 << pCore);        // the result will be something like 11111111111111111111111111111011
	code &= sc[SC_CPU_DIS];           // mask the current register
	code &= 0xFFFFF;                  // make sure the security field is cleared
	code |= 0x5EC00000;               // set the security field
	sc[SC_CPU_DIS] = code;            // write to the register

	// Additionally, set CPU OK to indicate that processor is believed to be functional (info from Datasheet)
	sc[SC_SET_OK] |= (1 << pCore);    // switch only pCore-bit

	io_printf(IO_STD, "Core-%u is enabled!\n", virtCoreID);	sark_delay_us(1000);
}


void terminateProfiler(uint arg0, uint arg1)
{
	changePLL(0, 0);
	spin1_exit(0);
}

void sendReply(uint replyCode, uint seq)
{
	replyMsg.cmd_rc = (ushort)replyCode;
	replyMsg.seq = (ushort)seq;
	replyMsg.length = sizeof(sdp_hdr_t) + sizeof(uint);
	spin1_send_sdp_msg(&replyMsg, DEF_TIMEOUT);
}

/* SYNOPSIS
 * 		changeFreq()
 * IDEA:
 *		The clock frequency of cpus are determined by PLL1. It is better if clock source for System
 * 		AHB and Router is changed to PLL2, so that we can play only with the cpu.
 *		Using my readPLL.aplx, I found that system AHB and router has 133MHz. That's why, we should
 *		change its divisor to 2, so that the frequency of these elements are kept around 130MHz
 *
 * In function changePLL(), the flag in changePLL means:
 * 0: set to original value
 * 1: set to use PLL2 for system AHB and router
 * 2: set system AHB and router to use the same clock as CPUs
*/
void changePLL(uint flag, uint replyCode)
{
	if(flag==0) {
		sc[SC_CLKMUX] = _r24;
		sc[SC_PLL1] = _r20;
		sc[SC_PLL2] = _r21;
	}
	else if(flag==1) {
		r24 = sc[SC_CLKMUX];
#if (DEBUG_LEVEL>0)
		/* Let's change so that System AHB and Router use PLL2 */
		io_printf(IO_BUF, "System AHB and Router divisor will be changed to 2 instead of 3\n");
#endif
		// the System AHB
		r24 &= 0xFF0FFFFF; //clear "Sdiv" and "Sys"
		r24 |= 0x00600000; //set Sdiv = 2 and "Sys" = 2
		// the Router
		r24 &= 0xFFF87FFF; //clear "Rdiv" and "Rtr"
		r24 |= 0x00030000; //set Rdiv = 2 and "Rtr" = 2
#if (DEBUG_LEVEL>0)
		io_printf(IO_BUF, "System AHB and Router is set to PLL2\n");
#endif
		sc[SC_CLKMUX] = r24;
	}
	else if(flag==2) {
		r24 = sc[SC_CLKMUX];
#if (DEBUG_LEVEL>0)
		/* Let's change so that System AHB and Router use PLL2 */
		io_printf(IO_BUF, "System AHB and Router divisor will be changed to 2 instead of 3\n");
#endif
		// the System AHB
		r24 &= 0xFF0FFFFF; //clear "Sdiv" and "Sys"
		r24 |= 0x00500000; //set Sdiv = 2 and "Sys" = 1
		// the Router
		r24 &= 0xFFF87FFF; //clear "Rdiv" and "Rtr"
		r24 |= 0x00028000; //set Rdiv = 2 and "Rtr" = 1
#if (DEBUG_LEVEL>0)
		io_printf(IO_BUF, "System AHB and Router is set to PLL2\n");
#endif
		sc[SC_CLKMUX] = r24;

	}
	sendReply(replyCode, myChipID);
}

void timer1_timeout(uint tick, uint arg1)
{
	sark_led_set (LED_FLIP (1));			// Flip a LED
}

void updateReport(uint tick, uint arg1)
{
	// get the temperature sensor values in val[]
	readTemp(tick, arg1);
	//report.seq = (ushort)myChipID;
	report.seq = (ushort)_freq;
	report.arg1 = tempVal[0];
	report.arg2 = tempVal[1];
	report.arg3 = tempVal[2];
	// get the idle counters
	cpuIdleCntr[myPhysicalCore] = myOwnIdleCntr;	// replace the "always zero counter" that runs on myself
	sark_mem_cpy((void *)report.data, (void *)cpuIdleCntr, 18*sizeof(uint));
	// clear the idle counters and play with the LEDs
	for(_idleCntr=0; _idleCntr<18; _idleCntr++)
		cpuIdleCntr[_idleCntr] = 0;
	myOwnIdleCntr = 0;

	report.cmd_rc = SPINN_SEND_REPORT;
	report.length = szReport;
	if(bRptStreamEnable)
		spin1_send_sdp_msg(&report, DEF_TIMEOUT);
	//io_printf(IO_BUF, "Sending report using iptag-%u on port-%u...\n", report.tag, DEF_REPORT_PORT);

}

void setupTimer(uint periodT1, uint periodT2)
{
	// for Timer1
	spin1_set_timer_tick(periodT1);
	//spin1_callback_on(TIMER_TICK, updateReport, NON_CRITICAL_PRIORITY_VAL);
	spin1_callback_on(TIMER_TICK, timer1_timeout, NON_CRITICAL_PRIORITY_VAL);

	// Using interrupt example
	ticks2 = 0;
	sark_vic_set (TIMER2_SLOT, TIMER2_INT, 1, isr_for_timer2);
	tc[T2_CONTROL] = 0xe2;			// Set up count-down mode
	reset_timer2(periodT2, 0);


	/*
	update_VIC();
	cpu_freq = 200;						// This is initial value, might be change during run time
	set_timer2_tick(periodT2);			// this only assign the value of timer2_tick to something
	//configure_timer2(timer2_tick)
	{
	  // do not enable yet!
	  tc[T2_CONTROL] = 0;
	  tc[T2_INT_CLR] = 1;
	  tc[T2_LOAD] = cpu_freq * timer2_tick;
	  tc[T2_BG_LOAD] = cpu_freq * timer2_tick;
	}
	tc[T2_CONTROL] = 0xe2;			// 0xe2 = 1110 0010 = enable with periodic interrupt in 32-bit wrapping mode
	ticks2 = 0;							// actually, it's a general purpose variable
	*/


	// apa ini?
	sark_vic_set(TIMER2_SLOT + 1, SLOW_CLK_INT, 1, coba_slow_timer_int);
}

INT_HANDLER coba_slow_timer_int()
{
	io_printf(IO_BUF, "Apa ini?\n");

}

void c_main(void)
{
	// test...
	sv->utmp0 = 0;

	// initialize idle process
	for(_idleCntr=0; _idleCntr<18; _idleCntr++)
		cpuIdleCntr[_idleCntr] = 0;
	myOwnIdleCntr = 0;
	spin1_schedule_callback(idle, 0, 0, LOWEST_PRIORITY_VAL);

	// get chip ID
	uint myAppID = sark_app_id();
	if(myAppID != DEF_MY_APP_ID) {
		io_printf(IO_STD, "Please assign me app-ID %u\n", DEF_MY_APP_ID);
		return;
	}
	myChipID = sark_chip_id();
	myCoreID = sark_core_id();

	// get the original PLL configuration and current frequency
	_r20 = 0x70128;										// _r20 = sc[SC_PLL1];
	_r21 = 0x7011a;										// _r21 = sc[SC_PLL2];
	_r24 = 0x809488a5;									// _r24 = sc[SC_CLKMUX];
	_freq = readSpinFreqVal();

	/* initialize SDP
	 * report will be used to send continues update data. It uses DEF_REPORT_IPTAG (20001)
	 * replyMsg will be used occasionally when requested by the host. It uses DEF_GENERIC_IPTAG (20000)
	 * It needs to be specified using iptag in SpiNNaker!
	 */
	szHeaderOnly = sizeof(sdp_hdr_t) + sizeof(cmd_hdr_t);
	szReport = sizeof(sdp_hdr_t) + sizeof(cmd_hdr_t) + 18*sizeof(uint);		// contains temperature and idle counter
	report.tag = DEF_REPORT_IPTAG;						// Don't forget to specify the proper iptag in ybug
	report.dest_port = PORT_ETH;						// Ethernet
	report.dest_addr = sv->eth_addr;					// Nearest Ethernet chip
	report.flags = 0x07;								// no need for reply
	report.srce_port = DEF_SPINN_SDP_PORT;
	report.srce_addr = sv->p2p_addr;
	//io_printf(IO_STD, "sv->p2p_addr = 0x%x\n", sv->p2p_addr);

	replyMsg.tag = DEF_GENERIC_IPTAG;
	replyMsg.dest_port = PORT_ETH;
	replyMsg.dest_addr = sv->eth_addr;
	replyMsg.flags = 0x07;
	replyMsg.srce_port = DEF_SPINN_SDP_PORT;
	replyMsg.srce_addr = sv->p2p_addr;

	// Anything else
	bRptStreamEnable = 1;								// by default, it streams data
	myPhysicalCore = sv->v2p_map[myCoreID];

	io_printf(IO_STD, "Chip profiler is started at <%u,%u,%u>!\nStreaming is disable!\n", myChipID >> 8, myChipID & 0xFF, myCoreID);

	setupTimer(REPORT_TIMER_TICK_PERIOD_US, REPORT_TIMER_TICK_PERIOD_US);

	spin1_callback_on(SDP_PACKET_RX, readSCP, SCP_PRIORITY_VAL);

	spin1_start(SYNC_NOWAIT);
	io_printf(IO_STD, "Chip profiler is terminated!\n");
	/*-------------------------------- Regarding Timer 2 -------------------------------------*/
	terminate_timer2();
	/*----------------------------------------------------------------------------------------*/
}


/*-------------------------------- Here are my extensions on Timer2 ---------------------------------*/

/****f* spin1_isr.c/timer2_isr
*
* SUMMARY
*  This interrupt service routine is called upon countdown of the processor's
*  primary timer to zero. In response, a callback is scheduled.
*
* SYNOPSIS
*  INT_HANDLER timer2_isr()
*
* SOURCE
*/
INT_HANDLER isr_for_timer2 ()
{
	// Clear timer interrupt
	//tc[T2_INT_CLR] = 1;
	tc[T2_INT_CLR] = (uint) tc;			// Clear interrupt in timer

	// Increment simulation "time" and call the "longer" handler

	ticks2++;
	//spin1_schedule_callback(timer2_timeout, ticks2, 0, NON_CRITICAL_PRIORITY_VAL);
	spin1_schedule_callback(updateReport, ticks2, 0, NON_CRITICAL_PRIORITY_VAL);
	//updateReport(ticks2, 0);
	//sark_led_set (LED_FLIP (1));			// Flip a LED

	// Ack VIC
	//vic[VIC_VADDR] = 1;		// Tell VIC we're done
	vic[VIC_VADDR] = (uint) vic;			// Tell VIC we're done
}

/*
void update_VIC (void)
{
	sark_vic_set (TIMER2_PRIORITY, TIMER2_INT, 1, isr_for_timer2);	// Set VIC slot 0
	return;
	// interrupt service routine
#ifdef __GNUC__
	typedef void (*isr_t) (void);
#else
	typedef __irq void (*isr_t) (void);
#endif
	// ------------------------------------------------------------------------
	// internal variables -- not visible in the API
	// ------------------------------------------------------------------------
	// VIC vector tables
	static volatile isr_t * const vic_vectors  = (isr_t *) (VIC_BASE + 0x100);
	static volatile uint * const vic_controls = (uint *) (VIC_BASE + 0x200);

	// ------------------------------------------------------------------------

  // disable the relevant interrupts while configuring the VIC
  uint int_select = (1 << TIMER2_INT);
  //vic[VIC_DISABLE] = vic[VIC_DISABLE] | int_select;
  vic[VIC_DISABLE] = int_select;

  // Move the SARK interrupt to chosen slot
  vic_controls[sark_vec->sark_slot] = 0;	  // Disable previous slot

  //vic_vectors[SARK_PRIORITY]  = sark_int_han;
  //vic_controls[SARK_PRIORITY] = 0x20 | CPU_INT;

	// Configure Timer2 interrupts
	// vic_controls is defined in spin1_api_params.h as:
	//		static volatile uint * const vic_controls = (uint *) (VIC_BASE + 0x200);
  vic_vectors[TIMER2_PRIORITY] = isr_for_timer2;
  vic_controls[TIMER2_PRIORITY] = 0x20 | TIMER2_INT;

  // then enable VIC
  //vic[VIC_ENABLE] = vic[VIC_ENABLE] | int_select;
  vic[VIC_ENABLE] = int_select;
}

/*
*******/

// Indar: add this
void reset_timer2 (uint _time, uint null)
{
  timer2_tick = _time;

  /*
  // (REAL)timer2_tick will produce negative value!
  REAL fRatio = (REAL)_freq / (REAL)FREQ_REF_200MHZ;
  io_printf(IO_BUF, "fRatio = %k\n", fRatio);
  io_printf(IO_BUF, "(REAL)timer2_tick = %k, (REAL)FREQ_REF_200MHZ = %k\n", (REAL)timer2_tick, (REAL)FREQ_REF_200MHZ);

  fRatio = (REAL)timer2_tick * (REAL)FREQ_REF_200MHZ;
  io_printf(IO_BUF, "fRatio = %k\n", fRatio);
  iLoad = (uint)(fRatio);
  io_printf(IO_BUF, "iLoad = %u\n", iLoad);
  //iLoad = _freq * timer2_tick;
  */


  _freq = readSpinFreqVal();
  // io_printf(IO_STD, "freq = %u\n", _freq);
  // try to compensate freq under 50MHz
  iLoad = _freq * timer2_tick;
  tc[T2_LOAD] = iLoad;		// Load time in microsecs
  // io_printf(IO_STD, "iLoad = %u\n", iLoad);
}

/* terminate_timer2() is modified from clean_up() and spin1_exit() */
void terminate_timer2 (void)
{
	tc[T2_INT_CLR] = 1;	// Indar: add this
	// Disable API-enabled interrupts to allow simulation to stop,
	vic[VIC_DISABLE] = vic[VIC_DISABLE] | (1 << TIMER2_INT);  		// Indar: add this
}
