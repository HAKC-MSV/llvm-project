; ModuleID = '/home/al32163/hakc/HAKC_CURR/llvm-project/llvm/test/Transforms/Compartmentalization/hakc_test9/hakc_test9.c'
source_filename = "/home/al32163/hakc/HAKC_CURR/llvm-project/llvm/test/Transforms/Compartmentalization/hakc_test9/hakc_test9.c"
target datalayout = "e-m:e-p270:32:32-p271:32:32-p272:64:64-i64:64-i128:128-f80:128-n8:16:32:64-S128"
target triple = "x86_64-unknown-linux-gnu"

%struct.data_struct = type { i32 }

; Function Attrs: noinline nounwind optnone uwtable
define dso_local i32 @bar(ptr noundef %0) #0 !dbg !10 {
  %2 = alloca i32, align 4
  %3 = alloca ptr, align 8
  store ptr %0, ptr %3, align 8
    #dbg_declare(ptr %3, !20, !DIExpression(), !21)
  %4 = load ptr, ptr %3, align 8, !dbg !22
  %5 = icmp ne ptr %4, null, !dbg !22
  br i1 %5, label %6, label %11, !dbg !24

6:                                                ; preds = %1
  %7 = load ptr, ptr %3, align 8, !dbg !25
  %8 = getelementptr inbounds %struct.data_struct, ptr %7, i32 0, i32 0, !dbg !27
  %9 = load i32, ptr %8, align 4, !dbg !28
  %10 = add nsw i32 %9, 1, !dbg !28
  store i32 %10, ptr %8, align 4, !dbg !28
  store i32 %9, ptr %2, align 4, !dbg !29
  br label %16, !dbg !29

11:                                               ; preds = %1
  %12 = load ptr, ptr %3, align 8, !dbg !30
  %13 = getelementptr inbounds %struct.data_struct, ptr %12, i32 0, i32 0, !dbg !31
  %14 = load i32, ptr %13, align 4, !dbg !32
  %15 = add nsw i32 %14, -1, !dbg !32
  store i32 %15, ptr %13, align 4, !dbg !32
  store i32 %14, ptr %2, align 4, !dbg !33
  br label %16, !dbg !33

16:                                               ; preds = %11, %6
  %17 = load i32, ptr %2, align 4, !dbg !34
  ret i32 %17, !dbg !34
}

; Function Attrs: noinline nounwind optnone uwtable
define dso_local i32 @foo(ptr noundef %0) #0 !dbg !35 {
  %2 = alloca i32, align 4
  %3 = alloca ptr, align 8
  %4 = alloca ptr, align 8
  store ptr %0, ptr %3, align 8
    #dbg_declare(ptr %3, !36, !DIExpression(), !37)
  %5 = load ptr, ptr %3, align 8, !dbg !38
  %6 = icmp ne ptr %5, null, !dbg !38
  br i1 %6, label %7, label %12, !dbg !40

7:                                                ; preds = %1
    #dbg_declare(ptr %4, !41, !DIExpression(), !43)
  %8 = load ptr, ptr %4, align 8, !dbg !44
  %9 = getelementptr inbounds %struct.data_struct, ptr %8, i32 0, i32 0, !dbg !45
  store i32 0, ptr %9, align 4, !dbg !46
  %10 = load ptr, ptr %4, align 8, !dbg !47
  %11 = call i32 @bar(ptr noundef %10), !dbg !48
  store i32 %11, ptr %2, align 4, !dbg !49
  br label %13, !dbg !49

12:                                               ; preds = %1
  store i32 0, ptr %2, align 4, !dbg !50
  br label %13, !dbg !50

13:                                               ; preds = %12, %7
  %14 = load i32, ptr %2, align 4, !dbg !51
  ret i32 %14, !dbg !51
}

attributes #0 = { noinline nounwind optnone uwtable "frame-pointer"="all" "min-legal-vector-width"="0" "no-trapping-math"="true" "stack-protector-buffer-size"="8" "target-cpu"="x86-64" "target-features"="+cmov,+cx8,+fxsr,+mmx,+sse,+sse2,+x87" "tune-cpu"="generic" }

!llvm.dbg.cu = !{!0}
!llvm.module.flags = !{!2, !3, !4, !5, !6, !7, !8}
!llvm.ident = !{!9}

!0 = distinct !DICompileUnit(language: DW_LANG_C11, file: !1, producer: "clang version 19.1.2 (git@g53gitlab.llan.ll.mit.edu:inherently-secure/llvm-project.git 6ac24fd1f99e0a3d7173c1e017764e588a5ed686)", isOptimized: false, runtimeVersion: 0, emissionKind: FullDebug, splitDebugInlining: false, nameTableKind: None)
!1 = !DIFile(filename: "/home/al32163/hakc/HAKC_CURR/llvm-project/llvm/test/Transforms/Compartmentalization/hakc_test9/hakc_test9.c", directory: "/home/al32163/hakc/HAKC_CURR/cmake-build-hakc-llvm/llvm-project/llvm/test", checksumkind: CSK_MD5, checksum: "ec4723975cc01af06e9a32a0ec9498cb")
!2 = !{i32 7, !"Dwarf Version", i32 5}
!3 = !{i32 2, !"Debug Info Version", i32 3}
!4 = !{i32 1, !"wchar_size", i32 4}
!5 = !{i32 8, !"PIC Level", i32 2}
!6 = !{i32 7, !"PIE Level", i32 2}
!7 = !{i32 7, !"uwtable", i32 2}
!8 = !{i32 7, !"frame-pointer", i32 2}
!9 = !{!"clang version 19.1.2 (git@g53gitlab.llan.ll.mit.edu:inherently-secure/llvm-project.git 6ac24fd1f99e0a3d7173c1e017764e588a5ed686)"}
!10 = distinct !DISubprogram(name: "bar", scope: !11, file: !11, line: 15, type: !12, scopeLine: 15, flags: DIFlagPrototyped, spFlags: DISPFlagDefinition, unit: !0, retainedNodes: !19)
!11 = !DIFile(filename: "llvm-project/llvm/test/Transforms/Compartmentalization/hakc_test9/hakc_test9.c", directory: "/home/al32163/hakc/HAKC_CURR", checksumkind: CSK_MD5, checksum: "ec4723975cc01af06e9a32a0ec9498cb")
!12 = !DISubroutineType(types: !13)
!13 = !{!14, !15}
!14 = !DIBasicType(name: "int", size: 32, encoding: DW_ATE_signed)
!15 = !DIDerivedType(tag: DW_TAG_pointer_type, baseType: !16, size: 64)
!16 = distinct !DICompositeType(tag: DW_TAG_structure_type, name: "data_struct", file: !11, line: 11, size: 32, elements: !17)
!17 = !{!18}
!18 = !DIDerivedType(tag: DW_TAG_member, name: "a", scope: !16, file: !11, line: 12, baseType: !14, size: 32)
!19 = !{}
!20 = !DILocalVariable(name: "b", arg: 1, scope: !10, file: !11, line: 15, type: !15)
!21 = !DILocation(line: 15, column: 29, scope: !10)
!22 = !DILocation(line: 16, column: 8, scope: !23)
!23 = distinct !DILexicalBlock(scope: !10, file: !11, line: 16, column: 8)
!24 = !DILocation(line: 16, column: 8, scope: !10)
!25 = !DILocation(line: 17, column: 17, scope: !26)
!26 = distinct !DILexicalBlock(scope: !23, file: !11, line: 16, column: 10)
!27 = !DILocation(line: 17, column: 20, scope: !26)
!28 = !DILocation(line: 17, column: 22, scope: !26)
!29 = !DILocation(line: 17, column: 9, scope: !26)
!30 = !DILocation(line: 19, column: 13, scope: !10)
!31 = !DILocation(line: 19, column: 16, scope: !10)
!32 = !DILocation(line: 19, column: 18, scope: !10)
!33 = !DILocation(line: 19, column: 5, scope: !10)
!34 = !DILocation(line: 20, column: 1, scope: !10)
!35 = distinct !DISubprogram(name: "foo", scope: !11, file: !11, line: 22, type: !12, scopeLine: 22, flags: DIFlagPrototyped, spFlags: DISPFlagDefinition, unit: !0, retainedNodes: !19)
!36 = !DILocalVariable(name: "a", arg: 1, scope: !35, file: !11, line: 22, type: !15)
!37 = !DILocation(line: 22, column: 29, scope: !35)
!38 = !DILocation(line: 23, column: 9, scope: !39)
!39 = distinct !DILexicalBlock(scope: !35, file: !11, line: 23, column: 9)
!40 = !DILocation(line: 23, column: 9, scope: !35)
!41 = !DILocalVariable(name: "b", scope: !42, file: !11, line: 24, type: !15)
!42 = distinct !DILexicalBlock(scope: !39, file: !11, line: 23, column: 12)
!43 = !DILocation(line: 24, column: 30, scope: !42)
!44 = !DILocation(line: 25, column: 9, scope: !42)
!45 = !DILocation(line: 25, column: 12, scope: !42)
!46 = !DILocation(line: 25, column: 14, scope: !42)
!47 = !DILocation(line: 26, column: 20, scope: !42)
!48 = !DILocation(line: 26, column: 16, scope: !42)
!49 = !DILocation(line: 26, column: 9, scope: !42)
!50 = !DILocation(line: 28, column: 5, scope: !35)
!51 = !DILocation(line: 29, column: 1, scope: !35)
