;-----------------------------------------------------------------------------
; chkstk.asm - check stack upon procedure entry
;-----------------------------------------------------------------------------
                .386
_TEXT           segment use32 para public 'CODE'
                public  __chkstk
		public __alloca_probe
         
PAGESIZE        equ     4096

__chkstk        proc    near
                assume  cs:_TEXT

__alloca_probe  =  __chkstk

		cmp     eax, PAGESIZE           ; more than one page?
		jae     short probesetup        ;   yes, go setup probe loop
						;   no
		neg     eax                     ; compute new stack pointer in eax
		add     eax,esp
		add     eax,4
		test    dword ptr [eax],eax     ; probe it
		xchg    eax,esp
		mov     eax,dword ptr [eax]
		push    eax
		ret

probesetup:
		push    ecx                     ; save ecx
		lea     ecx,[esp] + 8           ; compute new stack pointer in ecx
			                        ; correct for return address and
				                ; saved ecx

probepages:
		sub     ecx,PAGESIZE            ; yes, move down a page
		sub     eax,PAGESIZE            ; adjust request and...

		test    dword ptr [ecx],eax     ; ...probe it

		cmp     eax,PAGESIZE            ; more than one page requested?
		jae     short probepages        ; no

lastpage:
		sub     ecx,eax                 ; move stack down by eax
		mov     eax,esp                 ; save current tos and do a...

		test    dword ptr [ecx],eax     ; ...probe in case a page was crossed

		mov     esp,ecx                 ; set the new stack pointer

		mov     ecx,dword ptr [eax]     ; recover ecx
		mov     eax,dword ptr [eax + 4] ; recover return address

		push    eax                     ; prepare return address
			                        ; ...probe in case a page was crossed
		ret

__chkstk        endp

_TEXT           ends
                end
