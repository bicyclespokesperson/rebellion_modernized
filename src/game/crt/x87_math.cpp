// The game's x87 math from the statically linked VC5 CRT, which the compiler calls with its arguments on the x87 stack
// (the /Oi "_CI" intrinsic convention) and which therefore cannot be written as ordinary C++ functions:
//
//   _CIpow   0x6197b0  ST1 ** ST0 -> ST0       (body 0x6197d2, helpers 0x6199a5 0x6209b0-0x620b3a, special values 0x620b40)
//   _CIacos  0x61a340  acos(ST0) -> ST0        (body 0x61a35d)
//   __ftol   0x619780  truncate ST0 -> EDX:EAX (the compiler's double-to-integer cast)
//
// The results depend on the exact instruction sequence (FYL2X/F2XM1/FSCALE at the caller's precision control: pow(10, 2)
// is 100.00000000000004 here, not 100), so these are transliterations of the original machine code in Intel syntax.
// Local labels are the original addresses (.L6197d2 = 0x6197d2). The constants are the original's .rdata values.
//
// Deliberately not reconstructed: error reporting. Where the original calls _matherr and sets errno (domain errors,
// overflow, underflow: _handle_math_error 0x620950/0x620967 -> __call_matherr 0x61fbb0), .L620967 below only returns the
// value the original returns when the default _matherr leaves it unchanged and restores the control word. Nothing in the
// game reads errno or installs _matherr, and the game never reaches those cases (volume sliders, planet distances).
//
// Callers: smacker_window.cpp and strategy_ui/game_options_window.cpp (`call _CIpow` in inline assembly); everything
// else in the original goes through the hooks.
asm(R"REBASM(
    .intel_syntax noprefix
    .section .rdata,"dr"
    .p2align 3
reb_x87_half:         .double 0.5
reb_x87_one:          .double 1.0
reb_x87_one2:         .double 1.0
reb_x87_two:          .double 2.0
reb_x87_dbl_max:      .quad 0x7fefffffffffffff
reb_x87_dbl_min:      .quad 0x0010000000000000
reb_x87_minus_1536:   .double -1536.0
reb_x87_plus_1536:    .double 1536.0
reb_x87_dbl_inf_scale: .quad 0x7ff0000000000000
reb_x87_zero_scale:   .quad 0
reb_x87_zero:         .quad 0
reb_x87_inf:          .quad 0x7ff0000000000000
reb_x87_qnan:         .quad 0xfff8000000000000
reb_x87_minus_zero:   .quad 0x8000000000000000
reb_x87_fastflag:     .long 0
    .p2align 4
reb_x87_ext_inf:      .quad 0x8000000000000000
                      .word 0x7fff
    .p2align 4
reb_x87_ext_qnan:     .quad 0xc000000000000000
                      .word 0xffff
reb_x87_name_pow:     .asciz "pow"
reb_x87_name_acos:    .asciz "acos"

    .text
    .p2align 4

    .globl _CIpow
    .def _CIpow; .scl 2; .type 32; .endef
_CIpow:
    sub esp, 0x10
    fxch st(1)
    fstp qword ptr [esp]
    fst qword ptr [esp+8]
    mov eax, dword ptr [esp+0xc]
    call .L6197d2
    add esp, 0x10
    ret
)REBASM");

// pow body (0x6197d2): classify both operands, FYL2X + exp2 helper, sign fix for odd integer exponents of negative bases
asm(R"REBASM(
.L6197d2:
    mov ecx, eax
    push eax
    fstcw word ptr [esp]
    cmp word ptr [esp], 0x27f
    jz .L6197e6
    call .L6209c5
.L6197e6:
    and ecx, 0x7ff00000
    lea edx, [esp+0x8]
    cmp ecx, 0x7ff00000
    jz .L619899
    call .L6209f5
    jz .L619895
    test eax, 0x7ff00000
    jz .L619908
.L619812:
    mov cl, byte ptr [esp+0xf]
    and cl, 0x80
    jnz .L619980
.L61981f:
    fyl2x
    call .L6209b0
    cmp cl, 0x1
    jnz .L61982d
    fchs
.L61982d:
    cmp dword ptr [reb_x87_fastflag], 0x0
    jnz .L620a4e
    lea ecx, [reb_x87_name_pow]
    mov edx, 0x1d
    jmp .L620a99
.L61984a:
    cmp dword ptr [reb_x87_fastflag], 0x0
    jnz .L620a4e
    lea ecx, [reb_x87_name_pow]
    mov edx, 0x1d
    call .L620950
    pop edx
    ret
.L619869:
    lea edx, [esp+0x8]
    call .L6209f5
    test byte ptr [esp+0x16], 0x8
    jnz .L61987c
    inc ecx
    jmp .L6198af
.L61987c:
    faddp
    mov eax, 0x1
    jmp .L61984a
.L619885:
    test byte ptr [esp+0xe], 0x8
    jnz .L61987c
.L61988c:
    faddp
    mov eax, 0x7
    jmp .L61984a
.L619895:
    xor ecx, ecx
    jmp .L6198af
.L619899:
    xor ecx, ecx
    and eax, 0xfffff
    or eax, dword ptr [esp+0x10]
    jnz .L619869
    lea edx, [esp+0x8]
    call .L6209f5
.L6198af:
    mov eax, dword ptr [esp+0xc]
    mov edx, eax
    and eax, 0x7ff00000
    and edx, 0xfffff
    cmp eax, 0x7ff00000
    jnz .L6198cd
    or edx, dword ptr [esp+0x8]
    jnz .L619885
.L6198cd:
    test ecx, ecx
    jnz .L61988c
    sub esp, 0x74
    mov ecx, esp
    push ecx
    sub esp, 0x10
    fstp qword ptr [esp]
    fstp qword ptr [esp+0x8]
    fsave [ecx+0x8]
    call .L620b40
    add esp, 0x10
    pop ecx
    frstor [ecx+0x8]
    fld qword ptr [ecx]
    add esp, 0x74
    test eax, eax
    jz .L620a4e
    mov eax, 0x1
    jmp .L61984a
.L619908:
    mov eax, dword ptr [esp+0xc]
    and eax, 0xfffff
    or eax, dword ptr [esp+0x8]
    jnz .L619812
    fstp st(0)
    mov eax, dword ptr [esp+0x14]
    test eax, 0x7ff00000
    jz .L619965
    call .L6199a5
    mov ch, byte ptr [esp+0xf]
    shr ch, 0x7
    test dword ptr [esp+0x17], 0x80
    jz .L619954
    fld tbyte ptr [reb_x87_ext_inf]
    test ch, cl
    jz .L61994a
    fchs
.L61994a:
    mov eax, 0x2
    jmp .L61984a
.L619954:
    fldz
    test ch, cl
    jz .L620a4e
    fchs
    jmp .L620a4e
.L619965:
    fstp st(0)
    and eax, 0xfffff
    or eax, dword ptr [esp+0x10]
    jnz .L619979
    fld1
    jmp .L620a4e
.L619979:
    fldz
    jmp .L620a4e
.L619980:
    fld st(1)
    call .L6199a5
    fchs
    test cl, cl
    jnz .L61981f
    fstp st(0)
    fstp st(0)
    fld tbyte ptr [reb_x87_ext_qnan]
    mov eax, 0x1
    jmp .L61984a
)REBASM");

// 0x6199a5: CL = 0 not an integer, 1 odd integer, 2 even integer (ST0 = the exponent, popped unless an integer)
asm(R"REBASM(
.L6199a5:
    fld st(0)
    frndint
    fcomp
    mov cl, 0x0
    fstsw ax
    sahf
    jnz .L6199ca
    fmul qword ptr [reb_x87_half]
    inc cl
    fld st(0)
    frndint
    fcompp
    fstsw ax
    sahf
    jnz .L6199c9
    inc cl
.L6199c9:
    ret
.L6199ca:
    fstp st(0)
    ret
)REBASM");

// 0x6209b0: 2 ** ST0 = FSCALE(1 + F2XM1(frac), int)
asm(R"REBASM(
.L6209b0:
    fld st(0)
    frndint
    fsubr st(1), st(0)
    fxch
    fchs
    f2xm1
    fld1
    faddp
    fscale
    fstp st(1)
    ret
)REBASM");

// 0x6209c5: control word := (cw & 0x300) | 0x7f (all exceptions masked, round to nearest, keep precision)
asm(R"REBASM(
.L6209c5:
    mov edx, dword ptr [esp+0x4]
    and edx, 0x300
    or edx, 0x7f
    mov word ptr [esp+0x6], dx
    fldcw word ptr [esp+0x6]
    ret
)REBASM");

// 0x6209dc: NaN operand of acos: a quiet NaN is returned as is (EAX = 7), a signalling one is quieted by an FADD (EAX = 1)
asm(R"REBASM(
.L6209dc:
    test eax, 0x80000
    jz .L6209e9
    mov eax, 0x7
    ret
.L6209e9:
    fadd qword ptr [reb_x87_one]
    mov eax, 0x1
    ret
)REBASM");

// 0x6209f5: ST0 = the double at [edx], NaN/infinity payloads widened to extended precision; ZF set for those
asm(R"REBASM(
.L6209f5:
    mov eax, dword ptr [edx+0x4]
    and eax, 0x7ff00000
    cmp eax, 0x7ff00000
    jz .L620a07
    fld qword ptr [edx]
    ret
.L620a07:
    mov eax, dword ptr [edx+0x4]
    sub esp, 0xa
    or eax, 0x7fff0000
    mov dword ptr [esp+0x6], eax
    mov eax, dword ptr [edx+0x4]
    mov ecx, dword ptr [edx]
    shld eax, ecx, 0xb
    shl ecx, 0xb
    mov dword ptr [esp+0x4], eax
    mov dword ptr [esp], ecx
    fld tbyte ptr [esp]
    add esp, 0xa
    test eax, 0x0
    mov eax, dword ptr [edx+0x4]
    ret
)REBASM");

// 0x620a38: EAX = exponent bits of the double on the caller's stack; ZF set when all ones (infinity/NaN)
asm(R"REBASM(
.L620a38:
    mov eax, dword ptr [esp+0x8]
    and eax, 0x7ff00000
    cmp eax, 0x7ff00000
    jz .L620a49
    ret
.L620a49:
    mov eax, dword ptr [esp+0x8]
    ret
)REBASM");

// 0x620a4e: return, restoring the caller's control word
asm(R"REBASM(
.L620a4e:
    cmp word ptr [esp], 0x27f
    jz .L620a59
    fldcw word ptr [esp]
.L620a59:
    pop edx
    ret
)REBASM");

// 0x620a5b: acos exit: restore the control word, raise an inexact result as an error only if it is unmasked
asm(R"REBASM(
.L620a5b:
    mov ax, word ptr [esp]
    cmp ax, 0x27f
    jz .L620a83
    and ax, 0x20
    jz .L620a80
    fstsw ax
    and ax, 0x20
    jz .L620a80
    mov eax, 0x8
    call .L620967
    pop edx
    ret
.L620a80:
    fldcw word ptr [esp]
.L620a83:
    pop edx
    ret
)REBASM");

// 0x620a99: pow result check: rescale overflow/underflow results (EAX = 3/4), restore the control word
asm(R"REBASM(
.L620a99:
    sub esp, 0x8
    fst qword ptr [esp]
    mov eax, dword ptr [esp+0x4]
    add esp, 0x8
    and eax, 0x7ff00000
    jz .L620aea
    cmp eax, 0x7ff00000
    jz .L620b13
    mov ax, word ptr [esp]
    cmp ax, 0x27f
    jz .L620ae8
    and ax, 0x20
    jnz .L620ae5
    fstsw ax
    and ax, 0x20
    jz .L620ae5
    mov eax, 0x8
.L620ad2:
    cmp edx, 0x1d
    jz .L620ade
    call .L620967
    pop edx
    ret
.L620ade:
    call .L620950
    pop edx
    ret
.L620ae5:
    fldcw word ptr [esp]
.L620ae8:
    pop edx
    ret
.L620aea:
    fld qword ptr [reb_x87_plus_1536]
    fxch
    fscale
    fstp st(1)
    fld st(0)
    fabs
    fcomp qword ptr [reb_x87_dbl_min]
    fstsw ax
    sahf
    mov eax, 0x4
    jnc .L620ad2
    fmul qword ptr [reb_x87_zero_scale]
    jmp .L620ad2
.L620b13:
    fld qword ptr [reb_x87_minus_1536]
    fxch
    fscale
    fstp st(1)
    fld st(0)
    fabs
    fcomp qword ptr [reb_x87_dbl_max]
    fstsw ax
    sahf
    mov eax, 0x3
    jbe .L620ad2
    fmul qword ptr [reb_x87_dbl_inf_scale]
    jmp .L620ad2
)REBASM");

// 0x620b40 pow special cases for infinite operands: stores the result through the pointer argument, returns an error code
asm(R"REBASM(
.L620b40:
    fld qword ptr [esp+0x4]
    fcomp qword ptr [reb_x87_zero]
    mov edx, dword ptr [esp+0x8]
    push esi
    mov esi, dword ptr [esp+0x8]
    push edi
    xor edi, edi
    fnstsw ax
    test ah, 0x1
    jz .L620b69
    fld qword ptr [esp+0xc]
    fchs
    fstp qword ptr [esp+0xc]
    jmp .L620b71
.L620b69:
    mov dword ptr [esp+0xc], esi
    mov dword ptr [esp+0x10], edx
.L620b71:
    mov eax, dword ptr [esp+0x18]
    mov ecx, dword ptr [esp+0x14]
    cmp eax, 0x7ff00000
    jnz .L620bce
    cmp ecx, edi
    jnz .L620bce
    fld qword ptr [esp+0xc]
    fcomp qword ptr [reb_x87_one2]
    fnstsw ax
    test ah, 0x41
    jnz .L620baf
    mov eax, dword ptr [esp+0x1c]
    mov ecx, dword ptr [reb_x87_inf]
    mov dword ptr [eax], ecx
    mov edx, dword ptr [reb_x87_inf+4]
    mov dword ptr [eax+0x4], edx
    mov eax, edi
    pop edi
    pop esi
    ret
.L620baf:
    fld qword ptr [esp+0xc]
    fcomp qword ptr [reb_x87_one2]
    fnstsw ax
    test ah, 0x1
    jz .L620c23
    mov eax, dword ptr [esp+0x1c]
    mov dword ptr [eax], edi
    mov dword ptr [eax+0x4], edi
    mov eax, edi
    pop edi
    pop esi
    ret
.L620bce:
    cmp eax, 0xfff00000
    jnz .L620c42
    cmp ecx, edi
    jnz .L620c42
    fld qword ptr [esp+0xc]
    fcomp qword ptr [reb_x87_one2]
    fnstsw ax
    test ah, 0x41
    jnz .L620bf8
    mov eax, dword ptr [esp+0x1c]
    mov dword ptr [eax], edi
    mov dword ptr [eax+0x4], edi
    mov eax, edi
    pop edi
    pop esi
    ret
.L620bf8:
    fld qword ptr [esp+0xc]
    fcomp qword ptr [reb_x87_one2]
    fnstsw ax
    test ah, 0x1
    jz .L620c23
    mov eax, dword ptr [esp+0x1c]
    mov ecx, dword ptr [reb_x87_inf]
    mov dword ptr [eax], ecx
    mov edx, dword ptr [reb_x87_inf+4]
    mov dword ptr [eax+0x4], edx
    mov eax, edi
    pop edi
    pop esi
    ret
.L620c23:
    mov eax, dword ptr [esp+0x1c]
    mov ecx, dword ptr [reb_x87_qnan]
    mov edi, 0x1
    mov dword ptr [eax], ecx
    mov edx, dword ptr [reb_x87_qnan+4]
    mov dword ptr [eax+0x4], edx
    mov eax, edi
    pop edi
    pop esi
    ret
.L620c42:
    cmp edx, 0x7ff00000
    jnz .L620c9c
    cmp esi, edi
    jnz .L620c9c
    fld qword ptr [esp+0x14]
    fcomp qword ptr [reb_x87_zero]
    fnstsw ax
    test ah, 0x41
    jnz .L620c79
    mov eax, dword ptr [esp+0x1c]
    mov ecx, dword ptr [reb_x87_inf]
    mov dword ptr [eax], ecx
    mov edx, dword ptr [reb_x87_inf+4]
    mov dword ptr [eax+0x4], edx
    mov eax, edi
    pop edi
    pop esi
    ret
.L620c79:
    fld qword ptr [esp+0x14]
    fcomp qword ptr [reb_x87_zero]
    fnstsw ax
    test ah, 0x1
    mov eax, dword ptr [esp+0x1c]
    mov dword ptr [eax], edi
    jz .L620d55
    mov dword ptr [eax+0x4], edi
    mov eax, edi
    pop edi
    pop esi
    ret
.L620c9c:
    cmp edx, 0xfff00000
    jnz .L620d5c
    cmp esi, edi
    jnz .L620d5c
    push eax
    push ecx
    call .L620d70
    fld qword ptr [esp+0x1c]
    fcomp qword ptr [reb_x87_zero]
    mov ecx, eax
    add esp, 0x8
    fnstsw ax
    test ah, 0x41
    jnz .L620d0e
    cmp ecx, 0x1
    jnz .L620cf4
    fld qword ptr [reb_x87_inf]
    mov eax, dword ptr [esp+0x1c]
    fchs
    fstp qword ptr [esp+0x14]
    mov ecx, dword ptr [esp+0x14]
    mov edx, dword ptr [esp+0x18]
    mov dword ptr [eax], ecx
    mov dword ptr [eax+0x4], edx
    mov eax, edi
    pop edi
    pop esi
    ret
.L620cf4:
    mov eax, dword ptr [esp+0x1c]
    mov ecx, dword ptr [reb_x87_inf]
    mov edx, dword ptr [reb_x87_inf+4]
    mov dword ptr [eax], ecx
    mov dword ptr [eax+0x4], edx
    mov eax, edi
    pop edi
    pop esi
    ret
.L620d0e:
    fld qword ptr [esp+0x14]
    fcomp qword ptr [reb_x87_zero]
    fnstsw ax
    test ah, 0x1
    jz .L620d4f
    cmp ecx, 0x1
    jnz .L620d3d
    mov edx, dword ptr [esp+0x1c]
    mov eax, [reb_x87_minus_zero]
    mov ecx, dword ptr [reb_x87_minus_zero+4]
    mov dword ptr [edx], eax
    mov dword ptr [edx+0x4], ecx
    mov eax, edi
    pop edi
    pop esi
    ret
.L620d3d:
    mov edx, dword ptr [esp+0x1c]
    xor eax, eax
    xor ecx, ecx
    mov dword ptr [edx], eax
    mov dword ptr [edx+0x4], ecx
    mov eax, edi
    pop edi
    pop esi
    ret
.L620d4f:
    mov eax, dword ptr [esp+0x1c]
    mov dword ptr [eax], edi
.L620d55:
    mov dword ptr [eax+0x4], 0x3ff00000
.L620d5c:
    mov eax, edi
    pop edi
    pop esi
    ret
)REBASM");

// 0x620d70: 0 not an integer, 1 odd, 2 even (infinite base, exponent in the arguments)
asm(R"REBASM(
.L620d70:
    sub esp, 0x8
    push esi
    mov esi, dword ptr [esp+0x14]
    push edi
    mov edi, dword ptr [esp+0x14]
    push esi
    push edi
    call .L626720
    add esp, 0x8
    test al, 0x90
    jnz .L620de9
    push esi
    push edi
    call .L61f410
    fstp qword ptr [esp+0x10]
    fld qword ptr [esp+0x10]
    fcomp qword ptr [esp+0x1c]
    add esp, 0x8
    fnstsw ax
    test ah, 0x40
    jz .L620de9
    fld qword ptr [esp+0x14]
    fdiv qword ptr [reb_x87_two]
    fstp qword ptr [esp+0x14]
    mov eax, dword ptr [esp+0x18]
    mov ecx, dword ptr [esp+0x14]
    push eax
    push ecx
    call .L61f410
    fcomp qword ptr [esp+0x1c]
    add esp, 0x8
    fnstsw ax
    test ah, 0x40
    jz .L620dde
    mov eax, 0x2
    pop edi
    pop esi
    add esp, 0x8
    ret
.L620dde:
    mov eax, 0x1
    pop edi
    pop esi
    add esp, 0x8
    ret
.L620de9:
    pop edi
    xor eax, eax
    pop esi
    add esp, 0x8
    ret
)REBASM");

// _fpclass 0x626720
asm(R"REBASM(
.L626720:
    mov eax, dword ptr [esp+0xa]
    mov ecx, eax
    and ecx, 0x7ff0
    cmp cx, 0x7ff0
    jnz .L626766
    mov eax, dword ptr [esp+0x8]
    mov ecx, dword ptr [esp+0x4]
    push eax
    push ecx
    call .L61fd40
    add esp, 0x8
    dec eax
    jz .L626760
    dec eax
    jz .L62675a
    dec eax
    jz .L626754
    mov eax, 0x1
    ret
.L626754:
    mov eax, 0x2
    ret
.L62675a:
    mov eax, 0x4
    ret
.L626760:
    mov eax, 0x200
    ret
.L626766:
    and eax, 0x8000
    test cx, cx
    mov edx, eax
    jnz .L626792
    test dword ptr [esp+0x8], 0xfffff
    jnz .L626784
    mov eax, dword ptr [esp+0x4]
    test eax, eax
    jz .L626792
.L626784:
    mov eax, edx
    neg eax
    sbb eax, eax
    and al, 0x90
    add eax, 0x80
    ret
.L626792:
    fld qword ptr [esp+0x4]
    fcomp qword ptr [reb_x87_zero]
    fnstsw ax
    test ah, 0x40
    mov eax, edx
    jz .L6267af
    neg eax
    sbb eax, eax
    and al, 0xe0
    add eax, 0x40
    ret
.L6267af:
    neg eax
    sbb eax, eax
    and al, 0x8
    add eax, 0x100
    ret
)REBASM");

// _sptype 0x61fd40: 1 +inf, 2 -inf, 3 quiet NaN, 4 signalling NaN, 0 other
asm(R"REBASM(
.L61fd40:
    mov edx, dword ptr [esp+0x8]
    mov ecx, dword ptr [esp+0x4]
    cmp edx, 0x7ff00000
    jnz .L61fd5a
    test ecx, ecx
    jnz .L61fd5a
    mov eax, 0x1
    ret
.L61fd5a:
    cmp edx, 0xfff00000
    jnz .L61fd6c
    test ecx, ecx
    jnz .L61fd6c
    mov eax, 0x2
    ret
.L61fd6c:
    mov eax, dword ptr [esp+0xa]
    and eax, 0x7ff8
    cmp ax, 0x7ff8
    jnz .L61fd81
    mov eax, 0x3
    ret
.L61fd81:
    cmp ax, 0x7ff0
    jnz .L61fd99
    test edx, 0x7ffff
    jnz .L61fd93
    test ecx, ecx
    jz .L61fd99
.L61fd93:
    mov eax, 0x4
    ret
.L61fd99:
    xor eax, eax
    ret
)REBASM");

// __frnd 0x61f410: round to integer in the current rounding mode
asm(R"REBASM(
.L61f410:
    push ebp
    mov ebp, esp
    sub esp, 0x8
    fld qword ptr [ebp+0x8]
    frndint
    fstp qword ptr [ebp-0x8]
    fld qword ptr [ebp-0x8]
    mov esp, ebp
    pop ebp
    ret

)REBASM");

// _CIacos 0x61a340
asm(R"REBASM(
    .globl _CIacos
    .def _CIacos; .scl 2; .type 32; .endef
_CIacos:
    sub esp, 8
    fst qword ptr [esp]
    call .L620a38
    call .L61a35d
    add esp, 8
    ret
)REBASM");

// acos body (0x61a35d): atan2(sqrt((1 + x)(1 - x)), x); |x| = 1 gives 0 or pi
asm(R"REBASM(
.L61a35d:
    push edx
    fstcw word ptr [esp]
    jz .L61a3d1
    cmp word ptr [esp], 0x27f
    jz .L61a371
    call .L6209c5
.L61a371:
    cmp eax, 0x3ff00000
    jnc .L61a3a5
    fld1
    fadd st(0), st(1)
    fld1
    fsub st(0), st(2)
    fmulp
    fsqrt
    fxch
    fpatan
.L61a388:
    cmp dword ptr [reb_x87_fastflag], 0x0
    jnz .L620a4e
    mov edx, 0xd
    lea ecx, [reb_x87_name_acos]
    jmp .L620a5b
.L61a3a5:
    ja .L61a3df
    mov eax, dword ptr [esp+0xc]
    mov ecx, eax
    and eax, 0xfffff
    or eax, dword ptr [esp+0x8]
    jnz .L61a3df
    and ecx, 0x80000000
    fstp st(0)
    jz .L61a3c6
    fldpi
    jmp .L61a388
.L61a3c6:
    fldz
    jmp .L61a388
.L61a3ca:
    call .L6209dc
    jmp .L61a3ec
.L61a3d1:
    test eax, 0xfffff
    jnz .L61a3ca
    cmp dword ptr [esp+0x8], 0x0
    jnz .L61a3ca
.L61a3df:
    fstp st(0)
    fld tbyte ptr [reb_x87_ext_qnan]
    mov eax, 0x1
.L61a3ec:
    cmp dword ptr [reb_x87_fastflag], 0x0
    jnz .L620a4e
    mov edx, 0xd
    lea ecx, [reb_x87_name_acos]
    call .L620967
    pop edx
    ret

)REBASM");

// simplified _handle_math_error (see the header): round ST0 to double, restore the control word, return it
asm(R"REBASM(
.L620950:
.L620967:
    push ebp
    mov ebp, esp
    sub esp, 0x20
    fstp qword ptr [ebp-8]
    fld qword ptr [ebp-8]
    cmp word ptr [ebp+8], 0x27f
    jz .L620967_done
    fldcw word ptr [ebp+8]
.L620967_done:
    leave
    ret

)REBASM");

// __ftol 0x619780: FISTP with the rounding forced to truncation, control word restored
asm(R"REBASM(
    .globl __ftol
    .def __ftol; .scl 2; .type 32; .endef
__ftol:
    push ebp
    mov ebp, esp
    add esp, -0xc
    fstcw word ptr [ebp-2]
    fwait
    mov ax, word ptr [ebp-2]
    or ah, 0xc
    mov word ptr [ebp-4], ax
    fldcw word ptr [ebp-4]
    fistp qword ptr [ebp-0xc]
    fldcw word ptr [ebp-2]
    mov eax, dword ptr [ebp-0xc]
    mov edx, dword ptr [ebp-8]
    leave
    ret
    .att_syntax
)REBASM");
