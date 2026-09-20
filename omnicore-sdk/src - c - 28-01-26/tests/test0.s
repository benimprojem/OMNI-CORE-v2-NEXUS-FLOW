; OmniCore v2 Backend Output
; Target: x86_64, OS: none, Format: bin
; Base Address: 0x7C00

[BITS 64]
org 0x7C00
section .text
global _start

_start:
    ; --- NexusFlow IR Generated Code ---

section .text_main
global main
main:
    push rbp
    mov rbp, rsp
    ; TODO: Implement codegen for node type 2
    ; TODO: Implement codegen for node type 24
    ; TODO: Implement codegen for node type 2
    ; TODO: Implement codegen for node type 24
    ; TODO: Implement codegen for node type 2
    ; TODO: Implement codegen for node type 32
    ; TODO: Implement codegen for node type 24
    ; TODO: Implement codegen for node type 2
    ; TODO: Implement codegen for node type 17
    mov rsp, rbp
    pop rbp
    ret
    hlt ; Halt for baremetal safety
