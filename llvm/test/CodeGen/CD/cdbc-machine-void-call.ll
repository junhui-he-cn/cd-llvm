; RUN: llc -mtriple=cd-unknown-unknown -cd-backend=machine %s -o - | FileCheck %s

define void @side_effect() {
entry:
  ret void
}

define i32 @main() {
entry:
  call void @side_effect()
  ret i32 0
}

; CHECK: cdbc 0.1
; CHECK: constants:
; CHECK-NEXT:   c0 = number 0
; CHECK-NEXT:   c1 = nil
; CHECK: main registers=3:
; CHECK-NEXT:   r0 = make_function f0
; CHECK-NEXT:   r1 = call r0 []
; CHECK-NEXT:   r2 = constant c0
; CHECK-NEXT:   return r2
; CHECK: function f0 name="side_effect" arity=0 registers=1:
; CHECK-NEXT:   r0 = constant c1
; CHECK-NEXT:   return r0
