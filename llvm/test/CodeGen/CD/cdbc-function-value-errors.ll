; RUN: split-file %s %t
; RUN: not --crash llc -mtriple=cd-unknown-unknown %t/unmarked-parameter.ll -o - 2>&1 | FileCheck %s --check-prefix=UNMARKED-PARAM-DIRECT
; RUN: not --crash llc -mtriple=cd-unknown-unknown -cd-backend=machine %t/unmarked-parameter.ll -o - 2>&1 | FileCheck %s --check-prefix=UNMARKED-PARAM-MACHINE
; RUN: not --crash llc -mtriple=cd-unknown-unknown %t/unmarked-return.ll -o - 2>&1 | FileCheck %s --check-prefix=UNMARKED-RETURN-DIRECT
; RUN: not --crash llc -mtriple=cd-unknown-unknown -cd-backend=machine %t/unmarked-return.ll -o - 2>&1 | FileCheck %s --check-prefix=UNMARKED-RETURN-MACHINE
; RUN: not --crash llc -mtriple=cd-unknown-unknown %t/bad-list.ll -o - 2>&1 | FileCheck %s --check-prefix=BAD-LIST-DIRECT
; RUN: not --crash llc -mtriple=cd-unknown-unknown -cd-backend=machine %t/bad-list.ll -o - 2>&1 | FileCheck %s --check-prefix=BAD-LIST-MACHINE
; RUN: not --crash llc -mtriple=cd-unknown-unknown %t/bad-index.ll -o - 2>&1 | FileCheck %s --check-prefix=BAD-INDEX-DIRECT
; RUN: not --crash llc -mtriple=cd-unknown-unknown -cd-backend=machine %t/bad-index.ll -o - 2>&1 | FileCheck %s --check-prefix=BAD-INDEX-MACHINE
; RUN: not --crash llc -mtriple=cd-unknown-unknown %t/alloca-argument.ll -o - 2>&1 | FileCheck %s --check-prefix=ALLOCA-DIRECT
; RUN: not --crash llc -mtriple=cd-unknown-unknown -cd-backend=machine %t/alloca-argument.ll -o - 2>&1 | FileCheck %s --check-prefix=ALLOCA-MACHINE
; RUN: not --crash llc -mtriple=cd-unknown-unknown %t/global-argument.ll -o - 2>&1 | FileCheck %s --check-prefix=GLOBAL-DIRECT
; RUN: not --crash llc -mtriple=cd-unknown-unknown -cd-backend=machine %t/global-argument.ll -o - 2>&1 | FileCheck %s --check-prefix=GLOBAL-MACHINE
; RUN: not --crash llc -mtriple=cd-unknown-unknown %t/ordinary-return.ll -o - 2>&1 | FileCheck %s --check-prefix=ORDINARY-RETURN-DIRECT
; RUN: not --crash llc -mtriple=cd-unknown-unknown -cd-backend=machine %t/ordinary-return.ll -o - 2>&1 | FileCheck %s --check-prefix=ORDINARY-RETURN-MACHINE

; UNMARKED-PARAM-DIRECT: CD target does not support LLVM operation: every pointer function parameter must be listed by cd.value.params
; UNMARKED-PARAM-MACHINE: CD machine backend does not support every pointer function parameter must be listed by cd.value.params
; UNMARKED-RETURN-DIRECT: CD target does not support LLVM operation: every pointer function return must carry cd.value.return
; UNMARKED-RETURN-MACHINE: CD machine backend does not support every pointer function return must carry cd.value.return
; BAD-LIST-DIRECT: CD target does not support LLVM operation: cd.value.params indexes must be strictly increasing
; BAD-LIST-MACHINE: CD machine backend does not support cd.value.params indexes must be strictly increasing
; BAD-INDEX-DIRECT: CD target does not support LLVM operation: cd.value.params parameter index is out of range
; BAD-INDEX-MACHINE: CD machine backend does not support cd.value.params parameter index is out of range
; ALLOCA-DIRECT: CD target does not support LLVM operation: cd.value.params requires a proven CD value argument
; ALLOCA-MACHINE: CD machine backend does not support cd.value.params requires a proven CD value argument
; GLOBAL-DIRECT: CD target only supports globals used by CD string/name intrinsics
; GLOBAL-MACHINE: CD machine backend does not support globals used outside CD string/name intrinsics
; ORDINARY-RETURN-DIRECT: CD target does not support LLVM operation: cd.value.return requires every pointer return value to have proven CD provenance
; ORDINARY-RETURN-MACHINE: CD machine backend does not support cd.value.return requires every pointer return value to have proven CD provenance

;--- unmarked-parameter.ll
define i32 @takes_pointer(ptr %value) {
entry:
  ret i32 0
}

define i32 @main() {
entry:
  ret i32 0
}

;--- unmarked-return.ll
define ptr @returns_pointer() {
entry:
  ret ptr null
}

define i32 @main() {
entry:
  ret i32 0
}

;--- bad-list.ll
define void @takes(ptr %first, ptr %second) #0 {
entry:
  ret void
}

define i32 @main() {
entry:
  ret i32 0
}

attributes #0 = { "cd.value.params"="1,0" }

;--- bad-index.ll
define void @takes(ptr %value) #0 {
entry:
  ret void
}

define i32 @main() {
entry:
  ret i32 0
}

attributes #0 = { "cd.value.params"="1" }

;--- alloca-argument.ll
define void @takes(ptr %value) #0 {
entry:
  ret void
}

define i32 @main() {
entry:
  %slot = alloca i8
  call void @takes(ptr %slot)
  ret i32 0
}

attributes #0 = { "cd.value.params"="0" }

;--- global-argument.ll
@ordinary = global i8 0

define void @takes(ptr %value) #0 {
entry:
  ret void
}

define i32 @main() {
entry:
  call void @takes(ptr @ordinary)
  ret i32 0
}

attributes #0 = { "cd.value.params"="0" }

;--- ordinary-return.ll
define ptr @returns_pointer(ptr %value) #0 {
entry:
  %slot = alloca i8
  ret ptr %slot
}

define i32 @main() {
entry:
  ret i32 0
}

attributes #0 = { "cd.value.params"="0" "cd.value.return" }
