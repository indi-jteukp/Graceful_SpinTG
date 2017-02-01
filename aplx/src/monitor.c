/* SYNOPSIS:
 *   - If run in the same node as SOURCE/SINK, I'm supposed to:
 *       1. receives TG network info and generate initial mapping
 *          - Note: in the beginning, all nodes should report available PE so that this
 *                  monitor program is able to assign correct task ID
 *       2. monitor the traffic (count P2P traffic) and do traffic management (in the future)
 *       3. do process migration, amelioration, etc. (by invoking the optimizer cores)
 *   - If run in other nodes, I monitor P2P traffic used by sdp and alter P2P table (in the future)
 *
 * TODO:
 * 1. Modify diagnostic register, such that:
 *    Dump-NN will be re-configured as East-counter, Dump-FF will be re-configured as North-East-counter
 *    cntr12 to cntr15 will be configured as North- to South-counter
 * */
#include "tgsdp.h"
#include <sark.h>

#define DIAG_CNTR_TICK		1000000	// every 1s

// Modified from sark_hw.c
// Sepertinya kalau di set local-P2P dia akan menghitung paket yang dikirim ke luar,
// sedangkan kalau di set non-local-P2P sepertinya dia menghitung paket yang datang dari luar
// Dan juga, supaya tidak terpolusi, jangan io_printf()
static const uint new_dgf_init[] =
{
  0x01fe7cf1,   // 0  local MC packets
  0x01febcf1,   // 1  non-local MC packets
  0x01fe7cf2,   // 2  local P2P packets
  0x01febcf2,   // 3  non-local P2P packets: Dest=0x1FE
  0x01fe7cf4,   // 4  local NN packets
  0x01febcf4,   // 5  non-local NN packets
  0x01fe7cf8,   // 6  local FR packets
  0x01febcf8,   // 7  non-local FR packets
  0x0001fcf1,   // 8  dumped MC packets
  0x0001fcf2,   // 9  dumped P2P packets
  0x0008BCF2,   // 10 PP to east:       non-local: Dest-b000001000=0x008 Loc.PL-0xB Def.M-0xC er-0xF type-0x2
  0x0010BCF2,   // 11 PP to north-east: non-local: Dest-b000010000=0x010 Loc.PL-0xB Def.M-0xC er-0xF type-0x2
  0x0020BCF2,	// 12 PP to north:      non-local: Dest-b000100000=0x020 Loc.PL-0xB Def.M-0xC er-0xF type-0x2
  0x0040BCF2,	// 13 PP to west:       non-local: Dest-b001000000=0x040 Loc.PL-0xB Def.M-0xC er-0xF type-0x2
  0x0080BCF2,	// 14 PP to south-west: non-local: Dest-b010000000=0x080 Loc.PL-0xB Def.M-0xC er-0xF type-0x2
  0x0100BCF2	// 15 PP to south:      non-local: Dest-b100000000=0x100 Loc.PL-0xB Def.M-0xC er-0xF type-0x2
//	0x00087CF2,   // 10 PP to east:       local: Dest-b000001000=0x008 Loc.PL-0xB Def.M-0xC er-0xF type-0x2
//	0x00107CF2,   // 11 PP to north-east: local: Dest-b000010000=0x010 Loc.PL-0x7 Def.M-0xC er-0xF type-0x2
//	0x00207CF2,	// 12 PP to north:      local: Dest-b000100000=0x020 Loc.PL-0x7 Def.M-0xC er-0xF type-0x2
//	0x00407CF2,	// 13 PP to west:       local: Dest-b001000000=0x040 Loc.PL-0x7 Def.M-0xC er-0xF type-0x2
//	0x00807CF2,	// 14 PP to south-west: local: Dest-b010000000=0x080 Loc.PL-0x7 Def.M-0xC er-0xF type-0x2
//	0x01007CF2	// 15 PP to south:      local: Dest-b100000000=0x100 Loc.PL-0x7 Def.M-0xC er-0xF type-0x2
};


uint x_idx[48] = {0,1,2,3,4,0,1,2,3,4,5,0,1,2,3,4,5,6,0,1,2,3,4,5,6,7,1,2,3,4,5,6,7,2,3,4,5,6,7,3,4,5,6,7,4,5,6,7};
uint y_idx[48] = {0,0,0,0,0,1,1,1,1,1,1,2,2,2,2,2,2,2,3,3,3,3,3,3,3,3,4,4,4,4,4,4,4,5,5,5,5,5,5,6,6,6,6,6,7,7,7,7};

char strBuf[10];

void get_dir(uint dir)
{
  switch(dir) {
  case 0: io_printf(strBuf, "East"); break;
  case 1: io_printf(strBuf, "North-East"); break;
  case 2: io_printf(strBuf, "North"); break;
  case 3: io_printf(strBuf, "West"); break;
  case 4: io_printf(strBuf, "South-West"); break;
  case 5: io_printf(strBuf, "South"); break;
  case 6: io_printf(strBuf, "none"); break;
  case 7: io_printf(strBuf, "Local"); break;
  }
}

void printP2Ptable(void)
{
  uint dir;
  io_printf(IO_BUF, "\n==========================================\n");
  for(uint i=0; i<48; i++) {
	 dir = rtr_p2p_get((x_idx[i] << 8) + y_idx[i]);
	 get_dir(dir);
	 io_printf(IO_BUF, "From my point, chip<%d,%d> is the %s\n",x_idx[i],y_idx[i],strBuf);
  }
  io_printf(IO_BUF, "==========================================\n");
}

void printP2Pcntr(uint tick)
{
	io_printf(IO_BUF, "\n======= Counting-%d =========\n", tick);
	io_printf(IO_BUF, "E-cntr = %d\n", rtr[RTR_DGC10]);
	io_printf(IO_BUF, "NE-ctr = %d\n", rtr[RTR_DGC11]);
	io_printf(IO_BUF, "N-cntr = %d\n", rtr[RTR_DGC12]);
	io_printf(IO_BUF, "W-cntr = %d\n", rtr[RTR_DGC13]);
	io_printf(IO_BUF, "SW-ctr = %d\n", rtr[RTR_DGC14]);
	io_printf(IO_BUF, "S-cntr = %d\n", rtr[RTR_DGC15]);
	io_printf(IO_BUF, "------------------------\n");
}

void resetP2Pcntr()
{
	rtr[RTR_DGC10] = 0;
	rtr[RTR_DGC11] = 0;
	rtr[RTR_DGC12] = 0;
	rtr[RTR_DGC13] = 0;
	rtr[RTR_DGC14] = 0;
	rtr[RTR_DGC15] = 0;
}

void hTimer(uint tick, uint Null)
{
	printP2Pcntr(tick);
}

void c_main()
{
	io_printf(IO_BUF, "monitor is running in core-%d\n", sark_core_id());
	printP2Ptable();
	rtr_diag_init(new_dgf_init);
	resetP2Pcntr();
	// from time to time (every 1s) I'll print the diagnostic counter
	spin1_set_timer_tick(DIAG_CNTR_TICK);
	spin1_callback_on(TIMER_TICK, hTimer, PRIORITY_TIMER);
	spin1_start(SYNC_NOWAIT);
}

