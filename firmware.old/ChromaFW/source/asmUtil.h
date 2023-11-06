#ifndef _ASM_UTIL_H_
#define _ASM_UTIL_H_


#include <stdint.h>

struct RngState {
	uint64_t a, b;
	uint32_t c;
};


//SDCC may have uint64_t support, but it is so shitty, we're better off not using it
//sdcc is brain dead when compiling multiplication, so we write our own asm code to make it better...le sigh...
//SDCC also has issues managing xdata memory ops, we write our own

#pragma callee_saves u64_copy
#pragma callee_saves u64_isLt
#pragma callee_saves u64_isEq
#pragma callee_saves u64_sub
#pragma callee_saves u64_add
#pragma callee_saves u64_inc
#pragma callee_saves u64_dec

#pragma callee_saves xMemSet
#pragma callee_saves xMemEqual
#pragma callee_saves xMemCopy
#pragma callee_saves xStrLen

#pragma callee_saves rngGen

#pragma callee_saves mathPrvMul8x8
#pragma callee_saves mathPrvMul16x8
#pragma callee_saves mathPrvMul16x16
#pragma callee_saves mathPrvMul32x8
#pragma callee_saves mathPrvDiv32x8
#pragma callee_saves mathPrvDiv32x16
#pragma callee_saves mathPrvMod32x16
#pragma callee_saves mathPrvDiv16x8

#pragma callee_saves mathPrvI16Asr1

#define u64_copyFromCode(_dst, _src) 	u64_copy((_dst), (uint64_t __xdata*)(_src))			//ti cc1110 xdata == code

void u64_copy(uint64_t __xdata *dst, const uint64_t __xdata *src) __reentrant;
__bit u64_isLt(const uint64_t __xdata *lhs, const uint64_t __xdata *rhs) __reentrant;
__bit u64_isEq(const uint64_t __xdata *lhs, const uint64_t __xdata *rhs) __reentrant;
void u64_sub(uint64_t __xdata *lhs, const uint64_t __xdata *rhs) __reentrant;
void u64_add(uint64_t __xdata *lhs, const uint64_t __xdata *rhs) __reentrant;
void u64_and(uint64_t __xdata *lhs, const uint64_t __xdata *rhs) __reentrant;
void u64_inc(uint64_t __xdata *dst) __reentrant;
void u64_dec(uint64_t __xdata *dst) __reentrant;

#define U64FMT			"%04x%04x%04x%04x"
#define U64CVT(v)		((uint16_t __xdata*)&v)[3], ((uint16_t __xdata*)&v)[2], ((uint16_t __xdata*)&v)[1], ((uint16_t __xdata*)&v)[0]

int16_t mathPrvI16Asr1(int16_t val) __reentrant;

uint16_t mathPrvMul8x8(uint8_t a, uint8_t b) __reentrant;
uint32_t mathPrvMul16x8(uint16_t a, uint8_t b) __reentrant;
uint32_t mathPrvMul16x16(uint16_t a, uint16_t b) __reentrant;
uint32_t mathPrvMul32x8(uint32_t a, uint8_t b) __reentrant;
uint32_t mathPrvDiv32x8(uint32_t num, uint8_t denom) __reentrant;
uint32_t mathPrvDiv32x16(uint32_t num, uint16_t denom) __reentrant;
uint16_t mathPrvMod32x16(uint32_t num, uint16_t denom) __reentrant;
uint16_t mathPrvDiv16x8(uint16_t num, uint8_t denom) __reentrant;

void xMemSet(void __xdata* mem, uint8_t val, uint16_t num) __reentrant;
__bit xMemEqual(void __xdata* memA, void __xdata* memB, uint8_t num) __reentrant;
void xMemCopy(void __xdata* dst, const void __xdata* src, uint16_t num) __reentrant;
uint16_t xStrLen(const char __xdata *str) __reentrant;

uint32_t rngGen(struct RngState __xdata *state) __naked;


#endif
