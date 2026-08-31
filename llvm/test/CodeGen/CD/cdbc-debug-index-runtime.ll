; RUN: llc -mtriple=cd-unknown-unknown %s -o %t.direct
; RUN: FileCheck --check-prefix=DIRECT %s < %t.direct
; RUN: llc -mtriple=cd-unknown-unknown -cd-backend=machine %s -o %t.machine
; RUN: FileCheck --check-prefix=MACHINE %s < %t.machine

declare ptr @llvm.cd.array(i32, ...)
declare ptr @llvm.cd.index(ptr, double)

define i32 @main() !dbg !5 {
entry:
  %array = call ptr (i32, ...) @llvm.cd.array(i32 1, i64 7)
  %value = call ptr @llvm.cd.index(ptr %array, double 1.0), !dbg !10
  ret i32 0, !dbg !11
}

!cd.sources = !{!20}
!20 = !{!"index.cd", !"let x = [7][1];\0A"}
!llvm.module.flags = !{!30}
!llvm.dbg.cu = !{!0}
!0 = distinct !DICompileUnit(language: DW_LANG_C, file: !1,
    producer: "cd", isOptimized: false, emissionKind: FullDebug)
!1 = !DIFile(filename: "index.cd", directory: "")
!2 = !{}
!3 = !DISubroutineType(types: !2)
!5 = distinct !DISubprogram(name: "main", scope: !1, file: !1, line: 1,
    type: !3, unit: !0, retainedNodes: !2)
!10 = !DILocation(line: 1, column: 14, scope: !5)
!11 = !DILocation(line: 1, column: 1, scope: !5)
!30 = !{i32 2, !"Debug Info Version", i32 3}

; DIRECT: debug_locations:
; DIRECT: main 4 = s0:1:14
; DIRECT: main 6 = s0:1:1
; MACHINE: debug_locations:
; MACHINE: main 4 = s0:1:14
; MACHINE: main 6 = s0:1:1
