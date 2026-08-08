; RUN: llc -mtriple=cd-unknown-unknown %s -o %t.direct
; RUN: FileCheck --check-prefix=DIRECT %s < %t.direct
; RUN: FileCheck --check-prefix=DIRECT-PRINTS %s < %t.direct
; RUN: llc -mtriple=cd-unknown-unknown -cd-backend=machine %s -o %t.machine
; RUN: FileCheck --check-prefix=MACHINE %s < %t.machine
; RUN: FileCheck --check-prefix=MACHINE-PRINTS %s < %t.machine

@find_name = private unnamed_addr constant [5 x i8] c"find\00"
@one = private unnamed_addr constant [4 x i8] c"one\00"
@two = private unnamed_addr constant [4 x i8] c"two\00"

declare ptr @llvm.cd.array(i32, ...)
declare ptr @llvm.cd.native(ptr, ...)
declare ptr @llvm.cd.string(ptr)
declare void @cd_print(ptr)

define i1 @yes(ptr %value) #0 {
entry:
  ret i1 true
}

define i1 @no(ptr %value) #0 {
entry:
  ret i1 false
}

define i32 @main() {
entry:
  %empty = call ptr (i32, ...) @llvm.cd.array(i32 0)
  %empty_result = call ptr (ptr, ...) @llvm.cd.native(
      ptr @find_name, ptr %empty, ptr @yes)
  call void @cd_print(ptr %empty_result)

  %first = call ptr @llvm.cd.string(ptr @one)
  %second = call ptr @llvm.cd.string(ptr @two)
  %source = call ptr (i32, ...) @llvm.cd.array(
      i32 2, ptr %first, ptr %second)
  %found = call ptr (ptr, ...) @llvm.cd.native(
      ptr @find_name, ptr %source, ptr @yes)
  call void @cd_print(ptr %found)
  %missing = call ptr (ptr, ...) @llvm.cd.native(
      ptr @find_name, ptr %source, ptr @no)
  call void @cd_print(ptr %missing)
  ret i32 0
}

attributes #0 = { "cd.value.params"="0" }

; DIRECT: cdbc 0.1
; DIRECT: main registers=
; DIRECT-COUNT-3: native_call
; DIRECT: function f0 name="yes" arity=1
; DIRECT: function f1 name="no" arity=1
; DIRECT-PRINTS-COUNT-3: print
; MACHINE: cdbc 0.1
; MACHINE: main registers=
; MACHINE-COUNT-3: native_call
; MACHINE: function f0 name="yes" arity=1
; MACHINE: function f1 name="no" arity=1
; MACHINE-PRINTS-COUNT-3: print
