; RUN: llc -mtriple=cd-unknown-unknown -O0 %s -o %t.first
; RUN: llc -mtriple=cd-unknown-unknown -O0 %s -o %t.second
; RUN: cmp %t.first %t.second
; RUN: FileCheck %s < %t.first

declare void @cd_print(double)

define double @second(double %value) {
entry:
  %result = fmul double %value, 2.0
  ret double %result
}

define double @first(double %value) {
entry:
  %result = fadd double %value, 1.0
  ret double %result
}

define i32 @main() {
entry:
  %first_result = call double @first(double 4.0)
  %second_result = call double @second(double %first_result)
  call void @cd_print(double %second_result)
  ret i32 0
}

; CHECK: cdbc 0.1
; CHECK: constants:
; CHECK: names:
; CHECK: main registers=
; CHECK: make_function f1
; CHECK: make_function f0
; CHECK: function f0 name="second"
; CHECK: function f1 name="first"
