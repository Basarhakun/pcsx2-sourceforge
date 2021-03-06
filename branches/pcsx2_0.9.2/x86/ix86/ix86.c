/*
 * ix86 core v0.6.2
 *  Authors: linuzappz <linuzappz@pcsx.net>
 *           alexey silinov
 *           goldfinger
 *			 zerofrog(@gmail.com)
 */

#include <stdio.h>
#include <string.h>
#include <assert.h>
#include "ix86.h"

#define SWAP(x, y) *(u32*)&y ^= *(u32*)&x ^= *(u32*)&y ^= *(u32*)&x;

s8  *x86Ptr;
u8  *j8Ptr[32];
u32 *j32Ptr[32];

extern void SysPrintf(char *fmt, ...);

// This function is just for rec debugging purposes
void CheckX86Ptr( void )
{
}

void write64( u64 val )
{ 
#ifdef _DEBUG
   CheckX86Ptr( );
#endif

   *(u64*)x86Ptr = val; 
   x86Ptr += 8; 
}

void Rex( int w, int r, int x, int b )
{
	assert(0);
	write8( 0x40 | (w << 3) | (r << 2) | (x << 1) | (b) );
}

void ModRM( int mod, int rm, int reg )
{
	write8( ( mod << 6 ) | ( (rm & 7) << 3 ) | ( reg & 7 ) );
}

void SibSB( int ss, int rm, int index )
{
	write8( ( ss << 6 ) | ( rm << 3 ) | ( index ) );
}

void SET8R( int cc, int to )
{
	write8( 0x0F );
   write8( cc );
	write8( 0xC0 | ( to ) );
}

u8* J8Rel( int cc, int to )
{
	write8( cc );
   write8( to );
   return x86Ptr - 1;
}

u16* J16Rel( int cc, u32 to )
{
	write16( 0x0F66 );
	write8( cc );
	write16( to );
	return (u16*)( x86Ptr - 2 );
}

u32* J32Rel( int cc, u32 to )
{
	write8( 0x0F );
   write8( cc );
   write32( to );
   return (u32*)( x86Ptr - 4 );
}

void CMOV32RtoR( int cc, int to, int from )
{
	write8( 0x0F );
   write8( cc );
	ModRM( 3, to, from );
}

void CMOV32MtoR( int cc, int to, u32 from )
{
	write8( 0x0F );
	write8( cc );
	ModRM( 0, to, DISP32 );
	write32( MEMADDR(from, 4) );
}

////////////////////////////////////////////////////
void x86SetPtr( char* ptr ) 
{
	x86Ptr = ptr;
}

////////////////////////////////////////////////////
void x86Shutdown( void )
{
}

////////////////////////////////////////////////////
void x86SetJ8( u8* j8 )
{
	u32 jump = ( x86Ptr - (s8*)j8 ) - 1;

	if ( jump > 0x7f ) {
		assert(0);
		SysPrintf( "j8 greater than 0x7f!!\n" );
	}
	*j8 = (u8)jump;
}

void x86SetJ8A( u8* j8 )
{
	u32 jump = ( x86Ptr - (s8*)j8 ) - 1;

	if ( jump > 0x7f ) {
		assert(0);
		SysPrintf( "j8 greater than 0x7f!!\n" );
	}

	if( ((u32)x86Ptr&0xf) > 4 ) {

		u32 newjump = jump + 16-((u32)x86Ptr&0xf);

		if( newjump <= 0x7f ) {
			jump = newjump;
			while((u32)x86Ptr&0xf) *x86Ptr++ = 0x90;
		}
	}
	*j8 = (u8)jump;
}

void x86SetJ16( u16 *j16 )
{
	// doesn't work
	u32 jump = ( x86Ptr - (s8*)j16 ) - 2;

	if ( jump > 0x7fff ) {
		assert(0);
		SysPrintf( "j16 greater than 0x7fff!!\n" );
	}
	*j16 = (u16)jump;
}

void x86SetJ16A( u16 *j16 )
{
	if( ((u32)x86Ptr&0xf) > 4 ) {
		while((u32)x86Ptr&0xf) *x86Ptr++ = 0x90;
	}
	x86SetJ16(j16);
}

////////////////////////////////////////////////////
void x86SetJ32( u32* j32 ) 
{
	*j32 = ( x86Ptr - (s8*)j32 ) - 4;
}

void x86SetJ32A( u32* j32 )
{
	while((u32)x86Ptr&0xf) *x86Ptr++ = 0x90;
	x86SetJ32(j32);
}

////////////////////////////////////////////////////
void x86Align( int bytes ) 
{
	// fordward align
	x86Ptr = (s8*)( ( (uptr)x86Ptr + bytes - 1) & ~( bytes - 1 ) );
}

/********************/
/* IX86 intructions */
/********************/

void STC( void )
{
   write8( 0xF9 );
}

void CLC( void )
{
   write8( 0xF8 );
}

////////////////////////////////////
// mov instructions                /
////////////////////////////////////

/* mov r64 to r64 */
void MOV64RtoR( x86IntRegType to, x86IntRegType from ) 
{
	Rex(1, from >> 3, 0, to >> 3);
	write8( 0x89 );
	ModRM( 3, from, to );
}

/* mov r64 to m64 */
void MOV64RtoM( u64 to, x86IntRegType from ) 
{
	Rex(1, from >> 3, 0, 0);
	write8( 0x89 );
	ModRM( 0, from, DISP32 );
	write32( (u32)MEMADDR(to, 4) );
}

/* mov m64 to r64 */
void MOV64MtoR( x86IntRegType to, u64 from ) 
{
	Rex(1, to >> 3, 0, 0);
	write8( 0x8B );
	ModRM( 0, to, DISP32 );
	write32( (u32)MEMADDR(from, 4) ); 
}

/* mov imm32 to m64 */
void MOV64ItoM(u32 to, u32 from ) 
{
	Rex(1, 0, 0, 0);
	write8( 0xC7 );
	ModRM( 0, 0, DISP32 );
	write32( MEMADDR(to, 8) );
	write32( from ); 
}

/* mov imm32 to r64 */
void MOV32Ito64R( x86IntRegType to, s32 from ) 
{
	Rex(1, 0, 0, to >> 3);
	write8( 0xC7 ); 
	ModRM( 0, 0, to );
	write32( from );
}

/* mov imm64 to r64 */
void MOV64ItoR( x86IntRegType to, u64 from ) {
	Rex(1, 0, 0, to >> 3);
	write8( 0xB8 | (to & 0x7) ); 
	write64( from );
}

/* mov [r64] to r64 */
void MOV64RmtoR( x86IntRegType to, x86IntRegType from ) {
	Rex(1, from >> 3, 0, to >> 3);
	write8( 0x8B );
	ModRM( 0, to, from );
}

/* mov [r64][r64*scale] to r64 */
void MOV64RmStoR( x86IntRegType to, x86IntRegType from, x86IntRegType from2, int scale) {
	Rex(1, from >> 3, 0, to >> 3);
	write8( 0x8B );
	ModRM( 0, to, 0x4 );
	SibSB(scale, from2, from );
}

/* mov r64 to [r64] */
void MOV64RtoRm( x86IntRegType to, x86IntRegType from ) {
	Rex(1, from >> 3, 0, to >> 3);
	write8( 0x89 );
	ModRM( 0, from, to );
}

/* mov r64 to [r64][r64*scale] */
void MOV64RtoRmS( x86IntRegType to, x86IntRegType from, x86IntRegType from2, int scale) {
	Rex(1, from >> 3, 0, to >> 3);
	write8( 0x89 );
	ModRM( 0, to, 0x4 );
	SibSB(scale, from2, from );
}


/* mov r32 to r32 */
void MOV32RtoR( x86IntRegType to, x86IntRegType from ) 
{
	if (to > 7 || from > 7) {
		Rex(0, from >> 3, 0, to >> 3);
	}
	write8( 0x89 );
	ModRM( 3, from, to );
}

/* mov r32 to m32 */
void MOV32RtoM( u32 to, x86IntRegType from ) 
{
	if (from > 7) {
		Rex(0, from >> 3, 0, 0);
	}
	write8( 0x89 );
	ModRM( 0, from, DISP32 );
	write32( MEMADDR(to, 4) );
}

/* mov m32 to r32 */
void MOV32MtoR( x86IntRegType to, u32 from ) 
{
	if (to > 7) {
		Rex(0, to >> 3, 0, 0);
	}
	write8( 0x8B );
	ModRM( 0, to, DISP32 );
	write32( MEMADDR(from, 4) ); 
}

/* mov [r32] to r32 */
void MOV32RmtoR( x86IntRegType to, x86IntRegType from ) {
	if (from > 7 || to > 7) {
		Rex(0, to >> 3, 0, from >> 3);
	}
	if (from == ESP) {
		write8( 0x8B );
		ModRM( 0, to, 0x4 );
		SibSB( 0, 0x4, 0x4 );
	} else {
		write8( 0x8B );
		ModRM( 0, to, from );
	}
}

void MOV32RmtoROffset( x86IntRegType to, x86IntRegType from, u32 offset ) {
	if (from > 7 || to > 7) {
		Rex(0, to >> 3, 0, from >> 3);
	}

	write8( 0x8B );

	if( offset == 0 ) {
		if (from == ESP) {
			ModRM( 0, to, 0x4 );
			SibSB( 0, 0x4, 0x4 );
		} else {
			ModRM( 0, to, from );
		}
	}
	else if( offset < 128 ) {
		if (from == ESP) {
			ModRM( 1, to, 0x4 );
			SibSB( 0, 0x4, 0x4 );
		} else {
			ModRM( 1, to, from );
		}
		write8(offset);
	}
	else {
		if (from == ESP) {
			ModRM( 2, to, 0x4 );
			SibSB( 0, 0x4, 0x4 );
		} else {
			ModRM( 2, to, from );
		}
		write32(offset);
	}
}

/* mov [r32][r32*scale] to r32 */
void MOV32RmStoR( x86IntRegType to, x86IntRegType from, x86IntRegType from2, int scale) {
	write8( 0x8B );
	ModRM( 0, to, 0x4 );
	SibSB(scale, from2, from );
}

void MOV32RmSOffsettoR( x86IntRegType to, x86IntRegType from1, u32 from2, int scale )
{
	write8( 0x8B );
	ModRM( 0, to, 0x4 );
	ModRM( scale, from1, 5);
	write32(from2);
}

/* mov r32 to [r32] */
void MOV32RtoRm( x86IntRegType to, x86IntRegType from ) {
	if (from > 7 || to > 7) {
		Rex(0, from >> 3, 0, to >> 3);
	}
	if (to == ESP) {
		write8( 0x89 );
		ModRM( 0, from, 0x4 );
		SibSB( 0, 0x4, 0x4 );
	} else {
		write8( 0x89 );
		ModRM( 0, from, to );
	}
}

/* mov r32 to [r32][r32*scale] */
void MOV32RtoRmS( x86IntRegType to, x86IntRegType from, x86IntRegType from2, int scale) {
	write8( 0x89 );
	ModRM( 0, to, 0x4 );
	SibSB(scale, from2, from );
}

/* mov imm32 to r32 */
void MOV32ItoR( x86IntRegType to, u32 from ) 
{
	if (to > 7) {
		Rex(0, 0, 0, to >> 3);
	}
	write8( 0xB8 | (to & 0x7) ); 
	write32( from );
}

/* mov imm32 to m32 */
void MOV32ItoM(u32 to, u32 from ) 
{
	write8( 0xC7 );
	ModRM( 0, 0, DISP32 );
	write32( MEMADDR(to, 8) );
	write32( from ); 
}

// mov imm32 to [r32+off]
void MOV32ItoRmOffset( x86IntRegType to, u32 from, u32 offset)
{
	write8( 0xC7 );

	if( to == ESP ) {

		if( offset == 0 ) {
			ModRM( 0, 0, 4 );
			ModRM( 0, ESP, 4 );
		}
		else if( offset < 128 ) {
			ModRM( 1, 0, 4 );
			ModRM( 0, ESP, 4 );
			write8(offset);
		}
		else {
			ModRM( 2, 0, 4 );
			ModRM( 0, ESP, 4 );
			write32(offset);
		}
	}
	else {
		if( offset == 0 ) {
			ModRM( 0, 0, to );
		}
		else if( offset < 128 ) {
			ModRM( 1, 0, to );
			write8(offset);
		}
		else {
			ModRM( 2, 0, to );
			write32(offset);
		}
	}

	write32(from);
}

// mov r32 to [r32+off]
void MOV32RtoRmOffset( x86IntRegType to, x86IntRegType from, u32 offset)
{
	write8( 0x89 );

	if (to == ESP) {
		if( offset == 0 ) {
			ModRM( 0, from, 0x4 );
			SibSB( 0, 0x4, 0x4 );
		}
		else if( offset < 128 ) {
			ModRM( 1, from, 0x4 );
			SibSB( 0, 0x4, 0x4 );
			write8(offset);
		}
		else {
			ModRM( 2, from, 0x4 );
			SibSB( 0, 0x4, 0x4 );
			SibSB( 0, 0x4, 0x4 );
		}
	}
	else {
		if( offset == 0 ) {
			ModRM( 0, from, to );
		}
		else if( offset < 128 ) {
			ModRM( 1, from, to );
			write8(offset);
		}
		else {
			ModRM( 2, from, to );
			write32(offset);
		}
	}
}

/* mov r16 to m16 */
void MOV16RtoM(u32 to, x86IntRegType from ) 
{
	write8( 0x66 );
	write8( 0x89 );
	ModRM( 0, from, DISP32 );
	write32( MEMADDR(to, 4) );
}

/* mov m16 to r16 */
void MOV16MtoR( x86IntRegType to, u32 from ) 
{
	write8( 0x66 );
	write8( 0x8B );
	ModRM( 0, to, DISP32 );
	write32( MEMADDR(from, 4) ); 
}

void MOV16RmtoR( x86IntRegType to, x86IntRegType from) 
{
	write8( 0x66 );
	write8( 0x8B );
	ModRM( 0, to, from );
}

void MOV16RmtoROffset( x86IntRegType to, x86IntRegType from, u32 offset )
{
	write8( 0x66 );
	write8( 0x8B );
	ModRM( 2, to, from );
	write32( offset );
}

void MOV16RmSOffsettoR( x86IntRegType to, x86IntRegType from1, u32 from2, int scale )
{
	write16( 0x8B66 );
	ModRM( 0, to, 0x4 );
	ModRM( scale, from1, 5);
	write32(from2);
}

void MOV16RtoRm(x86IntRegType to, x86IntRegType from)
{
	write8( 0x66 );
	write8( 0x89 );
	ModRM( 0, from, to );
}

/* mov imm16 to m16 */
void MOV16ItoM( u32 to, u16 from ) 
{
	write8( 0x66 );
	write8( 0xC7 );
	ModRM( 0, 0, DISP32 );
	write32( MEMADDR(to, 6) );
	write16( from ); 
}

/* mov r16 to [r32][r32*scale] */
void MOV16RtoRmS( x86IntRegType to, x86IntRegType from, x86IntRegType from2, int scale) {
	write8( 0x66 );
	write8( 0x89 );
	ModRM( 0, to, 0x4 );
	SibSB(scale, from2, from );
}

void MOV16ItoR( x86IntRegType to, u16 from )
{
	if (to > 7) {
		Rex(0, 0, 0, to >> 3);
	}
	write16( 0xB866 | ((to & 0x7)<<8) ); 
	write16( from );
}

// mov imm16 to [r16+off]
void MOV16ItoRmOffset( x86IntRegType to, u16 from, u32 offset)
{
	assert( to != ESP );
	write16( 0xC766 );
	ModRM( 2, 0, to );
	write32(offset);
	write16(from);
}

// mov r16 to [r16+off]
void MOV16RtoRmOffset( x86IntRegType to, x86IntRegType from, u32 offset)
{
	assert( to != ESP );
	write16( 0x8966 );

	if( offset < 128 ) {
		ModRM( 1, from, to );
		write8(offset);
	}
	else {
		ModRM( 2, from, to );
		write32(offset);
	}
}

/* mov r8 to m8 */
void MOV8RtoM( u32 to, x86IntRegType from ) 
{
	write8( 0x88 );
	ModRM( 0, from, DISP32 );
	write32( MEMADDR(to, 4) );
}

/* mov m8 to r8 */
void MOV8MtoR( x86IntRegType to, u32 from ) 
{
	write8( 0x8A );
	ModRM( 0, to, DISP32 );
	write32( MEMADDR(from, 4) ); 
}

/* mov [r32] to r8 */
void MOV8RmtoR(x86IntRegType to, x86IntRegType from)
{
	write8( 0x8A );
	ModRM( 0, to, from );
}

void MOV8RmtoROffset(x86IntRegType to, x86IntRegType from, u32 offset)
{
	write8( 0x8A );
	ModRM( 2, to, from );
	write32(offset);
}

void MOV8RtoRm(x86IntRegType to, x86IntRegType from)
{
	write8( 0x88 );
	ModRM( 0, from, to );
}

/* mov imm8 to m8 */
void MOV8ItoM( u32 to, u8 from ) 
{
	write8( 0xC6 );
	ModRM( 0, 0, DISP32 );
	write32( MEMADDR(to, 5) );
	write8( from ); 
}

// mov imm8 to r8
void MOV8ItoR( x86IntRegType to, u8 from )
{
	if (to > 7) {
		Rex(0, 0, 0, to >> 3);
	}
	write8( 0xB0 | (to & 0x7) ); 
	write8( from );
}

// mov imm8 to [r8+off]
void MOV8ItoRmOffset( x86IntRegType to, u8 from, u32 offset)
{
	assert( to != ESP );
	write8( 0xC6 );
	ModRM( 2, 0, to );
	write32(offset);
	write8(from);
}

// mov r8 to [r8+off]
void MOV8RtoRmOffset( x86IntRegType to, x86IntRegType from, u32 offset)
{
	assert( to != ESP );
	write8( 0x88 );

	if( offset < 128 ) {
		ModRM( 1, from, to );
		write8(offset);
	}
	else {
		ModRM( 2, from, to );
		write32(offset);
	}
}

/* movsx r8 to r32 */
void MOVSX32R8toR( x86IntRegType to, x86IntRegType from ) 
{
	write16( 0xBE0F ); 
	ModRM( 3, to, from ); 
}

void MOVSX32Rm8toR( x86IntRegType to, x86IntRegType from )
{
	write16( 0xBE0F ); 
	ModRM( 0, to, from ); 
}

void MOVSX32Rm8toROffset( x86IntRegType to, x86IntRegType from, u32 offset )
{
	write16( 0xBE0F ); 
	ModRM( 2, to, from ); 
	write32(offset);
}

/* movsx m8 to r32 */
void MOVSX32M8toR( x86IntRegType to, u32 from ) 
{
	write16( 0xBE0F ); 
	ModRM( 0, to, DISP32 );
	write32( MEMADDR(from, 4) );
}

/* movsx r16 to r32 */
void MOVSX32R16toR( x86IntRegType to, x86IntRegType from ) 
{
	write16( 0xBF0F ); 
	ModRM( 3, to, from ); 
}

void MOVSX32Rm16toR( x86IntRegType to, x86IntRegType from )
{
	write16( 0xBF0F ); 
	ModRM( 0, to, from ); 
}

void MOVSX32Rm16toROffset( x86IntRegType to, x86IntRegType from, u32 offset )
{
	write16( 0xBF0F ); 
	ModRM( 2, to, from ); 
	write32(offset);
}

/* movsx m16 to r32 */
void MOVSX32M16toR( x86IntRegType to, u32 from ) 
{
	write16( 0xBF0F ); 
	ModRM( 0, to, DISP32 );
	write32( MEMADDR(from, 4) );
}

/* movzx r8 to r32 */
void MOVZX32R8toR( x86IntRegType to, x86IntRegType from ) 
{
	write16( 0xB60F ); 
	ModRM( 3, to, from ); 
}

void MOVZX32Rm8toR( x86IntRegType to, x86IntRegType from )
{
	write16( 0xB60F ); 
	ModRM( 0, to, from );
}

void MOVZX32Rm8toROffset( x86IntRegType to, x86IntRegType from, u32 offset )
{
	write16( 0xB60F ); 
	ModRM( 2, to, from );
	write32(offset);
}

/* movzx m8 to r32 */
void MOVZX32M8toR( x86IntRegType to, u32 from ) 
{
	write16( 0xB60F ); 
	ModRM( 0, to, DISP32 );
	write32( MEMADDR(from, 4) );
}

/* movzx r16 to r32 */
void MOVZX32R16toR( x86IntRegType to, x86IntRegType from ) 
{
	write16( 0xB70F ); 
	ModRM( 3, to, from ); 
}

void MOVZX32Rm16toR( x86IntRegType to, x86IntRegType from )
{
	write16( 0xB70F ); 
	ModRM( 0, to, from ); 
}

void MOVZX32Rm16toROffset( x86IntRegType to, x86IntRegType from, u32 offset )
{
	write16( 0xB70F ); 
	ModRM( 2, to, from ); 
	write32(offset);
}

/* movzx m16 to r32 */
void MOVZX32M16toR( x86IntRegType to, u32 from ) 
{
	write16( 0xB70F ); 
	ModRM( 0, to, DISP32 );
	write32( MEMADDR(from, 4) );
}

// mov r32+off to r32
void MOV32RmOffsettoR( x86IntRegType to, x86IntRegType from, u32 offset)
{
	if( offset == 0 ) {
		MOV32RtoR(to, from);
		return;
	}

	write8(0x8d);

	if( offset < 128 ) {
		ModRM( 1, to, from );
		write8( offset );
	}
	else {
		ModRM( 2, to, from );
		write32( offset );
	}
}

// mov r32+r32 to r32
void MOV32Rm2toR( x86IntRegType to, x86IntRegType from1, x86IntRegType from2)
{
	write8(0x8d);
	ModRM( 0, to, 4 );
	ModRM( 0, from1, from2 );
}

/* cmovbe r32 to r32 */
void CMOVBE32RtoR( x86IntRegType to, x86IntRegType from )
{
	CMOV32RtoR( 0x46, to, from );
}

/* cmovbe m32 to r32*/
void CMOVBE32MtoR( x86IntRegType to, u32 from )
{
	CMOV32MtoR( 0x46, to, from );
}

/* cmovb r32 to r32 */
void CMOVB32RtoR( x86IntRegType to, x86IntRegType from )
{
	CMOV32RtoR( 0x42, to, from );
}

/* cmovb m32 to r32*/
void CMOVB32MtoR( x86IntRegType to, u32 from )
{
	CMOV32MtoR( 0x42, to, from );
}

/* cmovae r32 to r32 */
void CMOVAE32RtoR( x86IntRegType to, x86IntRegType from )
{
	CMOV32RtoR( 0x43, to, from );
}

/* cmovae m32 to r32*/
void CMOVAE32MtoR( x86IntRegType to, u32 from )
{
	CMOV32MtoR( 0x43, to, from );
}

/* cmova r32 to r32 */
void CMOVA32RtoR( x86IntRegType to, x86IntRegType from )
{
	CMOV32RtoR( 0x47, to, from );
}

/* cmova m32 to r32*/
void CMOVA32MtoR( x86IntRegType to, u32 from )
{
	CMOV32MtoR( 0x47, to, from );
}

/* cmovo r32 to r32 */
void CMOVO32RtoR( x86IntRegType to, x86IntRegType from )
{
	CMOV32RtoR( 0x40, to, from );
}

/* cmovo m32 to r32 */
void CMOVO32MtoR( x86IntRegType to, u32 from )
{
	CMOV32MtoR( 0x40, to, from );
}

/* cmovp r32 to r32 */
void CMOVP32RtoR( x86IntRegType to, x86IntRegType from )
{
	CMOV32RtoR( 0x4A, to, from );
}

/* cmovp m32 to r32 */
void CMOVP32MtoR( x86IntRegType to, u32 from )
{
	CMOV32MtoR( 0x4A, to, from );
}

/* cmovs r32 to r32 */
void CMOVS32RtoR( x86IntRegType to, x86IntRegType from )
{
	CMOV32RtoR( 0x48, to, from );
}

/* cmovs m32 to r32 */
void CMOVS32MtoR( x86IntRegType to, u32 from )
{
	CMOV32MtoR( 0x48, to, from );
}

/* cmovno r32 to r32 */
void CMOVNO32RtoR( x86IntRegType to, x86IntRegType from )
{
	CMOV32RtoR( 0x41, to, from );
}

/* cmovno m32 to r32 */
void CMOVNO32MtoR( x86IntRegType to, u32 from )
{
	CMOV32MtoR( 0x41, to, from );
}

/* cmovnp r32 to r32 */
void CMOVNP32RtoR( x86IntRegType to, x86IntRegType from )
{
	CMOV32RtoR( 0x4B, to, from );
}

/* cmovnp m32 to r32 */
void CMOVNP32MtoR( x86IntRegType to, u32 from )
{
	CMOV32MtoR( 0x4B, to, from );
}

/* cmovns r32 to r32 */
void CMOVNS32RtoR( x86IntRegType to, x86IntRegType from )
{
	CMOV32RtoR( 0x49, to, from );
}

/* cmovns m32 to r32 */
void CMOVNS32MtoR( x86IntRegType to, u32 from )
{
	CMOV32MtoR( 0x49, to, from );
}

/* cmovne r32 to r32 */
void CMOVNE32RtoR( x86IntRegType to, x86IntRegType from )
{
	CMOV32RtoR( 0x45, to, from );
}

/* cmovne m32 to r32*/
void CMOVNE32MtoR( x86IntRegType to, u32 from ) 
{
	CMOV32MtoR( 0x45, to, from );
}

/* cmove r32 to r32*/
void CMOVE32RtoR( x86IntRegType to, x86IntRegType from ) 
{
	CMOV32RtoR( 0x44, to, from );
}

/* cmove m32 to r32*/
void CMOVE32MtoR( x86IntRegType to, u32 from ) 
{
	CMOV32MtoR( 0x44, to, from );
}

/* cmovg r32 to r32*/
void CMOVG32RtoR( x86IntRegType to, x86IntRegType from ) 
{
	CMOV32RtoR( 0x4F, to, from );
}

/* cmovg m32 to r32*/
void CMOVG32MtoR( x86IntRegType to, u32 from ) 
{
	CMOV32MtoR( 0x4F, to, from );
}

/* cmovge r32 to r32*/
void CMOVGE32RtoR( x86IntRegType to, x86IntRegType from ) 
{
	CMOV32RtoR( 0x4D, to, from );
}

/* cmovge m32 to r32*/
void CMOVGE32MtoR( x86IntRegType to, u32 from ) 
{
	CMOV32MtoR( 0x4D, to, from );
}

/* cmovl r32 to r32*/
void CMOVL32RtoR( x86IntRegType to, x86IntRegType from ) 
{
	CMOV32RtoR( 0x4C, to, from );
}

/* cmovl m32 to r32*/
void CMOVL32MtoR( x86IntRegType to, u32 from ) 
{
	CMOV32MtoR( 0x4C, to, from );
}

/* cmovle r32 to r32*/
void CMOVLE32RtoR( x86IntRegType to, x86IntRegType from ) 
{
	CMOV32RtoR( 0x4E, to, from );
}

/* cmovle m32 to r32*/
void CMOVLE32MtoR( x86IntRegType to, u32 from ) 
{
	CMOV32MtoR( 0x4E, to, from );
}

////////////////////////////////////
// arithmetic instructions         /
////////////////////////////////////

/* add imm32 to r64 */
void ADD64ItoR( x86IntRegType to, u32 from ) 
{
	Rex(1, 0, 0, to >> 3);
	if ( to == EAX) {
		write8( 0x05 ); 
	} else {
		write8( 0x81 ); 
		ModRM( 3, 0, to );
	}
	write32( from );
}

/* add m64 to r64 */
void ADD64MtoR( x86IntRegType to, u32 from ) 
{
	Rex(1, to >> 3, 0, 0);
	write8( 0x03 ); 
	ModRM( 0, to, DISP32 );
	write32( MEMADDR(from, 4) );
}

/* add r64 to r64 */
void ADD64RtoR( x86IntRegType to, x86IntRegType from ) 
{
	Rex(1, from >> 3, 0, to >> 3);
	write8( 0x01 ); 
	ModRM( 3, from, to );
}

/* add imm32 to r32 */
void ADD32ItoR( x86IntRegType to, u32 from ) 
{
	if (to > 7) {
		Rex(0, to >> 3, 0, 0);
	}
	if ( to == EAX) 
   {
		write8( 0x05 ); 
	}
   else 
   {
		write8( 0x81 ); 
		ModRM( 3, 0, to );
	}
	write32( from );
}

/* add imm32 to m32 */
void ADD32ItoM( u32 to, u32 from ) 
{
	write8( 0x81 ); 
	ModRM( 0, 0, DISP32 );
	write32( MEMADDR(to, 8) );
	write32( from );
}

// add imm32 to [r32+off]
void ADD32ItoRmOffset( x86IntRegType to, u32 from, u32 offset)
{
    write8( 0x81 );

	if( to == ESP ) {

		if( offset == 0 ) {
			ModRM( 0, 0, 4 );
			ModRM( 0, ESP, 4 );
		}
		else if( offset < 128 ) {
			ModRM( 1, 0, 4 );
			ModRM( 0, ESP, 4 );
			write8(offset);
		}
		else {
			ModRM( 2, 0, 4 );
			ModRM( 0, ESP, 4 );
			write32(offset);
		}
	}
	else {
		if( offset == 0 ) {
			ModRM( 0, 0, to );
		}
		else if( offset < 128 ) {
			ModRM( 1, 0, to );
			write8(offset);
		}
		else {
			ModRM( 2, 0, to );
			write32(offset);
		}
	}

	write32(from);
}

/* add r32 to r32 */
void ADD32RtoR( x86IntRegType to, x86IntRegType from ) 
{
	if (to > 7 || from > 7) {
		Rex(0, from >> 3, 0, to >> 3);
	}
	write8( 0x01 ); 
	ModRM( 3, from, to );
}

/* add r32 to m32 */
void ADD32RtoM(u32 to, x86IntRegType from ) 
{
	write8( 0x01 ); 
	ModRM( 0, from, DISP32 );
	write32( MEMADDR(to, 4) );
}

/* add m32 to r32 */
void ADD32MtoR( x86IntRegType to, u32 from ) 
{
	write8( 0x03 ); 
	ModRM( 0, to, DISP32 );
	write32( MEMADDR(from, 4) );
}

// add r16 to r16 
void ADD16RtoR( x86IntRegType to , x86IntRegType from )
{
	write16( 0x0366 ); 
	ModRM( 3, to, from );
}

/* add imm16 to r16 */
void ADD16ItoR( x86IntRegType to, u16 from ) 
{
	write8( 0x66 );
	if ( to == EAX) 
   {
		write8( 0x05 ); 
	}
   else 
   {
		write8( 0x81 ); 
		ModRM( 3, 0, to );
	}
	write16( from );
}

/* add imm16 to m16 */
void ADD16ItoM( u32 to, u16 from ) 
{
	write8( 0x66 );
	write8( 0x81 ); 
	ModRM( 0, 0, DISP32 );
	write32( MEMADDR(to, 6) );
	write16( from );
}

/* add r16 to m16 */
void ADD16RtoM(u32 to, x86IntRegType from ) 
{
	write8( 0x66 );
	write8( 0x01 ); 
	ModRM( 0, from, DISP32 );
	write32( MEMADDR(to, 4) );
}

/* add m16 to r16 */
void ADD16MtoR( x86IntRegType to, u32 from ) 
{
	write8( 0x66 );
	write8( 0x03 ); 
	ModRM( 0, to, DISP32 );
	write32( MEMADDR(from, 4) );
}

// add m8 to r8
void ADD8MtoR( x86IntRegType to, u32 from )
{
	write8( 0x02 ); 
	ModRM( 0, to, DISP32 );
	write32( MEMADDR(from, 4) );
}

/* adc imm32 to r32 */
void ADC32ItoR( x86IntRegType to, u32 from ) 
{
	if ( to == EAX )
   {
		write8( 0x15 );
	}
   else 
   {
		write8( 0x81 );
		ModRM( 3, 2, to );
	}
	write32( from ); 
}

/* adc imm32 to m32 */
void ADC32ItoM( u32 to, u32 from ) 
{
	write8( 0x81 ); 
	ModRM( 0, 2, DISP32 );
	write32( MEMADDR(to, 8) );
	write32( from );
}

/* adc r32 to r32 */
void ADC32RtoR( x86IntRegType to, x86IntRegType from ) 
{
	write8( 0x11 ); 
	ModRM( 3, from, to );
}

/* adc m32 to r32 */
void ADC32MtoR( x86IntRegType to, u32 from ) 
{
	write8( 0x13 ); 
	ModRM( 0, to, DISP32 );
	write32( MEMADDR(from, 4) ); 
}

// adc r32 to m32 
void ADC32RtoM( u32 to, x86IntRegType from )
{
	write8( 0x11 ); 
	ModRM( 0, from, DISP32 );
	write32( MEMADDR(to, 4) );
}

/* inc r32 */
void INC32R( x86IntRegType to ) 
{
	write8( 0x40 + to );
}

/* inc m32 */
void INC32M( u32 to ) 
{
	write8( 0xFF );
	ModRM( 0, 0, DISP32 );
	write32( MEMADDR(to, 4) );
}

/* inc r16 */
void INC16R( x86IntRegType to ) 
{
	write8( 0x66 );
	write8( 0x40 + to );
}

/* inc m16 */
void INC16M( u32 to ) 
{
	write8( 0x66 );
	write8( 0xFF );
	ModRM( 0, 0, DISP32 );
	write32( MEMADDR(to, 4) );
}


/* sub imm32 to r64 */
void SUB64ItoR( x86IntRegType to, u32 from ) 
{
	Rex(1, 0, 0, to >> 3);
	if ( to == EAX )
   {
		write8( 0x2D ); 
	} 
   else 
   {
		write8( 0x81 ); 
		ModRM( 3, 5, to );
	}
	write32( from ); 
}

/* sub r64 to r64 */
void SUB64RtoR( x86IntRegType to, x86IntRegType from ) 
{
	Rex(1, from >> 3, 0, to >> 3);
	write8( 0x29 ); 
	ModRM( 3, from, to );
}

/* sub m64 to r64 */
void SUB64MtoR( x86IntRegType to, u32 from ) 
{
	Rex(1, to >> 3, 0, 0);
	write8( 0x2B ); 
	ModRM( 0, to, DISP32 );
	write32( MEMADDR(from, 4) ); 
}

/* sub imm32 to r32 */
void SUB32ItoR( x86IntRegType to, u32 from ) 
{
	if ( to == EAX )
   {
		write8( 0x2D ); 
	} 
   else 
   {
		write8( 0x81 ); 
		ModRM( 3, 5, to );
	}
	write32( from ); 
}

/* sub imm32 to m32 */
void SUB32ItoM( u32 to, u32 from ) 
{
	write8( 0x81 ); 
	ModRM( 0, 5, DISP32 );
	write32( MEMADDR(to, 8) );
	write32( from );
}

/* sub r32 to r32 */
void SUB32RtoR( x86IntRegType to, x86IntRegType from ) 
{
	if (to > 7 || from > 7) {
		Rex(0, from >> 3, 0, to >> 3);
	}
	write8( 0x29 ); 
	ModRM( 3, from, to );
}

/* sub m32 to r32 */
void SUB32MtoR( x86IntRegType to, u32 from ) 
{
	write8( 0x2B ); 
	ModRM( 0, to, DISP32 );
	write32( MEMADDR(from, 4) ); 
}

// sub r32 to m32 
void SUB32RtoM( u32 to, x86IntRegType from )
{
	write8( 0x29 ); 
	ModRM( 0, from, DISP32 );
	write32( MEMADDR(to, 4) );
}

// sub r16 to r16 
void SUB16RtoR( x86IntRegType to, u16 from )
{
	write16( 0x2b66 ); 
	ModRM( 3, to, from );
}

/* sub imm16 to r16 */
void SUB16ItoR( x86IntRegType to, u16 from ) {
	write8( 0x66 );
	if ( to == EAX ) {
		write8( 0x2D ); 
	} else {
		write8( 0x81 ); 
		ModRM( 3, 5, to );
	}
	write16( from ); 
}

/* sub imm16 to m16 */
void SUB16ItoM( u32 to, u16 from ) {
	write8( 0x66 ); 
	write8( 0x81 ); 
	ModRM( 0, 5, DISP32 );
	write32( MEMADDR(to, 6) );
	write16( from );
}

/* sub m16 to r16 */
void SUB16MtoR( x86IntRegType to, u32 from ) {
	write8( 0x66 ); 
	write8( 0x2B ); 
	ModRM( 0, to, DISP32 );
	write32( MEMADDR(from, 4) ); 
}

/* sbb r64 to r64 */
void SBB64RtoR( x86IntRegType to, x86IntRegType from ) {
	Rex(1, to >> 3, 0, from >> 3);
	write8( 0x19 ); 
	ModRM( 3, from, to );
}

/* sbb imm32 to r32 */
void SBB32ItoR( x86IntRegType to, u32 from ) {
	if ( to == EAX ) {
		write8( 0x1D );
	} else {
		write8( 0x81 );
		ModRM( 3, 3, to );
	}
	write32( from ); 
}

/* sbb imm32 to m32 */
void SBB32ItoM( u32 to, u32 from ) {
	write8( 0x81 );
	ModRM( 0, 3, DISP32 );
	write32( MEMADDR(to, 8) );
	write32( from );
}

/* sbb r32 to r32 */
void SBB32RtoR( x86IntRegType to, x86IntRegType from ) 
{
	write8( 0x19 ); 
	ModRM( 3, from, to );
}

/* sbb m32 to r32 */
void SBB32MtoR( x86IntRegType to, u32 from ) 
{
	write8( 0x1B ); 
	ModRM( 0, to, DISP32 );
	write32( MEMADDR(from, 4) ); 
}

/* sbb r32 to m32 */
void SBB32RtoM( u32 to, x86IntRegType from ) 
{
	write8( 0x19 ); 
	ModRM( 0, from, DISP32 );
	write32( MEMADDR(to, 4) );
}


/* dec r32 */
void DEC32R( x86IntRegType to ) 
{
	write8( 0x48 + to );
}

/* dec m32 */
void DEC32M( u32 to ) 
{
	write8( 0xFF );
	ModRM( 0, 1, DISP32 );
	write32( MEMADDR(to, 4) );
}

/* dec r16 */
void DEC16R( x86IntRegType to ) 
{
	write8( 0x66 );
	write8( 0x48 + to );
}

/* dec m16 */
void DEC16M( u32 to ) 
{
	write8( 0x66 );
	write8( 0xFF );
	ModRM( 0, 1, DISP32 );
	write32( MEMADDR(to, 4) );
}

/* mul eax by r32 to edx:eax */
void MUL32R( x86IntRegType from ) 
{
	write8( 0xF7 ); 
	ModRM( 3, 4, from );
}

/* imul eax by r32 to edx:eax */
void IMUL32R( x86IntRegType from ) 
{
	write8( 0xF7 ); 
	ModRM( 3, 5, from );
}

/* mul eax by m32 to edx:eax */
void MUL32M( u32 from ) 
{
	write8( 0xF7 ); 
	ModRM( 0, 4, DISP32 );
	write32( MEMADDR(from, 4) ); 
}

/* imul eax by m32 to edx:eax */
void IMUL32M( u32 from ) 
{
	write8( 0xF7 ); 
	ModRM( 0, 5, DISP32 );
	write32( MEMADDR(from, 4) ); 
}

/* imul r32 by r32 to r32 */
void IMUL32RtoR( x86IntRegType to, x86IntRegType from ) 
{
	write16( 0xAF0F ); 
	ModRM( 3, to, from );
}

/* div eax by r32 to edx:eax */
void DIV32R( x86IntRegType from ) 
{
	write8( 0xF7 ); 
	ModRM( 3, 6, from );
}

/* idiv eax by r32 to edx:eax */
void IDIV32R( x86IntRegType from ) 
{
	write8( 0xF7 ); 
	ModRM( 3, 7, from );
}

/* div eax by m32 to edx:eax */
void DIV32M( u32 from ) 
{
	write8( 0xF7 ); 
	ModRM( 0, 6, DISP32 );
	write32( MEMADDR(from, 4) ); 
}

/* idiv eax by m32 to edx:eax */
void IDIV32M( u32 from ) 
{
	write8( 0xF7 ); 
	ModRM( 0, 7, DISP32 );
	write32( MEMADDR(from, 4) ); 
}

////////////////////////////////////
// shifting instructions           /
////////////////////////////////////

/* shl imm8 to r64 */
void SHL64ItoR( x86IntRegType to, u8 from ) 
{
	if ( from == 1 )
	{
		Rex(1, 0, 0, to >> 3);
		write8( 0xD1 );
		ModRM( 3, 4, to );
		return;
	}
	Rex(1, 0, 0, to >> 3);
	write8( 0xC1 ); 
	ModRM( 3, 4, to );
	write8( from ); 
}

/* shl cl to r64 */
void SHL64CLtoR( x86IntRegType to ) 
{
	Rex(1, to >> 3, 0, 0);
	write8( 0xD3 ); 
	ModRM( 3, 4, to );
}

/* shr imm8 to r64 */
void SHR64ItoR( x86IntRegType to, u8 from ) 
{
	if ( from == 1 )
	{
		Rex(1, 0, 0, to >> 3);
		write8( 0xD1 );
		ModRM( 3, 5, to );
		return;
	}
	Rex(1, 0, 0, to >> 3);
   write8( 0xC1 ); 
   ModRM( 3, 5, to );
   write8( from ); 
}

/* shr cl to r64 */
void SHR64CLtoR( x86IntRegType to ) 
{
	Rex(1, to >> 3, 0, 0);
	write8( 0xD3 ); 
	ModRM( 3, 5, to );
}

/* shl imm8 to r32 */
void SHL32ItoR( x86IntRegType to, u8 from ) 
{
	if (to > 7) {
		Rex(0, 0, 0, to >> 3);
	}
	if ( from == 1 )
	{
		write8( 0xD1 );
		write8( 0xE0 | (to & 0x7) );
		return;
	}
	write8( 0xC1 ); 
	ModRM( 3, 4, to );
	write8( from ); 
}

/* shl imm8 to m32 */
void SHL32ItoM( u32 to, u8 from ) 
{
   if ( from == 1 )
   {
      write8( 0xD1 );
      ModRM( 0, 4, DISP32 );
	  write32( MEMADDR(to, 4) );
   }
   else
   {
      write8( 0xC1 ); 
      ModRM( 0, 4, DISP32 );
	  write32( MEMADDR(to, 4) );
      write8( from ); 
   }
}

/* shl cl to r32 */
void SHL32CLtoR( x86IntRegType to ) 
{
	if (to > 7) {
		Rex(0, 0, 0, to >> 3);
	}
	write8( 0xD3 ); 
	ModRM( 3, 4, to );
}

// shl imm8 to r16
void SHL16ItoR( x86IntRegType to, u8 from )
{
	if ( from == 1 )
	{
		write16( 0xD166 );
		write8( 0xE0 | (to & 0x7) );
		return;
	}
	write16( 0xC166 ); 
	ModRM( 3, 4, to );
	write8( from ); 
}

// shl imm8 to r8
void SHL8ItoR( x86IntRegType to, u8 from )
{
	if ( from == 1 )
	{
		write8( 0xD0 );
		write8( 0xE0 | (to & 0x7) );
		return;
	}
	write8( 0xC0 ); 
	ModRM( 3, 4, to );
	write8( from ); 
}

/* shr imm8 to r32 */
void SHR32ItoR( x86IntRegType to, u8 from ) {
	if (to > 7) {
		Rex(0, 0, 0, to >> 3);
	}
	if ( from == 1 )
	{
		write8( 0xD1 );
		write8( 0xE8 | (to & 0x7) );
	}
   else
   {
	   write8( 0xC1 ); 
	   ModRM( 3, 5, to );
	   write8( from ); 
   }
}

/* shr imm8 to m32 */
void SHR32ItoM( u32 to, u8 from ) 
{
   if ( from == 1 )
   {
      write8( 0xD1 );
      ModRM( 0, 5, DISP32 );
	  write32( MEMADDR(to, 4) );
   }
   else
   {
      write8( 0xC1 ); 
      ModRM( 0, 5, DISP32 );
	  write32( MEMADDR(to, 4) );
      write8( from ); 
   }
}

/* shr cl to r32 */
void SHR32CLtoR( x86IntRegType to ) 
{
	if (to > 7) {
		Rex(0, 0, 0, to >> 3);
	}
	write8( 0xD3 ); 
	ModRM( 3, 5, to );
}

// shr imm8 to r8
void SHR8ItoR( x86IntRegType to, u8 from )
{
	if ( from == 1 )
	{
		write8( 0xD0 );
		write8( 0xE8 | (to & 0x7) );
	}
	else
	{
		write8( 0xC0 ); 
		ModRM( 3, 5, to );
		write8( from ); 
	}
}

/* sar imm8 to r64 */
void SAR64ItoR( x86IntRegType to, u8 from ) 
{
	if ( from == 1 )
	{
		Rex(1, 0, 0, to >> 3);
		write8( 0xD1 );
		ModRM( 3, 7, to );
		return;
	}
	Rex(1, 0, 0, to >> 3);
   write8( 0xC1 ); 
   ModRM( 3, 7, to );
   write8( from ); 
}

/* sar cl to r64 */
void SAR64CLtoR( x86IntRegType to ) 
{
	Rex(1, to >> 3, 0, 0);
	write8( 0xD3 ); 
	ModRM( 3, 7, to );
}

/* sar imm8 to r32 */
void SAR32ItoR( x86IntRegType to, u8 from ) 
{
	if (to > 7) {
		Rex(0, 0, 0, to >> 3);
	}
	if ( from == 1 )
	{
		write8( 0xD1 );
		ModRM( 3, 7, to );
		return;
	}
	write8( 0xC1 ); 
	ModRM( 3, 7, to );
	write8( from ); 
}

/* sar imm8 to m32 */
void SAR32ItoM( u32 to, u8 from )
{
	write8( 0xC1 ); 
	ModRM( 0, 7, DISP32 );
	write32( MEMADDR(to, 5) );
	write8( from );
}

/* sar cl to r32 */
void SAR32CLtoR( x86IntRegType to ) 
{
	if (to > 7) {
		Rex(0, 0, 0, to >> 3);
	}
	write8( 0xD3 ); 
	ModRM( 3, 7, to );
}

// sar imm8 to r16
void SAR16ItoR( x86IntRegType to, u8 from )
{
	if (to > 7) {
		Rex(0, 0, 0, to >> 3);
	}
	if ( from == 1 )
	{
		write16( 0xD166 );
		ModRM( 3, 7, to );
		return;
	}
	write16( 0xC166 ); 
	ModRM( 3, 7, to );
	write8( from ); 
}

void ROR32ItoR( x86IntRegType to,u8 from )
{
	if ( from == 1 ) {
		write8( 0xd1 );
		write8( 0xc8 | to );
	} 
	else 
	{
		write8( 0xc1 );
		write8( 0xc8 | to );
		write8( from );
	}
}

void RCR32ItoR( x86IntRegType to, u8 from ) 
{
	if ( from == 1 ) {
		write8( 0xd1 );
		write8( 0xd8 | to );
	} 
   else 
   {
		write8( 0xc1 );
		write8( 0xd8 | to );
		write8( from );
	}
}

// shld imm8 to r32
void SHLD32ItoR( u32 to, u32 from, u8 shift )
{
	write8( 0x0F );
	write8( 0xA4 );
	ModRM( 3, from, to );
	write8( shift );
}

// shrd imm8 to r32
void SHRD32ItoR( u32 to, u32 from, u8 shift )
{
	write8( 0x0F );
	write8( 0xAC );
	ModRM( 3, from, to );
	write8( shift );
}

////////////////////////////////////
// logical instructions            /
////////////////////////////////////

/* or imm32 to r32 */
void OR64ItoR( x86IntRegType to, u32 from ) 
{
	Rex(1, 0, 0, to >> 3);
	if ( to == EAX ) {
		write8( 0x0D ); 
	} else {
		write8( 0x81 ); 
		ModRM( 3, 1, to );
	}
	write32( from ); 
}

/* or m64 to r64 */
void OR64MtoR( x86IntRegType to, u32 from ) 
{
	Rex(1, to >> 3, 0, 0);
	write8( 0x0B ); 
	ModRM( 0, to, DISP32 );
	write32( MEMADDR(from, 4) ); 
}

/* or r64 to r64 */
void OR64RtoR( x86IntRegType to, x86IntRegType from ) 
{
	Rex(1, from >> 3, 0, to >> 3);
	write8( 0x09 ); 
	ModRM( 3, from, to );
}


/* or imm32 to r32 */
void OR32ItoR( x86IntRegType to, u32 from ) 
{
	if ( to == EAX ) {
		write8( 0x0D ); 
	}
	else {
		write8( 0x81 ); 
		ModRM( 3, 1, to );
	}
	write32( from ); 
}

/* or imm32 to m32 */
void OR32ItoM(u32 to, u32 from ) 
{
	write8( 0x81 ); 
	ModRM( 0, 1, DISP32 );
	write32( MEMADDR(to, 8) );
	write32( from ); 
}

/* or r32 to r32 */
void OR32RtoR( x86IntRegType to, x86IntRegType from ) 
{
	write8( 0x09 ); 
	ModRM( 3, from, to );
}

/* or r32 to m32 */
void OR32RtoM(u32 to, x86IntRegType from ) 
{
	write8( 0x09 ); 
	ModRM( 0, from, DISP32 );
	write32( MEMADDR(to, 4) );
}

/* or m32 to r32 */
void OR32MtoR( x86IntRegType to, u32 from ) 
{
	write8( 0x0B ); 
	ModRM( 0, to, DISP32 );
	write32( MEMADDR(from, 4) ); 
}

// or r16 to r16
void OR16RtoR( x86IntRegType to, x86IntRegType from )
{
	write16( 0x0966 ); 
	ModRM( 3, from, to );
}

// or imm16 to r16
void OR16ItoR( x86IntRegType to, u16 from )
{
	if ( to == EAX ) {
		write16( 0x0D66 ); 
	}
	else {
		write16( 0x8166 ); 
		ModRM( 3, 1, to );
	}
	write16( from ); 
}

// or imm16 to m316
void OR16ItoM( u32 to, u16 from )
{
	write16( 0x8166 ); 
	ModRM( 0, 1, DISP32 );
	write32( MEMADDR(to, 8) );
	write16( from ); 
}

/* or m16 to r16 */
void OR16MtoR( x86IntRegType to, u32 from ) 
{
	write16( 0x0B66 ); 
	ModRM( 0, to, DISP32 );
	write32( MEMADDR(from, 4) ); 
}

// or r16 to m16 
void OR16RtoM( u32 to, x86IntRegType from )
{
	write16( 0x0966 ); 
	ModRM( 0, from, DISP32 );
	write32( MEMADDR(to, 4) );
}

// or r8 to r8
void OR8RtoR( x86IntRegType to, x86IntRegType from )
{
	write8( 0x08 ); 
	ModRM( 3, from, to );
}

// or r8 to m8
void OR8RtoM( u32 to, x86IntRegType from )
{
	write8( 0x08 ); 
	ModRM( 0, from, DISP32 );
	write32( MEMADDR(to, 4) ); 
}

// or imm8 to m8
void OR8ItoM( u32 to, u8 from )
{
	write8( 0x80 ); 
	ModRM( 0, 1, DISP32 );
	write32( MEMADDR(to, 8) );
	write8( from ); 
}

// or m8 to r8
void OR8MtoR( x86IntRegType to, u32 from )
{
	write8( 0x0A ); 
	ModRM( 0, to, DISP32 );
	write32( MEMADDR(from, 4) ); 
}

/* xor imm32 to r64 */
void XOR64ItoR( x86IntRegType to, u32 from ) 
{
	Rex(1, 0, 0, to >> 3);
	if ( to == EAX ) {
		write8( 0x35 ); 
	} else {
		write8( 0x81 ); 
		ModRM( 3, 6, to );
	}
	write32( from ); 
}

/* xor r64 to r64 */
void XOR64RtoR( x86IntRegType to, x86IntRegType from ) 
{
	Rex(1, from >> 3, 0, to >> 3);
	write8( 0x31 ); 
	ModRM( 3, from, to );
}

/* xor m64 to r64 */
void XOR64MtoR( x86IntRegType to, u32 from ) 
{
	Rex(1, to >> 3, 0, 0);
	write8( 0x33 ); 
	ModRM( 0, to, DISP32 );
	write32( MEMADDR(from, 4) ); 
}

/* xor imm32 to r32 */
void XOR32ItoR( x86IntRegType to, u32 from ) 
{
	if ( to == EAX ) {
		write8( 0x35 ); 
	}
	else  {
		write8( 0x81 ); 
		ModRM( 3, 6, to );
	}
	write32( from ); 
}

/* xor imm32 to m32 */
void XOR32ItoM( u32 to, u32 from ) 
{
	write8( 0x81 ); 
	ModRM( 0, 6, DISP32 );
	write32( MEMADDR(to, 8) ); 
	write32( from ); 
}

/* xor r32 to r32 */
void XOR32RtoR( x86IntRegType to, x86IntRegType from ) 
{
	write8( 0x31 ); 
	ModRM( 3, from, to );
}

/* xor r16 to r16 */
void XOR16RtoR( x86IntRegType to, x86IntRegType from ) 
{
	write8( 0x66 );
	write8( 0x31 ); 
	ModRM( 3, from, to );
}

/* xor r32 to m32 */
void XOR32RtoM( u32 to, x86IntRegType from ) 
{
	write8( 0x31 ); 
	ModRM( 0, from, DISP32 );
	write32( MEMADDR(to, 4) );
}

/* xor m32 to r32 */
void XOR32MtoR( x86IntRegType to, u32 from ) 
{
	write8( 0x33 ); 
	ModRM( 0, to, DISP32 );
	write32( MEMADDR(from, 4) ); 
}

// xor imm16 to r16
void XOR16ItoR( x86IntRegType to, u16 from )
{
	if ( to == EAX ) {
		write16( 0x3566 ); 
	}
	else  {
		write16( 0x8166 ); 
		ModRM( 3, 6, to );
	}
	write16( from ); 
}

// xor r16 to m16
void XOR16RtoM( u32 to, x86IntRegType from )
{
	write16( 0x3166 ); 
	ModRM( 0, from, DISP32 );
	write32( MEMADDR(to, 4) );
}

/* and imm32 to r64 */
void AND64ItoR( x86IntRegType to, u32 from ) 
{
	Rex(1, 0, 0, to >> 3);
	if ( to == EAX ) {
		write8( 0x25 ); 
	} else {
		write8( 0x81 ); 
		ModRM( 3, 0x4, to );
	}
	write32( from ); 
}

/* and m64 to r64 */
void AND64MtoR( x86IntRegType to, u32 from ) 
{
	Rex(1, to >> 3, 0, 0);
	write8( 0x23 ); 
	ModRM( 0, to, DISP32 );
	write32( MEMADDR(from, 4) ); 
}

/* and r64 to m64 */
void AND64RtoM( u32 to, x86IntRegType from ) 
{
	Rex(1, 0, 0, 0);
	write8( 0x21 ); 
	ModRM( 0, from, DISP32 );
	write32( MEMADDR(to, 4) ); 
}

/* and r64 to r64 */
void AND64RtoR( x86IntRegType to, x86IntRegType from ) 
{
	Rex(1, from >> 3, 0, to >> 3);
	write8( 0x21 ); 
	ModRM( 3, from, to );
}

/* and imm32 to r32 */
void AND32ItoR( x86IntRegType to, u32 from ) 
{
	if (to > 7) {
		Rex(0, 0, 0, to >> 3);
	}
	if ( to == EAX ) {
		write8( 0x25 ); 
	} else {
		write8( 0x81 ); 
		ModRM( 3, 0x4, to );
	}
	write32( from ); 
}

/* and sign ext imm8 to r32 */
void AND32I8toR( x86IntRegType to, u8 from ) 
{
	if (to > 7) {
		Rex(0, 0, 0, to >> 3);
	}
	write8( 0x83 ); 
	ModRM( 3, 0x4, to );
	write8( from ); 
}

/* and imm32 to m32 */
void AND32ItoM( u32 to, u32 from ) 
{
	write8( 0x81 ); 
	ModRM( 0, 0x4, DISP32 );
	write32( MEMADDR(to, 8) );
	write32( from ); 
}

/* and sign ext imm8 to m32 */
void AND32I8toM( u32 to, u8 from ) 
{
	write8( 0x83 ); 
	ModRM( 0, 0x4, DISP32 );
	write32( MEMADDR(to, 8) );
	write8( from ); 
}

/* and r32 to r32 */
void AND32RtoR( x86IntRegType to, x86IntRegType from ) 
{
	write8( 0x21 ); 
	ModRM( 3, from, to );
}

/* and r32 to m32 */
void AND32RtoM( u32 to, x86IntRegType from ) 
{
	write8( 0x21 ); 
	ModRM( 0, from, DISP32 );
	write32( MEMADDR(to, 4) ); 
}

/* and m32 to r32 */
void AND32MtoR( x86IntRegType to, u32 from ) 
{
	write8( 0x23 ); 
	ModRM( 0, to, DISP32 );
	write32( MEMADDR(from, 4) ); 
}

// and r16 to r16
void AND16RtoR( x86IntRegType to, x86IntRegType from )
{
	write16( 0x2366 ); 
	ModRM( 3, to, from );
}

/* and imm16 to r16 */
void AND16ItoR( x86IntRegType to, u16 from ) 
{
	if (to > 7) {
		Rex(0, 0, 0, to >> 3);
	}
	if ( to == EAX ) {
		write16( 0x2566 ); 
	} else {
		write16( 0x8166 ); 
		ModRM( 3, 0x4, to );
	}
	write16( from ); 
}

/* and imm16 to m16 */
void AND16ItoM( u32 to, u16 from ) 
{
	write8( 0x8166 ); 
	ModRM( 0, 0x4, DISP32 );
	write32( MEMADDR(to, 8) );
	write16( from ); 
}

/* and r16 to m16 */
void AND16RtoM( u32 to, x86IntRegType from ) 
{
	write8( 0x66 );
	write8( 0x21 ); 
	ModRM( 0, from, DISP32 );
	write32( MEMADDR(to, 4) ); 
}

/* and m16 to r16 */
void AND16MtoR( x86IntRegType to, u32 from ) 
{
	write8( 0x66 );
	write8( 0x23 ); 
	ModRM( 0, to, DISP32 );
	write32( MEMADDR(from, 4)); 
}

/* and imm8 to r8 */
void AND8ItoR( x86IntRegType to, u8 from ) 
{
	if (to > 7) {
		Rex(0, 0, 0, to >> 3);
	}
	if ( to == EAX ) {
		write8( 0x24 ); 
	} else {
		write8( 0x80 ); 
		ModRM( 3, 0x4, to );
	}
	write8( from ); 
}

/* and imm8 to m8 */
void AND8ItoM( u32 to, u8 from ) 
{
	write8( 0x80 ); 
	ModRM( 0, 0x4, DISP32 );
	write32( MEMADDR(to, 8) );
	write8( from ); 
}

// and r8 to r8
void AND8RtoR( x86IntRegType to, x86IntRegType from )
{
	write8( 0x22 ); 
	ModRM( 3, to, from );
}

/* and r8 to m8 */
void AND8RtoM( u32 to, x86IntRegType from ) 
{
	write8( 0x20 ); 
	ModRM( 0, from, DISP32 );
	write32( MEMADDR(to, 4) ); 
}

/* and m8 to r8 */
void AND8MtoR( x86IntRegType to, u32 from ) 
{
	write8( 0x22 ); 
	ModRM( 0, to, DISP32 );
	write32( MEMADDR(from, 4)); 
}

/* not r64 */
void NOT64R( x86IntRegType from ) 
{
	Rex(1, 0, 0, from >> 3);
	write8( 0xF7 ); 
	ModRM( 3, 2, from );
}

/* not r32 */
void NOT32R( x86IntRegType from ) 
{
	write8( 0xF7 ); 
	ModRM( 3, 2, from );
}

// not m32 
void NOT32M( u32 from )
{
	write8( 0xF7 ); 
	ModRM( 0, 2, DISP32 );
	write32( MEMADDR(from, 4)); 
}

/* neg r64 */
void NEG64R( x86IntRegType from ) 
{
	Rex(1, from >> 3, 0, 0);
	write8( 0xF7 ); 
	ModRM( 3, 3, from );
}

/* neg r32 */
void NEG32R( x86IntRegType from ) 
{
	write8( 0xF7 ); 
	ModRM( 3, 3, from );
}

void NEG32M( u32 from )
{
	write8( 0xF7 ); 
	ModRM( 0, 3, DISP32 );
	write32( MEMADDR(from, 4)); 
}

/* neg r16 */
void NEG16R( x86IntRegType from ) 
{
	write8( 0x66 ); 
	write8( 0xF7 ); 
	ModRM( 3, 3, from );
}

////////////////////////////////////
// jump instructions               /
////////////////////////////////////

u8* JMP( u32 to ) {
	u32 jump = ( x86Ptr - (s8*)to ) - 1;

	if ( jump > 0x7f ) {
      return (u8*)JMP32( to );
	} else {
      return (u8*)JMP8( to );
	}
}

/* jmp rel8 */
u8* JMP8( u8 to ) 
{
	write8( 0xEB ); 
	write8( to );
	return x86Ptr - 1;
}

/* jmp rel32 */
u32* JMP32( u32 to ) 
{
	write8( 0xE9 ); 
	write32( to ); 
	return (u32*)(x86Ptr - 4 );
}

/* jmp r32 */
void JMP32R( x86IntRegType to ) 
{
	write8( 0xFF ); 
	ModRM( 3, 4, to );
}

// jmp m32 
void JMP32M( u32 to )
{
	write8( 0xFF ); 
	ModRM( 0, 4, DISP32 );
	write32( MEMADDR(to, 4)); 
}

/* jmp r64 */
void JMP64R( x86IntRegType to ) {
	Rex(1, 0, 0, to >> 3);
	write8( 0xFF );
	ModRM( 3, 4, to );
}

/* jp rel8 */
u8* JP8( u8 to ) {
	return J8Rel( 0x7A, to );
}

/* jnp rel8 */
u8* JNP8( u8 to ) {
	return J8Rel( 0x7B, to );
}

/* je rel8 */
u8* JE8( u8 to ) {
	return J8Rel( 0x74, to );
}

/* jz rel8 */
u8* JZ8( u8 to ) 
{
	return J8Rel( 0x74, to ); 
}

/* js rel8 */
u8* JS8( u8 to ) 
{ 
	return J8Rel( 0x78, to );
}

/* jns rel8 */
u8* JNS8( u8 to ) 
{ 
	return J8Rel( 0x79, to );
}

/* jg rel8 */
u8* JG8( u8 to ) 
{ 
	return J8Rel( 0x7F, to );
}

/* jge rel8 */
u8* JGE8( u8 to ) 
{ 
	return J8Rel( 0x7D, to ); 
}

/* jl rel8 */
u8* JL8( u8 to ) 
{ 
	return J8Rel( 0x7C, to ); 
}

/* ja rel8 */
u8* JA8( u8 to ) 
{ 
	return J8Rel( 0x77, to ); 
}

u8* JAE8( u8 to ) 
{ 
	return J8Rel( 0x73, to ); 
}

/* jb rel8 */
u8* JB8( u8 to ) 
{ 
	return J8Rel( 0x72, to ); 
}

/* jbe rel8 */
u8* JBE8( u8 to ) 
{ 
	return J8Rel( 0x76, to ); 
}

/* jle rel8 */
u8* JLE8( u8 to ) 
{ 
	return J8Rel( 0x7E, to ); 
}

/* jne rel8 */
u8* JNE8( u8 to ) 
{ 
	return J8Rel( 0x75, to ); 
}

/* jnz rel8 */
u8* JNZ8( u8 to ) 
{ 
	return J8Rel( 0x75, to ); 
}

/* jng rel8 */
u8* JNG8( u8 to ) 
{ 
	return J8Rel( 0x7E, to ); 
}

/* jnge rel8 */
u8* JNGE8( u8 to ) 
{ 
	return J8Rel( 0x7C, to ); 
}

/* jnl rel8 */
u8* JNL8( u8 to ) 
{ 
	return J8Rel( 0x7D, to ); 
}

/* jnle rel8 */
u8* JNLE8( u8 to ) 
{ 
	return J8Rel( 0x7F, to ); 
}

/* jo rel8 */
u8* JO8( u8 to ) 
{ 
	return J8Rel( 0x70, to ); 
}

/* jno rel8 */
u8* JNO8( u8 to ) 
{ 
	return J8Rel( 0x71, to ); 
}

// jb rel8 
u16* JB16( u16 to )
{
	return J16Rel( 0x82, to );
}

// jb rel32 
u32* JB32( u32 to )
{
	return J32Rel( 0x82, to );
}

/* je rel32 */
u32* JE32( u32 to ) 
{
	return J32Rel( 0x84, to );
}

/* jz rel32 */
u32* JZ32( u32 to ) 
{
	return J32Rel( 0x84, to ); 
}

/* jg rel32 */
u32* JG32( u32 to ) 
{ 
	return J32Rel( 0x8F, to );
}

/* jge rel32 */
u32* JGE32( u32 to ) 
{ 
	return J32Rel( 0x8D, to ); 
}

/* jl rel32 */
u32* JL32( u32 to ) 
{ 
	return J32Rel( 0x8C, to ); 
}

/* jle rel32 */
u32* JLE32( u32 to ) 
{ 
	return J32Rel( 0x8E, to ); 
}

/* jae rel32 */
u32* JAE32( u32 to ) 
{ 
	return J32Rel( 0x83, to ); 
}

/* jne rel32 */
u32* JNE32( u32 to ) 
{ 
	return J32Rel( 0x85, to ); 
}

/* jnz rel32 */
u32* JNZ32( u32 to ) 
{ 
	return J32Rel( 0x85, to ); 
}

/* jng rel32 */
u32* JNG32( u32 to ) 
{ 
	return J32Rel( 0x8E, to ); 
}

/* jnge rel32 */
u32* JNGE32( u32 to ) 
{ 
	return J32Rel( 0x8C, to ); 
}

/* jnl rel32 */
u32* JNL32( u32 to ) 
{ 
	return J32Rel( 0x8D, to ); 
}

/* jnle rel32 */
u32* JNLE32( u32 to ) 
{ 
	return J32Rel( 0x8F, to ); 
}

/* jo rel32 */
u32* JO32( u32 to ) 
{ 
	return J32Rel( 0x80, to ); 
}

/* jno rel32 */
u32* JNO32( u32 to ) 
{ 
	return J32Rel( 0x81, to ); 
}

// js rel32
u32*  JS32( u32 to )
{
	return J32Rel( 0x88, to );
}


/* call func */
void CALLFunc( u32 func ) 
{
	CALL32( func - ( (u32)x86Ptr + 5 ) );
}

/* call rel32 */
void CALL32( u32 to )
{
	write8( 0xE8 ); 
	write32( to ); 
}

/* call r32 */
void CALL32R( x86IntRegType to ) 
{
	write8( 0xFF );
	ModRM( 3, 2, to );
}

/* call r64 */
void CALL64R( x86IntRegType to ) 
{
	Rex(1, 0, 0, to >> 3);
	write8( 0xFF );
	ModRM( 3, 2, to );
}

/* call m32 */
void CALL32M( u32 to ) 
{
	write8( 0xFF );
	ModRM( 0, 2, DISP32 );
	write32( MEMADDR(to, 4) );
}

////////////////////////////////////
// misc instructions               /
////////////////////////////////////

/* cmp imm32 to r64 */
void CMP64ItoR( x86IntRegType to, u32 from ) 
{
	Rex(1, to >> 3, 0, 0);
	if ( to == EAX )
   {
		write8( 0x3D );
	} 
   else 
   {
		write8( 0x81 );
		ModRM( 3, 7, to );
	}
	write32( from ); 
}

/* cmp m64 to r64 */
void CMP64MtoR( x86IntRegType to, u32 from ) 
{
	Rex(1, to >> 3, 0, 0);
	write8( 0x3B );
	ModRM( 0, to, DISP32 );
	write32( MEMADDR(from, 4) ); 
}

/* cmp imm32 to r32 */
void CMP32ItoR( x86IntRegType to, u32 from ) 
{
	if ( to == EAX )
   {
		write8( 0x3D );
	} 
   else 
   {
		write8( 0x81 );
		ModRM( 3, 7, to );
	}
	write32( from ); 
}

/* cmp imm32 to m32 */
void CMP32ItoM( u32 to, u32 from ) 
{
	write8( 0x81 ); 
	ModRM( 0, 7, DISP32 );
	write32( MEMADDR(to, 8) ); 
	write32( from ); 
}

/* cmp r32 to r32 */
void CMP32RtoR( x86IntRegType to, x86IntRegType from ) 
{
	write8( 0x39 );
	ModRM( 3, from, to );
}

/* cmp m32 to r32 */
void CMP32MtoR( x86IntRegType to, u32 from ) 
{
	write8( 0x3B );
	ModRM( 0, to, DISP32 );
	write32( MEMADDR(from, 4) ); 
}

// cmp imm8 to [r32]
void CMP32I8toRm( x86IntRegType to, u8 from)
{
	write8( 0x83 );
	ModRM( 0, 7, to );
	write8(from);
}

// cmp imm32 to [r32+off]
void CMP32I8toRmOffset8( x86IntRegType to, u8 from, u8 off)
{
	assert( to != ESP );
	write8( 0x83 );
	ModRM( 1, 7, to );
	write8(off);
	write8(from);
}

// cmp imm8 to [r32]
void CMP32I8toM( u32 to, u8 from)
{
	write8( 0x83 );
	ModRM( 0, 7, DISP32 );
	write32( MEMADDR(to, 8) ); 
	write8( from ); 
}

/* cmp imm16 to r16 */
void CMP16ItoR( x86IntRegType to, u16 from ) 
{
	write8( 0x66 ); 
	if ( to == EAX )
	{
		write8( 0x3D );
	} 
	else 
	{
		write8( 0x81 );
		ModRM( 3, 7, to );
	}
	write16( from ); 
}

/* cmp imm16 to m16 */
void CMP16ItoM( u32 to, u16 from ) 
{
	write8( 0x66 ); 
	write8( 0x81 ); 
	ModRM( 0, 7, DISP32 );
	write32( MEMADDR(to, 6) ); 
	write16( from ); 
}

/* cmp r16 to r16 */
void CMP16RtoR( x86IntRegType to, x86IntRegType from ) 
{
	write8( 0x66 ); 
	write8( 0x39 );
	ModRM( 3, from, to );
}

/* cmp m16 to r16 */
void CMP16MtoR( x86IntRegType to, u32 from ) 
{
	write8( 0x66 ); 
	write8( 0x3B );
	ModRM( 0, to, DISP32 );
	write32( MEMADDR(from, 4) ); 
}

// cmp imm8 to r8
void CMP8ItoR( x86IntRegType to, u8 from )
{
	if ( to == EAX )
	{
		write8( 0x3C );
	} 
	else 
	{
		write8( 0x80 );
		ModRM( 3, 7, to );
	}
	write8( from ); 
}

// cmp m8 to r8
void CMP8MtoR( x86IntRegType to, u32 from )
{
	write8( 0x3A );
	ModRM( 0, to, DISP32 );
	write32( MEMADDR(from, 4) ); 
}

/* test imm32 to r32 */
void TEST32ItoR( x86IntRegType to, u32 from ) 
{
	if ( to == EAX )
	{
		write8( 0xA9 );
	} 
	else 
	{
		write8( 0xF7 );
		ModRM( 3, 0, to );
	}
	write32( from ); 
}

void TEST32ItoM( u32 to, u32 from )
{
	write8( 0xF7 );
	ModRM( 0, 0, DISP32 );
	write32( MEMADDR(to, 8) );
	write32( from );
}

/* test r32 to r32 */
void TEST32RtoR( x86IntRegType to, x86IntRegType from ) 
{
	write8( 0x85 );
	ModRM( 3, from, to );
}

// test imm32 to [r32]
void TEST32ItoRm( x86IntRegType to, u32 from )
{
	write8( 0xF7 );
	ModRM( 0, 0, to );
	write32(from);
}

// test imm16 to r16
void TEST16ItoR( x86IntRegType to, u16 from )
{
	if ( to == EAX )
	{
		write16( 0xA966 );
	} 
	else 
	{
		write16( 0xF766 );
		ModRM( 3, 0, to );
	}
	write16( from ); 
}

// test r16 to r16
void TEST16RtoR( x86IntRegType to, x86IntRegType from )
{
	write16( 0x8566 );
	ModRM( 3, from, to );
}

// test imm8 to r8
void TEST8ItoR( x86IntRegType to, u8 from )
{
	if ( to == EAX )
	{
		write8( 0xA8 );
	} 
	else 
	{
		write8( 0xF6 );
		ModRM( 3, 0, to );
	}
	write8( from ); 
}

// test imm8 to r8
void TEST8ItoM( u32 to, u8 from )
{
	write8( 0xF6 );
	ModRM( 0, 0, DISP32 );
	write32( MEMADDR(to, 8) );
	write8( from );
}

/* sets r8 */
void SETS8R( x86IntRegType to ) 
{ 
   SET8R( 0x98, to ); 
}

/* setl r8 */
void SETL8R( x86IntRegType to ) 
{ 
	SET8R( 0x9C, to ); 
}

// setge r8 
void SETGE8R( x86IntRegType to ) { SET8R(0x9d, to); }
// setg r8 
void SETG8R( x86IntRegType to ) { SET8R(0x9f, to); }
// seta r8 
void SETA8R( x86IntRegType to ) { SET8R(0x97, to); }
// setae r8 
void SETAE8R( x86IntRegType to ) { SET8R(0x99, to); }
/* setb r8 */
void SETB8R( x86IntRegType to ) { SET8R( 0x92, to ); }
/* setb r8 */
void SETNZ8R( x86IntRegType to )  { SET8R( 0x95, to ); }
// setz r8 
void SETZ8R( x86IntRegType to ) { SET8R(0x94, to); }
// sete r8 
void SETE8R( x86IntRegType to ) { SET8R(0x94, to); }

/* push r32 */
void PUSH32R( x86IntRegType from ) 
{
	write8( 0x50 | from ); 
}

/* push m32 */
void PUSH32M( u32 from ) 
{
	write8( 0xFF );
	ModRM( 0, 6, DISP32 );
	write32( MEMADDR(from, 4) ); 
}

/* push imm32 */
void PUSH32I( u32 from ) 
{
	write8( 0x68 ); 
   write32( from ); 
}

/* pop r32 */
void POP32R( x86IntRegType from ) 
{
	write8( 0x58 | from ); 
}

/* pushad */
void PUSHA32( void )
{
	write8( 0x60 ); 
}

/* popad */
void POPA32( void ) 
{
	write8( 0x61 ); 
}

/* pushfd */
void PUSHFD( void )
{
	write8( 0x9C );
}

/* popfd */
void POPFD( void )
{
	write8( 0x9D ); 
}

void RET( void ) { write8( 0xC3 ); }
void RET2( void ) { write16( 0xc3f3 ); }

void CBW( void ) { write16( 0x9866 );  }
void CWD( void )  { write8( 0x98 ); }
void CDQ( void ) { write8( 0x99 ); }
void CWDE() { write8(0x98); }

void LAHF() { write8(0x9f); }
void SAHF() { write8(0x9e); }

void BT32ItoR( x86IntRegType to, x86IntRegType from ) 
{
	write16( 0xBA0F );
	write8( 0xE0 | to );
	write8( from );
}

void BSRRtoR(x86IntRegType to, x86IntRegType from)
{
	write16( 0xBD0F );
	ModRM( 3, from, to );
}

void BSWAP32R( x86IntRegType to ) 
{
	write8( 0x0F );
	write8( 0xC8 + to );
}

// to = from + offset
void LEA16RtoR(x86IntRegType to, x86IntRegType from, u16 offset)
{
	write8(0x66);
	LEA32RtoR(to, from, offset);
}

void LEA32RtoR(x86IntRegType to, x86IntRegType from, u32 offset)
{
	write8(0x8d);

	if( from == ESP ) {
		if( offset == 0 ) {
			ModRM(1, to, from);
			write8(0x24);
		}
		else if( offset < 128 ) {
			ModRM(1, to, from);
			write8(0x24);
			write8(offset);
		}
		else {
			ModRM(2, to, from);
			write8(0x24);
			write32(offset);
		}
	}
	else {
		if( offset == 0 && from != EBP && from!=ESP ) {
			ModRM(0, to, from);
		}
		else if( offset < 128 ) {
			ModRM(1, to, from);
			write8(offset);
		}
		else {
			ModRM(2, to, from);
			write32(offset);
		}
	}
}

// to = from0 + from1
void LEA16RRtoR(x86IntRegType to, x86IntRegType from0, x86IntRegType from1)
{
	write8(0x66);
	LEA32RRtoR(to, from0, from1);
}

void LEA32RRtoR(x86IntRegType to, x86IntRegType from0, x86IntRegType from1)
{
	write8(0x8d);
	if( from0 == EBP || from1 == EBP ) {

		if( from0 == EBP ) SWAP(from0, from1);
		ModRM(1, to, 4);
		ModRM(0, from0, from1);
		write8(0);
	}
	else {
		ModRM(0, to, 4);
		ModRM(0, from0, from1);
	}
}

// to = from << scale (max is 3)
void LEA16RStoR(x86IntRegType to, x86IntRegType from, u32 scale)
{
	write8(0x66);
	LEA32RStoR(to, from, scale);
}

void LEA32RStoR(x86IntRegType to, x86IntRegType from, u32 scale)
{
	if( to == from ) {
		SHL32ItoR(to, scale);
		return;
	}

	if( from != ESP ) {
		write8(0x8d);
		ModRM(0, to, 4);
		ModRM(scale, from, 5);
		write32(0);
	}
	else {
		MOV32RtoR(to, from);
		LEA32RStoR(to, to, scale);
	}
}
