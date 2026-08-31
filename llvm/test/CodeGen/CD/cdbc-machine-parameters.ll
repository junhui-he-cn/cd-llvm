; RUN: llc -mtriple=cd-unknown-unknown -cd-backend=machine %s -o - | FileCheck %s

define i32 @increment(i32 %input) {
entry:
  %value = add i32 %input, 1
  ret i32 %value
}

define i32 @main() {
entry:
  %value = call i32 @increment(i32 41)
  ret i32 %value
}

; CHECK: cdbc 0.2
; CHECK: constants:
; CHECK-NEXT:   c0 = number 41
; CHECK-NEXT:   c1 = number 1
; CHECK: names:
; CHECK-NEXT:   n0 = "input"
; CHECK: main registers=3:
; CHECK: make_function f0
; CHECK: constant c0
; CHECK: call r{{[0-9]+}} [r{{[0-9]+}}]
; CHECK: return r{{[0-9]+}}
; CHECK: function f0 name="increment" arity=1 registers=3:
; CHECK-NEXT:   param 0 = "input"
; CHECK-NEXT:   block b0:
; CHECK-NEXT:   r0 = load_local l0
; CHECK-NEXT:   r1 = constant c1
; CHECK-NEXT:   r2 = add r0, r1
; CHECK-NEXT:   return r2
