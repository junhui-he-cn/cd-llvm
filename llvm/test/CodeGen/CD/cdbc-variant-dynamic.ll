; RUN: llc -mtriple=cd-unknown-unknown %s -o %t.direct
; RUN: FileCheck --check-prefix=DIRECT %s < %t.direct
; RUN: llc -mtriple=cd-unknown-unknown -cd-backend=machine %s -o %t.machine
; RUN: FileCheck --check-prefix=MACHINE %s < %t.machine

@result = private unnamed_addr constant [7 x i8] c"Result\00"
@ok = private unnamed_addr constant [3 x i8] c"Ok\00"

declare ptr @llvm.cd.array(i32, ...)
declare ptr @llvm.cd.variant(ptr, ptr, i32, ...)
declare ptr @llvm.cd.variant.field(ptr, i32)
declare double @llvm.cd.len(ptr)
declare void @cd_print(ptr)
declare void @print(double)

define i32 @main() {
entry:
  %items = call ptr (i32, ...) @llvm.cd.array(i32 2, i64 3, i64 4)
  %value = call ptr (ptr, ptr, i32, ...) @llvm.cd.variant(
      ptr @result, ptr @ok, i32 1, ptr %items)
  %payload = call ptr @llvm.cd.variant.field(ptr %value, i32 0)
  %length = call double @llvm.cd.len(ptr %payload)
  call void @cd_print(ptr %value)
  call void @cd_print(ptr %payload)
  call void @print(double %length)
  ret i32 0
}

; DIRECT: array
; DIRECT: variant
; DIRECT: variant_field
; DIRECT: len
; MACHINE: array
; MACHINE: variant
; MACHINE: variant_field
; MACHINE: len
