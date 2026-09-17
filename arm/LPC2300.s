;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
;;
;; Part one of the system initialization code,
;; contains low-level
;; initialization.
;;
;; Copyright 2007 IAR Systems. All rights reserved.
;;
;; $Revision: 34157 $
;;

        MODULE  ?cstartup

        ;; Forward declaration of sections.
        SECTION SVC_STACK:DATA:NOROOT(3)
        SECTION IRQ_STACK:DATA:NOROOT(3)
        SECTION FIQ_STACK:DATA:NOROOT(3)
        SECTION CSTACK:DATA:NOROOT(3)

;
; The module in this file are included in the libraries, and may be
; replaced by any user-defined modules that define the PUBLIC symbol
; __iar_program_start or a user defined start symbol.
;
; To override the cstartup defined in the library, simply add your
; modified version to the workbench project.

        SECTION .intvec:CODE:NOROOT(2)

        PUBLIC  __vector
        PUBLIC  __iar_program_start
        ;//EXTERN  Undef_Handler
        ;//EXTERN  SWI_Handler
        ;//EXTERN  Prefetch_Handler
        ;//EXTERN  Abort_Handler
        EXTERN  IRQ_Handler
        ;//EXTERN  FIQ_Handler
        ;//EXTERN  Reset_Handler
        EXTERN  VICADDRESS
        ARM

__iar_init$$done:               ; The vector table is not needed
                                ; until after copy initialization is done

__vector:
;        ; All default exception handlers (except reset) are
;        ; defined as weak symbol definitions.
;        ; If a handler is defined by the application it will take precedence.
;        LDR     PC,Reset_Addr           ; Reset
;        LDR     PC,Undef_Addr       ; Undefined instructions
;        LDR     PC,SWI_Addr             ; Software interrupt (SWI/SVC)
;        LDR     PC,Prefetch_Addr        ; Prefetch abort
;        LDR     PC,Abort_Addr           ; Data abort
;        DCD     0                       ; RESERVED
;        LDR     PC, VICADDRESS; [PC, #-0x0120]      ; Vector from VicVectAddr
;        LDR     PC,FIQ_Addr             ; FIQ


    ;#pragma location=0x00 ; //org 0x00
        LDR     PC, =__iar_program_start ;//Reset_Handler
    ;#pragma location=0x04 ; //org 0x04
        LDR     PC, =Undef_Handler
    ;#pragma location=0x08 ; //org 0x08
        LDR     PC, =SWI_Handler
    ;#pragma location=0x0C ; //org 0x0C
        LDR     PC, =Prefetch_Handler
    ;#pragma location=0x10 ; //org 0x10
        LDR     PC, =Abort_Handler
    ;#pragma location=0x18 ; //org 0x18
        DCD     0                       ; RESERVED
        ;LDR     PC, VICADDRESS ;[PC,#-0xFF0] ;[PC, #-0x0120]  ; Vector from VicVectAddr
        LDR     PC, [PC, #-0x0120]  ; Vector from VicVectAddr
    ;#pragma location=0x1C ; //org 0x1C
        LDR     PC, =FIQ_Handler
        DATA

Reset_Addr:     DCD   __iar_program_start
Undef_Addr:     DCD   Undef_Handler
SWI_Addr:       DCD   SWI_Handler
Prefetch_Addr:  DCD   Prefetch_Handler
Abort_Addr:     DCD   Abort_Handler
IRQ_Addr:       DCD   IRQ_Handler
FIQ_Addr:       DCD   FIQ_Handler


; --------------------------------------------------
; ?cstartup -- low-level system initialization code.
;
; After a reset execution starts here, the mode is ARM, supervisor
; with interrupts disabled.
;



        SECTION .text:CODE:NOROOT(2)

        EXTERN  ?main
        REQUIRE __vector
        EXTWEAK __iar_init_core
        EXTWEAK __iar_init_vfp

        //PUBLIC  Reset_Handler, Undef_Handler, SWI_Handler, Prefetch_Handler, Abort_Handler, FIQ_Handler

        ARM

Undef_Handler
SWI_Handler
Prefetch_Handler
Abort_Handler
FIQ_Handler
        B .

__iar_program_start:
?cstartup:

;
; Add initialization needed before setup of stackpointers here.
;

;
; Initialize the stack pointers.
; The pattern below can be used for any of the exception stacks:
; FIQ, IRQ, SVC, ABT, UND, SYS.
; The USR mode uses the same stack as SYS.
; The stack segments must be defined in the linker command file,
; and be declared above.
;


; --------------------
; Mode, correspords to bits 0-5 in CPSR

MODE_MSK DEFINE 0x1F            ; Bit mask for mode bits in CPSR

USR_MODE DEFINE 0x10            ; User mode
FIQ_MODE DEFINE 0x11            ; Fast Interrupt Request mode
IRQ_MODE DEFINE 0x12            ; Interrupt Request mode
SVC_MODE DEFINE 0x13            ; Supervisor mode
ABT_MODE DEFINE 0x17            ; Abort mode
UND_MODE DEFINE 0x1B            ; Undefined Instruction mode
SYS_MODE DEFINE 0x1F            ; System mode


        MRS     r0, cpsr                ; Original PSR value

        ;; Set up the svc stack pointer.

        BIC     r0, r0, #MODE_MSK       ; Clear the mode bits
        ORR     r0, r0, #SVC_MODE       ; Set SVC mode bits
        MSR     cpsr_c, r0              ; Change the mode
        LDR     sp, =SFE(SVC_STACK)     ; End of IRQ_STACK
        BIC     sp,sp,#0x7              ; Make sure SP is 8 aligned

        ;; Set up the interrupt stack pointer.

        BIC     r0, r0, #MODE_MSK       ; Clear the mode bits
        ORR     r0, r0, #IRQ_MODE       ; Set IRQ mode bits
        MSR     cpsr_c, r0              ; Change the mode
        LDR     sp, =SFE(IRQ_STACK)     ; End of IRQ_STACK
        BIC     sp,sp,#0x7              ; Make sure SP is 8 aligned

        ;; Set up the fast interrupt stack pointer.

        BIC     r0, r0, #MODE_MSK       ; Clear the mode bits
        ORR     r0, r0, #FIQ_MODE       ; Set FIR mode bits
        MSR     cpsr_c, r0              ; Change the mode
        LDR     sp, =SFE(FIQ_STACK)     ; End of FIQ_STACK
        BIC     sp,sp,#0x7              ; Make sure SP is 8 aligned

        ;; Set up the normal stack pointer.

        BIC     r0 ,r0, #MODE_MSK       ; Clear the mode bits
        ORR     r0 ,r0, #SYS_MODE       ; Set System mode bits
        MSR     cpsr_c, r0              ; Change the mode
        LDR     sp, =SFE(CSTACK)        ; End of CSTACK
        BIC     sp,sp,#0x7              ; Make sure SP is 8 aligned

        ;; Turn on core features assumed to be enabled.
        BL      __iar_init_core
	
        ;; Initialize VFP (if needed).
        BL      __iar_init_vfp

;;;
;;; Add more initialization here
;;;

;;; Continue to ?main for C-level initialization.

        B       ?main

        END
