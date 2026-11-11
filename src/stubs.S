# 汇编写的小段代码 (改游戏代码的地方跳到这里)。Intel 写法。
.intel_syntax noprefix
.text

# ---- CBitmapFont::ParseFontFile 读到一个字 (char id=...) 以后 ----
# 原来: cmp r14d, 0xff ; ja 报错 ; cmp r14d, 0xa ; je ...
# 现在: 0x100 以上 (汉字等) 交给 cjk_add_glyph 存到我们自己的表里
.globl _cjk_parse_stub
_cjk_parse_stub:
    cmp     r14d, 0xff
    ja      1f
    cmp     r14d, 0xa
    jmp     qword ptr [rip + _cjk_parse_ret_small]
1:
    cmp     r14d, 0xffff
    ja      2f
    mov     rdi, qword ptr [rbp - 0xd10]     # 字体 (this)
    mov     esi, r14d                        # id
    mov     edx, dword ptr [rbp - 0xd4c]     # x
    mov     ecx, dword ptr [rbp - 0xd48]     # y
    mov     r8d, dword ptr [rbp - 0xd44]     # width
    mov     r9d, r12d                        # height
    mov     rax, rsp
    and     rsp, -16
    push    rax
    push    rbx                              # xadvance
    push    r13                              # yoffset
    push    r15                              # xoffset
    call    _cjk_add_glyph
    add     rsp, 24
    pop     rsp
2:
    jmp     qword ptr [rip + _cjk_parse_ret_loop]

# ---- 读字的地方: 认出中文模组的转义 (0x10~0x13 + 两个字节 = 一个汉字) ----
# 进来时 \s1 = 指向当前字的指针; 已经 push \s1, push \s2。
# base: 字体; dst: 原来放字形指针的寄存器; idx: 下标 (汉字要多加 2); ret: 跳回去的地址 (原来的 test 那条)
# seteax: 原来 eax 里是这个字 (就照样放字 / 汉字编号)
# layout: 排版用 (把汉字当成可以断行的地方: 用高度为 0 的副本字形)
# extra: 汉字时还要做的事 (把后两个字节也放进单词缓冲区)
.macro CJK_LOOKUP base, dst, idx, ret, s1, s1d, s2, s2d, seteax, layout, extra
    movzx   \s2d, byte ptr [\s1]
    sub     \s2d, 0x10
    cmp     \s2d, 3
    jbe     80f
    movzx   \s2d, byte ptr [\s1]
    .if \seteax
    mov     eax, \s2d
    .endif
    mov     \s2, qword ptr [\base + 8*\s2 + 0xe8]
    jmp     89f
80:
    push    \s2
    movzx   \s2d, word ptr [\s1 + 1]
    cmp     dword ptr [rsp], 1
    jne     81f
    sub     \s2d, 0x0E
    jmp     84f
81: cmp     dword ptr [rsp], 2
    jne     82f
    add     \s2d, 0x900
    jmp     84f
82: cmp     dword ptr [rsp], 3
    jne     84f
    add     \s2d, 0x8F2
84: add     rsp, 8
    and     \s2d, 0xFFFF
    cmp     \s2d, 0x98F
    ja      85f
    mov     \s2d, 0x2026
85: add     \idx, 2
    \extra
    .if \seteax
    mov     eax, \s2d
    .endif
    mov     \s1, qword ptr [\base + 0x10*8 + 0xe8]
    test    \s1, \s1
    jz      86f
    mov     \s2, qword ptr [\s1 + 8*\s2]
    .if \layout
    test    \s2, \s2
    jz      89f
    add     \s2, 0x100000
    .endif
    jmp     89f
86: xor     \s2d, \s2d
89: xchg    \s2, qword ptr [rsp]
    pop     \dst
    pop     \s1
    jmp     qword ptr [rip + \ret]
.endm

# 普通的一处: 原来是 movzx eax, byte ptr [rax] ; mov dst, [base+8*rax+0xe8]
.macro SITE_A name, base, dst, idx, layout, pre=
.globl _\name
_\name:
    \pre
    push    rsi
    push    rdi
    mov     rsi, rax
    CJK_LOOKUP \base, \dst, "\idx", _\name\()_ret, rsi, esi, rdi, edi, 1, \layout, " "
.globl _\name\()_ret
.data
_\name\()_ret: .quad 0
.text
.endm

SITE_A cjk_rtt2,   r14, r10, r12d, 0
SITE_A cjk_gh,     rbx, rax, r13d, 1
SITE_A cjk_fvb1,   r15, r13, r12d, 0
SITE_A cjk_fvb2,   r15, rax, ebx,  0
SITE_A cjk_gars,   rcx, r12, r13d, 1, "mov rcx, qword ptr [rbp - 0x1128]"
SITE_A cjk_grs,    r12, rbx, "dword ptr [rbp - 0xb4]", 1
SITE_A cjk_garrsa, r13, rbx, "dword ptr [rbp - 0xb4]", 1

# RenderToTexture 排版: 每个字先 "单词 += 这个字", 汉字的后两个字节也要放进去
.macro RTT1_EXTRA
    push    rsi
    push    rdi
    lea     rdi, [rbp - 0x2740]
    call    _cjk_append2
    pop     rdi
    pop     rsi
.endm
.globl _cjk_rtt1
_cjk_rtt1:
    push    rsi
    push    rdi
    mov     rsi, rax
    CJK_LOOKUP r15, rbx, r12d, _cjk_rtt1_ret, rsi, esi, rdi, edi, 1, 1, RTT1_EXTRA
.data
.globl _cjk_rtt1_ret
_cjk_rtt1_ret: .quad 0
.text

# RenderToScreen 排版: 字放在全局的 pWord[ebx], r12d = ebx+1; 汉字的后两个字节接着放
.macro RTS1_EXTRA
    push    rax
    push    rcx
    movsxd  rax, r12d
    cmp     eax, 0xF0
    jae     87f
    mov     cl, byte ptr [r8 + 1]
    mov     byte ptr [rsi + rax], cl
    mov     cl, byte ptr [r8 + 2]
    mov     byte ptr [rsi + rax + 1], cl
    add     r12d, 2
87: pop     rcx
    pop     rax
.endm
.globl _cjk_rts1
_cjk_rts1:
    mov     rcx, r14
    push    r8
    push    r9
    lea     r8, [rdi + r11]
    CJK_LOOKUP rcx, r14, r13d, _cjk_rts1_ret, r8, r8d, r9, r9d, 0, 1, RTS1_EXTRA
.data
.globl _cjk_rts1_ret
_cjk_rts1_ret: .quad 0
.text

# RenderToScreen 画字: 字在全局 pText[r12], 下标 r15d
.globl _cjk_rts2
_cjk_rts2:
    push    r8
    push    r9
    lea     r8, [rip + _cjk_ptext]
    mov     r8, qword ptr [r8]
    add     r8, r12
    CJK_LOOKUP r14, rbx, r15d, _cjk_rts2_ret, r8, r8d, r9, r9d, 1, 0, " "
.data
.globl _cjk_rts2_ret, _cjk_ptext
_cjk_rts2_ret: .quad 0
_cjk_ptext: .quad 0
.text

# GetWidthOfString: 字在 esi, 指针 = r14 + r12, 字体在 rdi
.globl _cjk_gw
_cjk_gw:
    push    r8
    push    r9
    lea     r8, [r14 + r12]
    CJK_LOOKUP rdi, rbx, r15d, _cjk_gw_ret, r8, r8d, r9, r9d, 0, 1, " "
.data
.globl _cjk_gw_ret
_cjk_gw_ret: .quad 0
.text
