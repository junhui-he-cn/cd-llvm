declare void @cd_print(double)

define i32 @main() {
entry:
  call void @cd_print(double 2.0)
  ret i32 0
}

!cd.module = !{!0}
!0 = !{!"/workspace/cd-llvm-dependency.cd", !"dependency.cd", !"/workspace/cd-llvm-dependency.cd", i1 false}
