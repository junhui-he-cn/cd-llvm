; RUN: llc -mtriple=cd-unknown-unknown -cd-backend=machine %s -o - | FileCheck %s

define double @main() {
entry:
  %extended = fpext float 1.5 to double
  ret double %extended
}

; CHECK: cdbc 0.2
; CHECK: constants:
; CHECK-NEXT:   c0 = number 1.5
; CHECK: main registers=2:
; CHECK-NEXT:   block b0:
; CHECK-NEXT:   r0 = constant c0
; CHECK-NEXT:   r1 = move r0
; CHECK-NEXT:   return r1
