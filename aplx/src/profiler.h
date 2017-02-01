/* Profiler untuk TGSDP. Spesifikasi:
 * - Chip <0,0> berfungsi sebagai "Master" profiler: fixed clock frequency
 * - Master profiler mendistribusikan 'clock-tick' every 1s ke semua node untuk sinkronisasi
 *   (node yang lain mungkin memiliki clock yang berbeda, jadi butuh sinkronikasi)
 * - Mekanisme sinkronisasi butuh "default" FR route
 */

#ifndef PROFILER_H
#define PROFILER_H

/*
 * Note:
 * - percobaan tentang timer bisa dilihat di project tryMyTimer
 * - dari https://www.arm.com/products/processors/classic/arm9/arm968.php
 *   sepertinya maximum frequency-nya adalah 270MHz
 *
 * CPUs use PLL1
 */
#include <spin1_api.h>
#include <stdfix.h>


/*---------------------------- regarding Timer 2 --------------------------------*/
// -----------------------
// VIC priorities
// -----------------------
#define TIMER2_SLOT		   8		// Indar: similar to TIMER2_PRIORITY, which must be > 7, according to current spin1_api.h
//void update_VIC (void);
void reset_timer2(uint _time, uint null);	// Indar: add this
//void configure_timer2 (uint time);
void terminate_timer2 (void);
void timer1_timeout(uint tick, uint arg1);
INT_HANDLER isr_for_timer2 ();
INT_HANDLER coba_slow_timer_int();

static uint timer2_tick;			// Indar: add this
static uint ticks2;
static uint timer_tick;  	        // timer tick period
uint iLoad;

/*---------------------------- regarding Timer 2 --------------------------------*/
/*-------------------------------------------------------------------------------*/


// general/global parameters
#define REAL						accum
#define REAL_CONST(x)				x##k
#define DEF_DELAY_VAL				1000	// used mainly for io_printf
#define DEF_MY_APP_ID				255
#define DEBUG_LEVEL					1		// 0 = no_debug, 1 = intermediate, 2 = full debug info

//#define REPORT_TIMER_TICK_PERIOD_US	1000000	// to get 1s resolution in FREQ_REF_200MHZ
#define REPORT_TIMER_TICK_PERIOD_US	100000	// to get 0.1s resolution in FREQ_REF_200MHZ
#define FREQ_REF_200MHZ				200

// priority setup
#define SCP_PRIORITY_VAL			0
#define APP_PRIORITY_VAL			1
#define LOW_PRIORITY_VAL			2
#define TEMP_TIMER_PRIORITY_VAL		3
#define NON_CRITICAL_PRIORITY_VAL	3
#define LOWEST_PRIORITY_VAL			4		// I found from In spin1_api_params.h I found this: #define NUM_PRIORITIES    5
//#define IDLE_PRIORITY_VAL			NON_CRITICAL_PRIORITY_VAL
#define IDLE_PRIORITY_VAL			LOWEST_PRIORITY_VAL


// SDP-related parameters
#define DEF_GENERIC_IPTAG			2		// remember to put this value in ybug
#define DEF_GENERIC_UDP_PORT		20000
#define DEF_REPORT_IPTAG			3
#define DEF_REPORT_PORT				20001
#define DEF_ERR_INFO_TAG			4
#define DEF_ERR_INFO_PORT			20002
#define DEF_SPINN_SDP_PORT			7		// port-7 has a special purpose, usually related with ETH

#define DEF_TIMEOUT					10		// as recommended
#define HOST_SEND_TERMINATE			0xFFFF
#define SPINN_SEND_TEMP				1
#define HOST_SET_CHANGE_PLL			2
#define HOST_REQ_REVERT_PLL			3
#define HOST_SET_FREQ_VALUE			4
#define HOST_REQ_FREQ_VALUE			5
#define SPINN_SEND_IDLE_CNTR		6
#define HOST_SEND_START_REPORT		7
#define HOST_SEND_STOP_REPORT		8
#define HOST_REQ_CPU_MAP			9
#define SPINN_SEND_REPORT			10		// contains both temperature and idle counter
#define HOST_REQ_DISABLE_CPU		11
#define HOST_REQ_ENABLE_CPU			12
#define HOST_REQ_ACTIVE_CORES		13
#define HOST_REQ_TO_DEACTIVATE_CORE	14
#define HOST_REQ_TO_ACTIVATE_CORE	15

// related with frequency scalling
#define PLL_ORIGINAL_TAG			0
#define PLL_RUN_EXPERIMENT_TAG		1
#define DESIRED_FREQ				50
#define lnMemTable					93
#define wdMemTable					3
// memTable format: freq, MS, NS --> with default dv = 2, so that we don't have
// to modify r24
uchar memTable[lnMemTable][wdMemTable] = {
{10,1,2},
{11,5,11},
{12,5,12},
{13,5,13},
{14,5,14},
{15,1,3},
{16,5,16},
{17,5,17},
{18,5,18},
{19,5,19},
{20,1,4},
{21,5,21},
{22,5,22},
{23,5,23},
{24,5,24},
{25,1,5},
{26,5,26},
{27,5,27},
{28,5,28},
{29,5,29},
{30,1,6},
{31,5,31},
{32,5,32},
{33,5,33},
{34,5,34},
{35,1,7},
{36,5,36},
{37,5,37},
{38,5,38},
{39,5,39},
{40,1,8},
{41,5,41},
{42,5,42},
{43,5,43},
{44,5,44},
{45,1,9},
{46,5,46},
{47,5,47},
{48,5,48},
{49,5,49},
{50,1,10},
{51,5,51},
{52,5,52},
{53,5,53},
{54,5,54},
{55,1,11},
{56,5,56},
{57,5,57},
{58,5,58},
{59,5,59},
{60,1,12},
{61,5,61},
{62,5,62},
{63,5,63},
{65,1,13},
{70,1,14},
{75,1,15},
{80,1,16},
{85,1,17},
{90,1,18},
{95,1,19},
{100,1,20},
{105,1,21},
{110,1,22},
{115,1,23},
{120,1,24},
{125,1,25},
{130,1,26},
{135,1,27},
{140,1,28},
{145,1,29},
{150,1,30},
{155,1,31},
{160,1,32},
{165,1,33},
{170,1,34},
{175,1,35},
{180,1,36},
{185,1,37},
{190,1,38},
{195,1,39},
{200,1,40},
{205,1,41},
{210,1,42},
{215,1,43},
{220,1,44},
{225,1,45},
{230,1,46},
{235,1,47},
{240,1,48},
{245,1,49},
{250,1,50},
{255,1,51},
};

/* global variables */
uint myChipID;
uint myCoreID;
uchar myPhysicalCore;
sdp_msg_t report;							// for other parameters report
sdp_msg_t replyMsg;
uint szReport;
uint szHeaderOnly;

// reading temperature sensors
uint tempVal[3];							// there are 3 sensors in each chip
uint cpuIdleCntr[18];						// for all cpus
uint myOwnIdleCntr;							// since my flag in r25 is alway on, it gives me ALWAYS zero counts

// PLL and frequency related:
uint _r20, _r21, _r24;						// the original value of r20, r21, and r24
uint r20, r21, r24, r25;							// the current value of r20, r21 and r24 during *this* experiment
uint _freq;
volatile uint _idleCntr;
volatile uint bRptStreamEnable;
/*-------------------------------------------------------------------------------------------*/

// function prototypes
void readTemp(uint ticks, uint arg2);
void readSCP(uint mailbox, uint port);
void setupTimer(uint periodT1, uint periodT2);
void getFreqParams(uint f, uint *ms, uint *ns);
void changeFreq(uint f, uint replyCode);					// we use uchar to limit the frequency to 255
void changePLL(uint flag, uint replyCode);
REAL getFreq(uchar sel, uchar dv);
void readSpinFreqReg(uint arg0, uint arg1);
uint readSpinFreqVal();
void terminateProfiler(uint arg0, uint arg1);
void sendReply(uint replyCode, uint seq);
void idle(uint arg0, uint arg1);
void updateReport(uint tick, uint arg1);
void disableCPU(uint virtCoreID, uint none);
void enableCPU(uint virtCoreID, uint none);

#endif // PROFILER_H
