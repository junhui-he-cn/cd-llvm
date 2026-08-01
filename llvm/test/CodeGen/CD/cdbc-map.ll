; RUN: llc -mtriple=cd-unknown-unknown %s -o %t.direct
; RUN: FileCheck --check-prefix=DIRECT %s < %t.direct
; RUN: llc -mtriple=cd-unknown-unknown -cd-backend=machine %s -o %t.machine
; RUN: FileCheck --check-prefix=MACHINE %s < %t.machine

declare ptr @llvm.cd.map(i32, ...)
declare ptr @llvm.cd.string(ptr)
declare ptr @llvm.cd.array(i32, ...)
declare ptr @llvm.cd.index(ptr, double)
declare double @llvm.cd.len(ptr)
declare void @cd_print(ptr)
declare void @print(double)

@key = private unnamed_addr constant [2 x i8] c"k\00"

define i32 @main() {
entry:
  %empty = call ptr (i32, ...) @llvm.cd.map(i32 0)
  %key_token = call ptr @llvm.cd.string(ptr @key)
  %nested_array = call ptr (i32, ...) @llvm.cd.array(i32 1, i64 7)
  %nested_map = call ptr (i32, ...) @llvm.cd.map(i32 1, i64 2, i64 3)
  %map = call ptr (i32, ...) @llvm.cd.map(
      i32 5, i64 1, i64 10, i64 1, i64 20, ptr %key_token,
      ptr %nested_array, ptr null, ptr %nested_map, i1 true, i64 30)
  %value = call ptr @llvm.cd.index(ptr %map, double 1.0)
  %length = call double @llvm.cd.len(ptr %map)
  call void @cd_print(ptr %empty)
  call void @cd_print(ptr %map)
  call void @cd_print(ptr %value)
  call void @print(double %length)
  ret i32 0
}

; DIRECT: r{{[0-9]+}} = map []
; DIRECT: r{{[0-9]+}} = map
; DIRECT: r{{[0-9]+}} = map
; MACHINE: r{{[0-9]+}} = map []
; MACHINE: r{{[0-9]+}} = map
; MACHINE: r{{[0-9]+}} = map
