; RUN: llc -mtriple=cd-unknown-unknown -O0 %s -o - | FileCheck %s

declare void @cd_print(double)

define i32 @main() {
entry:
  %negative = fneg double 2.5
  call void @cd_print(double %negative)
  ret i32 0
}

; CHECK: cdbc 0.1
; CHECK: constants:
; CHECK: c0 = number 2.5
; CHECK: main registers=
; CHECK: = constant c0
; CHECK: = negate r{{[0-9]+}}
; CHECK: print r{{[0-9]+}}
