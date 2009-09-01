/***************************************************************************
 *   Copyright (C) 2007 Ryan Schultz, PCSX-df Team, PCSX team              *
 *   schultz.ryan@gmail.com, http://rschultz.ath.cx/code.php               *
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 *   This program is distributed in the hope that it will be useful,       *
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of        *
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the         *
 *   GNU General Public License for more details.                          *
 *                                                                         *
 *   You should have received a copy of the GNU General Public License     *
 *   along with this program; if not, write to the                         *
 *   Free Software Foundation, Inc.,                                       *
 *   59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.             *
 ***************************************************************************/

/*  Playstation Memory Map (from Playstation doc by Joshua Walker)
0x0000_0000-0x0000_ffff		Kernel (64K)	
0x0001_0000-0x001f_ffff		User Memory (1.9 Meg)	
		
0x1f00_0000-0x1f00_ffff		Parallel Port (64K)	
		
0x1f80_0000-0x1f80_03ff		Scratch Pad (1024 bytes)	
		
0x1f80_1000-0x1f80_2fff		Hardware Registers (8K)	
		
0x8000_0000-0x801f_ffff		Kernel and User Memory Mirror (2 Meg) Cached	
		
0xa000_0000-0xa01f_ffff		Kernel and User Memory Mirror (2 Meg) Uncached	
		
0xbfc0_0000-0xbfc7_ffff		BIOS (512K)
*/
 
/*
* PSX memory functions.
*/

#include <malloc.h>
#include <gccore.h>
#include <stdlib.h>
#include "PsxMem.h"
#include "R3000A/R3000A.h"
#include "PsxHw.h"

#ifdef HW_RVL
#include "Gamecube/MEM2.h"
#endif

int psxMemInit() {
	int i;

	psxMemRLUT = (u8**)memalign(32,0x10000 * sizeof(void*));
	psxMemWLUT = (u8**)memalign(32,0x10000 * sizeof(void*));
	memset(psxMemRLUT, 0, 0x10000 * sizeof(void*));
	memset(psxMemWLUT, 0, 0x10000 * sizeof(void*));
	psxM = memalign(32,0x00220000);

	psxP = &psxM[0x200000];
	psxH = &psxM[0x210000];
#ifdef HW_RVL
	psxR = (s8*)BIOS_LO;
#else
	psxR = (s8*)memalign(32,0x00080000);
#endif
	if (psxMemRLUT == NULL || psxMemWLUT == NULL || 
		psxM == NULL || psxP == NULL || psxH == NULL) {
		SysMessage(_("Error allocating memory!")); return -1;
	}

// MemR
	for (i=0; i<0x80; i++) psxMemRLUT[i + 0x0000] = (u8*)&psxM[(i & 0x1f) << 16];
	
	memcpy(psxMemRLUT + 0x8000, psxMemRLUT, 0x80 * sizeof(void*));
	memcpy(psxMemRLUT + 0xa000, psxMemRLUT, 0x80 * sizeof(void*));

	for (i=0; i<0x01; i++) psxMemRLUT[i + 0x1f00] = (u8*)&psxP[i << 16];

	for (i=0; i<0x01; i++) psxMemRLUT[i + 0x1f80] = (u8*)&psxH[i << 16];

	for (i=0; i<0x08; i++) psxMemRLUT[i + 0xbfc0] = (u8*)&psxR[i << 16];

// MemW
	for (i=0; i<0x80; i++) psxMemWLUT[i + 0x0000] = (u8*)&psxM[(i & 0x1f) << 16];
	memcpy(psxMemWLUT + 0x8000, psxMemWLUT, 0x80 * sizeof(void*));
	memcpy(psxMemWLUT + 0xa000, psxMemWLUT, 0x80 * sizeof(void*));

	for (i=0; i<0x01; i++) psxMemWLUT[i + 0x1f00] = (u8*)&psxP[i << 16];

	for (i=0; i<0x01; i++) psxMemWLUT[i + 0x1f80] = (u8*)&psxH[i << 16];

	return 0;
}

void psxMemReset() {
	FILE *f = NULL;
	char bios[256];

	memset(psxM, 0, 0x00200000);
	memset(psxP, 0, 0x00010000);

	if (strcmp(Config.Bios, "HLE")) {
		sprintf (bios,"%s%s",Config.BiosDir, Config.Bios);
		f = fopen(bios, "rb");

		if (f == NULL) {
			SysMessage (_("Could not open BIOS:\"%s\". Enabling HLE Bios!\n"), bios);
			memset(psxR, 0, 0x80000);
			Config.HLE = BIOS_HLE;
		}
		else {
			fread(psxR, 1, 0x80000, f);
			fclose(f);
			Config.HLE = BIOS_USER_DEFINED;
		}
	} else Config.HLE = BIOS_HLE;
}

void psxMemShutdown() {
#ifndef HW_RVL
	free(psxR);
#endif
	free(psxM);
	free(psxMemRLUT);
	free(psxMemWLUT);
}

static int writeok=1;

u8 psxMemRead8(u32 mem) {
	u32 t = mem >> 16;

	if (t == 0x1f80)
	{
		if( mem & 0xf0001000 )
			return psxHwRead8(mem);
		else
			return psxHu8(mem);
	}
	else if (t == 0x1f40)
	{
		return psxHwRead8(mem);
	}
	else {
		char *p = (char *)(psxMemRLUT[t]);
		if (p != NULL) {
			return *(u8 *)(p + (mem & 0xffff));
		} else {
#ifdef PSXMEM_LOG
			PSXMEM_LOG("err lb %8.8lx\n", mem);
#endif
			return 0;
		}
	}
}

u16 psxMemRead16(u32 mem) {
	u32 t = mem >> 16;

	if (t == 0x1f80)
	{
		if( mem & 0xf0001000 )
			return psxHwRead16(mem);
		else
			return psxHu16(mem);
	}
	else {
		char *p = (char *)(psxMemRLUT[t]);
		if (p != NULL) {
			return GETLE16((u16 *)(p + (mem & 0xffff)));
		} else {
#ifdef PSXMEM_LOG
			PSXMEM_LOG("err lh %8.8lx\n", mem);
#endif
			return 0;
		}
	}
}

u32 psxMemRead32(u32 mem) {
	u32 t = mem >> 16;

	if (t == 0x1f80)
	{
		if( mem & 0xf0001000 )
			return psxHwRead32(mem);
		else
			return psxHu32(mem);
	}
	else {
		char *p = (char *)(psxMemRLUT[t]);
		if (p != NULL) {
			return GETLE32((u32 *)(p + (mem & 0xffff)));
		} else {
#ifdef PSXMEM_LOG
			if (writeok) { PSXMEM_LOG("err lw %8.8lx\n", mem); }
#endif
			return 0;
		}
	}
}

void psxMemWrite8(u32 mem, u8 value) {
	u32 t = mem >> 16;

	if (t == 0x1f80) {
		if( mem & 0xf0001000 )
			psxHwWrite8(mem,value);
		else
			psxHu8(mem) = value;
	}
	else if (t == 0x1f40)
	{
		psxHwWrite8(mem, value);
	} 
	else {
		char *p = (char *)(psxMemWLUT[t]);
		if (p != NULL) {
			*(u8  *)(p + (mem & 0xffff)) = value;
#ifdef PSXREC
			psxCpu->Clear((mem & (~3)), 1);
#endif
		} else {
#ifdef PSXMEM_LOG
			PSXMEM_LOG("err sb %8.8lx\n", mem);
#endif
		}
	}
}

void psxMemWrite16(u32 mem, u16 value) {
	u32 t = mem >> 16;

	if (t == 0x1f80) {
		if( mem & 0xf0001000 )
			psxHwWrite16(mem, value);
		else
			PUTLE16(&psxHu16ref(mem), value);
	} 
	else {
		char *p = (char *)(psxMemWLUT[t]);
		if (p != NULL && !(psxRegs.CP0.n.Status & 0x10000) ) {
			PUTLE16((u16 *)(p + (mem & 0xffff)), value);
#ifdef PSXREC
			psxCpu->Clear((mem & (~1)), 1);
#endif
		} else {
#ifdef PSXMEM_LOG
			PSXMEM_LOG("err sh %8.8lx\n", mem);
#endif
		}
	}
}

void psxMemWrite32(u32 mem, u32 value) {
	u32 t = mem >> 16;

	if (t == 0x1f80) {
		if( mem & 0xf0001000 )
			psxHwWrite32(mem, value);
		else
			PUTLE32(&psxHu32ref(mem), value);
	} 
	else {
		char *p = (char *)(psxMemWLUT[t]);
		if (p != NULL && !(psxRegs.CP0.n.Status & 0x10000)) {
			PUTLE32((u32 *)(p + (mem & 0xffff)), value);
#ifdef PSXREC
			psxCpu->Clear(mem, 1);
#endif
		}
		else 
		{
#ifdef PSXREC
			if (mem != 0xfffe0130) 
			{

				if (!writeok)
					psxCpu->Clear(mem, 1);

#ifdef PSXMEM_LOG
				if (writeok) { PSXMEM_LOG("err sw %8.8lx\n", mem); }
#endif
			} 
			else 
#endif	//PSXREC
			{
				if (t == 0x1d00)
				{
					int i;

					switch (value) {
						case 0x800: case 0x804:
							if (writeok == 0) break;
							writeok = 0;
							memset(psxMemWLUT + 0x0000, 0, 0x80 * sizeof(void*));
							memset(psxMemWLUT + 0x8000, 0, 0x80 * sizeof(void*));
							memset(psxMemWLUT + 0xa000, 0, 0x80 * sizeof(void*));
							break;
						case 0x1e988:
							if (writeok == 1) break;
							writeok = 1;

							for (i=0; i<0x80; i++) 
								psxMemWLUT[i + 0x0000] = (void*)&psxM[(i & 0x1f) << 16];
							memcpy(psxMemWLUT + 0x8000, psxMemWLUT, 0x80 * sizeof(void*));
							memcpy(psxMemWLUT + 0xa000, psxMemWLUT, 0x80 * sizeof(void*));

							break;
						default:
#ifdef PSXMEM_LOG
							PSXMEM_LOG("unk %8.8lx = %x\n", mem, value);
#endif
							break;
					}
				}
			}
		}
	}
}

void *psxMemPointer(u32 mem) {
	
	u32 t = mem >> 16;

	if (t == 0x1f80) {
		if (mem < 0x1f801000)
			return (void *)&psxH[mem];
		else
			return NULL;
	} else {
		char *p = (char *)(psxMemWLUT[t]);
		if (p != NULL) {
			return (void *)(p + (mem & 0xffff));
		}
		return NULL;
	}
}
