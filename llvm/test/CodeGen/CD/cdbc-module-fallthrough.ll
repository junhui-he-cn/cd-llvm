; RUN: llc -mtriple=cd-unknown-unknown -cd-artifact=module %s -o %t.direct
; RUN: FileCheck --check-prefix=DIRECT %s < %t.direct
; RUN: llc -mtriple=cd-unknown-unknown -cd-backend=machine -cd-artifact=module %s -o %t.machine
; RUN: FileCheck --check-prefix=MACHINE %s < %t.machine

declare void @cd_print(double)

define i32 @main() {
entry:
  call void @cd_print(double 2.0)
  ret i32 0
}

!cd.module = !{!0}
!0 = !{!"/workspace/dependency.cd", !"dependency.cd", !"/workspace/dependency.cd", i1 false}

; DIRECT: main registers=0:
; DIRECT-NEXT: block b0:
; DIRECT-NEXT:   return_nil
; DIRECT: function f0 name="__module_init" arity=0 registers=2:
; DIRECT-NEXT: block b0:
; DIRECT-NEXT:   r0 = constant c0
; DIRECT-NEXT:   r1 = call_native i0 [r0]
; DIRECT-NEXT:   return_nil

; MACHINE: main registers=0:
; MACHINE-NEXT: block b0:
; MACHINE-NEXT:   return_nil
; MACHINE: function f0 name="__module_init" arity=0 registers=2:
; MACHINE-NEXT: block b0:
; MACHINE-NEXT:   r0 = constant c0
; MACHINE-NEXT:   r1 = call_native i0 [r0]
; MACHINE-NEXT:   return_nil
