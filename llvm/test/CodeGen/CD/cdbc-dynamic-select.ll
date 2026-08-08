; RUN: llc -mtriple=cd-unknown-unknown %s -o %t.direct
; RUN: FileCheck --check-prefix=DIRECT %s < %t.direct
; RUN: llc -mtriple=cd-unknown-unknown -cd-backend=machine %s -o %t.machine
; RUN: FileCheck --check-prefix=MACHINE %s < %t.machine

@left_text = private unnamed_addr constant [5 x i8] c"left\00"
@right_text = private unnamed_addr constant [6 x i8] c"right\00"

declare ptr @llvm.cd.string(ptr)
declare void @cd_print(ptr)

define ptr @choose(ptr %left, ptr %right, i1 %condition) #0 {
entry:
  %selected = select i1 %condition, ptr %left, ptr %right
  ret ptr %selected
}

define i32 @main() {
entry:
  %left = call ptr @llvm.cd.string(ptr @left_text)
  %right = call ptr @llvm.cd.string(ptr @right_text)
  %first = call ptr @choose(ptr %left, ptr %right, i1 true)
  call void @cd_print(ptr %first)
  %second = call ptr @choose(ptr %left, ptr %right, i1 false)
  call void @cd_print(ptr %second)
  ret i32 0
}

attributes #0 = { "cd.value.params"="0,1" "cd.value.return" }

; DIRECT: cdbc 0.1
; DIRECT: main registers=
; DIRECT: make_function f0
; DIRECT: call
; DIRECT: print
; DIRECT: make_function f0
; DIRECT: call
; DIRECT: print
; DIRECT: function f0 name="choose" arity=3
; DIRECT: param 0 = "left"
; DIRECT: param 1 = "right"
; DIRECT: param 2 = "condition"
; DIRECT: jump_if_false
; DIRECT: move
; DIRECT: jump
; DIRECT: move
; DIRECT: return
; MACHINE: cdbc 0.1
; MACHINE: main registers=
; MACHINE: make_function f0
; MACHINE: call
; MACHINE: print
; MACHINE: make_function f0
; MACHINE: call
; MACHINE: print
; MACHINE: function f0 name="choose" arity=3
; MACHINE: param 0 = "left"
; MACHINE: param 1 = "right"
; MACHINE: param 2 = "condition"
; MACHINE: jump_if_false
; MACHINE: move
; MACHINE: jump
; MACHINE: move
; MACHINE: return
