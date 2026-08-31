; RUN: llc -mtriple=cd-unknown-unknown -cd-backend=machine %s -o - | FileCheck %s

define i32 @main() {
entry:
  %slot = alloca i32
  store i32 41, ptr %slot
  %value = load i32, ptr %slot
  ret i32 %value
}

; CHECK: cdbc 0.2
; CHECK: constants:
; CHECK-NEXT:   c0 = number 41
; CHECK: names:
; CHECK-NEXT:   n0 = "slot"
; CHECK: main registers=2:
; CHECK-NEXT:   block b0:
; CHECK-NEXT:   r0 = constant c0
; CHECK-NEXT:   init_global g0, r0
; CHECK-NEXT:   r1 = load_global g0
; CHECK-NEXT:   return r1
