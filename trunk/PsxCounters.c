/*  Pcsx - Pc Psx Emulator
 *  Copyright (C) 1999-2003  Pcsx Team
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */

#include "PsxCommon.h"
#include "PsxCounters.h"
#include "Cheat.h"
#include "R3000A.h"
#include "PsxMem.h"
#include "plugins.h"

psxCounter psxCounters[5];

static int cnts = 4;

// used to disable targets until after an overflow
#define PSXCNT_FUTURE_TARGET	(0x1000000000ULL)

static __inline void _rcntTestOverflow( int i )
{
	u64 maxTarget = ( i < 3 ) ? 0xffff : 0xfffffffful;
	if( psxCounters[i].count <= maxTarget ) return;

	if(psxCounters[i].mode & 0x20)
	{
		// Overflow interrupt
		psxHu32ref(0x1070) |= SWAPu32(psxCounters[i].interrupt);
		psxRegs.interrupt |= 0x80000000;
		psxCounters[i].mode |= 0x1000; // Overflow flag
		if(psxCounters[i].mode & 0x80)
		psxCounters[i].mode &= ~0x0400; // Interrupt flag
	}

	// Update count and target.
	// Count wraps around back to zero, while the target is restored (if needed).
	// (high bit of the target gets set by rcntWtarget when the target is behind
	// the counter value, and thus should not be flagged until after an overflow)
	
	psxCounters[i].count &= maxTarget;
	psxCounters[i].target &= maxTarget;
}

static void _rcntTestTarget( int i )
{
	if( psxCounters[i].count < psxCounters[i].target ) return;

	if (psxCounters[i].mode & 0x10)
	{
		// Target interrupt
		if(psxCounters[i].mode & 0x80)
			psxCounters[i].mode &= ~0x0400; // Interrupt flag
		psxCounters[i].mode |= 0x0800; // Target flag

		psxHu32ref(0x1070) |= SWAPu32(psxCounters[i].interrupt);
		psxRegs.interrupt |= 0x80000000;
	}
	
	if (psxCounters[i].mode & 0x08)
	{
		// Reset on target
		psxCounters[i].count -= psxCounters[i].target;
		if(!(psxCounters[i].mode & 0x40))
		{
			psxCounters[i].target |= PSXCNT_FUTURE_TARGET;
		}
	} else psxCounters[i].target |= PSXCNT_FUTURE_TARGET;
}

static void psxRcntUpd(int index) {
	psxCounters[index].sCycle = psxRegs.cycle;
	if (((!(psxCounters[index].mode & 1)) || (index!=2)) &&
		psxCounters[index].mode & 0x30) {
		if (psxCounters[index].mode & 0x10) { // Interrupt on target
			psxCounters[index].Cycle = ((psxCounters[index].target - psxCounters[index].count) * psxCounters[index].rate) / BIAS;//  - (psxRegs.cycle - psxCounters[index].sCycle);
		} else { // Interrupt on 0xffff
			psxCounters[index].Cycle = ((0xffff - psxCounters[index].count) * psxCounters[index].rate) / BIAS;// - (psxRegs.cycle - psxCounters[index].sCycle);
		}
	} else psxCounters[index].Cycle = 0xffffffff;
//	if (index == 2) SysPrintf("Cycle %x\n", psxCounters[index].Cycle);
}

static __inline void _rcntSet(int i) {
	s32 count;
	
	if (psxCounters[i].Cycle == 0xffffffff) return;
	
	u64 overflowCap = (i >= 3) ? 0x100000000ULL : 0x10000;

	if( psxCounters[i].count > overflowCap || psxCounters[i].count > psxCounters[i].target )
	{
		psxNextCounter = 4;
		return;
	}

	count = psxCounters[i].Cycle - (psxRegs.cycle - psxCounters[i].sCycle);

	if (count < 0) {
		psxNextCounter = 0;
		return;
	}

	if (count < (s32)psxNextCounter) {
		psxNextCounter = count;
	}
}

static void psxRcntSet() {
	int i;

	for (i = 0; i < cnts; i++) {
		_rcntSet( i );
	}
}

void psxRcntInit() {
	int i;
	memset(psxCounters, 0, sizeof(psxCounters));

	for(i = 0; i < 5; i++) {
		psxCounters[i].rate = 1;
		psxCounters[i].mode |= 0x0400;
		psxCounters[i].target = PSXCNT_FUTURE_TARGET;
		psxCounters[i].sCycle = psxRegs.cycle;
	}

	psxCounters[0].interrupt = 0x10;
	psxCounters[1].interrupt = 0x20;
	psxCounters[2].interrupt = 0x40;

	psxCounters[3].interrupt = 1;
	psxCounters[3].mode = 0x58; // The VSync counter mode
	psxCounters[3].target = 1;
	psxUpdateVSyncRate();

	if (SPU_async != NULL) {
		cnts = 5;

		psxCounters[4].rate = 768 * 64;
		psxCounters[4].Cycle = psxCounters[4].rate;
		psxCounters[4].mode = 0x58;
	} else cnts = 4;

	// Tell the PSX to branch ASAP, so that timers can get
	// configured properly.
	psxNextCounter = 1;
	psxNextsCounter = psxRegs.cycle;
}

void psxUpdateVSyncRate() {
	if (Config.PsxType) // ntsc - 0 | pal - 1
	     psxCounters[3].rate = (PSXCLK / 50);// / BIAS;		// Dr.Hell's doc	674399.5367
	else psxCounters[3].rate = (PSXCLK / 60);// / BIAS;		//					566107.5005
	psxCounters[3].rate-= (psxCounters[3].rate / 262) * 22;
	if (Config.VSyncWA) psxCounters[3].rate/= 2;
}

void psxUpdateVSyncRateEnd() {
	if (Config.PsxType) // ntsc - 0 | pal - 1
	     psxCounters[3].rate = (PSXCLK / 50);// / BIAS;
	else psxCounters[3].rate = (PSXCLK / 60);// / BIAS;
	psxCounters[3].rate = (psxCounters[3].rate / 262) * 22;
	if (Config.VSyncWA) psxCounters[3].rate/= 2;
}

void psxRcntUpdate() {
	int i;
	psxNextCounter = 0x7fffffff;
	psxNextsCounter = psxRegs.cycle;

	if( psxTestCycle( psxCounters[3].sCycle, psxCounters[3].Cycle ) ) {
		if (psxCounters[3].mode & 0x10000) { // VSync End (22 hsyncs)
			psxCounters[3].mode &=~ 0x10000;
			psxUpdateVSyncRate();
			psxRcntUpd(3);
			GPU_updateLace(); // updateGPU
			SysUpdate();
			ApplyCheats();
#ifdef GTE_LOG
			GTE_LOG("VSync\n");
#endif
		} else { // VSync Start (240 hsyncs) 
			psxCounters[3].mode |= 0x10000;
			psxUpdateVSyncRateEnd();
			psxRcntUpd(3);
			psxHu32ref(0x1070) |= SWAPu32(1);
			psxRegs.interrupt|= 0x80000000;
		}
	}

	for(i = 0; i < 3; i++){
		s32 change = psxRegs.cycle - psxCounters[i].sCycle;

		if( change < 0 ) change = 0;
		psxCounters[i].count += change / psxCounters[i].rate;
		change -= (change / psxCounters[i].rate) * psxCounters[i].rate;
		psxCounters[i].sCycle = psxRegs.cycle - change;
		_rcntTestTarget( i );
		_rcntTestOverflow( i );
	}

		if (cnts >= 5) {
		const s32 difference = psxRegs.cycle - psxCounters[4].sCycle;
		s32 c = psxCounters[4].Cycle;
		if(difference >= psxCounters[4].Cycle) {
			SPU_async(difference);
			psxCounters[4].sCycle = psxRegs.cycle;
			psxCounters[4].Cycle = psxCounters[4].rate;
			psxHu32ref(0x1070) |= SWAPu32(psxCounters[4].interrupt);
			psxRegs.interrupt |= 0x80000000;
		}
		else c -= difference;
		psxNextCounter = c;
	}

	psxRcntSet();
}

void psxRcntWcount(u32 index, u32 value) {
//	SysPrintf("writeCcount[%d] = %x\n", index, value);
//	PSXCPU_LOG("writeCcount[%d] = %x\n", index, value);
	//psxCounters[index].count = value;
	if(index < 3)
	{
		psxCounters[index].count = value & 0xffff;
		psxCounters[index].target &= 0xffff;
	}
	else
	{
		psxCounters[index].count = value & 0xffffffff;
		psxCounters[index].target &= 0xffffffff;
	}
	psxRcntUpd(index);
	_rcntSet(index);
}

void psxRcntWmode(u32 index, u32 value)  {
//	SysPrintf("writeCmode[%ld] = %lx\n", index, value);
	psxCounters[index].mode = value;
	psxCounters[index].mode |= 0x0400;

	psxCounters[index].count = 0;
	if(index == 0) {
		switch (value & 0x300) {
			case 0x100:
				psxCounters[index].rate = ((psxCounters[3].rate) / 386) / 262; // seems ok
				break;
			default:
				psxCounters[index].rate = 1;
		}
	}
	else if(index == 1) {
		switch (value & 0x300) {
			case 0x100:
				psxCounters[index].rate = (psxCounters[3].rate) / 262; // seems ok
				break;
			default:
				psxCounters[index].rate = 1;
		}
	}
	else if(index == 2) {
		switch (value & 0x300) {
			case 0x200:
				psxCounters[index].rate = 8; // 1/8 speed
				break;
			default:
				psxCounters[index].rate = 1; // normal speed
		}
	}

	// Need to set a rate and target
	if(index < 3)
		psxCounters[index].target &= 0xffff;
	else
		psxCounters[index].target &= 0xffffffff;
	psxCounters[index].sCycle = psxRegs.cycle;
	psxRcntUpd(index);
	_rcntSet(index);
}

void psxRcntWtarget(u32 index, u32 value) {
//	SysPrintf("writeCtarget[%ld] = %lx\n", index, value);
	//psxCounters[index].target = value;
	if(index < 3)
		psxCounters[index].target = value & 0xffff;
	else
		psxCounters[index].target = value;
	psxRcntUpd(index);
	_rcntSet(index);
}

u32 psxRcntRcount(u32 index) {
	u32 ret;
        
//	if ((!(psxCounters[index].mode & 1)) || (index!=2)) {
		if (psxCounters[index].mode & 0x08) { // Wrap at target
			if (Config.RCntFix) { // Parasite Eve 2
				ret = (psxCounters[index].count + /*BIAS **/ ((psxRegs.cycle - psxCounters[index].sCycle) / psxCounters[index].rate)) & 0xffff;
			} else {
				ret = (psxCounters[index].count + BIAS * ((psxRegs.cycle - psxCounters[index].sCycle) / psxCounters[index].rate)) & 0xffff;
			}
		} else { // Wrap at 0xffff
			ret = (psxCounters[index].count + BIAS * (psxRegs.cycle / psxCounters[index].rate)) & 0xffff;
			if (Config.RCntFix) { // Vandal Hearts 1/2
				ret/= 16;
			}
		}
//		return (psxCounters[index].count + BIAS * ((psxRegs.cycle - psxCounters[index].sCycle) / psxCounters[index].rate)) & 0xffff;
//	} else return 0;

//	SysPrintf("readCcount[%ld] = %lx (mode %lx, target %lx, cycle %lx)\n", index, ret, psxCounters[index].mode, psxCounters[index].target, psxRegs.cycle);

	return ret;
}

int psxRcntFreeze(gzFile f, int Mode) {
	char Unused[4096 - sizeof(psxCounter)];
	
	gzfreezel(psxCounters);
	gzfreezel(Unused);

	return 0;
}