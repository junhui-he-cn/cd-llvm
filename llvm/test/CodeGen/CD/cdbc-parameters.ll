; RUN: llc -mtriple=cd-unknown-unknown -O0 %s -o - | FileCheck %s

define i32 @parameter_names(i32 %arg0, i32) {
entry:
  %value = add i32 %arg0, 0
  ret i32 %value
}

define i32 @main() {
entry:
  %value = call i32 @parameter_names(i32 7, i32 8)
  ret i32 %value
}

; CHECK: cdbc 0.2
; CHECK: function f0 name="parameter_names" arity=2 registers=
; CHECK: param 0 = "arg0"
; CHECK: param 1 = "arg0#1"
; CHECK: r0 = load_local l0
; CHECK: r1 = load_local l1
