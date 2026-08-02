; RUN: llc -mtriple=cd-unknown-unknown %s -o %t.direct
; RUN: FileCheck --check-prefix=DIRECT %s < %t.direct
; RUN: llc -mtriple=cd-unknown-unknown -cd-backend=machine %s -o %t.machine
; RUN: FileCheck --check-prefix=MACHINE %s < %t.machine

define i32 @main() !dbg !5 {
entry:
  %sum = add i32 40, 2, !dbg !10
  ret i32 %sum, !dbg !11
}

define i32 @add_one(i32 %value) !dbg !6 {
entry:
  %result = add i32 %value, 1, !dbg !12
  ret i32 %result, !dbg !13
}

!cd.sources = !{!20}
!20 = !{!"demo.cd", !"let sum = 40 + 2;\0A"}

!llvm.module.flags = !{!30}
!llvm.dbg.cu = !{!0}
!0 = distinct !DICompileUnit(language: DW_LANG_C, file: !1,
    producer: "cd", isOptimized: false, emissionKind: FullDebug)
!1 = !DIFile(filename: "demo.cd", directory: "")
!2 = !{}
!3 = !DISubroutineType(types: !2)
!5 = distinct !DISubprogram(name: "main", scope: !1, file: !1, line: 1,
    type: !3, unit: !0, retainedNodes: !2)
!6 = distinct !DISubprogram(name: "add_one", scope: !1, file: !1, line: 1,
    type: !3, unit: !0, retainedNodes: !2)
!10 = !DILocation(line: 1, column: 11, scope: !5)
!11 = !DILocation(line: 1, column: 1, scope: !5)
!12 = !DILocation(line: 1, column: 15, scope: !6)
!13 = !DILocation(line: 1, column: 1, scope: !6)
!30 = !{i32 2, !"Debug Info Version", i32 3}

; DIRECT: debug_locations:
; DIRECT: main 2 = s0:1:11
; DIRECT: main 3 = s0:1:1
; DIRECT: function f0 2 = s0:1:15
; DIRECT: function f0 3 = s0:1:1
; MACHINE: debug_locations:
; MACHINE: main 2 = s0:1:11
; MACHINE: main 3 = s0:1:1
; MACHINE: function f0 2 = s0:1:15
; MACHINE: function f0 3 = s0:1:1
