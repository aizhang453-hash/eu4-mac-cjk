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
# base: 字体; dst: 原来放字形指针的寄存器; idx: 下标 (汉字要多加 2; 写 none 就不加, 由循环结尾的 SKIP_AT_END 来加); ret: 跳回去的地址 (原来的 test 那条)
# seteax: 原来 eax 里是这个字 (就照样放字 / 汉字编号)
# layout: 排版用 (把汉字当成可以断行的地方: 用高度为 0 的副本字形)
# extra: 汉字时还要做的事 (把后两个字节也放进单词缓冲区)
# off: 字形表在 base 里的位置 (CBitmapFont 是 0xe8; 直接给 CBitmapCharacterSet 的是 0)
.macro CJK_LOOKUP base, dst, idx, ret, s1, s1d, s2, s2d, seteax, layout, extra, off=0xe8
    movzx   \s2d, byte ptr [\s1]
    sub     \s2d, 0x10
    cmp     \s2d, 3
    jbe     80f
    movzx   \s2d, byte ptr [\s1]
    .if \seteax
    mov     eax, \s2d
    .endif
    mov     \s2, qword ptr [\base + 8*\s2 + \off]
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
85:
    .ifnc \idx,none
    add     \idx, 2
    .endif
    \extra
    .if \seteax
    mov     eax, \s2d
    .endif
    mov     \s1, qword ptr [\base + 0x10*8 + \off]
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
.macro SITE_A name, base, dst, idx, layout, pre=, off=0xe8
.globl _\name
_\name:
    \pre
    push    rsi
    push    rdi
    mov     rsi, rax
    CJK_LOOKUP \base, \dst, "\idx", _\name\()_ret, rsi, esi, rdi, edi, 1, \layout, " ", \off
.globl _\name\()_ret
.data
_\name\()_ret: .quad 0
.text
.endm

SITE_A cjk_rtt2,   r14, r10, r12d, 0
# 量大小的函数用真的字形 (不能用高度 0 的副本: 地图名字会量成高度 0 不画;
# 换行时游戏会把「空格」那个字节换成换行符, 用副本就会把汉字的一个字节换掉, 提示框里出怪字)
SITE_A cjk_gh,     rbx, rax, r13d, 0
SITE_A cjk_fvb2,   r15, rax, ebx,  0
SITE_A cjk_gars,   rcx, r12, r13d, 0, "mov rcx, qword ptr [rbp - 0x1128]"
# 这两个会把「每个字从第几个字节开始」记进一张表 (换行时从表里挑地方断开), 所以读字时下标不能先加 2,
# 要等这个字处理完, 在循环结尾 (SKIP_AT_END) 再跳过汉字的后两个字节; 不然断行会断在汉字中间, 出怪字
SITE_A cjk_grs,    r12, rbx, none, 0
SITE_A cjk_garrsa, r13, rbx, none, 0
# 地图上的名字: 数有几个看得见的字 (两处), 沿着弯曲排字 (CurveText, 直接给的字形表)
SITE_A cjk_ana,    r14, rax, ebx,  0
SITE_A cjk_ann,    r14, rax, r13d, 0
SITE_A cjk_curve,  r12, r15, r14d, 0, " ", 0

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
    CJK_LOOKUP rdi, rbx, r15d, _cjk_gw_ret, r8, r8d, r9, r9d, 0, 0, " "
.data
.globl _cjk_gw_ret
_cjk_gw_ret: .quad 0
.text

# Render3d 排版: 跟 RenderToTexture 一样, 单词缓冲区在 [rbp-0x128]; rcx (字体) 后面还要用, 调函数前存起来
.macro R3D1_EXTRA
    push    rsi
    push    rdi
    push    rcx
    push    rax
    lea     rdi, [rbp - 0x128]
    call    _cjk_append2
    pop     rax
    pop     rcx
    pop     rdi
    pop     rsi
.endm
.globl _cjk_r3d1
_cjk_r3d1:
    mov     rcx, qword ptr [rbp - 0xf0]
    push    rsi
    push    rdi
    mov     rsi, rax
    CJK_LOOKUP rcx, rbx, r15d, _cjk_r3d1_ret, rsi, esi, rdi, edi, 1, 1, R3D1_EXTRA
.data
.globl _cjk_r3d1_ret
_cjk_r3d1_ret: .quad 0
.text

SITE_A cjk_r3d2, r15, rax, r12d, 0

# 地图名字插空格: 整段换成 cjk_spaced(&结果[rbp-0x128], &名字[rbp-0x90], &空格[rbp-0x220])
.globl _cjk_ana_space
_cjk_ana_space:
    mov     dword ptr [rbp - 0x70], r14d
    lea     rdi, [rbp - 0x128]
    lea     rsi, [rbp - 0x90]
    lea     rdx, [rbp - 0x220]
    call    _cjk_spaced
    lea     rbx, [rbp - 0x128]
    jmp     qword ptr [rip + _cjk_ana_space_ret]
.data
.globl _cjk_ana_space_ret
_cjk_ana_space_ret: .quad 0
.text

# ---- 调试用的探针: 在游戏代码某处停一下, 把所有寄存器交给 cjk_spy(编号, 寄存器) 记日志, 再接着跑原来的指令 ----
# 寄存器在内存里的顺序: r15 r14 r13 r12 r11 r10 r9 r8 rdi rsi rbp rbx rdx rcx rax 标志
.macro SPY id
.globl _cjk_spy_\id
_cjk_spy_\id:
    lea     rsp, [rsp - 128]
    pushfq
    push    rax
    push    rcx
    push    rdx
    push    rbx
    push    rbp
    push    rsi
    push    rdi
    push    r8
    push    r9
    push    r10
    push    r11
    push    r12
    push    r13
    push    r14
    push    r15
    mov     edi, \id
    mov     rsi, rsp
    mov     rbx, rsp
    and     rsp, -16
    call    _cjk_spy
    mov     rsp, rbx
    pop     r15
    pop     r14
    pop     r13
    pop     r12
    pop     r11
    pop     r10
    pop     r9
    pop     r8
    pop     rdi
    pop     rsi
    pop     rbp
    pop     rbx
    pop     rdx
    pop     rcx
    pop     rax
    popfq
    lea     rsp, [rsp + 128]
    jmp     qword ptr [rip + _cjk_spy_\id\()_cont]
.data
.globl _cjk_spy_\id\()_cont
_cjk_spy_\id\()_cont: .quad 0
.text
.endm
SPY 0
SPY 1
SPY 2
SPY 3
SPY 4
SPY 5

# FillVertexBuffer 第一遍: 每个字先 "单词[rbp-0x1000] += 这个字", 汉字的后两个字节也要放进去
# (第二遍是照拼好的整行 [rbp-0x1048] 生成方块; 少了这两个字节, 地图上的名字只剩一两个字)
.macro FVB1_EXTRA
    push    rsi
    push    rdi
    lea     rdi, [rbp - 0x1000]
    call    _cjk_append2
    pop     rdi
    pop     rsi
.endm
.globl _cjk_fvb1
_cjk_fvb1:
    push    rsi
    push    rdi
    mov     rsi, rax
    CJK_LOOKUP r15, r13, r12d, _cjk_fvb1_ret, rsi, esi, rdi, edi, 1, 0, FVB1_EXTRA
.data
.globl _cjk_fvb1_ret
_cjk_fvb1_ret: .quad 0
.text

# 循环结尾: 原来是 mov ebx, [rbp-0xb4] ; inc ebx。当前这个字是汉字的话多加 2 (cstr: 字符串 CString 在哪个寄存器)
.macro SKIP_AT_END name, cstr
.globl _\name
_\name:
    mov     ebx, dword ptr [rbp - 0xb4]
    push    rax
    mov     rax, \cstr
    test    byte ptr [rax], 1
    jz      1f
    mov     rax, qword ptr [rax + 0x10]
    jmp     2f
1:  inc     rax
2:  movzx   eax, byte ptr [rax + rbx]
    sub     eax, 0x10
    cmp     eax, 3
    ja      3f
    add     ebx, 2
3:  pop     rax
    inc     ebx
    jmp     qword ptr [rip + _\name\()_ret]
.data
.globl _\name\()_ret
_\name\()_ret: .quad 0
.text
.endm
SKIP_AT_END cjk_grs_end, r15
SKIP_AT_END cjk_garrsa_end, r12

# ---- 收到输入法 (SDL_TEXTINPUT) 的文字: 交给 cjk_textin (input.c), 然后去处理下一个事件 ----
# 原来 0x101407de1 开始: 只取第一个字。r15 = 键盘, [rbp-0x70] = 事件处理, 文字在 [rbp-0x5c]
.globl _cjk_textin_stub
_cjk_textin_stub:
    lea     rdi, [rbp - 0x5c]
    mov     rsi, r15
    mov     edx, dword ptr [rbp - 0x54]
    mov     rcx, qword ptr [rbp - 0x70]
    call    _cjk_textin
    jmp     qword ptr [rip + _cjk_textin_ret]
