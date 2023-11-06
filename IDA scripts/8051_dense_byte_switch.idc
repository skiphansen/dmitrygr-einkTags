#include "idc.idc"


/*

		code:0752 __dense_byte_switch:                    ; CODE XREF: ROM_205B:ROM_20FCp
		code:0752                                         ; ROM:2A00p ...
		code:0752                 pop     DPH0            ; pop return address
		code:0754                 pop     DPL0
		code:0756                 xch     A, B            ; B Register
		code:0758                 clr     A
		code:0759                 movc    A, @A+DPTR      ; read byte from return address
		code:075A                 inc     DPTR            ; and increment
		code:075B                 xch     A, B            ; B Register
		code:075D                 clr     C               ; B is now first byte after call, A is our selector
		code:075E                 subb    A, B            ; selector minus first byte
		code:0760                 mov     B, A            ; B Register
		code:0762                 clr     A
		code:0763                 movc    A, @A+DPTR      ; next byte after that one in code
		code:0764                 inc     DPTR            ; increment
		code:0765                 clr     C
		code:0766                 subb    A, B            ; selectpr minus first byte minus second byte
		code:0768                 jc      out_of_range_use_selector_0 ; taken if selector is not between first and second value
		code:076A
		code:076A in_range_now_restore_selector:          ; B Register
		code:076A                 mov     A, B
		code:076C                 inc     A
		code:076D                 sjmp    jump_to_entry_index_A
		code:076F ; ---------------------------------------------------------------------------
		code:076F
		code:076F out_of_range_use_selector_0:            ; CODE XREF: __dense_byte_switch+16j
		code:076F                 clr     A
		code:0770
		code:0770 jump_to_entry_index_A:                  ; CODE XREF: __dense_byte_switch+1Bj
		code:0770                 mov     B, #2           ; each index is 2 bytes, it is an absolute address
		code:0773                 mul     AB
		code:0774                 add     A, DPL0
		code:0776                 mov     DPL0, A
		code:0778                 mov     A, B            ; B Register
		code:077A                 addc    A, DPH0
		code:077C                 mov     DPH0, A
		code:077E                 clr     A
		code:077F                 movc    A, @A+DPTR
		code:0780                 inc     DPTR
		code:0781                 push    ACC             ; Accumulator
		code:0783                 clr     A
		code:0784                 movc    A, @A+DPTR
		code:0785                 push    ACC             ; Accumulator
		code:0787                 ret
		code:0787 ; End of function __dense_byte_switch

*/

static getByteAt(addr)
{
	return GetOriginalByte(addr) & 0xff;
}

static getHalfWord(addr)
{
	
	auto a, b;
	
	a = GetOriginalByte(addr + 0) & 0xFF;
	b = GetOriginalByte(addr + 1) & 0xFF;
	
	return (a) + (b << 8);
}

static switchMakeCaseLabel(at, switchStart, isLong, label)
{
	MakeComm(at, label);
	
	if (isLong) {
		MakeDword(at);
		at = at + 4;
	}
	else {
		MakeWord(at);
		at = at + 2;
	}
}

static switchMakeJumpAt(at, switchStart, labelName, labelDstName)
{
	auto to;
	
	to = getHalfWord(at);
	
	MakeByte(at + 0);
	MakeByte(at + 1);
	MakeComm(at, sprintf("%s for switch at %a -> %a", labelName, switchStart, to));
	MakeNameEx(to, sprintf("switch_%a_case_%s", switchStart, labelDstName), SN_AUTO);
	AddCodeXref(switchStart, to, fl_JN);
	AddCodeXref(at, to, fl_JN);
	add_dref(switchStart, to, dr_O); 
	add_dref(at, to, dr_O); 
	
	return at + 2;
}

static main()
{
	auto switchCallAddr, dataStart, i, min, num, at, caseVal;
	
	
	switchCallAddr =  AskAddr(BADADDR, "Please enter an address of the switch call");
	if (switchCallAddr == BADADDR)
		return;
	
	dataStart = switchCallAddr + 3;	//call instr size
	min = getByteAt(dataStart + 0);
	MakeComm(dataStart + 0, "min value");
	num = 1 + getByteAt(dataStart + 1);
	MakeComm(dataStart + 1, "num cases minus one");
	at = dataStart + 2;
	
	at = switchMakeJumpAt(at, switchCallAddr, "Default case", "default");
	if (at == BADADDR)
		return;
	
	for (i = 0; i < num; i++) {
		
		caseVal = min + i;
		caseVal = caseVal & 0xff;
		
		at = switchMakeJumpAt(at, switchCallAddr, sprintf("Case 0x0%x (%u)", caseVal, caseVal),  sprintf("0x0%x", caseVal));
		if (at == BADADDR)
			return;
	}
}
