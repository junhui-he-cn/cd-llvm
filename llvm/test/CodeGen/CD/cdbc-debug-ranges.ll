; RUN: llc -mtriple=cd-unknown-unknown %s -o %t.direct
; RUN: FileCheck --check-prefix=DIRECT %s < %t.direct
; RUN: llc -mtriple=cd-unknown-unknown -cd-backend=machine %s -o %t.machine
; RUN: FileCheck --check-prefix=MACHINE %s < %t.machine

declare void @cd_print(double)

define i32 @main() !dbg !5 {
entry:
  %value = fdiv double 4.0, 2.0, !dbg !10
  call void @cd_print(double %value), !dbg !11
  ret i32 0, !dbg !12
}

define double @identity(double %input) !dbg !6 {
entry:
  %result = fadd double %input, 1.0, !dbg !13
  ret double %result, !dbg !14
}

!cd.sources = !{!20, !21}
!20 = !{!"ranges.cd", !"print 4 / 2;\0A"}
!21 = !{!"helper.cd", !"return value + 1;\0A"}
!cd.ranges = !{!40, !41, !42, !43}
!40 = !{!10, i64 6, i64 11}
!41 = !{!11, i64 0, i64 12}
!42 = !{!13, i64 0, i64 16}
!43 = !{!14, i64 0, i64 16}

!llvm.module.flags = !{!30}
!llvm.dbg.cu = !{!0}
!0 = distinct !DICompileUnit(language: DW_LANG_C, file: !1,
    producer: "cd", isOptimized: false, emissionKind: FullDebug)
!1 = !DIFile(filename: "ranges.cd", directory: "")
!4 = !DIFile(filename: "helper.cd", directory: "")
!2 = !{}
!3 = !DISubroutineType(types: !2)
!5 = distinct !DISubprogram(name: "main", scope: !1, file: !1, line: 1,
    type: !3, unit: !0, retainedNodes: !2)
!6 = distinct !DISubprogram(name: "identity", scope: !4, file: !4, line: 1,
    type: !3, unit: !0, retainedNodes: !2)
!10 = !DILocation(line: 1, column: 7, scope: !5)
!11 = !DILocation(line: 1, column: 1, scope: !5)
!12 = !DILocation(line: 1, column: 1, scope: !5)
!13 = !DILocation(line: 1, column: 8, scope: !6)
!14 = !DILocation(line: 1, column: 1, scope: !6)
!30 = !{i32 2, !"Debug Info Version", i32 3}

; DIRECT: debug_ranges:
; DIRECT: main 2 = s0:6:11
; DIRECT: main 3 = s0:0:12
; DIRECT: main 5 = s0:0:12
; DIRECT: function f0 2 = s1:0:16
; DIRECT: function f0 3 = s1:0:16
; MACHINE: debug_ranges:
; MACHINE: main 2 = s0:6:11
; MACHINE: main 3 = s0:0:12
; MACHINE: main 5 = s0:0:12
; MACHINE: function f0 2 = s1:0:16
; MACHINE: function f0 3 = s1:0:16
