//
// float.c
//
// Copyright (c) 2001 Michael Ringgaard. All rights reserved.
//
// Floatinf point support routines
//

#include "msvcrt.h"

__declspec(naked) void _ftol()
{
  __asm 
  {
    push        ebp
    mov         ebp,esp
    add         esp,0F4h
    wait
    fnstcw      word ptr [ebp-2]
    wait
    mov         ax,word ptr [ebp-2]
    or          ah,0Ch
    mov         word ptr [ebp-4],ax
    fldcw       word ptr [ebp-4]
    fistp       qword ptr [ebp-0Ch]
    fldcw       word ptr [ebp-2]
    mov         eax,dword ptr [ebp-0Ch]
    mov         edx,dword ptr [ebp-8]
    leave
    ret
  }
}

__declspec(naked) void _isnan()
{
  __asm 
  {
    mov         eax,dword ptr [esp+0Ah]
    mov         ecx,7FF8h
    and         eax,ecx
    cmp         ax,7FF0h
    jne         00000022
    test        dword ptr [esp+8],7FFFFh

    jne         _isnan1
    cmp         dword ptr [esp+4],0
    jne         00000027
    cmp         ax,cx
    jne         _isnan2
    _isnan1:
    push        1
    pop         eax
    ret
    _isnan2:
    xor         eax,eax
    ret
  }
}

__declspec(naked) double _copysign(double a, double b)
{
  __asm 
  {
    push        ebp
    mov         ebp,esp
    push        ecx
    push        ecx
    mov         eax,dword ptr [ebp+8]
    mov         dword ptr [ebp-8],eax
    mov         eax,dword ptr [ebp+14h]
    xor         eax,dword ptr [ebp+0Ch]
    and         eax,7FFFFFFFh
    xor         eax,dword ptr [ebp+14h]
    mov         dword ptr [ebp-4],eax
    fld         qword ptr [ebp-8]
    leave
    ret
  }
}

__declspec(naked) void _finite()
{
  __asm 
  {
    mov         eax,dword ptr [esp+0Ah]
    xor         ecx,ecx
    and         ax,7FF0h
    cmp         ax,7FF0h
    setne       cl
    mov         eax,ecx
    ret
  }
}

void _CIfmod()
{
  panic("_CIfmod not implemented");
}

unsigned int _control87(unsigned int new, unsigned int mask)
{
  syslog(LOG_WARNING, "_control87 not implemented, ignored\n");
  return 0;
}

unsigned int _controlfp(unsigned int new, unsigned int mask)
{
  syslog(LOG_WARNING, "_controlcp not implemented, ignored\n");
  return 0;
}

#if 0
__declspec(naked) unsigned int _control87(unsigned int new, unsigned int mask)
{
  __asm
  {
    push        ebp
    mov         ebp,esp
    push        ecx
    push        esi
    wait
    fnstcw      word ptr [ebp-4]
    push        dword ptr [ebp-4]
    call        _control87_1
  _control87_1:
    mov         esi,eax
    mov         eax,dword ptr [ebp+0Ch]
    not         eax
    and         esi,eax
    mov         eax,dword ptr [ebp+8]
    and         eax,dword ptr [ebp+0Ch]
    or          esi,eax
    push        esi
    call        _control87_2
  _control87_2:
    pop         ecx
    mov         dword ptr [ebp+0Ch],eax
    pop         ecx
    fldcw       word ptr [ebp+0Ch]
    mov         eax,esi
    pop         esi
    leave
    ret
  }
}
#endif
