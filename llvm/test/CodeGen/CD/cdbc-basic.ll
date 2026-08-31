; RUN: llc -mtriple=cd-unknown-unknown -O0 %s -o - | FileCheck %s
; RUN: not llc -mtriple=cd-unknown-unknown -filetype=obj %s -o - 2>&1 | FileCheck --check-prefix=NO-OBJECT %s

declare void @cd_print(double)

define double @add_one(double %value) {
entry:
  %result = fadd double %value, 1.0
  ret double %result
}

define i32 @main() {
entry:
  %value = call double @add_one(double 41.0)
  call void @cd_print(double %value)
  %condition = icmp eq i32 1, 1
  br i1 %condition, label %yes, label %no
yes:
  call void @cd_print(double 1.0)
  br label %done
no:
  call void @cd_print(double 0.0)
  br label %done
done:
  ret i32 0
}

; CHECK: cdbc 0.2
; CHECK: constants:
; CHECK: names:
; CHECK: main registers=
; CHECK: make_function f0
; CHECK: call
; CHECK: call_native i0
; CHECK: equal
; CHECK: br_if
; CHECK: br
; CHECK: function f0 name="add_one" arity=1 registers=
; CHECK: param 0 = "value"
; CHECK: add r{{[0-9]+}}, r{{[0-9]+}}
; CHECK: return

; NO-OBJECT: target does not support generation of this file type
