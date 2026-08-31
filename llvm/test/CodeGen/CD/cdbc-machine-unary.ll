; RUN: llc -mtriple=cd-unknown-unknown -cd-backend=machine %s -o - | FileCheck %s

define double @main() {
entry:
  %negated = fneg double 3.5
  ret double %negated
}

; CHECK: cdbc 0.2
; CHECK: constants:
; CHECK-NEXT:   c0 = number 3.5
; CHECK: main registers=2:
; CHECK-NEXT:   block b0:
; CHECK-NEXT:   r0 = constant c0
; CHECK-NEXT:   r1 = negate r0
; CHECK-NEXT:   return r1
