; RUN: split-file %s %t
; RUN: not --crash llc -mtriple=cd-unknown-unknown %t/shape.ll -o - 2>&1 | FileCheck %s --check-prefix=SHAPE-DIRECT
; RUN: not --crash llc -mtriple=cd-unknown-unknown -cd-backend=machine %t/shape.ll -o - 2>&1 | FileCheck %s --check-prefix=SHAPE-MACHINE
; RUN: not --crash llc -mtriple=cd-unknown-unknown %t/non-location.ll -o - 2>&1 | FileCheck %s --check-prefix=NON-LOCATION-DIRECT
; RUN: not --crash llc -mtriple=cd-unknown-unknown -cd-backend=machine %t/non-location.ll -o - 2>&1 | FileCheck %s --check-prefix=NON-LOCATION-MACHINE
; RUN: not --crash llc -mtriple=cd-unknown-unknown %t/non-integer.ll -o - 2>&1 | FileCheck %s --check-prefix=NON-INTEGER-DIRECT
; RUN: not --crash llc -mtriple=cd-unknown-unknown -cd-backend=machine %t/non-integer.ll -o - 2>&1 | FileCheck %s --check-prefix=NON-INTEGER-MACHINE
; RUN: not --crash llc -mtriple=cd-unknown-unknown %t/negative.ll -o - 2>&1 | FileCheck %s --check-prefix=NEGATIVE-DIRECT
; RUN: not --crash llc -mtriple=cd-unknown-unknown -cd-backend=machine %t/negative.ll -o - 2>&1 | FileCheck %s --check-prefix=NEGATIVE-MACHINE
; RUN: not --crash llc -mtriple=cd-unknown-unknown %t/reversed.ll -o - 2>&1 | FileCheck %s --check-prefix=REVERSED-DIRECT
; RUN: not --crash llc -mtriple=cd-unknown-unknown -cd-backend=machine %t/reversed.ll -o - 2>&1 | FileCheck %s --check-prefix=REVERSED-MACHINE
; RUN: not --crash llc -mtriple=cd-unknown-unknown %t/outside.ll -o - 2>&1 | FileCheck %s --check-prefix=OUTSIDE-DIRECT
; RUN: not --crash llc -mtriple=cd-unknown-unknown -cd-backend=machine %t/outside.ll -o - 2>&1 | FileCheck %s --check-prefix=OUTSIDE-MACHINE
; RUN: not --crash llc -mtriple=cd-unknown-unknown %t/no-source-location.ll -o - 2>&1 | FileCheck %s --check-prefix=NO-SOURCE-LOCATION-DIRECT
; RUN: not --crash llc -mtriple=cd-unknown-unknown -cd-backend=machine %t/no-source-location.ll -o - 2>&1 | FileCheck %s --check-prefix=NO-SOURCE-LOCATION-MACHINE
; RUN: not --crash llc -mtriple=cd-unknown-unknown %t/duplicate.ll -o - 2>&1 | FileCheck %s --check-prefix=DUPLICATE-DIRECT
; RUN: not --crash llc -mtriple=cd-unknown-unknown -cd-backend=machine %t/duplicate.ll -o - 2>&1 | FileCheck %s --check-prefix=DUPLICATE-MACHINE

; SHAPE-DIRECT: CD target does not support LLVM operation: llvm.cd.ranges record 0 must contain a DILocation and two byte offsets
; SHAPE-MACHINE: CD machine backend does not support llvm.cd.ranges record 0 must contain a DILocation and two byte offsets
; NON-LOCATION-DIRECT: CD target does not support LLVM operation: llvm.cd.ranges record 0 operand 0 must be a DILocation
; NON-LOCATION-MACHINE: CD machine backend does not support llvm.cd.ranges record 0 operand 0 must be a DILocation
; NON-INTEGER-DIRECT: CD target does not support LLVM operation: llvm.cd.ranges record 0 operand 1 must be a non-negative 64-bit integer byte offset
; NON-INTEGER-MACHINE: CD machine backend does not support llvm.cd.ranges record 0 operand 1 must be a non-negative 64-bit integer byte offset
; NEGATIVE-DIRECT: CD target does not support LLVM operation: llvm.cd.ranges record 0 operand 1 must be a non-negative 64-bit integer byte offset
; NEGATIVE-MACHINE: CD machine backend does not support llvm.cd.ranges record 0 operand 1 must be a non-negative 64-bit integer byte offset
; REVERSED-DIRECT: CD target does not support LLVM operation: llvm.cd.ranges record 0 has a reversed byte range
; REVERSED-MACHINE: CD machine backend does not support llvm.cd.ranges record 0 has a reversed byte range
; OUTSIDE-DIRECT: CD target does not support LLVM operation: llvm.cd.ranges record 0 exceeds the source text length
; OUTSIDE-MACHINE: CD machine backend does not support llvm.cd.ranges record 0 exceeds the source text length
; NO-SOURCE-LOCATION-DIRECT: CD target does not support LLVM operation: llvm.cd.ranges record 0 does not resolve to an explicit source-backed location
; NO-SOURCE-LOCATION-MACHINE: CD machine backend does not support llvm.cd.ranges record 0 does not resolve to an explicit source-backed location
; DUPLICATE-DIRECT: CD target does not support LLVM operation: llvm.cd.ranges record 1 duplicates a DILocation range
; DUPLICATE-MACHINE: CD machine backend does not support llvm.cd.ranges record 1 duplicates a DILocation range

;--- shape.ll
define i32 @main() !dbg !5 {
entry:
  ret i32 0, !dbg !10
}
!cd.sources = !{!20}
!20 = !{!"ranges.cd", !"x\0A"}
!cd.ranges = !{!40}
!40 = !{!10, i64 0}
!llvm.module.flags = !{!30}
!llvm.dbg.cu = !{!0}
!0 = distinct !DICompileUnit(language: DW_LANG_C, file: !1, producer: "cd",
    isOptimized: false, emissionKind: FullDebug)
!1 = !DIFile(filename: "ranges.cd", directory: "")
!2 = !{}
!3 = !DISubroutineType(types: !2)
!5 = distinct !DISubprogram(name: "main", scope: !1, file: !1, line: 1,
    type: !3, unit: !0, retainedNodes: !2)
!10 = !DILocation(line: 1, column: 1, scope: !5)
!30 = !{i32 2, !"Debug Info Version", i32 3}

;--- non-location.ll
define i32 @main() !dbg !5 {
entry:
  ret i32 0, !dbg !10
}
!cd.sources = !{!20}
!20 = !{!"ranges.cd", !"x\0A"}
!cd.ranges = !{!40}
!40 = !{!"not-location", i64 0, i64 1}
!llvm.module.flags = !{!30}
!llvm.dbg.cu = !{!0}
!0 = distinct !DICompileUnit(language: DW_LANG_C, file: !1, producer: "cd",
    isOptimized: false, emissionKind: FullDebug)
!1 = !DIFile(filename: "ranges.cd", directory: "")
!2 = !{}
!3 = !DISubroutineType(types: !2)
!5 = distinct !DISubprogram(name: "main", scope: !1, file: !1, line: 1,
    type: !3, unit: !0, retainedNodes: !2)
!10 = !DILocation(line: 1, column: 1, scope: !5)
!30 = !{i32 2, !"Debug Info Version", i32 3}

;--- non-integer.ll
define i32 @main() !dbg !5 {
entry:
  ret i32 0, !dbg !10
}
!cd.sources = !{!20}
!20 = !{!"ranges.cd", !"x\0A"}
!cd.ranges = !{!40}
!40 = !{!10, !"start", i64 1}
!llvm.module.flags = !{!30}
!llvm.dbg.cu = !{!0}
!0 = distinct !DICompileUnit(language: DW_LANG_C, file: !1, producer: "cd",
    isOptimized: false, emissionKind: FullDebug)
!1 = !DIFile(filename: "ranges.cd", directory: "")
!2 = !{}
!3 = !DISubroutineType(types: !2)
!5 = distinct !DISubprogram(name: "main", scope: !1, file: !1, line: 1,
    type: !3, unit: !0, retainedNodes: !2)
!10 = !DILocation(line: 1, column: 1, scope: !5)
!30 = !{i32 2, !"Debug Info Version", i32 3}

;--- negative.ll
define i32 @main() !dbg !5 {
entry:
  ret i32 0, !dbg !10
}
!cd.sources = !{!20}
!20 = !{!"ranges.cd", !"x\0A"}
!cd.ranges = !{!40}
!40 = !{!10, i64 -1, i64 1}
!llvm.module.flags = !{!30}
!llvm.dbg.cu = !{!0}
!0 = distinct !DICompileUnit(language: DW_LANG_C, file: !1, producer: "cd",
    isOptimized: false, emissionKind: FullDebug)
!1 = !DIFile(filename: "ranges.cd", directory: "")
!2 = !{}
!3 = !DISubroutineType(types: !2)
!5 = distinct !DISubprogram(name: "main", scope: !1, file: !1, line: 1,
    type: !3, unit: !0, retainedNodes: !2)
!10 = !DILocation(line: 1, column: 1, scope: !5)
!30 = !{i32 2, !"Debug Info Version", i32 3}

;--- reversed.ll
define i32 @main() !dbg !5 {
entry:
  ret i32 0, !dbg !10
}
!cd.sources = !{!20}
!20 = !{!"ranges.cd", !"x\0A"}
!cd.ranges = !{!40}
!40 = !{!10, i64 1, i64 0}
!llvm.module.flags = !{!30}
!llvm.dbg.cu = !{!0}
!0 = distinct !DICompileUnit(language: DW_LANG_C, file: !1, producer: "cd",
    isOptimized: false, emissionKind: FullDebug)
!1 = !DIFile(filename: "ranges.cd", directory: "")
!2 = !{}
!3 = !DISubroutineType(types: !2)
!5 = distinct !DISubprogram(name: "main", scope: !1, file: !1, line: 1,
    type: !3, unit: !0, retainedNodes: !2)
!10 = !DILocation(line: 1, column: 1, scope: !5)
!30 = !{i32 2, !"Debug Info Version", i32 3}

;--- outside.ll
define i32 @main() !dbg !5 {
entry:
  ret i32 0, !dbg !10
}
!cd.sources = !{!20}
!20 = !{!"ranges.cd", !"x\0A"}
!cd.ranges = !{!40}
!40 = !{!10, i64 0, i64 3}
!llvm.module.flags = !{!30}
!llvm.dbg.cu = !{!0}
!0 = distinct !DICompileUnit(language: DW_LANG_C, file: !1, producer: "cd",
    isOptimized: false, emissionKind: FullDebug)
!1 = !DIFile(filename: "ranges.cd", directory: "")
!2 = !{}
!3 = !DISubroutineType(types: !2)
!5 = distinct !DISubprogram(name: "main", scope: !1, file: !1, line: 1,
    type: !3, unit: !0, retainedNodes: !2)
!10 = !DILocation(line: 1, column: 1, scope: !5)
!30 = !{i32 2, !"Debug Info Version", i32 3}

;--- no-source-location.ll
define i32 @main() !dbg !5 {
entry:
  ret i32 0, !dbg !10
}
!cd.sources = !{!20}
!20 = !{!"ranges.cd", !"x\0A"}
!cd.ranges = !{!40}
!40 = !{!10, i64 0, i64 1}
!llvm.module.flags = !{!30}
!llvm.dbg.cu = !{!0}
!0 = distinct !DICompileUnit(language: DW_LANG_C, file: !1, producer: "cd",
    isOptimized: false, emissionKind: FullDebug)
!1 = !DIFile(filename: "other.cd", directory: "")
!2 = !{}
!3 = !DISubroutineType(types: !2)
!5 = distinct !DISubprogram(name: "main", scope: !1, file: !1, line: 1,
    type: !3, unit: !0, retainedNodes: !2)
!10 = !DILocation(line: 1, column: 1, scope: !5)
!30 = !{i32 2, !"Debug Info Version", i32 3}

;--- duplicate.ll
define i32 @main() !dbg !5 {
entry:
  ret i32 0, !dbg !10
}
!cd.sources = !{!20}
!20 = !{!"ranges.cd", !"x\0A"}
!cd.ranges = !{!40, !41}
!40 = !{!10, i64 0, i64 1}
!41 = !{!10, i64 0, i64 1}
!llvm.module.flags = !{!30}
!llvm.dbg.cu = !{!0}
!0 = distinct !DICompileUnit(language: DW_LANG_C, file: !1, producer: "cd",
    isOptimized: false, emissionKind: FullDebug)
!1 = !DIFile(filename: "ranges.cd", directory: "")
!2 = !{}
!3 = !DISubroutineType(types: !2)
!5 = distinct !DISubprogram(name: "main", scope: !1, file: !1, line: 1,
    type: !3, unit: !0, retainedNodes: !2)
!10 = !DILocation(line: 1, column: 1, scope: !5)
!30 = !{i32 2, !"Debug Info Version", i32 3}
