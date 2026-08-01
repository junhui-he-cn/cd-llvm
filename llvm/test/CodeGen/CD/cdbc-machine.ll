; RUN: llc -mtriple=cd-unknown-unknown -cd-backend=machine %s -o - | FileCheck %s

define i32 @main() {
entry:
  ret i32 42
}

; CHECK: cdbc 0.1
; CHECK: constants:
; CHECK-NEXT:   c0 = number 42
; CHECK: main registers=1:
; CHECK-NEXT:   r0 = constant c0
; CHECK-NEXT:   return r0
