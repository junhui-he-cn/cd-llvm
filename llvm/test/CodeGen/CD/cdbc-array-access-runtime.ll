; RUN: llc -mtriple=cd-unknown-unknown %s -o %t.direct
; RUN: FileCheck --check-prefix=DIRECT %s < %t.direct
; RUN: llc -mtriple=cd-unknown-unknown -cd-backend=machine %s -o %t.machine
; RUN: FileCheck --check-prefix=MACHINE %s < %t.machine

declare ptr @llvm.cd.assert.array(ptr)
declare void @cd_print(ptr)

define i32 @main() {
entry:
  %value = call ptr @llvm.cd.assert.array(ptr null)
  call void @cd_print(ptr %value)
  ret i32 0
}

; DIRECT: r{{[0-9]+}} = assert_array
; MACHINE: r{{[0-9]+}} = assert_array
