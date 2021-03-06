#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <malloc.h>

extern "C" {

#include "PS2Etypes.h"

#if defined(_WIN32)
#include <windows.h>
#endif

#include "System.h"
#include "R5900.h"
#include "Vif.h"
#include "VU.h"
#include "ix86/ix86.h"
#include "iCore.h"
#include "R3000A.h"

u16 x86FpuState, iCWstate;
u16 g_mmxAllocCounter = 0;

// used to make sure regs don't get changed while in recompiler
// use FreezeMMXRegs, FreezeXMMRegs
u8 g_globalMMXSaved = 0;

#ifdef _DEBUG
char g_globalMMXLocked = 0;
#endif

PCSX2_ALIGNED16(u64 g_globalMMXData[8]);

// X86 caching
extern _x86regs x86regs[X86REGS];
int g_x86checknext;
extern u16 g_x86AllocCounter;

} // end extern "C"

#include <vector>
using namespace std;

// use special x86 register allocation for ia32
#ifndef __x86_64__

void _initX86regs() {
	memset(x86regs, 0, sizeof(x86regs));
	g_x86AllocCounter = 0;
	g_x86checknext = 0;
}

u32 _x86GetAddr(int type, int reg)
{
	switch(type&~X86TYPE_VU1) {
		case X86TYPE_GPR: return (u32)&cpuRegs.GPR.r[reg];
		case X86TYPE_VI: {
			//assert( reg < 16 || reg == REG_R );
			return (type&X86TYPE_VU1)?(u32)&VU1.VI[reg]:(u32)&VU0.VI[reg];
		}
		case X86TYPE_MEMOFFSET: return 0;
		case X86TYPE_VIMEMOFFSET: return 0;
		case X86TYPE_VUQREAD: return (type&X86TYPE_VU1)?(u32)&VU1.VI[REG_Q]:(u32)&VU0.VI[REG_Q];
		case X86TYPE_VUPREAD: return (type&X86TYPE_VU1)?(u32)&VU1.VI[REG_P]:(u32)&VU0.VI[REG_P];
		case X86TYPE_VUQWRITE: return (type&X86TYPE_VU1)?(u32)&VU1.q:(u32)&VU0.q;
		case X86TYPE_VUPWRITE: return (type&X86TYPE_VU1)?(u32)&VU1.p:(u32)&VU0.p;
		case X86TYPE_PSX: return (u32)&psxRegs.GPR.r[reg];
		case X86TYPE_PCWRITEBACK:
			return (u32)&g_recWriteback;
		case X86TYPE_VUJUMP:
			return (u32)&g_recWriteback;
		default: assert(0);
	}

	return 0;
}

int _getFreeX86reg(int mode)
{
	int i, tempi;
	u32 bestcount = 0x10000;

	int maxreg = (mode&MODE_8BITREG)?4:X86REGS;

	for (i=0; i<X86REGS; i++) {
		int reg = (g_x86checknext+i)%X86REGS;
		if( reg == 0 || reg == ESP ) continue;
		if( reg >= maxreg ) continue;
		if( (mode&MODE_NOFRAME) && reg==EBP ) continue;

		if (x86regs[reg].inuse == 0) {
			g_x86checknext = (reg+1)%X86REGS;
			return reg;
		}
	}

	tempi = -1;
	for (i=1; i<maxreg; i++) {
		if( i == ESP ) continue;
		if( (mode&MODE_NOFRAME) && i==EBP ) continue;

		if (x86regs[i].needed) continue;
		if (x86regs[i].type != X86TYPE_TEMP) {

			if( x86regs[i].counter < bestcount ) {
				tempi = i;
				bestcount = x86regs[i].counter;
			}
			continue;
		}

		_freeX86reg(i);
		return i;
	}

	if( tempi != -1 ) {
		_freeX86reg(tempi);
		return tempi;
	}
	SysPrintf("*PCSX2*: x86 error\n");
	assert(0);

	return -1;
}

int _allocX86reg(int x86reg, int type, int reg, int mode)
{
	int i;
	assert( reg >= 0 && reg < 32 );

//	if( X86_ISVI(type) )
//		assert( reg < 16 || reg == REG_R );
	 
	// don't alloc EAX and ESP,EBP if MODE_NOFRAME
	int oldmode = mode;
	int noframe = mode&MODE_NOFRAME;
	int maxreg = (mode&MODE_8BITREG)?4:X86REGS;
	mode &= ~(MODE_NOFRAME|MODE_8BITREG);
	int readfromreg = -1;

	if( type != X86TYPE_TEMP ) {

		if( maxreg < X86REGS ) {
			// make sure reg isn't in the higher regs
			
			for(i = maxreg; i < X86REGS; ++i) {
				if (!x86regs[i].inuse || x86regs[i].type != type || x86regs[i].reg != reg) continue;

				if( mode & MODE_READ ) {
					readfromreg = i;
					x86regs[i].inuse = 0;
					break;
				}
				else if( mode & MODE_WRITE ) {
					x86regs[i].inuse = 0;
					break;
				}
			}
		}

		for (i=1; i<maxreg; i++) {
			if( i == ESP ) continue;

			if (!x86regs[i].inuse || x86regs[i].type != type || x86regs[i].reg != reg) continue;

			if( (noframe && i == EBP) || (i >= maxreg) ) {
				if( x86regs[i].mode & MODE_READ )
					readfromreg = i;
				//if( xmmregs[i].mode & MODE_WRITE ) mode |= MODE_WRITE;
				mode |= x86regs[i].mode&MODE_WRITE;
				x86regs[i].inuse = 0;
				break;
			}

			if( x86reg >= 0 ) {
				// requested specific reg, so return that instead
				if( i != x86reg ) {
					if( x86regs[i].mode & MODE_READ ) readfromreg = i;
					//if( x86regs[i].mode & MODE_WRITE ) mode |= MODE_WRITE;
					mode |= x86regs[i].mode&MODE_WRITE;
					x86regs[i].inuse = 0;
					break;
				}
			}

			if( type != X86TYPE_TEMP && !(x86regs[i].mode & MODE_READ) && (mode&MODE_READ)) {

				if( type == X86TYPE_GPR ) _flushConstReg(reg);
				
				if( X86_ISVI(type) && reg < 16 ) MOVZX32M16toR(i, _x86GetAddr(type, reg));
				else MOV32MtoR(i, _x86GetAddr(type, reg));

				x86regs[i].mode |= MODE_READ;
			}

			x86regs[i].needed = 1;
			x86regs[i].mode|= mode;
			return i;
		}
	}

	if (x86reg == -1) {
		x86reg = _getFreeX86reg(oldmode);
	}
	else {
		_freeX86reg(x86reg);
	}

	x86regs[x86reg].type = type;
	x86regs[x86reg].reg = reg;
	x86regs[x86reg].mode = mode;
	x86regs[x86reg].needed = 1;
	x86regs[x86reg].inuse = 1;

	if( mode & MODE_READ ) {
		if( readfromreg >= 0 ) MOV32RtoR(x86reg, readfromreg);
		else {
			if( type == X86TYPE_GPR ) {

				if( reg == 0 ) {
					XOR32RtoR(x86reg, x86reg);
				}
				else {
					_flushConstReg(reg);
					_deleteMMXreg(MMX_GPR+reg, 1);
					_deleteGPRtoXMMreg(reg, 1);
					
					_eeMoveGPRtoR(x86reg, reg);
					
					_deleteMMXreg(MMX_GPR+reg, 0);
					_deleteGPRtoXMMreg(reg, 0);
				}
			}
			else {
				if( X86_ISVI(type) && reg < 16 ) {
					if( reg == 0 ) XOR32RtoR(x86reg, x86reg);
					else MOVZX32M16toR(x86reg, _x86GetAddr(type, reg));
				}
				else MOV32MtoR(x86reg, _x86GetAddr(type, reg));
			}
		}
}

	return x86reg;
}

int _checkX86reg(int type, int reg, int mode)
{
	int i;

	for (i=0; i<X86REGS; i++) {
		if (x86regs[i].inuse && x86regs[i].reg == reg && x86regs[i].type == type) {

			if( !(x86regs[i].mode & MODE_READ) && (mode&MODE_READ) ) {
				if( X86_ISVI(type) ) MOVZX32M16toR(i, _x86GetAddr(type, reg));
				else MOV32MtoR(i, _x86GetAddr(type, reg));
			}

			x86regs[i].mode |= mode;
			x86regs[i].counter = g_x86AllocCounter++;
			x86regs[i].needed = 1;
			return i;
		}
	}

	return -1;
}

void _addNeededX86reg(int type, int reg)
{
	int i;

	for (i=0; i<X86REGS; i++) {
		if (!x86regs[i].inuse || x86regs[i].reg != reg || x86regs[i].type != type ) continue;

		x86regs[i].counter = g_x86AllocCounter++;
		x86regs[i].needed = 1;
	}
}

void _clearNeededX86regs() {
	int i;

	for (i=0; i<X86REGS; i++) {
		if (x86regs[i].needed ) {
			if( x86regs[i].inuse && (x86regs[i].mode&MODE_WRITE) )
				x86regs[i].mode |= MODE_READ;
		}
		x86regs[i].needed = 0;
	}
}

void _deleteX86reg(int type, int reg, int flush)
{
	int i;

	for (i=0; i<X86REGS; i++) {
		if (x86regs[i].inuse && x86regs[i].reg == reg && x86regs[i].type == type) {
			switch(flush) {
				case 0:
					_freeX86reg(i);
					break;
				case 1:
					if( x86regs[i].mode & MODE_WRITE) {

						if( X86_ISVI(type) && x86regs[i].reg < 16 ) MOV16RtoM(_x86GetAddr(type, x86regs[i].reg), i);
						else MOV32RtoM(_x86GetAddr(type, x86regs[i].reg), i);
						
						// get rid of MODE_WRITE since don't want to flush again
						x86regs[i].mode &= ~MODE_WRITE;
						x86regs[i].mode |= MODE_READ;
					}
					return;
				case 2:
					x86regs[i].inuse = 0;
					break;
			}
		}
	}
}

void _freeX86reg(int x86reg)
{
	assert( x86reg >= 0 && x86reg < X86REGS );

	if( x86regs[x86reg].inuse && (x86regs[x86reg].mode&MODE_WRITE) ) {
		x86regs[x86reg].mode &= ~MODE_WRITE;

		if( X86_ISVI(x86regs[x86reg].type) && x86regs[x86reg].reg < 16 ) {
			MOV16RtoM(_x86GetAddr(x86regs[x86reg].type, x86regs[x86reg].reg), x86reg);
		}
		else
			MOV32RtoM(_x86GetAddr(x86regs[x86reg].type, x86regs[x86reg].reg), x86reg);
	}

	x86regs[x86reg].inuse = 0;
}

void _freeX86regs() {
	int i;

	for (i=0; i<X86REGS; i++) {
		if (!x86regs[i].inuse) continue;

		_freeX86reg(i);
	}
}

#endif // __x86_64__

// MMX Caching
_mmxregs mmxregs[8], s_saveMMXregs[8];
static int s_mmxchecknext = 0;

void _initMMXregs()
{
	memset(mmxregs, 0, sizeof(mmxregs));
	g_mmxAllocCounter = 0;
	s_mmxchecknext = 0;
}

__forceinline void* _MMXGetAddr(int reg)
{
	assert( reg != MMX_TEMP );
	
	if( reg == MMX_LO ) return &cpuRegs.LO;
	if( reg == MMX_HI ) return &cpuRegs.HI;
	if( reg == MMX_FPUACC ) return &fpuRegs.ACC;

	if( reg >= MMX_GPR && reg < MMX_GPR+32 ) return &cpuRegs.GPR.r[reg&31];
	if( reg >= MMX_FPU && reg < MMX_FPU+32 ) return &fpuRegs.fpr[reg&31];
	if( reg >= MMX_COP0 && reg < MMX_COP0+32 ) return &cpuRegs.CP0.r[reg&31];
	
	assert( 0 );
	return NULL;
}

int  _getFreeMMXreg()
{
	int i, tempi;
	u32 bestcount = 0x10000;

	for (i=0; i<MMXREGS; i++) {
		if (mmxregs[(s_mmxchecknext+i)%MMXREGS].inuse == 0) {
			int ret = (s_mmxchecknext+i)%MMXREGS;
			s_mmxchecknext = (s_mmxchecknext+i+1)%MMXREGS;
			return ret;
		}
	}

	// check for dead regs
	for (i=0; i<MMXREGS; i++) {
		if (mmxregs[i].needed) continue;
		if (mmxregs[i].reg >= MMX_GPR && mmxregs[i].reg < MMX_GPR+34 ) {
			if( !(g_pCurInstInfo->regs[mmxregs[i].reg-MMX_GPR] & (EEINST_LIVE0|EEINST_LIVE1)) ) {
				_freeMMXreg(i);
				return i;
			}
			if( !(g_pCurInstInfo->regs[mmxregs[i].reg-MMX_GPR]&EEINST_USED) ) {
				_freeMMXreg(i);
				return i;
			}
		}
	}

	// check for future xmm usage
	for (i=0; i<MMXREGS; i++) {
		if (mmxregs[i].needed) continue;
		if (mmxregs[i].reg >= MMX_GPR && mmxregs[i].reg < MMX_GPR+34 ) {
			if( !(g_pCurInstInfo->regs[mmxregs[i].reg] & EEINST_MMX) ) {
				_freeMMXreg(i);
				return i;
			}
		}
	}

	tempi = -1;
	for (i=0; i<MMXREGS; i++) {
		if (mmxregs[i].needed) continue;
		if (mmxregs[i].reg != MMX_TEMP) {

			if( mmxregs[i].counter < bestcount ) {
				tempi = i;
				bestcount = mmxregs[i].counter;
			}
			continue;
		}

		_freeMMXreg(i);
		return i;
	}

	if( tempi != -1 ) {
		_freeMMXreg(tempi);
		return tempi;
	}
	SysPrintf("*PCSX2*: mmx error\n");
	assert(0);

	return -1;
}

int _allocMMXreg(int mmxreg, int reg, int mode)
{
	int i;

	if( reg != MMX_TEMP ) {
		for (i=0; i<MMXREGS; i++) {
			if (mmxregs[i].inuse == 0 || mmxregs[i].reg != reg ) continue;

			if( MMX_ISGPR(reg)) {
				assert( _checkXMMreg(XMMTYPE_GPRREG, reg-MMX_GPR, 0) == -1 );
			}

			mmxregs[i].needed = 1;

			if( !(mmxregs[i].mode & MODE_READ) && (mode&MODE_READ) && reg != MMX_TEMP ) {

				SetMMXstate();
				if( reg == MMX_GPR ) {
					// moving in 0s
					PXORRtoR(i, i);
				}
				else {
					if( MMX_ISGPR(reg) ) _flushConstReg(reg-MMX_GPR);
					if( (mode & MODE_READHALF) || (MMX_IS32BITS(reg)&&(mode&MODE_READ)) )
						MOVDMtoMMX(i, (u32)_MMXGetAddr(reg));
					else {
						MOVQMtoR(i, (u32)_MMXGetAddr(reg));
					}
				}

				mmxregs[i].mode |= MODE_READ;
			}

			mmxregs[i].counter = g_mmxAllocCounter++;
			mmxregs[i].mode|= mode;
			return i;
		}
	}

	if (mmxreg == -1) {
		mmxreg = _getFreeMMXreg();
	}

	mmxregs[mmxreg].inuse = 1;
	mmxregs[mmxreg].reg = reg;
	mmxregs[mmxreg].mode = mode&~MODE_READHALF;
	mmxregs[mmxreg].needed = 1;
	mmxregs[mmxreg].counter = g_mmxAllocCounter++;

	SetMMXstate();
	if( reg == MMX_GPR ) {
		// moving in 0s
		PXORRtoR(mmxreg, mmxreg);
	}
	else {
		int xmmreg;
		if( MMX_ISGPR(reg) && (xmmreg = _checkXMMreg(XMMTYPE_GPRREG, reg-MMX_GPR, 0)) >= 0 ) {
			if (cpucaps.hasStreamingSIMD2Extensions) {
				SSE_MOVHPS_XMM_to_M64((u32)_MMXGetAddr(reg)+8, xmmreg);
				if( mode & MODE_READ )
					SSE2_MOVDQ2Q_XMM_to_MM(mmxreg, xmmreg);

				if( xmmregs[xmmreg].mode & MODE_WRITE )
					mmxregs[mmxreg].mode |= MODE_WRITE;

				// don't flush
				xmmregs[xmmreg].inuse = 0;
			}
			else {
				_freeXMMreg(xmmreg);

				if( (mode & MODE_READHALF) || (MMX_IS32BITS(reg)&&(mode&MODE_READ)) ) {
					MOVDMtoMMX(mmxreg, (u32)_MMXGetAddr(reg));
				}
				else if( mode & MODE_READ ) {
					MOVQMtoR(mmxreg, (u32)_MMXGetAddr(reg));
				}

			}
		}
		else {
			if( MMX_ISGPR(reg) ) {
				if(mode&(MODE_READHALF|MODE_READ)) _flushConstReg(reg-MMX_GPR);
			}

			if( (mode & MODE_READHALF) || (MMX_IS32BITS(reg)&&(mode&MODE_READ)) ) {
				MOVDMtoMMX(mmxreg, (u32)_MMXGetAddr(reg));
			}
			else if( mode & MODE_READ ) {
				MOVQMtoR(mmxreg, (u32)_MMXGetAddr(reg));
			}
		}
	}

	return mmxreg;
}

int _checkMMXreg(int reg, int mode)
{
	int i;
	for (i=0; i<MMXREGS; i++) {
		if (mmxregs[i].inuse && mmxregs[i].reg == reg ) {

			if( !(mmxregs[i].mode & MODE_READ) && (mode&MODE_READ) ) {

				if( reg == MMX_GPR ) {
					// moving in 0s
					PXORRtoR(i, i);
				}
				else {
					if( MMX_ISGPR(reg) && (mode&(MODE_READHALF|MODE_READ)) ) _flushConstReg(reg-MMX_GPR);
					if( (mode & MODE_READHALF) || (MMX_IS32BITS(reg)&&(mode&MODE_READ)) )
						MOVDMtoMMX(i, (u32)_MMXGetAddr(reg));
					else
						MOVQMtoR(i, (u32)_MMXGetAddr(reg));
				}
				SetMMXstate();
			}

			mmxregs[i].mode |= mode;
			mmxregs[i].counter = g_mmxAllocCounter++;
			mmxregs[i].needed = 1;
			return i;
		}
	}

	return -1;
}

void _addNeededMMXreg(int reg)
{
	int i;

	for (i=0; i<MMXREGS; i++) {
		if (mmxregs[i].inuse == 0) continue;
		if (mmxregs[i].reg != reg) continue;

		mmxregs[i].counter = g_mmxAllocCounter++;
		mmxregs[i].needed = 1;
	}
}

void _clearNeededMMXregs()
{
	int i;

	for (i=0; i<MMXREGS; i++) {

		if( mmxregs[i].needed ) {

			// setup read to any just written regs
			if( mmxregs[i].inuse && (mmxregs[i].mode&MODE_WRITE) )
				mmxregs[i].mode |= MODE_READ;
			mmxregs[i].needed = 0;
		}
	}
}

// when flush is 0 - frees all of the reg, when flush is 1, flushes all of the reg, when
// it is 2, just stops using the reg (no flushing)
void _deleteMMXreg(int reg, int flush)
{
	int i;
	for (i=0; i<MMXREGS; i++) {

		if (mmxregs[i].inuse && mmxregs[i].reg == reg ) {

			switch(flush) {
				case 0:
					_freeMMXreg(i);
					break;
				case 1:
					if( mmxregs[i].mode & MODE_WRITE) {
						assert( mmxregs[i].reg != MMX_GPR );

						if( MMX_IS32BITS(reg) )
							MOVDMMXtoM((u32)_MMXGetAddr(mmxregs[i].reg), i);
						else
							MOVQRtoM((u32)_MMXGetAddr(mmxregs[i].reg), i);
						SetMMXstate();

						// get rid of MODE_WRITE since don't want to flush again
						mmxregs[i].mode &= ~MODE_WRITE;
						mmxregs[i].mode |= MODE_READ;
					}
					return;
				case 2:
					mmxregs[i].inuse = 0;
					break;
			}

			
			return;
		}
	}
}

int _getNumMMXwrite()
{
	int num = 0, i;
	for (i=0; i<MMXREGS; i++) {
		if( mmxregs[i].inuse && (mmxregs[i].mode&MODE_WRITE) ) ++num;
	}

	return num;
}

u8 _hasFreeMMXreg()
{
	int i;
	for (i=0; i<MMXREGS; i++) {
		if (!mmxregs[i].inuse) return 1;
	}

	// check for dead regs
	for (i=0; i<MMXREGS; i++) {
		if (mmxregs[i].needed) continue;
		if (mmxregs[i].reg >= MMX_GPR && mmxregs[i].reg < MMX_GPR+34 ) {
			if( !EEINST_ISLIVE64(mmxregs[i].reg-MMX_GPR) ) {
				return 1;
			}
		}
	}

	// check for dead regs
	for (i=0; i<MMXREGS; i++) {
		if (mmxregs[i].needed) continue;
		if (mmxregs[i].reg >= MMX_GPR && mmxregs[i].reg < MMX_GPR+34 ) {
			if( !(g_pCurInstInfo->regs[mmxregs[i].reg-MMX_GPR]&EEINST_USED) ) {
				return 1;
			}
		}
	}

	return 0;
}

void _freeMMXreg(int mmxreg)
{
	assert( mmxreg < MMXREGS );
	if (!mmxregs[mmxreg].inuse) return;
	
	if (mmxregs[mmxreg].mode & MODE_WRITE ) {

		if( mmxregs[mmxreg].reg >= MMX_GPR && mmxregs[mmxreg].reg < MMX_GPR+32 )
			assert( !(g_cpuHasConstReg & (1<<(mmxregs[mmxreg].reg-MMX_GPR))) );

		assert( mmxregs[mmxreg].reg != MMX_GPR );
		
		if( MMX_IS32BITS(mmxregs[mmxreg].reg) )
			MOVDMMXtoM((u32)_MMXGetAddr(mmxregs[mmxreg].reg), mmxreg);
		else
			MOVQRtoM((u32)_MMXGetAddr(mmxregs[mmxreg].reg), mmxreg);

		SetMMXstate();
	}

	mmxregs[mmxreg].mode &= ~MODE_WRITE;
	mmxregs[mmxreg].inuse = 0;
}

void _moveMMXreg(int mmxreg)
{
	int i;
	if( !mmxregs[mmxreg].inuse ) return;

	for (i=0; i<MMXREGS; i++) {
		if (mmxregs[i].inuse) continue;
		break;
	}

	if( i == MMXREGS ) {
		_freeMMXreg(mmxreg);
		return;
	}

	// move
	mmxregs[i] = mmxregs[mmxreg];
	mmxregs[mmxreg].inuse = 0;
	MOVQRtoR(i, mmxreg);
}

// write all active regs
void _flushMMXregs()
{
	int i;

	for (i=0; i<MMXREGS; i++) {
		if (mmxregs[i].inuse == 0) continue;

		if( mmxregs[i].mode & MODE_WRITE ) {

			assert( !(g_cpuHasConstReg & (1<<mmxregs[i].reg)) );

			assert( mmxregs[i].reg != MMX_TEMP );
			assert( mmxregs[i].mode & MODE_READ );
			assert( mmxregs[i].reg != MMX_GPR );

			if( MMX_IS32BITS(mmxregs[i].reg) )
				MOVDMMXtoM((u32)_MMXGetAddr(mmxregs[i].reg), i);
			else
				MOVQRtoM((u32)_MMXGetAddr(mmxregs[i].reg), i);
			SetMMXstate();

			mmxregs[i].mode &= ~MODE_WRITE;
			mmxregs[i].mode |= MODE_READ;
		}
	}
}

void _freeMMXregs()
{
	int i;
	for (i=0; i<MMXREGS; i++) {
		if (mmxregs[i].inuse == 0) continue;

		assert( mmxregs[i].reg != MMX_TEMP );
		assert( mmxregs[i].mode & MODE_READ );

		_freeMMXreg(i);
	}
}

void FreezeMMXRegs_(int save)
{
	assert( g_EEFreezeRegs );

	if( save ) {
		if( g_globalMMXSaved )
			return;
		g_globalMMXSaved = 1;

#ifdef _MSC_VER
		__asm {
			movntq mmword ptr [g_globalMMXData + 0], mm0
			movntq mmword ptr [g_globalMMXData + 8], mm1
			movntq mmword ptr [g_globalMMXData + 16], mm2
			movntq mmword ptr [g_globalMMXData + 24], mm3
			movntq mmword ptr [g_globalMMXData + 32], mm4
			movntq mmword ptr [g_globalMMXData + 40], mm5
			movntq mmword ptr [g_globalMMXData + 48], mm6
			movntq mmword ptr [g_globalMMXData + 56], mm7
			emms
		}
#else
        __asm__(".intel_syntax\n"
                "movq [%0+0x00], %%mm0\n"
                "movq [%0+0x08], %%mm1\n"
                "movq [%0+0x10], %%mm2\n"
                "movq [%0+0x18], %%mm3\n"
                "movq [%0+0x20], %%mm4\n"
                "movq [%0+0x28], %%mm5\n"
                "movq [%0+0x30], %%mm6\n"
                "movq [%0+0x38], %%mm7\n"
                "emms\n"
                ".att_syntax\n" : : "r"(g_globalMMXData) );
#endif

	}
	else {
		if( !g_globalMMXSaved )
			return;
		g_globalMMXSaved = 0;

#ifdef _MSC_VER
		__asm {
			movq mm0, mmword ptr [g_globalMMXData + 0]
			movq mm1, mmword ptr [g_globalMMXData + 8]
			movq mm2, mmword ptr [g_globalMMXData + 16]
			movq mm3, mmword ptr [g_globalMMXData + 24]
			movq mm4, mmword ptr [g_globalMMXData + 32]
			movq mm5, mmword ptr [g_globalMMXData + 40]
			movq mm6, mmword ptr [g_globalMMXData + 48]
			movq mm7, mmword ptr [g_globalMMXData + 56]
			emms
		}
#else
        __asm__(".intel_syntax\n"
                "movq %%mm0, [%0+0x00]\n"
                "movq %%mm1, [%0+0x08]\n"
                "movq %%mm2, [%0+0x10]\n"
                "movq %%mm3, [%0+0x18]\n"
                "movq %%mm4, [%0+0x20]\n"
                "movq %%mm5, [%0+0x28]\n"
                "movq %%mm6, [%0+0x30]\n"
                "movq %%mm7, [%0+0x38]\n"
                "emms\n"
                ".att_syntax\n" : : "r"(g_globalMMXData) );
#endif
	}
}

void SetFPUstate() {
	_freeMMXreg(6);
	_freeMMXreg(7);

	if (x86FpuState==MMX_STATE) {
		if (cpucaps.has3DNOWInstructionExtensions) FEMMS();
		else EMMS();
		x86FpuState=FPU_STATE;
	}
}

void _callPushArg(u32 arg, uptr argmem, x86IntRegType X86ARG)
{
    if( IS_X86REG(arg) ) PUSH32R(arg&0xff);
    else if( IS_CONSTREG(arg) ) PUSH32I(argmem);
    else if( IS_GPRREG(arg) ) {
        SUB32ItoR(ESP, 4);
        _eeMoveGPRtoRm(ESP, arg&0xff);
    }
    else if( IS_XMMREG(arg) ) {
		SUB32ItoR(ESP, 4);
		SSEX_MOVD_XMM_to_Rm(ESP, arg&0xf);
	}
	else if( IS_MMXREG(arg) ) {
		SUB32ItoR(ESP, 4);
		MOVD32MMXtoRm(ESP, arg&0xf);
	}
	else if( IS_EECONSTREG(arg) ) {
		PUSH32I(g_cpuConstRegs[(arg>>16)&0x1f].UL[0]);
	}
	else if( IS_PSXCONSTREG(arg) ) {
		PUSH32I(g_psxConstRegs[(arg>>16)&0x1f]);
	}
    else if( IS_MEMORYREG(arg) ) PUSH32M(argmem);
    else {
        assert( (arg&0xfff0) == 0 );
        // assume it is a GPR reg
        PUSH32R(arg&0xf);
    }
}

void _callFunctionArg1(uptr fn, u32 arg1, uptr arg1mem)
{
    _callPushArg(arg1, arg1mem, -1);
    CALLFunc((uptr)fn);
    ADD32ItoR(ESP, 4);
}

void _callFunctionArg2(uptr fn, u32 arg1, u32 arg2, uptr arg1mem, uptr arg2mem)
{
    _callPushArg(arg2, arg2mem, -1);
    _callPushArg(arg1, arg1mem, -1);
    CALLFunc((uptr)fn);
    ADD32ItoR(ESP, 8);
}

void _callFunctionArg3(uptr fn, u32 arg1, u32 arg2, u32 arg3, uptr arg1mem, uptr arg2mem, uptr arg3mem)
{
    _callPushArg(arg3, arg3mem, -1);
    _callPushArg(arg2, arg2mem, -1);
    _callPushArg(arg1, arg1mem, -1);
    CALLFunc((uptr)fn);
    ADD32ItoR(ESP, 12);
}

// EE
void _eeMoveGPRtoR(x86IntRegType to, int fromgpr)
{
	if( GPR_IS_CONST1(fromgpr) )
		MOV32ItoR( to, g_cpuConstRegs[fromgpr].UL[0] );
	else {
		int mmreg;
		
		if( (mmreg = _checkXMMreg(XMMTYPE_GPRREG, fromgpr, MODE_READ)) >= 0 && (xmmregs[mmreg].mode&MODE_WRITE)) {
			SSE2_MOVD_XMM_to_R(to, mmreg);
		}
		else if( (mmreg = _checkMMXreg(MMX_GPR+fromgpr, MODE_READ)) >= 0 && (mmxregs[mmreg].mode&MODE_WRITE) ) {
			MOVD32MMXtoR(to, mmreg);
			SetMMXstate();
		}
		else {
			MOV32MtoR(to, (int)&cpuRegs.GPR.r[ fromgpr ].UL[ 0 ] );
		}
	}
}

void _eeMoveGPRtoM(u32 to, int fromgpr)
{
	if( GPR_IS_CONST1(fromgpr) )
		MOV32ItoM( to, g_cpuConstRegs[fromgpr].UL[0] );
	else {
		int mmreg;
		
		if( (mmreg = _checkXMMreg(XMMTYPE_GPRREG, fromgpr, MODE_READ)) >= 0 ) {
			SSEX_MOVD_XMM_to_M32(to, mmreg);
		}
		else if( (mmreg = _checkMMXreg(MMX_GPR+fromgpr, MODE_READ)) >= 0 ) {
			MOVDMMXtoM(to, mmreg);
			SetMMXstate();
		}
		else {
			MOV32MtoR(EAX, (int)&cpuRegs.GPR.r[ fromgpr ].UL[ 0 ] );
			MOV32RtoM(to, EAX );
		}
	}
}

void _eeMoveGPRtoRm(x86IntRegType to, int fromgpr)
{
	if( GPR_IS_CONST1(fromgpr) )
		MOV32ItoRmOffset( to, g_cpuConstRegs[fromgpr].UL[0], 0 );
	else {
		int mmreg;
		
		if( (mmreg = _checkXMMreg(XMMTYPE_GPRREG, fromgpr, MODE_READ)) >= 0 ) {
			SSEX_MOVD_XMM_to_Rm(to, mmreg);
		}
		else if( (mmreg = _checkMMXreg(MMX_GPR+fromgpr, MODE_READ)) >= 0 ) {
			MOVD32MMXtoRm(to, mmreg);
			SetMMXstate();
		}
		else {
			MOV32MtoR(EAX, (int)&cpuRegs.GPR.r[ fromgpr ].UL[ 0 ] );
			MOV32RtoRm(to, EAX );
		}
	}
}

void _recPushReg(int mmreg)
{
	if( IS_XMMREG(mmreg) ) {
		SUB32ItoR(ESP, 4);
		SSEX_MOVD_XMM_to_Rm(ESP, mmreg&0xf);
	}
	else if( IS_MMXREG(mmreg) ) {
		SUB32ItoR(ESP, 4);
		MOVD32MMXtoRm(ESP, mmreg&0xf);
	}
	else if( IS_EECONSTREG(mmreg) ) {
		PUSH32I(g_cpuConstRegs[(mmreg>>16)&0x1f].UL[0]);
	}
	else if( IS_PSXCONSTREG(mmreg) ) {
		PUSH32I(g_psxConstRegs[(mmreg>>16)&0x1f]);
	}
	else {
        assert( (mmreg&0xfff0) == 0 );
        PUSH32R(mmreg);
    }
}

void _signExtendSFtoM(u32 mem)
{
	LAHF();
	SAR16ItoR(EAX, 15);
	CWDE();
	MOV32RtoM(mem, EAX );
}

int _signExtendMtoMMX(x86MMXRegType to, u32 mem)
{
	int t0reg = _allocMMXreg(-1, MMX_TEMP, 0);
	MOVDMtoMMX(t0reg, mem);
	MOVQRtoR(to, t0reg);
	PSRADItoR(t0reg, 31);
	PUNPCKLDQRtoR(to, t0reg);
	_freeMMXreg(t0reg);
	return to;
}

int _signExtendGPRMMXtoMMX(x86MMXRegType to, u32 gprreg, x86MMXRegType from, u32 gprfromreg)
{
	assert( to >= 0 && from >= 0 );
	if( !EEINST_ISLIVE1(gprreg) ) {
		EEINST_RESETHASLIVE1(gprreg);
		if( to != from ) MOVQRtoR(to, from);
		return to;
	}

	if( to == from ) return _signExtendGPRtoMMX(to, gprreg, 0);
	if( !(g_pCurInstInfo->regs[gprfromreg]&EEINST_LASTUSE) ) {
		if( EEINST_ISLIVE64(gprfromreg) ) {
			MOVQRtoR(to, from);
			return _signExtendGPRtoMMX(to, gprreg, 0);
		}
	}

	// from is free for use
	SetMMXstate();

	if( g_pCurInstInfo->regs[gprreg] & EEINST_MMX ) {
		
		if( EEINST_ISLIVE64(gprfromreg) ) {
			_freeMMXreg(from);
		}

		MOVQRtoR(to, from);
		PSRADItoR(from, 31);
		PUNPCKLDQRtoR(to, from);
		return to;
	}
	else {
		MOVQRtoR(to, from);
		MOVDMMXtoM((u32)&cpuRegs.GPR.r[gprreg].UL[0], from);
		PSRADItoR(from, 31);
		MOVDMMXtoM((u32)&cpuRegs.GPR.r[gprreg].UL[1], from);
		mmxregs[to].inuse = 0;
		return -1;
	}

	assert(0);
}

int _signExtendGPRtoMMX(x86MMXRegType to, u32 gprreg, int shift)
{
	assert( to >= 0 && shift >= 0 );
	if( !EEINST_ISLIVE1(gprreg) ) {
		if( shift > 0 ) PSRADItoR(to, shift);
		EEINST_RESETHASLIVE1(gprreg);
		return to;
	}

	SetMMXstate();

	if( g_pCurInstInfo->regs[gprreg] & EEINST_MMX ) {
		if( _hasFreeMMXreg() ) {
			int t0reg = _allocMMXreg(-1, MMX_TEMP, 0);
			MOVQRtoR(t0reg, to);
			PSRADItoR(to, 31);
			if( shift > 0 ) PSRADItoR(t0reg, shift);
			PUNPCKLDQRtoR(t0reg, to);

			// swap mmx regs.. don't ask
			mmxregs[t0reg] = mmxregs[to];
			mmxregs[to].inuse = 0;
			return t0reg;
		}
		else {
			// will be used in the future as mmx
			if( shift > 0 ) PSRADItoR(to, shift);
			MOVDMMXtoM((u32)&cpuRegs.GPR.r[gprreg].UL[0], to);
			PSRADItoR(to, 31);
			MOVDMMXtoM((u32)&cpuRegs.GPR.r[gprreg].UL[1], to);

			// read again
			MOVQMtoR(to, (u32)&cpuRegs.GPR.r[gprreg].UL[0]);
			mmxregs[to].mode &= ~MODE_WRITE;
			return to;
		}
	}
	else {
		if( shift > 0 ) PSRADItoR(to, shift);
		MOVDMMXtoM((u32)&cpuRegs.GPR.r[gprreg].UL[0], to);
		PSRADItoR(to, 31);
		MOVDMMXtoM((u32)&cpuRegs.GPR.r[gprreg].UL[1], to);
		mmxregs[to].inuse = 0;
		return -1;
	}

	assert(0);
}

int _allocCheckGPRtoMMX(EEINST* pinst, int reg, int mode)
{
	if( pinst->regs[reg] & EEINST_MMX ) return _allocMMXreg(-1, MMX_GPR+reg, mode);
		
	return _checkMMXreg(MMX_GPR+reg, mode);
}

void _recMove128MtoM(u32 to, u32 from)
{
	MOV32MtoR(EAX, from);
	MOV32MtoR(EDX, from+4);
	MOV32RtoM(to, EAX);
	MOV32RtoM(to+4, EDX);
	MOV32MtoR(EAX, from+8);
	MOV32MtoR(EDX, from+12);
	MOV32RtoM(to+8, EAX);
	MOV32RtoM(to+12, EDX);
}

void _recMove128RmOffsettoM(u32 to, u32 offset)
{
	MOV32RmtoROffset(EAX, ECX, offset);
	MOV32RmtoROffset(EDX, ECX, offset+4);
	MOV32RtoM(to, EAX);
	MOV32RtoM(to+4, EDX);
	MOV32RmtoROffset(EAX, ECX, offset+8);
	MOV32RmtoROffset(EDX, ECX, offset+12);
	MOV32RtoM(to+8, EAX);
	MOV32RtoM(to+12, EDX);
}

void _recMove128MtoRmOffset(u32 offset, u32 from)
{
	MOV32MtoR(EAX, from);
	MOV32MtoR(EDX, from+4);
	MOV32RtoRmOffset(ECX, EAX, offset);
	MOV32RtoRmOffset(ECX, EDX, offset+4);
	MOV32MtoR(EAX, from+8);
	MOV32MtoR(EDX, from+12);
	MOV32RtoRmOffset(ECX, EAX, offset+8);
	MOV32RtoRmOffset(ECX, EDX, offset+12);
}

static PCSX2_ALIGNED16(u32 s_ones[2]) = {0xffffffff, 0xffffffff};

void LogicalOpRtoR(x86MMXRegType to, x86MMXRegType from, int op)
{
	switch(op) {
		case 0: PANDRtoR(to, from); break;
		case 1: PORRtoR(to, from); break;
		case 2: PXORRtoR(to, from); break;
		case 3:
			PORRtoR(to, from);
			PXORMtoR(to, (u32)&s_ones[0]);
			break;
	}
}

void LogicalOpMtoR(x86MMXRegType to, u32 from, int op)
{
	switch(op) {
		case 0: PANDMtoR(to, from); break;
		case 1: PORMtoR(to, from); break;
		case 2: PXORMtoR(to, from); break;
		case 3:
			PORRtoR(to, from);
			PXORMtoR(to, (u32)&s_ones[0]);
			break;
	}
}

void LogicalOp32RtoM(u32 to, x86IntRegType from, int op)
{
	switch(op) {
		case 0: AND32RtoM(to, from); break;
		case 1: OR32RtoM(to, from); break;
		case 2: XOR32RtoM(to, from); break;
		case 3: OR32RtoM(to, from); break;
	}
}

void LogicalOp32MtoR(x86IntRegType to, u32 from, int op)
{
	switch(op) {
		case 0: AND32MtoR(to, from); break;
		case 1: OR32MtoR(to, from); break;
		case 2: XOR32MtoR(to, from); break;
		case 3: OR32MtoR(to, from); break;
	}
}

void LogicalOp32ItoR(x86IntRegType to, u32 from, int op)
{
	switch(op) {
		case 0: AND32ItoR(to, from); break;
		case 1: OR32ItoR(to, from); break;
		case 2: XOR32ItoR(to, from); break;
		case 3: OR32ItoR(to, from); break;
	}
}

void LogicalOp32ItoM(u32 to, u32 from, int op)
{
	switch(op) {
		case 0: AND32ItoM(to, from); break;
		case 1: OR32ItoM(to, from); break;
		case 2: XOR32ItoM(to, from); break;
		case 3: OR32ItoM(to, from); break;
	}
}
