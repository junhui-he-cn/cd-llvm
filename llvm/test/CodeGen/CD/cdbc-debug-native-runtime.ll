; RUN: llc -mtriple=cd-unknown-unknown %s -o %t.direct
; RUN: FileCheck --check-prefix=DIRECT %s < %t.direct
; RUN: llc -mtriple=cd-unknown-unknown -cd-backend=machine %s -o %t.machine
; RUN: FileCheck --check-prefix=MACHINE %s < %t.machine

@sqrt_name = private unnamed_addr constant [5 x i8] c"sqrt\00"

declare double @llvm.cd.native(ptr, ...)

define i32 @main() !dbg !5 {
entry:
  %value = call double (ptr, ...) @llvm.cd.native(ptr @sqrt_name, double -1.0), !dbg !10
  ret i32 0, !dbg !11
}

!cd.sources = !{!20}
!20 = !{!"native.cd", !"sqrt(-1);\0A"}
!llvm.module.flags = !{!30}
!llvm.dbg.cu = !{!0}
!0 = distinct !DICompileUnit(language: DW_LANG_C, file: !1,
    producer: "cd", isOptimized: false, emissionKind: FullDebug)
!1 = !DIFile(filename: "native.cd", directory: "")
!2 = !{}
!3 = !DISubroutineType(types: !2)
!5 = distinct !DISubprogram(name: "main", scope: !1, file: !1, line: 1,
    type: !3, unit: !0, retainedNodes: !2)
!10 = !DILocation(line: 1, column: 1, scope: !5)
!11 = !DILocation(line: 1, column: 1, scope: !5)
!30 = !{i32 2, !"Debug Info Version", i32 3}

; DIRECT: debug_locations:
; DIRECT: main 2 = s0:1:1
; DIRECT: main 4 = s0:1:1
; MACHINE: debug_locations:
; MACHINE: main 2 = s0:1:1
; MACHINE: main 4 = s0:1:1
