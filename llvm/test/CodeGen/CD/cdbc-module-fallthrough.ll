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

; DIRECT: main registers=1:
; DIRECT-NEXT:   r0 = constant c0
; DIRECT-NEXT:   print r0
; DIRECT-NOT: return

; MACHINE: main registers=1:
; MACHINE-NEXT:   r0 = constant c0
; MACHINE-NEXT:   print r0
; MACHINE-NOT: return
