; RUN: llc -mtriple=cd-unknown-unknown %s -o %t.direct
; RUN: FileCheck --check-prefix=DIRECT %s < %t.direct
; RUN: llc -mtriple=cd-unknown-unknown -cd-backend=machine %s -o %t.machine
; RUN: FileCheck --check-prefix=MACHINE %s < %t.machine

@values_name = private unnamed_addr constant [7 x i8] c"values\00"
@first = private unnamed_addr constant [2 x i8] c"a\00"
@second = private unnamed_addr constant [2 x i8] c"b\00"

declare ptr @llvm.cd.map(i32, ...)
declare ptr @llvm.cd.string(ptr)
declare ptr @llvm.cd.native(ptr, ...)
declare void @cd_print(ptr)

define i32 @main() {
entry:
  %first_value = call ptr @llvm.cd.string(ptr @first)
  %second_value = call ptr @llvm.cd.string(ptr @second)
  %empty = call ptr (i32, ...) @llvm.cd.map(i32 0)
  %map = call ptr (i32, ...) @llvm.cd.map(
      i32 2, ptr %first_value, i64 1, ptr %second_value, i64 2)
  %empty_values = call ptr (ptr, ...) @llvm.cd.native(
      ptr @values_name, ptr %empty)
  %values = call ptr (ptr, ...) @llvm.cd.native(
      ptr @values_name, ptr %map)
  call void @cd_print(ptr %empty_values)
  call void @cd_print(ptr %values)
  ret i32 0
}

; DIRECT: cdbc 0.1
; DIRECT-COUNT-2: native_call
; DIRECT-COUNT-2: print
; MACHINE: cdbc 0.1
; MACHINE-COUNT-2: native_call
; MACHINE-COUNT-2: print
