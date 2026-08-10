; RUN: split-file %s %t
; RUN: not --crash llc -mtriple=cd-unknown-unknown %t/unknown-name.ll -o - 2>&1 | FileCheck %s --check-prefix=UNKNOWN-DIRECT
; RUN: not --crash llc -mtriple=cd-unknown-unknown -cd-backend=machine %t/unknown-name.ll -o - 2>&1 | FileCheck %s --check-prefix=UNKNOWN-MACHINE
; RUN: not --crash llc -mtriple=cd-unknown-unknown %t/map-shape.ll -o - 2>&1 | FileCheck %s --check-prefix=MAP-SHAPE-DIRECT
; RUN: not --crash llc -mtriple=cd-unknown-unknown -cd-backend=machine %t/map-shape.ll -o - 2>&1 | FileCheck %s --check-prefix=MAP-SHAPE-MACHINE
; RUN: not --crash llc -mtriple=cd-unknown-unknown %t/map-pointer.ll -o - 2>&1 | FileCheck %s --check-prefix=MAP-POINTER-DIRECT
; RUN: not --crash llc -mtriple=cd-unknown-unknown -cd-backend=machine %t/map-pointer.ll -o - 2>&1 | FileCheck %s --check-prefix=MAP-POINTER-MACHINE
; RUN: not --crash llc -mtriple=cd-unknown-unknown %t/map-callback.ll -o - 2>&1 | FileCheck %s --check-prefix=MAP-CALLBACK-DIRECT
; RUN: not --crash llc -mtriple=cd-unknown-unknown -cd-backend=machine %t/map-callback.ll -o - 2>&1 | FileCheck %s --check-prefix=MAP-CALLBACK-MACHINE
; RUN: not --crash llc -mtriple=cd-unknown-unknown %t/map-callback-declaration.ll -o - 2>&1 | FileCheck %s --check-prefix=MAP-CALLBACK-DECL-DIRECT
; RUN: not --crash llc -mtriple=cd-unknown-unknown -cd-backend=machine %t/map-callback-declaration.ll -o - 2>&1 | FileCheck %s --check-prefix=MAP-CALLBACK-DECL-MACHINE
; RUN: not --crash llc -mtriple=cd-unknown-unknown %t/map-callback-cast.ll -o - 2>&1 | FileCheck %s --check-prefix=MAP-CALLBACK-CAST-DIRECT
; RUN: not --crash llc -mtriple=cd-unknown-unknown -cd-backend=machine %t/map-callback-cast.ll -o - 2>&1 | FileCheck %s --check-prefix=MAP-CALLBACK-CAST-MACHINE
; RUN: not --crash llc -mtriple=cd-unknown-unknown %t/map-callback-main.ll -o - 2>&1 | FileCheck %s --check-prefix=MAP-CALLBACK-MAIN-DIRECT
; RUN: not --crash llc -mtriple=cd-unknown-unknown -cd-backend=machine %t/map-callback-main.ll -o - 2>&1 | FileCheck %s --check-prefix=MAP-CALLBACK-MAIN-MACHINE
; RUN: not --crash llc -mtriple=cd-unknown-unknown %t/filter-shape.ll -o - 2>&1 | FileCheck %s --check-prefix=FILTER-SHAPE-DIRECT
; RUN: not --crash llc -mtriple=cd-unknown-unknown -cd-backend=machine %t/filter-shape.ll -o - 2>&1 | FileCheck %s --check-prefix=FILTER-SHAPE-MACHINE
; RUN: not --crash llc -mtriple=cd-unknown-unknown %t/filter-pointer.ll -o - 2>&1 | FileCheck %s --check-prefix=FILTER-POINTER-DIRECT
; RUN: not --crash llc -mtriple=cd-unknown-unknown -cd-backend=machine %t/filter-pointer.ll -o - 2>&1 | FileCheck %s --check-prefix=FILTER-POINTER-MACHINE
; RUN: not --crash llc -mtriple=cd-unknown-unknown %t/filter-callback.ll -o - 2>&1 | FileCheck %s --check-prefix=FILTER-CALLBACK-DIRECT
; RUN: not --crash llc -mtriple=cd-unknown-unknown -cd-backend=machine %t/filter-callback.ll -o - 2>&1 | FileCheck %s --check-prefix=FILTER-CALLBACK-MACHINE
; RUN: not --crash llc -mtriple=cd-unknown-unknown %t/filter-callback-declaration.ll -o - 2>&1 | FileCheck %s --check-prefix=FILTER-CALLBACK-DECL-DIRECT
; RUN: not --crash llc -mtriple=cd-unknown-unknown -cd-backend=machine %t/filter-callback-declaration.ll -o - 2>&1 | FileCheck %s --check-prefix=FILTER-CALLBACK-DECL-MACHINE
; RUN: not --crash llc -mtriple=cd-unknown-unknown %t/filter-callback-cast.ll -o - 2>&1 | FileCheck %s --check-prefix=FILTER-CALLBACK-CAST-DIRECT
; RUN: not --crash llc -mtriple=cd-unknown-unknown -cd-backend=machine %t/filter-callback-cast.ll -o - 2>&1 | FileCheck %s --check-prefix=FILTER-CALLBACK-CAST-MACHINE
; RUN: not --crash llc -mtriple=cd-unknown-unknown %t/filter-callback-main.ll -o - 2>&1 | FileCheck %s --check-prefix=FILTER-CALLBACK-MAIN-DIRECT
; RUN: not --crash llc -mtriple=cd-unknown-unknown -cd-backend=machine %t/filter-callback-main.ll -o - 2>&1 | FileCheck %s --check-prefix=FILTER-CALLBACK-MAIN-MACHINE
; RUN: not --crash llc -mtriple=cd-unknown-unknown %t/filter-callback-return-marker.ll -o - 2>&1 | FileCheck %s --check-prefix=FILTER-CALLBACK-RETURN-DIRECT
; RUN: not --crash llc -mtriple=cd-unknown-unknown -cd-backend=machine %t/filter-callback-return-marker.ll -o - 2>&1 | FileCheck %s --check-prefix=FILTER-CALLBACK-RETURN-MACHINE
; RUN: not --crash llc -mtriple=cd-unknown-unknown %t/floor-arity.ll -o - 2>&1 | FileCheck %s --check-prefix=FLOOR-ARITY-DIRECT
; RUN: not --crash llc -mtriple=cd-unknown-unknown -cd-backend=machine %t/floor-arity.ll -o - 2>&1 | FileCheck %s --check-prefix=FLOOR-ARITY-MACHINE
; RUN: not --crash llc -mtriple=cd-unknown-unknown %t/floor-type.ll -o - 2>&1 | FileCheck %s --check-prefix=FLOOR-TYPE-DIRECT
; RUN: not --crash llc -mtriple=cd-unknown-unknown -cd-backend=machine %t/floor-type.ll -o - 2>&1 | FileCheck %s --check-prefix=FLOOR-TYPE-MACHINE
; RUN: not --crash llc -mtriple=cd-unknown-unknown %t/string-pointer.ll -o - 2>&1 | FileCheck %s --check-prefix=STRING-POINTER-DIRECT
; RUN: not --crash llc -mtriple=cd-unknown-unknown -cd-backend=machine %t/string-pointer.ll -o - 2>&1 | FileCheck %s --check-prefix=STRING-POINTER-MACHINE
; RUN: not --crash llc -mtriple=cd-unknown-unknown %t/bad-name-global.ll -o - 2>&1 | FileCheck %s --check-prefix=BAD-NAME-DIRECT
; RUN: not --crash llc -mtriple=cd-unknown-unknown -cd-backend=machine %t/bad-name-global.ll -o - 2>&1 | FileCheck %s --check-prefix=BAD-NAME-MACHINE
; RUN: not --crash llc -mtriple=cd-unknown-unknown %t/range-arity.ll -o - 2>&1 | FileCheck %s --check-prefix=RANGE-ARITY-DIRECT
; RUN: not --crash llc -mtriple=cd-unknown-unknown -cd-backend=machine %t/range-arity.ll -o - 2>&1 | FileCheck %s --check-prefix=RANGE-ARITY-MACHINE
; RUN: not --crash llc -mtriple=cd-unknown-unknown %t/floor-result.ll -o - 2>&1 | FileCheck %s --check-prefix=FLOOR-RESULT-DIRECT
; RUN: not --crash llc -mtriple=cd-unknown-unknown -cd-backend=machine %t/floor-result.ll -o - 2>&1 | FileCheck %s --check-prefix=FLOOR-RESULT-MACHINE
; RUN: not --crash llc -mtriple=cd-unknown-unknown %t/substr-arity.ll -o - 2>&1 | FileCheck %s --check-prefix=SUBSTR-ARITY-DIRECT
; RUN: not --crash llc -mtriple=cd-unknown-unknown -cd-backend=machine %t/substr-arity.ll -o - 2>&1 | FileCheck %s --check-prefix=SUBSTR-ARITY-MACHINE
; RUN: not --crash llc -mtriple=cd-unknown-unknown %t/substr-index-type.ll -o - 2>&1 | FileCheck %s --check-prefix=SUBSTR-INDEX-TYPE-DIRECT
; RUN: not --crash llc -mtriple=cd-unknown-unknown -cd-backend=machine %t/substr-index-type.ll -o - 2>&1 | FileCheck %s --check-prefix=SUBSTR-INDEX-TYPE-MACHINE
; RUN: not --crash llc -mtriple=cd-unknown-unknown %t/substr-scalar.ll -o - 2>&1 | FileCheck %s --check-prefix=SUBSTR-SCALAR-DIRECT
; RUN: not --crash llc -mtriple=cd-unknown-unknown -cd-backend=machine %t/substr-scalar.ll -o - 2>&1 | FileCheck %s --check-prefix=SUBSTR-SCALAR-MACHINE
; RUN: not --crash llc -mtriple=cd-unknown-unknown %t/substr-pointer.ll -o - 2>&1 | FileCheck %s --check-prefix=SUBSTR-POINTER-DIRECT
; RUN: not --crash llc -mtriple=cd-unknown-unknown -cd-backend=machine %t/substr-pointer.ll -o - 2>&1 | FileCheck %s --check-prefix=SUBSTR-POINTER-MACHINE
; RUN: not --crash llc -mtriple=cd-unknown-unknown %t/substr-result.ll -o - 2>&1 | FileCheck %s --check-prefix=SUBSTR-RESULT-DIRECT
; RUN: not --crash llc -mtriple=cd-unknown-unknown -cd-backend=machine %t/substr-result.ll -o - 2>&1 | FileCheck %s --check-prefix=SUBSTR-RESULT-MACHINE
; RUN: not --crash llc -mtriple=cd-unknown-unknown %t/char-at-arity.ll -o - 2>&1 | FileCheck %s --check-prefix=CHAR-AT-ARITY-DIRECT
; RUN: not --crash llc -mtriple=cd-unknown-unknown -cd-backend=machine %t/char-at-arity.ll -o - 2>&1 | FileCheck %s --check-prefix=CHAR-AT-ARITY-MACHINE
; RUN: not --crash llc -mtriple=cd-unknown-unknown %t/char-at-index-type.ll -o - 2>&1 | FileCheck %s --check-prefix=CHAR-AT-INDEX-TYPE-DIRECT
; RUN: not --crash llc -mtriple=cd-unknown-unknown -cd-backend=machine %t/char-at-index-type.ll -o - 2>&1 | FileCheck %s --check-prefix=CHAR-AT-INDEX-TYPE-MACHINE
; RUN: not --crash llc -mtriple=cd-unknown-unknown %t/char-at-scalar.ll -o - 2>&1 | FileCheck %s --check-prefix=CHAR-AT-SCALAR-DIRECT
; RUN: not --crash llc -mtriple=cd-unknown-unknown -cd-backend=machine %t/char-at-scalar.ll -o - 2>&1 | FileCheck %s --check-prefix=CHAR-AT-SCALAR-MACHINE
; RUN: not --crash llc -mtriple=cd-unknown-unknown %t/char-at-pointer.ll -o - 2>&1 | FileCheck %s --check-prefix=CHAR-AT-POINTER-DIRECT
; RUN: not --crash llc -mtriple=cd-unknown-unknown -cd-backend=machine %t/char-at-pointer.ll -o - 2>&1 | FileCheck %s --check-prefix=CHAR-AT-POINTER-MACHINE
; RUN: not --crash llc -mtriple=cd-unknown-unknown %t/char-at-result.ll -o - 2>&1 | FileCheck %s --check-prefix=CHAR-AT-RESULT-DIRECT
; RUN: not --crash llc -mtriple=cd-unknown-unknown -cd-backend=machine %t/char-at-result.ll -o - 2>&1 | FileCheck %s --check-prefix=CHAR-AT-RESULT-MACHINE
; RUN: not --crash llc -mtriple=cd-unknown-unknown %t/contains-shape.ll -o - 2>&1 | FileCheck %s --check-prefix=CONTAINS-SHAPE-DIRECT
; RUN: not --crash llc -mtriple=cd-unknown-unknown -cd-backend=machine %t/contains-shape.ll -o - 2>&1 | FileCheck %s --check-prefix=CONTAINS-SHAPE-MACHINE
; RUN: not --crash llc -mtriple=cd-unknown-unknown %t/contains-collection-pointer.ll -o - 2>&1 | FileCheck %s --check-prefix=CONTAINS-COLLECTION-DIRECT
; RUN: not --crash llc -mtriple=cd-unknown-unknown -cd-backend=machine %t/contains-collection-pointer.ll -o - 2>&1 | FileCheck %s --check-prefix=CONTAINS-COLLECTION-MACHINE
; RUN: not --crash llc -mtriple=cd-unknown-unknown %t/contains-needle-pointer.ll -o - 2>&1 | FileCheck %s --check-prefix=CONTAINS-NEEDLE-DIRECT
; RUN: not --crash llc -mtriple=cd-unknown-unknown -cd-backend=machine %t/contains-needle-pointer.ll -o - 2>&1 | FileCheck %s --check-prefix=CONTAINS-NEEDLE-MACHINE
; RUN: not --crash llc -mtriple=cd-unknown-unknown %t/contains-result.ll -o - 2>&1 | FileCheck %s --check-prefix=CONTAINS-RESULT-DIRECT
; RUN: not --crash llc -mtriple=cd-unknown-unknown -cd-backend=machine %t/contains-result.ll -o - 2>&1 | FileCheck %s --check-prefix=CONTAINS-RESULT-MACHINE
; RUN: not --crash llc -mtriple=cd-unknown-unknown %t/slice-arity.ll -o - 2>&1 | FileCheck %s --check-prefix=SLICE-ARITY-DIRECT
; RUN: not --crash llc -mtriple=cd-unknown-unknown -cd-backend=machine %t/slice-arity.ll -o - 2>&1 | FileCheck %s --check-prefix=SLICE-ARITY-MACHINE
; RUN: not --crash llc -mtriple=cd-unknown-unknown %t/slice-start-type.ll -o - 2>&1 | FileCheck %s --check-prefix=SLICE-START-TYPE-DIRECT
; RUN: not --crash llc -mtriple=cd-unknown-unknown -cd-backend=machine %t/slice-start-type.ll -o - 2>&1 | FileCheck %s --check-prefix=SLICE-START-TYPE-MACHINE
; RUN: not --crash llc -mtriple=cd-unknown-unknown %t/slice-length-type.ll -o - 2>&1 | FileCheck %s --check-prefix=SLICE-LENGTH-TYPE-DIRECT
; RUN: not --crash llc -mtriple=cd-unknown-unknown -cd-backend=machine %t/slice-length-type.ll -o - 2>&1 | FileCheck %s --check-prefix=SLICE-LENGTH-TYPE-MACHINE
; RUN: not --crash llc -mtriple=cd-unknown-unknown %t/slice-collection-pointer.ll -o - 2>&1 | FileCheck %s --check-prefix=SLICE-COLLECTION-DIRECT
; RUN: not --crash llc -mtriple=cd-unknown-unknown -cd-backend=machine %t/slice-collection-pointer.ll -o - 2>&1 | FileCheck %s --check-prefix=SLICE-COLLECTION-MACHINE
; RUN: not --crash llc -mtriple=cd-unknown-unknown %t/slice-result.ll -o - 2>&1 | FileCheck %s --check-prefix=SLICE-RESULT-DIRECT
; RUN: not --crash llc -mtriple=cd-unknown-unknown -cd-backend=machine %t/slice-result.ll -o - 2>&1 | FileCheck %s --check-prefix=SLICE-RESULT-MACHINE

; UNKNOWN-DIRECT: CD target does not support LLVM operation: llvm.cd.native native name is not supported by the bounded CD ABI: mystery
; UNKNOWN-MACHINE: CD machine backend does not support llvm.cd.native native name is not supported by the bounded CD ABI: mystery
; MAP-SHAPE-DIRECT: CD target does not support LLVM operation: llvm.cd.native map requires a CD dynamic-value array, a direct callback, and a ptr result
; MAP-SHAPE-MACHINE: CD machine backend does not support llvm.cd.native map requires a CD dynamic-value array, a direct callback, and a ptr result
; MAP-POINTER-DIRECT: CD target does not support LLVM operation: llvm.cd.native map requires a CD dynamic-value array, a direct callback, and a ptr result
; MAP-POINTER-MACHINE: CD machine backend does not support llvm.cd.native map requires a CD dynamic-value array, a direct callback, and a ptr result
; MAP-CALLBACK-DIRECT: CD target does not support LLVM operation: llvm.cd.native map requires a direct defined callback with one address-space-zero CD parameter and a cd.value.return pointer result
; MAP-CALLBACK-MACHINE: CD machine backend does not support llvm.cd.native map requires a direct defined callback with one address-space-zero CD parameter and a cd.value.return pointer result
; MAP-CALLBACK-DECL-DIRECT: CD target does not support LLVM operation: llvm.cd.native map requires a direct defined callback with one address-space-zero CD parameter and a cd.value.return pointer result
; MAP-CALLBACK-DECL-MACHINE: CD machine backend does not support llvm.cd.native map requires a direct defined callback with one address-space-zero CD parameter and a cd.value.return pointer result
; MAP-CALLBACK-CAST-DIRECT: CD target does not support LLVM operation: llvm.cd.native map requires a direct defined callback with one address-space-zero CD parameter and a cd.value.return pointer result
; MAP-CALLBACK-CAST-MACHINE: CD machine backend does not support llvm.cd.native map requires a direct defined callback with one address-space-zero CD parameter and a cd.value.return pointer result
; MAP-CALLBACK-MAIN-DIRECT: CD target does not support LLVM operation: llvm.cd.native map requires a direct defined callback with one address-space-zero CD parameter and a cd.value.return pointer result
; MAP-CALLBACK-MAIN-MACHINE: CD machine backend does not support llvm.cd.native map requires a direct defined callback with one address-space-zero CD parameter and a cd.value.return pointer result
; FILTER-SHAPE-DIRECT: CD target does not support LLVM operation: llvm.cd.native filter requires a CD dynamic-value array, a direct callback, and a ptr result
; FILTER-SHAPE-MACHINE: CD machine backend does not support llvm.cd.native filter requires a CD dynamic-value array, a direct callback, and a ptr result
; FILTER-POINTER-DIRECT: CD target does not support LLVM operation: llvm.cd.native filter requires a CD dynamic-value array, a direct callback, and a ptr result
; FILTER-POINTER-MACHINE: CD machine backend does not support llvm.cd.native filter requires a CD dynamic-value array, a direct callback, and a ptr result
; FILTER-CALLBACK-DIRECT: CD target does not support LLVM operation: llvm.cd.native filter requires a direct defined callback with one address-space-zero CD parameter and an i1 result
; FILTER-CALLBACK-MACHINE: CD machine backend does not support llvm.cd.native filter requires a direct defined callback with one address-space-zero CD parameter and an i1 result
; FILTER-CALLBACK-DECL-DIRECT: CD target does not support LLVM operation: llvm.cd.native filter requires a direct defined callback with one address-space-zero CD parameter and an i1 result
; FILTER-CALLBACK-DECL-MACHINE: CD machine backend does not support llvm.cd.native filter requires a direct defined callback with one address-space-zero CD parameter and an i1 result
; FILTER-CALLBACK-CAST-DIRECT: CD target does not support LLVM operation: llvm.cd.native filter requires a direct defined callback with one address-space-zero CD parameter and an i1 result
; FILTER-CALLBACK-CAST-MACHINE: CD machine backend does not support llvm.cd.native filter requires a direct defined callback with one address-space-zero CD parameter and an i1 result
; FILTER-CALLBACK-MAIN-DIRECT: CD target does not support LLVM operation: llvm.cd.native filter requires a direct defined callback with one address-space-zero CD parameter and an i1 result
; FILTER-CALLBACK-MAIN-MACHINE: CD machine backend does not support llvm.cd.native filter requires a direct defined callback with one address-space-zero CD parameter and an i1 result
; FILTER-CALLBACK-RETURN-DIRECT: CD target does not support LLVM operation: cd.value.return requires an address-space-zero pointer return
; FILTER-CALLBACK-RETURN-MACHINE: CD machine backend does not support cd.value.return requires an address-space-zero pointer return
; FLOOR-ARITY-DIRECT: CD target does not support LLVM operation: llvm.cd.native floor requires one double argument and a double result
; FLOOR-ARITY-MACHINE: CD machine backend does not support llvm.cd.native floor requires one double argument and a double result
; FLOOR-TYPE-DIRECT: CD target does not support LLVM operation: llvm.cd.native floor requires one double argument and a double result
; FLOOR-TYPE-MACHINE: CD machine backend does not support llvm.cd.native floor requires one double argument and a double result
; STRING-POINTER-DIRECT: CD target does not support LLVM operation: llvm.cd.native str requires one scalar or CD dynamic-value argument and a ptr result
; STRING-POINTER-MACHINE: CD machine backend does not support llvm.cd.native str requires one scalar or CD dynamic-value argument and a ptr result
; BAD-NAME-DIRECT: CD target does not support LLVM operation: llvm.cd.native requires a private non-empty string global native name
; BAD-NAME-MACHINE: CD machine backend does not support llvm.cd.native requires a private non-empty string global native name
; RANGE-ARITY-DIRECT: CD target does not support LLVM operation: llvm.cd.native range requires one to three double arguments and a ptr result
; RANGE-ARITY-MACHINE: CD machine backend does not support llvm.cd.native range requires one to three double arguments and a ptr result
; FLOOR-RESULT-DIRECT: CD target does not support LLVM operation: llvm.cd.native floor requires one double argument and a double result
; FLOOR-RESULT-MACHINE: CD machine backend does not support llvm.cd.native floor requires one double argument and a double result
; SUBSTR-ARITY-DIRECT: CD target does not support LLVM operation: llvm.cd.native substr requires a CD string value, two double arguments, and a ptr result
; SUBSTR-ARITY-MACHINE: CD machine backend does not support llvm.cd.native substr requires a CD string value, two double arguments, and a ptr result
; SUBSTR-INDEX-TYPE-DIRECT: CD target does not support LLVM operation: llvm.cd.native substr requires a CD string value, two double arguments, and a ptr result
; SUBSTR-INDEX-TYPE-MACHINE: CD machine backend does not support llvm.cd.native substr requires a CD string value, two double arguments, and a ptr result
; SUBSTR-SCALAR-DIRECT: CD target does not support LLVM operation: llvm.cd.native substr requires a CD string value, two double arguments, and a ptr result
; SUBSTR-SCALAR-MACHINE: CD machine backend does not support llvm.cd.native substr requires a CD string value, two double arguments, and a ptr result
; SUBSTR-POINTER-DIRECT: CD target does not support LLVM operation: llvm.cd.native substr requires a CD string value, two double arguments, and a ptr result
; SUBSTR-POINTER-MACHINE: CD machine backend does not support llvm.cd.native substr requires a CD string value, two double arguments, and a ptr result
; SUBSTR-RESULT-DIRECT: CD target does not support LLVM operation: llvm.cd.native substr requires a CD string value, two double arguments, and a ptr result
; SUBSTR-RESULT-MACHINE: CD machine backend does not support llvm.cd.native substr requires a CD string value, two double arguments, and a ptr result
; CHAR-AT-ARITY-DIRECT: CD target does not support LLVM operation: llvm.cd.native charAt requires a CD string value, one double argument, and a ptr result
; CHAR-AT-ARITY-MACHINE: CD machine backend does not support llvm.cd.native charAt requires a CD string value, one double argument, and a ptr result
; CHAR-AT-INDEX-TYPE-DIRECT: CD target does not support LLVM operation: llvm.cd.native charAt requires a CD string value, one double argument, and a ptr result
; CHAR-AT-INDEX-TYPE-MACHINE: CD machine backend does not support llvm.cd.native charAt requires a CD string value, one double argument, and a ptr result
; CHAR-AT-SCALAR-DIRECT: CD target does not support LLVM operation: llvm.cd.native charAt requires a CD string value, one double argument, and a ptr result
; CHAR-AT-SCALAR-MACHINE: CD machine backend does not support llvm.cd.native charAt requires a CD string value, one double argument, and a ptr result
; CHAR-AT-POINTER-DIRECT: CD target does not support LLVM operation: llvm.cd.native charAt requires a CD string value, one double argument, and a ptr result
; CHAR-AT-POINTER-MACHINE: CD machine backend does not support llvm.cd.native charAt requires a CD string value, one double argument, and a ptr result
; CHAR-AT-RESULT-DIRECT: CD target does not support LLVM operation: llvm.cd.native charAt requires a CD string value, one double argument, and a ptr result
; CHAR-AT-RESULT-MACHINE: CD machine backend does not support llvm.cd.native charAt requires a CD string value, one double argument, and a ptr result
; CONTAINS-SHAPE-DIRECT: CD target does not support LLVM operation: llvm.cd.native contains requires a CD dynamic-value collection, a scalar or CD dynamic-value needle, and an i1 result
; CONTAINS-SHAPE-MACHINE: CD machine backend does not support llvm.cd.native contains requires a CD dynamic-value collection, a scalar or CD dynamic-value needle, and an i1 result
; CONTAINS-COLLECTION-DIRECT: CD target does not support LLVM operation: llvm.cd.native contains requires a CD dynamic-value collection, a scalar or CD dynamic-value needle, and an i1 result
; CONTAINS-COLLECTION-MACHINE: CD machine backend does not support llvm.cd.native contains requires a CD dynamic-value collection, a scalar or CD dynamic-value needle, and an i1 result
; CONTAINS-NEEDLE-DIRECT: CD target does not support LLVM operation: llvm.cd.native contains requires a CD dynamic-value collection, a scalar or CD dynamic-value needle, and an i1 result
; CONTAINS-NEEDLE-MACHINE: CD machine backend does not support llvm.cd.native contains requires a CD dynamic-value collection, a scalar or CD dynamic-value needle, and an i1 result
; CONTAINS-RESULT-DIRECT: CD target does not support LLVM operation: llvm.cd.native contains requires a CD dynamic-value collection, a scalar or CD dynamic-value needle, and an i1 result
; CONTAINS-RESULT-MACHINE: CD machine backend does not support llvm.cd.native contains requires a CD dynamic-value collection, a scalar or CD dynamic-value needle, and an i1 result
; SLICE-ARITY-DIRECT: CD target does not support LLVM operation: llvm.cd.native slice requires a CD dynamic-value array, two double arguments, and a ptr result
; SLICE-ARITY-MACHINE: CD machine backend does not support llvm.cd.native slice requires a CD dynamic-value array, two double arguments, and a ptr result
; SLICE-START-TYPE-DIRECT: CD target does not support LLVM operation: llvm.cd.native slice requires a CD dynamic-value array, two double arguments, and a ptr result
; SLICE-START-TYPE-MACHINE: CD machine backend does not support llvm.cd.native slice requires a CD dynamic-value array, two double arguments, and a ptr result
; SLICE-LENGTH-TYPE-DIRECT: CD target does not support LLVM operation: llvm.cd.native slice requires a CD dynamic-value array, two double arguments, and a ptr result
; SLICE-LENGTH-TYPE-MACHINE: CD machine backend does not support llvm.cd.native slice requires a CD dynamic-value array, two double arguments, and a ptr result
; SLICE-COLLECTION-DIRECT: CD target does not support LLVM operation: llvm.cd.native slice requires a CD dynamic-value array, two double arguments, and a ptr result
; SLICE-COLLECTION-MACHINE: CD machine backend does not support llvm.cd.native slice requires a CD dynamic-value array, two double arguments, and a ptr result
; SLICE-RESULT-DIRECT: CD target does not support LLVM operation: llvm.cd.native slice requires a CD dynamic-value array, two double arguments, and a ptr result
; SLICE-RESULT-MACHINE: CD machine backend does not support llvm.cd.native slice requires a CD dynamic-value array, two double arguments, and a ptr result

;--- unknown-name.ll
@name = private unnamed_addr constant [8 x i8] c"mystery\00"
declare double @llvm.cd.native(ptr, ...)
define i32 @main() {
entry:
  %value = call double (ptr, ...) @llvm.cd.native(ptr @name, double 1.0)
  ret i32 0
}

;--- map-shape.ll
@name = private unnamed_addr constant [4 x i8] c"map\00"
declare ptr @llvm.cd.native(ptr, ...)
define i32 @main() {
entry:
  %value = call ptr (ptr, ...) @llvm.cd.native(ptr @name, i64 1)
  ret i32 0
}

;--- map-pointer.ll
@name = private unnamed_addr constant [4 x i8] c"map\00"
declare ptr @llvm.cd.native(ptr, ...)
define ptr @callback(ptr %value) #0 {
entry:
  ret ptr %value
}
define i32 @main() {
entry:
  %source = inttoptr i64 1 to ptr
  %mapped = call ptr (ptr, ...) @llvm.cd.native(
      ptr @name, ptr %source, ptr @callback)
  ret i32 0
}
attributes #0 = { "cd.value.params"="0" "cd.value.return" }

;--- map-callback.ll
@name = private unnamed_addr constant [4 x i8] c"map\00"
@item = private unnamed_addr constant [5 x i8] c"item\00"
declare ptr @llvm.cd.native(ptr, ...)
declare ptr @llvm.cd.array(i32, ...)
declare ptr @llvm.cd.string(ptr)

define i64 @callback(i64 %value) {
entry:
  ret i64 %value
}

define i32 @main() {
entry:
  %value = call ptr @llvm.cd.string(ptr @item)
  %array = call ptr (i32, ...) @llvm.cd.array(i32 1, ptr %value)
  %mapped = call ptr (ptr, ...) @llvm.cd.native(
      ptr @name, ptr %array, ptr @callback)
  ret i32 0
}

;--- map-callback-declaration.ll
@name = private unnamed_addr constant [4 x i8] c"map\00"
@item = private unnamed_addr constant [5 x i8] c"item\00"
declare ptr @llvm.cd.native(ptr, ...)
declare ptr @llvm.cd.array(i32, ...)
declare ptr @llvm.cd.string(ptr)
declare ptr @callback(ptr) #0
define i32 @main() {
entry:
  %value = call ptr @llvm.cd.string(ptr @item)
  %array = call ptr (i32, ...) @llvm.cd.array(i32 1, ptr %value)
  %mapped = call ptr (ptr, ...) @llvm.cd.native(
      ptr @name, ptr %array, ptr @callback)
  ret i32 0
}
attributes #0 = { "cd.value.params"="0" "cd.value.return" }

;--- map-callback-cast.ll
@name = private unnamed_addr constant [4 x i8] c"map\00"
@item = private unnamed_addr constant [5 x i8] c"item\00"
declare ptr @llvm.cd.native(ptr, ...)
declare ptr @llvm.cd.array(i32, ...)
declare ptr @llvm.cd.string(ptr)
define ptr @callback(ptr %value) #0 {
entry:
  ret ptr %value
}
define i32 @main() {
entry:
  %value = call ptr @llvm.cd.string(ptr @item)
  %array = call ptr (i32, ...) @llvm.cd.array(i32 1, ptr %value)
  %cast = addrspacecast ptr @callback to ptr addrspace(1)
  %mapped = call ptr (ptr, ...) @llvm.cd.native(
      ptr @name, ptr %array, ptr addrspace(1) %cast)
  ret i32 0
}
attributes #0 = { "cd.value.params"="0" "cd.value.return" }

;--- map-callback-main.ll
@name = private unnamed_addr constant [4 x i8] c"map\00"
@item = private unnamed_addr constant [5 x i8] c"item\00"
declare ptr @llvm.cd.native(ptr, ...)
declare ptr @llvm.cd.array(i32, ...)
declare ptr @llvm.cd.string(ptr)
define i32 @main() {
entry:
  %value = call ptr @llvm.cd.string(ptr @item)
  %array = call ptr (i32, ...) @llvm.cd.array(i32 1, ptr %value)
  %mapped = call ptr (ptr, ...) @llvm.cd.native(
      ptr @name, ptr %array, ptr @main)
  ret i32 0
}

;--- filter-shape.ll
@name = private unnamed_addr constant [7 x i8] c"filter\00"
declare ptr @llvm.cd.native(ptr, ...)
define i32 @main() {
entry:
  %value = call ptr (ptr, ...) @llvm.cd.native(ptr @name, i64 1)
  ret i32 0
}

;--- filter-pointer.ll
@name = private unnamed_addr constant [7 x i8] c"filter\00"
declare ptr @llvm.cd.native(ptr, ...)
define i1 @callback(ptr %value) #0 {
entry:
  ret i1 true
}
define i32 @main() {
entry:
  %source = inttoptr i64 1 to ptr
  %filtered = call ptr (ptr, ...) @llvm.cd.native(
      ptr @name, ptr %source, ptr @callback)
  ret i32 0
}
attributes #0 = { "cd.value.params"="0" }

;--- filter-callback.ll
@name = private unnamed_addr constant [7 x i8] c"filter\00"
@item = private unnamed_addr constant [5 x i8] c"item\00"
declare ptr @llvm.cd.native(ptr, ...)
declare ptr @llvm.cd.array(i32, ...)
declare ptr @llvm.cd.string(ptr)

define ptr @callback(ptr %value) #0 {
entry:
  ret ptr %value
}

define i32 @main() {
entry:
  %value = call ptr @llvm.cd.string(ptr @item)
  %array = call ptr (i32, ...) @llvm.cd.array(i32 1, ptr %value)
  %filtered = call ptr (ptr, ...) @llvm.cd.native(
      ptr @name, ptr %array, ptr @callback)
  ret i32 0
}
attributes #0 = { "cd.value.params"="0" "cd.value.return" }

;--- filter-callback-declaration.ll
@name = private unnamed_addr constant [7 x i8] c"filter\00"
@item = private unnamed_addr constant [5 x i8] c"item\00"
declare ptr @llvm.cd.native(ptr, ...)
declare ptr @llvm.cd.array(i32, ...)
declare ptr @llvm.cd.string(ptr)
declare i1 @callback(ptr) #0
define i32 @main() {
entry:
  %value = call ptr @llvm.cd.string(ptr @item)
  %array = call ptr (i32, ...) @llvm.cd.array(i32 1, ptr %value)
  %filtered = call ptr (ptr, ...) @llvm.cd.native(
      ptr @name, ptr %array, ptr @callback)
  ret i32 0
}
attributes #0 = { "cd.value.params"="0" }

;--- filter-callback-cast.ll
@name = private unnamed_addr constant [7 x i8] c"filter\00"
@item = private unnamed_addr constant [5 x i8] c"item\00"
declare ptr @llvm.cd.native(ptr, ...)
declare ptr @llvm.cd.array(i32, ...)
declare ptr @llvm.cd.string(ptr)
define i1 @callback(ptr %value) #0 {
entry:
  ret i1 true
}
define i32 @main() {
entry:
  %value = call ptr @llvm.cd.string(ptr @item)
  %array = call ptr (i32, ...) @llvm.cd.array(i32 1, ptr %value)
  %cast = addrspacecast ptr @callback to ptr addrspace(1)
  %filtered = call ptr (ptr, ...) @llvm.cd.native(
      ptr @name, ptr %array, ptr addrspace(1) %cast)
  ret i32 0
}
attributes #0 = { "cd.value.params"="0" }

;--- filter-callback-main.ll
@name = private unnamed_addr constant [7 x i8] c"filter\00"
@item = private unnamed_addr constant [5 x i8] c"item\00"
declare ptr @llvm.cd.native(ptr, ...)
declare ptr @llvm.cd.array(i32, ...)
declare ptr @llvm.cd.string(ptr)
define i32 @main() {
entry:
  %value = call ptr @llvm.cd.string(ptr @item)
  %array = call ptr (i32, ...) @llvm.cd.array(i32 1, ptr %value)
  %filtered = call ptr (ptr, ...) @llvm.cd.native(
      ptr @name, ptr %array, ptr @main)
  ret i32 0
}

;--- filter-callback-return-marker.ll
@name = private unnamed_addr constant [7 x i8] c"filter\00"
@item = private unnamed_addr constant [5 x i8] c"item\00"
declare ptr @llvm.cd.native(ptr, ...)
declare ptr @llvm.cd.array(i32, ...)
declare ptr @llvm.cd.string(ptr)
define i1 @callback(ptr %value) #0 {
entry:
  ret i1 true
}
define i32 @main() {
entry:
  %value = call ptr @llvm.cd.string(ptr @item)
  %array = call ptr (i32, ...) @llvm.cd.array(i32 1, ptr %value)
  %filtered = call ptr (ptr, ...) @llvm.cd.native(
      ptr @name, ptr %array, ptr @callback)
  ret i32 0
}
attributes #0 = { "cd.value.params"="0" "cd.value.return" }

;--- floor-arity.ll
@name = private unnamed_addr constant [6 x i8] c"floor\00"
declare double @llvm.cd.native(ptr, ...)
define i32 @main() {
entry:
  %value = call double (ptr, ...) @llvm.cd.native(ptr @name)
  ret i32 0
}

;--- floor-type.ll
@name = private unnamed_addr constant [6 x i8] c"floor\00"
declare double @llvm.cd.native(ptr, ...)
define i32 @main() {
entry:
  %value = call double (ptr, ...) @llvm.cd.native(ptr @name, i64 1)
  ret i32 0
}

;--- string-pointer.ll
@name = private unnamed_addr constant [4 x i8] c"str\00"
declare ptr @llvm.cd.native(ptr, ...)
define i32 @main() {
entry:
  %slot = alloca i8
  %value = call ptr (ptr, ...) @llvm.cd.native(ptr @name, ptr %slot)
  ret i32 0
}

;--- bad-name-global.ll
@name = global [6 x i8] c"floor\00"
declare double @llvm.cd.native(ptr, ...)
define i32 @main() {
entry:
  %value = call double (ptr, ...) @llvm.cd.native(ptr @name, double 1.0)
  ret i32 0
}

;--- range-arity.ll
@name = private unnamed_addr constant [6 x i8] c"range\00"
declare ptr @llvm.cd.native(ptr, ...)
define i32 @main() {
entry:
  %value = call ptr (ptr, ...) @llvm.cd.native(
      ptr @name, double 1.0, double 2.0, double 3.0, double 4.0)
  ret i32 0
}

;--- floor-result.ll
@name = private unnamed_addr constant [6 x i8] c"floor\00"
declare ptr @llvm.cd.native(ptr, ...)
define i32 @main() {
entry:
  %value = call ptr (ptr, ...) @llvm.cd.native(ptr @name, double 1.0)
  ret i32 0
}

;--- substr-arity.ll
@name = private unnamed_addr constant [7 x i8] c"substr\00"
@message = private unnamed_addr constant [6 x i8] c"hello\00"
declare ptr @llvm.cd.native(ptr, ...)
declare ptr @llvm.cd.string(ptr)
define i32 @main() {
entry:
  %source = call ptr @llvm.cd.string(ptr @message)
  %value = call ptr (ptr, ...) @llvm.cd.native(ptr @name, ptr %source, double 1.0)
  ret i32 0
}

;--- substr-index-type.ll
@name = private unnamed_addr constant [7 x i8] c"substr\00"
@message = private unnamed_addr constant [6 x i8] c"hello\00"
declare ptr @llvm.cd.native(ptr, ...)
declare ptr @llvm.cd.string(ptr)
define i32 @main() {
entry:
  %source = call ptr @llvm.cd.string(ptr @message)
  %value = call ptr (ptr, ...) @llvm.cd.native(ptr @name, ptr %source, i64 1, double 1.0)
  ret i32 0
}

;--- substr-scalar.ll
@name = private unnamed_addr constant [7 x i8] c"substr\00"
declare ptr @llvm.cd.native(ptr, ...)
define i32 @main() {
entry:
  %value = call ptr (ptr, ...) @llvm.cd.native(ptr @name, i64 1, double 1.0, double 1.0)
  ret i32 0
}

;--- substr-pointer.ll
@name = private unnamed_addr constant [7 x i8] c"substr\00"
declare ptr @llvm.cd.native(ptr, ...)
define i32 @main() {
entry:
  %slot = alloca i8
  %value = call ptr (ptr, ...) @llvm.cd.native(ptr @name, ptr %slot, double 1.0, double 1.0)
  ret i32 0
}

;--- substr-result.ll
@name = private unnamed_addr constant [7 x i8] c"substr\00"
@message = private unnamed_addr constant [6 x i8] c"hello\00"
declare double @llvm.cd.native(ptr, ...)
declare ptr @llvm.cd.string(ptr)
define i32 @main() {
entry:
  %source = call ptr @llvm.cd.string(ptr @message)
  %value = call double (ptr, ...) @llvm.cd.native(ptr @name, ptr %source, double 1.0, double 1.0)
  ret i32 0
}

;--- char-at-arity.ll
@name = private unnamed_addr constant [7 x i8] c"charAt\00"
@message = private unnamed_addr constant [6 x i8] c"hello\00"
declare ptr @llvm.cd.native(ptr, ...)
declare ptr @llvm.cd.string(ptr)
define i32 @main() {
entry:
  %source = call ptr @llvm.cd.string(ptr @message)
  %value = call ptr (ptr, ...) @llvm.cd.native(ptr @name, ptr %source)
  ret i32 0
}

;--- char-at-index-type.ll
@name = private unnamed_addr constant [7 x i8] c"charAt\00"
@message = private unnamed_addr constant [6 x i8] c"hello\00"
declare ptr @llvm.cd.native(ptr, ...)
declare ptr @llvm.cd.string(ptr)
define i32 @main() {
entry:
  %source = call ptr @llvm.cd.string(ptr @message)
  %value = call ptr (ptr, ...) @llvm.cd.native(ptr @name, ptr %source, i64 1)
  ret i32 0
}

;--- char-at-scalar.ll
@name = private unnamed_addr constant [7 x i8] c"charAt\00"
declare ptr @llvm.cd.native(ptr, ...)
define i32 @main() {
entry:
  %value = call ptr (ptr, ...) @llvm.cd.native(ptr @name, i64 1, double 1.0)
  ret i32 0
}

;--- char-at-pointer.ll
@name = private unnamed_addr constant [7 x i8] c"charAt\00"
declare ptr @llvm.cd.native(ptr, ...)
define i32 @main() {
entry:
  %slot = alloca i8
  %value = call ptr (ptr, ...) @llvm.cd.native(ptr @name, ptr %slot, double 1.0)
  ret i32 0
}

;--- char-at-result.ll
@name = private unnamed_addr constant [7 x i8] c"charAt\00"
@message = private unnamed_addr constant [6 x i8] c"hello\00"
declare double @llvm.cd.native(ptr, ...)
declare ptr @llvm.cd.string(ptr)
define i32 @main() {
entry:
  %source = call ptr @llvm.cd.string(ptr @message)
  %value = call double (ptr, ...) @llvm.cd.native(ptr @name, ptr %source, double 1.0)
  ret i32 0
}

;--- contains-shape.ll
@name = private unnamed_addr constant [9 x i8] c"contains\00"
declare i1 @llvm.cd.native(ptr, ...)
define i32 @main() {
entry:
  %value = call i1 (ptr, ...) @llvm.cd.native(ptr @name, i64 1)
  ret i32 0
}

;--- contains-collection-pointer.ll
@name = private unnamed_addr constant [9 x i8] c"contains\00"
declare i1 @llvm.cd.native(ptr, ...)
define i32 @main() {
entry:
  %collection = inttoptr i64 1 to ptr
  %value = call i1 (ptr, ...) @llvm.cd.native(
      ptr @name, ptr %collection, i64 1)
  ret i32 0
}

;--- contains-needle-pointer.ll
@name = private unnamed_addr constant [9 x i8] c"contains\00"
@item = private unnamed_addr constant [5 x i8] c"item\00"
declare i1 @llvm.cd.native(ptr, ...)
declare ptr @llvm.cd.array(i32, ...)
declare ptr @llvm.cd.string(ptr)
define i32 @main() {
entry:
  %item_value = call ptr @llvm.cd.string(ptr @item)
  %array = call ptr (i32, ...) @llvm.cd.array(i32 1, ptr %item_value)
  %slot = alloca i8
  %value = call i1 (ptr, ...) @llvm.cd.native(
      ptr @name, ptr %array, ptr %slot)
  ret i32 0
}

;--- contains-result.ll
@name = private unnamed_addr constant [9 x i8] c"contains\00"
declare ptr @llvm.cd.native(ptr, ...)
define i32 @main() {
entry:
  %value = call ptr (ptr, ...) @llvm.cd.native(
      ptr @name, ptr null, i64 1)
  ret i32 0
}

;--- slice-arity.ll
@name = private unnamed_addr constant [6 x i8] c"slice\00"
declare ptr @llvm.cd.native(ptr, ...)
define i32 @main() {
entry:
  %value = call ptr (ptr, ...) @llvm.cd.native(
      ptr @name, ptr null, double 0.0)
  ret i32 0
}

;--- slice-start-type.ll
@name = private unnamed_addr constant [6 x i8] c"slice\00"
declare ptr @llvm.cd.native(ptr, ...)
define i32 @main() {
entry:
  %value = call ptr (ptr, ...) @llvm.cd.native(
      ptr @name, ptr null, i64 0, double 1.0)
  ret i32 0
}

;--- slice-length-type.ll
@name = private unnamed_addr constant [6 x i8] c"slice\00"
declare ptr @llvm.cd.native(ptr, ...)
define i32 @main() {
entry:
  %value = call ptr (ptr, ...) @llvm.cd.native(
      ptr @name, ptr null, double 0.0, i64 1)
  ret i32 0
}

;--- slice-collection-pointer.ll
@name = private unnamed_addr constant [6 x i8] c"slice\00"
declare ptr @llvm.cd.native(ptr, ...)
define i32 @main() {
entry:
  %collection = inttoptr i64 1 to ptr
  %value = call ptr (ptr, ...) @llvm.cd.native(
      ptr @name, ptr %collection, double 0.0, double 1.0)
  ret i32 0
}

;--- slice-result.ll
@name = private unnamed_addr constant [6 x i8] c"slice\00"
declare double @llvm.cd.native(ptr, ...)
define i32 @main() {
entry:
  %value = call double (ptr, ...) @llvm.cd.native(
      ptr @name, ptr null, double 0.0, double 1.0)
  ret i32 0
}
