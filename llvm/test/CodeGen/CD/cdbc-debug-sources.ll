; RUN: llc -mtriple=cd-unknown-unknown %s -o %t.direct
; RUN: FileCheck --check-prefix=DIRECT %s < %t.direct
; RUN: llc -mtriple=cd-unknown-unknown -cd-backend=machine %s -o %t.machine
; RUN: FileCheck --check-prefix=MACHINE %s < %t.machine

define i32 @main() {
entry:
  ret i32 0
}

!cd.sources = !{!0, !1}
!0 = !{!"demo.cd", !"print 1;\0A"}
!1 = !{!"/workspace/lib.cd", !"lib.cd", !"fun fail() { return 1 / 0; }\0A"}

; DIRECT: debug_sources:
; DIRECT: s0 path="demo.cd" text="print 1;\n"
; DIRECT: s1 module="/workspace/lib.cd" path="lib.cd" text="fun fail() { return 1 / 0; }\n"
; MACHINE: debug_sources:
; MACHINE: s0 path="demo.cd" text="print 1;\n"
; MACHINE: s1 module="/workspace/lib.cd" path="lib.cd" text="fun fail() { return 1 / 0; }\n"
