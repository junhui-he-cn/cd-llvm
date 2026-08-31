; RUN: llc -mtriple=cd-unknown-unknown -O0 %s -o - | FileCheck %s

declare void @cd_print(i32)

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

define i32 @select_value(i1 %condition) {
entry:
  br i1 %condition, label %merge, label %other

other:
  br label %merge

merge:
  %value = phi i32 [ 1, %entry ], [ 2, %other ]
  ret i32 %value
}

define i32 @factorial(i32 %value) {
entry:
  %base = icmp sle i32 %value, 1
  br i1 %base, label %done, label %recurse

done:
  ret i32 1

recurse:
  %decrement = sub i32 %value, 1
  %recursive = call i32 @factorial(i32 %decrement)
  %product = mul i32 %value, %recursive
  ret i32 %product
}

define i32 @main() {
entry:
  %sum = call i32 @sum_to(i32 4)
  call void @cd_print(i32 %sum)
  %first = call i32 @select_value(i1 true)
  call void @cd_print(i32 %first)
  %second = call i32 @select_value(i1 false)
  call void @cd_print(i32 %second)
  %result = call i32 @factorial(i32 5)
  call void @cd_print(i32 %result)
  ret i32 0
}

; CHECK: cdbc 0.2
; CHECK: main registers=
; CHECK: call
; CHECK: call_native i0
; CHECK: function f0 name="sum_to" arity=1 registers=
; CHECK: br_if
; CHECK: {{(bind_local|set_local|init_global|set_global)}}
; CHECK: br
; CHECK: function f1 name="select_value" arity=1 registers=
; CHECK: br_if
; CHECK: function f2 name="factorial" arity=1 registers=
; CHECK: call
; CHECK: return
