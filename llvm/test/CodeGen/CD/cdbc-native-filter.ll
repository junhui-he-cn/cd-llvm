; RUN: llc -mtriple=cd-unknown-unknown %s -o %t.direct
; RUN: FileCheck --check-prefix=DIRECT %s < %t.direct
; RUN: llc -mtriple=cd-unknown-unknown -cd-backend=machine %s -o %t.machine
; RUN: FileCheck --check-prefix=MACHINE %s < %t.machine

@filter_name = private unnamed_addr constant [7 x i8] c"filter\00"
@one = private unnamed_addr constant [4 x i8] c"one\00"
@two = private unnamed_addr constant [4 x i8] c"two\00"

declare ptr @llvm.cd.array(i32, ...)
declare ptr @llvm.cd.native(ptr, ...)
declare ptr @llvm.cd.string(ptr)
declare void @cd_print(ptr)

define i1 @keep(ptr %value) #0 {
entry:
  ret i1 true
}

define i1 @reject(ptr %value) #0 {
entry:
  ret i1 false
}

define i32 @main() {
entry:
  %empty = call ptr (i32, ...) @llvm.cd.array(i32 0)
  %filtered_empty = call ptr (ptr, ...) @llvm.cd.native(
      ptr @filter_name, ptr %empty, ptr @keep)
  call void @cd_print(ptr %filtered_empty)

  %first = call ptr @llvm.cd.string(ptr @one)
  %second = call ptr @llvm.cd.string(ptr @two)
  %source = call ptr (i32, ...) @llvm.cd.array(
      i32 2, ptr %first, ptr %second)
  %filtered_none = call ptr (ptr, ...) @llvm.cd.native(
      ptr @filter_name, ptr %source, ptr @reject)
  call void @cd_print(ptr %filtered_none)
  %filtered_all = call ptr (ptr, ...) @llvm.cd.native(
      ptr @filter_name, ptr %source, ptr @keep)
  call void @cd_print(ptr %filtered_all)
  ret i32 0
}

attributes #0 = { "cd.value.params"="0" }

; DIRECT: cdbc 0.1
; DIRECT: main registers=
; DIRECT: make_function
; DIRECT: native_call
; DIRECT: print
; DIRECT: make_function
; DIRECT: native_call
; DIRECT: print
; DIRECT: make_function
; DIRECT: native_call
; DIRECT: print
; DIRECT: function
; DIRECT: param 0 = "value"
; DIRECT: return
; DIRECT: function
; DIRECT: param 0 = "value"
; DIRECT: return
; MACHINE: cdbc 0.1
; MACHINE: main registers=
; MACHINE: make_function
; MACHINE: native_call
; MACHINE: print
; MACHINE: make_function
; MACHINE: native_call
; MACHINE: print
; MACHINE: make_function
; MACHINE: native_call
; MACHINE: print
; MACHINE: function
; MACHINE: param 0 = "value"
; MACHINE: return
; MACHINE: function
; MACHINE: param 0 = "value"
; MACHINE: return
