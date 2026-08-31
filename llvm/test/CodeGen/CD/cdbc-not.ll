; RUN: llc -mtriple=cd-unknown-unknown -O0 %s -o - | FileCheck %s

declare void @cd_print(i1)

define i32 @main() {
entry:
  %value = icmp eq i32 0, 1
  %inverted = xor i1 %value, true
  call void @cd_print(i1 %inverted)
  ret i32 0
}

; CHECK: cdbc 0.2
; CHECK: constants:
; CHECK: c0 = number 1
; CHECK: c1 = number 0
; CHECK: main registers=
; CHECK: = equal r{{[0-9]+}}, r{{[0-9]+}}
; CHECK: = not r{{[0-9]+}}
; CHECK: r{{[0-9]+}} = call_native i0 [r{{[0-9]+}}]
