; RUN: llc -mtriple=cd-unknown-unknown -cd-backend=machine %s -o - | FileCheck %s

define i1 @main() {
entry:
  %less = icmp slt i32 1, 2
  ret i1 %less
}

; CHECK: cdbc 0.1
; CHECK: constants:
; CHECK-NEXT:   c0 = number 1
; CHECK-NEXT:   c1 = number 2
; CHECK: main registers=3:
; CHECK-NEXT:   r0 = constant c0
; CHECK-NEXT:   r1 = constant c1
; CHECK-NEXT:   r2 = less r0, r1
; CHECK-NEXT:   return r2
