; RUN: llc -mtriple=cd-unknown-unknown -cd-backend=machine %s -o - | FileCheck %s

define i32 @answer() {
entry:
  ret i32 42
}

define i32 @main() {
entry:
  %value = call i32 @answer()
  ret i32 %value
}

; CHECK: cdbc 0.1
; CHECK: constants:
; CHECK-NEXT:   c0 = number 42
; CHECK: main registers=2:
; CHECK-NEXT:   r0 = make_function f0
; CHECK-NEXT:   r1 = call r0 []
; CHECK-NEXT:   return r1
; CHECK: function f0 name="answer" arity=0 registers=1:
; CHECK-NEXT:   r0 = constant c0
; CHECK-NEXT:   return r0
