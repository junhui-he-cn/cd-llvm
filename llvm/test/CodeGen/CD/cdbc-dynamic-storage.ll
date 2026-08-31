; RUN: llc -mtriple=cd-unknown-unknown %s -o %t.direct
; RUN: FileCheck --check-prefix=DIRECT %s < %t.direct
; RUN: llc -mtriple=cd-unknown-unknown -cd-backend=machine %s -o %t.machine
; RUN: FileCheck --check-prefix=MACHINE %s < %t.machine

@left_text = private unnamed_addr constant [5 x i8] c"left\00"
@right_text = private unnamed_addr constant [6 x i8] c"right\00"

declare ptr @llvm.cd.string(ptr)
declare void @cd_print(ptr)

define ptr @branch_store(ptr %left, ptr %right, i1 %condition) #0 {
entry:
  %slot = alloca ptr
  br i1 %condition, label %left_block, label %right_block

left_block:
  store ptr %left, ptr %slot
  br label %merge

right_block:
  store ptr %right, ptr %slot
  br label %merge

merge:
  %loaded = load ptr, ptr %slot
  ret ptr %loaded
}

define i32 @main() {
entry:
  %left = call ptr @llvm.cd.string(ptr @left_text)
  %right = call ptr @llvm.cd.string(ptr @right_text)
  %first = call ptr @branch_store(ptr %left, ptr %right, i1 true)
  call void @cd_print(ptr %first)
  %second = call ptr @branch_store(ptr %left, ptr %right, i1 false)
  call void @cd_print(ptr %second)

  %slot = alloca ptr
  store ptr %left, ptr %slot
  %loaded = load ptr, ptr %slot
  call void @cd_print(ptr %loaded)
  store ptr null, ptr %slot
  %nil = load ptr, ptr %slot
  call void @cd_print(ptr %nil)
  ret i32 0
}

attributes #0 = { "cd.value.params"="0,1" "cd.value.return" }

; DIRECT: cdbc 0.2
; DIRECT: main registers=
; DIRECT: {{(bind_local|set_local|init_global|set_global)}}
; DIRECT: {{(load_local|load_global)}}
; DIRECT: call_native i0
; DIRECT: {{(bind_local|set_local|init_global|set_global)}}
; DIRECT: {{(load_local|load_global)}}
; DIRECT: call_native i0
; DIRECT: function f0 name="branch_store" arity=3
; DIRECT: {{(bind_local|set_local|init_global|set_global)}}
; DIRECT: {{(bind_local|set_local|init_global|set_global)}}
; DIRECT: {{(load_local|load_global)}}
; DIRECT: return
; MACHINE: cdbc 0.2
; MACHINE: main registers=
; MACHINE: {{(bind_local|set_local|init_global|set_global)}}
; MACHINE: {{(load_local|load_global)}}
; MACHINE: call_native i0
; MACHINE: {{(bind_local|set_local|init_global|set_global)}}
; MACHINE: {{(load_local|load_global)}}
; MACHINE: call_native i0
; MACHINE: function f0 name="branch_store" arity=3
; MACHINE: {{(bind_local|set_local|init_global|set_global)}}
; MACHINE: {{(bind_local|set_local|init_global|set_global)}}
; MACHINE: {{(load_local|load_global)}}
; MACHINE: return
