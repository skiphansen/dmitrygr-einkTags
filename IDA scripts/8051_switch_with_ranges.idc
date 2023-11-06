#include "idc.idc"


/*

code:0788
code:0788 ; (A = selector), data follows the call
code:0788 ; DATA:
code:0788 ;
code:0788 ;   u8 numRanges;
code:0788 ;   Range {
code:0788 ;     u8 min; //this range matches if min <= selector <= max
code:0788 ;     u8 max;
code:0788 ;     u16 addr;
code:0788 ;   }[numRanges];
code:0788 ;   u8 numMatches;
code:0788 ;   Match {
code:0788 ;     u8 val; //taken if selector is val
code:0788 ;     u16 addr;
code:0788 ;   } [numMatches];
code:0788 ;   u16 defaultAddr;
code:0788
code:0788 __switch_with_ranges:                   ; CODE XREF: code_1FDC+8p
code:0788                                         ; ROM_2632+12p ...
code:0788                 pop     DPH0            ; pop return address
code:078A                 pop     DPL0
code:078C                 xch     A, R4           ; stash selector into R4
code:078D                 push    ACC             ; push old value of R4
code:078F                 clr     A
code:0790                 movc    A, @A+DPTR      ; read byte after call
code:0791                 inc     DPTR
code:0792                 jz      no_more_ranges_left ; taken if the byte we read was ZERO
code:0794
code:0794 code_794:                               ; CODE XREF: __switch_with_ranges+10j
code:0794                 lcall   __switch_helper_ranges_1 ; (R4 = selector)
code:0797                 dec     A
code:0798                 jnz     code_794
code:079A
code:079A no_more_ranges_left:                    ; CODE XREF: __switch_with_ranges+Aj
code:079A                 clr     A
code:079B                 movc    A, @A+DPTR      ; read another counter?
code:079C                 inc     DPTR
code:079D                 jz      restore_pushed_r4_read_addr_from_DPTR_and_go_there
code:079F
code:079F code_79F:                               ; CODE XREF: __switch_with_ranges+1Bj
code:079F                 lcall   __switch_helper_ranges_2
code:07A2                 dec     A
code:07A3                 jnz     code_79F
code:07A5
code:07A5 restore_pushed_r4_read_addr_from_DPTR_and_go_there:
code:07A5                                         ; CODE XREF: __switch_with_ranges+15j
code:07A5                                         ; __switch_helper_ranges_1+16j ...
code:07A5                 pop     ACC             ; pop the pushed R4 above
code:07A7                 mov     R4, A           ; restore R4
code:07A8                 clr     A
code:07A9                 movc    A, @A+DPTR      ; pop address
code:07AA                 inc     DPTR
code:07AB                 push    ACC             ; Accumulator
code:07AD                 clr     A
code:07AE                 movc    A, @A+DPTR
code:07AF                 push    ACC             ; Accumulator
code:07B1                 ret                     ; and go
code:07B1 ; End of function __switch_with_ranges
code:07B1
code:07B2
code:07B2 ; =============== S U B R O U T I N E =======================================
code:07B2
code:07B2 ; (R4 = selector)
code:07B2
code:07B2 __switch_helper_ranges_1:               ; CODE XREF: __switch_with_ranges:code_794p
code:07B2                 push    ACC             ; Accumulator
code:07B4                 clr     A
code:07B5                 movc    A, @A+DPTR      ; read byte
code:07B6                 inc     DPTR
code:07B7                 setb    C               ; C = 1
code:07B8                 subb    A, R4           ; byte -= selector + 1
code:07B8                                         ; carry = byte < selector + 1 == byte <= selector
code:07B9                 jnc     _byte_gt_selector
code:07BB
code:07BB _byte_le_selector:
code:07BB                 clr     A
code:07BC                 movc    A, @A+DPTR      ; read next byte
code:07BD                 inc     DPTR
code:07BE                 clr     C
code:07BF                 subb    A, R4           ; carry = next byte < selector
code:07C0                 jc      _next_byte_lt_selector
code:07C2
code:07C2 _next_byte_ge_selector:                 ; byte[0] <= selector  <= byte[1]
code:07C2                 pop     ACC
code:07C4                 pop     ACC             ; Accumulator
code:07C6                 pop     ACC             ; pop our return address, and the byte we pushed above
code:07C8                 ljmp    restore_pushed_r4_read_addr_from_DPTR_and_go_there
code:07CB ; ---------------------------------------------------------------------------
code:07CB
code:07CB _byte_gt_selector:                      ; CODE XREF: __switch_helper_ranges_1+7j
code:07CB                 inc     DPTR
code:07CC
code:07CC _next_byte_lt_selector:                 ; CODE XREF: __switch_helper_ranges_1+Ej
code:07CC                 inc     DPTR
code:07CD                 inc     DPTR
code:07CE                 pop     ACC             ; Accumulator
code:07D0                 ret
code:07D0 ; End of function __switch_helper_ranges_1
code:07D0
code:07D1
code:07D1 ; =============== S U B R O U T I N E =======================================
code:07D1
code:07D1
code:07D1 __switch_helper_ranges_2:               ; CODE XREF: __switch_with_ranges:code_79Fp
code:07D1                 push    ACC             ; Accumulator
code:07D3                 clr     A
code:07D4                 movc    A, @A+DPTR      ; read byte
code:07D5                 inc     DPTR
code:07D6                 xrl     A, R4           ; is equal?
code:07D7                 jnz     code_7E2
code:07D9                 pop     ACC             ; Accumulator
code:07DB                 pop     ACC             ; pop and go
code:07DD                 pop     ACC             ; Accumulator
code:07DF                 ljmp    restore_pushed_r4_read_addr_from_DPTR_and_go_there
code:07E2 ; ---------------------------------------------------------------------------
code:07E2
code:07E2 code_7E2:                               ; CODE XREF: __switch_helper_ranges_2+6j
code:07E2                 inc     DPTR
code:07E3                 inc     DPTR
code:07E4                 pop     ACC             ; Accumulator
code:07E6                 ret
code:07E6 ; End of function __switch_helper_ranges_2

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
	auto switchCallAddr, at, i, num, val, min, max;
	
	
	switchCallAddr =  AskAddr(BADADDR, "Please enter an address of the switch call");
	if (switchCallAddr == BADADDR)
		return;
	
	at = switchCallAddr + 3;	//call instr size
	
	
	//ranges
	MakeByte(at);
	MakeComm(at, "num ranges");
	num = getByteAt(at);
	at++;
	for (i = 0; i < num; i++) {
	
		MakeByte(at + 0);
		MakeComm(at + 0, sprintf("range %u minimum", i));
		
		MakeByte(at + 1);
		MakeComm(at + 1, sprintf("range %u maximum", i));
		
		min = getByteAt(at + 0);
		max = getByteAt(at + 1);
		
		at = at + 2;
		
		at = switchMakeJumpAt(at, switchCallAddr, sprintf("Case 0x0%x (%u) .. 0x0%x (%u)", min, min, max, max),  sprintf("0x0%x_to_0x0%x", min, max));
		if (at == BADADDR)
			return;
	}
	
	//exact matches
	MakeByte(at);
	MakeComm(at, "num exact matches ranges");
	num = getByteAt(at);
	at++;
	
	for (i = 0; i < num; i++) {
	
		MakeByte(at + 0);
		MakeComm(at + 0, sprintf("match %u value", i));
		
		val = getByteAt(at + 0);
		
		at = at + 1;
		
		at = switchMakeJumpAt(at, switchCallAddr, sprintf("Case 0x0%x (%u)", val, val),  sprintf("0x0%x", val));
		if (at == BADADDR)
			return;
	}
	
	//default case
	at = switchMakeJumpAt(at, switchCallAddr, "Default case", "default");
	if (at == BADADDR)
		return;
}
