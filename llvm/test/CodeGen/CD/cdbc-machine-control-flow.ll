; RUN: llc -mtriple=cd-unknown-unknown -cd-backend=machine %s -o %t
; RUN: FileCheck %s < %t
; RUN: llc -mtriple=cd-unknown-unknown %s -o %t.direct
; RUN: FileCheck --check-prefix=DIRECT %s < %t.direct
; RUN: not llc -mtriple=cd-unknown-unknown -cd-backend=machine -filetype=obj %s -o - 2>&1 | FileCheck --check-prefix=NO-OBJECT %s

declare void @cd_print(i32)

define i32 @choose(i1 %condition) {
entry:
  br i1 %condition, label %yes, label %no

yes:
  br label %merge

no:
  br label %merge

merge:
  %value = phi i32 [ 11, %yes ], [ 22, %no ]
  ret i32 %value
}

define i32 @sum_to(i32 %limit) {
entry:
  br label %loop

loop:
  %index = phi i32 [ 0, %entry ], [ %next, %body ]
  %total = phi i32 [ 0, %entry ], [ %next_total, %body ]
  %condition = icmp sle i32 %index, %limit
  br i1 %condition, label %body, label %exit

body:
  %next = add i32 %index, 1
  %next_total = add i32 %total, %index
  br label %loop

exit:
  ret i32 %total
}

define i32 @critical(i1 %condition) {
entry:
  br label %dispatch

dispatch:
  br i1 %condition, label %merge, label %other

other:
  br label %merge

merge:
  %value = phi i32 [ 3, %dispatch ], [ 4, %other ]
  ret i32 %value
}

define i32 @same_constant(i1 %condition) {
entry:
  br i1 %condition, label %yes, label %no

yes:
  br label %merge

no:
  br label %merge

merge:
  %value = phi i32 [ 7, %yes ], [ 7, %no ]
  ret i32 %value
}

define i32 @main() {
entry:
  %first = call i32 @choose(i1 true)
  call void @cd_print(i32 %first)
  %second = call i32 @choose(i1 false)
  call void @cd_print(i32 %second)
  %sum = call i32 @sum_to(i32 4)
  call void @cd_print(i32 %sum)
  %critical_true = call i32 @critical(i1 true)
  call void @cd_print(i32 %critical_true)
  %critical_false = call i32 @critical(i1 false)
  call void @cd_print(i32 %critical_false)
  %same_true = call i32 @same_constant(i1 true)
  call void @cd_print(i32 %same_true)
  %same_false = call i32 @same_constant(i1 false)
  call void @cd_print(i32 %same_false)
  ret i32 0
}

; CHECK: cdbc 0.2
; CHECK: main registers=
; CHECK: call_native i0
; CHECK: function f0 name="choose" arity=1 registers=
; CHECK: br_if
; CHECK: {{(bind_local|set_local|init_global|set_global)}}
; CHECK: br
; CHECK: function f1 name="sum_to" arity=1 registers=
; CHECK: br_if
; CHECK: {{(bind_local|set_local|init_global|set_global)}}
; CHECK: br
; CHECK: function f2 name="critical" arity=1 registers=
; CHECK: br_if
; CHECK: {{(bind_local|set_local|init_global|set_global)}}
; CHECK: br
; CHECK: function f3 name="same_constant" arity=1 registers=
; CHECK: br_if
; CHECK: {{(bind_local|set_local|init_global|set_global)}}
; CHECK: br
; NO-OBJECT: target does not support generation of this file type
; DIRECT: function f3 name="same_constant" arity=1 registers=
; DIRECT: constant c8
; DIRECT: constant c8
