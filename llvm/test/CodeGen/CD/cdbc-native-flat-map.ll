; RUN: llc -mtriple=cd-unknown-unknown %s -o %t.direct
; RUN: FileCheck --check-prefix=DIRECT %s < %t.direct
; RUN: llc -mtriple=cd-unknown-unknown -cd-backend=machine %s -o %t.machine
; RUN: FileCheck --check-prefix=MACHINE %s < %t.machine

@flat_map_name = private unnamed_addr constant [8 x i8] c"flatMap\00"
@one = private unnamed_addr constant [4 x i8] c"one\00"
@two = private unnamed_addr constant [4 x i8] c"two\00"

declare ptr @llvm.cd.array(i32, ...)
declare ptr @llvm.cd.native(ptr, ...)
declare ptr @llvm.cd.string(ptr)
declare void @cd_print(ptr)

define ptr @identity(ptr %value) #0 {
entry:
  ret ptr %value
}

define i32 @main() {
entry:
  %empty = call ptr (i32, ...) @llvm.cd.array(i32 0)
  %flattened_empty = call ptr (ptr, ...) @llvm.cd.native(
      ptr @flat_map_name, ptr %empty, ptr @identity)
  call void @cd_print(ptr %flattened_empty)

  %first = call ptr @llvm.cd.string(ptr @one)
  %second = call ptr @llvm.cd.string(ptr @two)
  %left = call ptr (i32, ...) @llvm.cd.array(
      i32 2, ptr %first, ptr %second)
  %right = call ptr (i32, ...) @llvm.cd.array(i32 1, ptr %second)
  %source = call ptr (i32, ...) @llvm.cd.array(
      i32 2, ptr %left, ptr %right)
  %flattened = call ptr (ptr, ...) @llvm.cd.native(
      ptr @flat_map_name, ptr %source, ptr @identity)
  call void @cd_print(ptr %flattened)
  ret i32 0
}

attributes #0 = { "cd.value.params"="0" "cd.value.return" }

; DIRECT: cdbc 0.2
; DIRECT: main registers=
; DIRECT: make_function f0
; DIRECT: call_native i0
; DIRECT: call_native i1
; DIRECT: make_function f0
; DIRECT: call_native i0
; DIRECT: call_native i1
; DIRECT: function f0 name="identity" arity=1
; DIRECT: param 0 = "value"
; DIRECT: return
; MACHINE: cdbc 0.2
; MACHINE: main registers=
; MACHINE: make_function f0
; MACHINE: call_native i0
; MACHINE: call_native i1
; MACHINE: make_function f0
; MACHINE: call_native i0
; MACHINE: call_native i1
; MACHINE: function f0 name="identity" arity=1
; MACHINE: param 0 = "value"
; MACHINE: return
