; RUN: llc -mtriple=cd-unknown-unknown -cd-backend=machine %s -o - | FileCheck %s

define void @main() {
entry:
  ret void
}

; CHECK: cdbc 0.2
; CHECK: constants:
; CHECK-NEXT:   c0 = nil
; CHECK: main registers=1:
; CHECK-NEXT:   block b0:
; CHECK-NEXT:   r0 = constant c0
; CHECK-NEXT:   return r0
