; RUN: llc -mtriple=cd-unknown-unknown %s -o %t.direct
; RUN: FileCheck --check-prefix=DIRECT %s < %t.direct
; RUN: llc -mtriple=cd-unknown-unknown -cd-backend=machine %s -o %t.machine
; RUN: FileCheck --check-prefix=MACHINE %s < %t.machine

@left_text = private unnamed_addr constant [5 x i8] c"left\00"
@right_text = private unnamed_addr constant [6 x i8] c"right\00"

declare ptr @llvm.cd.string(ptr)
declare void @cd_print(ptr)

define ptr @choose_phi(ptr %left_value, ptr %right_value, i1 %condition) #0 {
entry:
  br i1 %condition, label %left_block, label %right_block

left_block:
  br label %merge

right_block:
  br label %merge

merge:
  %selected = phi ptr [ %left_value, %left_block ],
      [ %right_value, %right_block ]
  ret ptr %selected
}

define ptr @loop_phi(ptr %value, i32 %count) #1 {
entry:
  br label %loop

loop:
  %current = phi ptr [ %value, %entry ], [ %next, %body ]
  %index = phi i32 [ 0, %entry ], [ %next_index, %body ]
  %done = icmp eq i32 %index, %count
  br i1 %done, label %exit, label %body

body:
  %next = select i1 true, ptr %current, ptr null
  %next_index = add i32 %index, 1
  br label %loop

exit:
  ret ptr %current
}

define i32 @main() {
entry:
  %left = call ptr @llvm.cd.string(ptr @left_text)
  %right = call ptr @llvm.cd.string(ptr @right_text)
  %first = call ptr @choose_phi(ptr %left, ptr %right, i1 true)
  call void @cd_print(ptr %first)
  %second = call ptr @choose_phi(ptr %left, ptr %right, i1 false)
  call void @cd_print(ptr %second)
  %looped = call ptr @loop_phi(ptr %left, i32 2)
  call void @cd_print(ptr %looped)
  ret i32 0
}

attributes #0 = { "cd.value.params"="0,1" "cd.value.return" }
attributes #1 = { "cd.value.params"="0" "cd.value.return" }

; DIRECT: cdbc 0.2
; DIRECT: function f0 name="choose_phi" arity=3
; DIRECT: br_if
; DIRECT: {{(bind_local|set_local|init_global|set_global)}}
; DIRECT: br
; DIRECT: {{(bind_local|set_local|init_global|set_global)}}
; DIRECT: br
; DIRECT: {{(load_local|load_global)}}
; DIRECT: return
; DIRECT: function f1 name="loop_phi" arity=2
; DIRECT: {{(load_local|load_global)}}
; DIRECT: {{(load_local|load_global)}}
; DIRECT: {{(bind_local|set_local|init_global|set_global)}}
; DIRECT: br
; DIRECT: {{(load_local|load_global)}}
; DIRECT: {{(load_local|load_global)}}
; DIRECT: br_if
; DIRECT: {{(bind_local|set_local|init_global|set_global)}}
; DIRECT: {{(bind_local|set_local|init_global|set_global)}}
; DIRECT: br
; DIRECT: return
; MACHINE: cdbc 0.2
; MACHINE: function f0 name="choose_phi" arity=3
; MACHINE: br_if
; MACHINE: {{(bind_local|set_local|init_global|set_global)}}
; MACHINE: br
; MACHINE: {{(bind_local|set_local|init_global|set_global)}}
; MACHINE: br
; MACHINE: {{(load_local|load_global)}}
; MACHINE: return
; MACHINE: function f1 name="loop_phi" arity=2
; MACHINE: {{(load_local|load_global)}}
; MACHINE: {{(load_local|load_global)}}
; MACHINE: {{(bind_local|set_local|init_global|set_global)}}
; MACHINE: br
; MACHINE: {{(load_local|load_global)}}
; MACHINE: {{(load_local|load_global)}}
; MACHINE: br_if
; MACHINE: {{(bind_local|set_local|init_global|set_global)}}
; MACHINE: {{(bind_local|set_local|init_global|set_global)}}
; MACHINE: br
; MACHINE: return
