; ModuleID = '/home/al32163/hakc/HAKC_CURR/llvm-project/llvm/test/Transforms/Compartmentalization/hakc_test1/hakc_test1.c'
source_filename = "/home/al32163/hakc/HAKC_CURR/llvm-project/llvm/test/Transforms/Compartmentalization/hakc_test1/hakc_test1.c"
target datalayout = "e-m:e-p270:32:32-p271:32:32-p272:64:64-i64:64-i128:128-f80:128-n8:16:32:64-S128"
target triple = "x86_64-unknown-linux-gnu"

%struct.data_struct = type { i32 }
%struct.data_struct2 = type { ptr }

; Function Attrs: noinline nounwind optnone uwtable
define dso_local i32 @foo(ptr noundef %0) #0 !dbg !10 {
  %2 = alloca i32, align 4
  %3 = alloca ptr, align 8
  %4 = alloca %struct.data_struct, align 4
  store ptr %0, ptr %3, align 8
    #dbg_declare(ptr %3, !27, !DIExpression(), !28)
  %5 = load ptr, ptr %3, align 8, !dbg !29
  %6 = icmp ne ptr %5, null, !dbg !29
  br i1 %6, label %7, label %13, !dbg !31

7:                                                ; preds = %1
    #dbg_declare(ptr %4, !32, !DIExpression(), !34)
  %8 = getelementptr inbounds %struct.data_struct, ptr %4, i32 0, i32 0, !dbg !35
  store i32 0, ptr %8, align 4, !dbg !36
  %9 = load ptr, ptr %3, align 8, !dbg !37
  %10 = getelementptr inbounds %struct.data_struct2, ptr %9, i32 0, i32 0, !dbg !38
  %11 = load ptr, ptr %10, align 8, !dbg !38
  %12 = call i32 %11(ptr noundef %4), !dbg !37
  store i32 %12, ptr %2, align 4, !dbg !39
  br label %14, !dbg !39

13:                                               ; preds = %1
  store i32 0, ptr %2, align 4, !dbg !40
  br label %14, !dbg !40

14:                                               ; preds = %13, %7
  %15 = load i32, ptr %2, align 4, !dbg !41
  ret i32 %15, !dbg !41
}

attributes #0 = { noinline nounwind optnone uwtable "frame-pointer"="all" "min-legal-vector-width"="0" "no-trapping-math"="true" "stack-protector-buffer-size"="8" "target-cpu"="x86-64" "target-features"="+cmov,+cx8,+fxsr,+mmx,+sse,+sse2,+x87" "tune-cpu"="generic" }

!llvm.dbg.cu = !{!0}
!llvm.module.flags = !{!2, !3, !4, !5, !6, !7, !8}
!llvm.ident = !{!9}

!0 = distinct !DICompileUnit(language: DW_LANG_C11, file: !1, producer: "clang version 19.1.2 (git@g53gitlab.llan.ll.mit.edu:inherently-secure/llvm-project.git 3302becd236de1832c655b222b179974c9a2fd47)", isOptimized: false, runtimeVersion: 0, emissionKind: FullDebug, splitDebugInlining: false, nameTableKind: None)
!1 = !DIFile(filename: "/home/al32163/hakc/HAKC_CURR/llvm-project/llvm/test/Transforms/Compartmentalization/hakc_test1/hakc_test1.c", directory: "/home/al32163/hakc/HAKC_CURR/cmake-build-hakc-llvm/llvm-project/llvm/test", checksumkind: CSK_MD5, checksum: "a343fbff25df52b7595be1ae987d0236")
!2 = !{i32 7, !"Dwarf Version", i32 5}
!3 = !{i32 2, !"Debug Info Version", i32 3}
!4 = !{i32 1, !"wchar_size", i32 4}
!5 = !{i32 8, !"PIC Level", i32 2}
!6 = !{i32 7, !"PIE Level", i32 2}
!7 = !{i32 7, !"uwtable", i32 2}
!8 = !{i32 7, !"frame-pointer", i32 2}
!9 = !{!"clang version 19.1.2 (git@g53gitlab.llan.ll.mit.edu:inherently-secure/llvm-project.git 3302becd236de1832c655b222b179974c9a2fd47)"}
!10 = distinct !DISubprogram(name: "foo", scope: !11, file: !11, line: 16, type: !12, scopeLine: 16, flags: DIFlagPrototyped, spFlags: DISPFlagDefinition, unit: !0, retainedNodes: !26)
!11 = !DIFile(filename: "llvm-project/llvm/test/Transforms/Compartmentalization/hakc_test1/hakc_test1.c", directory: "/home/al32163/hakc/HAKC_CURR", checksumkind: CSK_MD5, checksum: "a343fbff25df52b7595be1ae987d0236")
!12 = !DISubroutineType(types: !13)
!13 = !{!14, !15}
!14 = !DIBasicType(name: "int", size: 32, encoding: DW_ATE_signed)
!15 = !DIDerivedType(tag: DW_TAG_pointer_type, baseType: !16, size: 64)
!16 = distinct !DICompositeType(tag: DW_TAG_structure_type, name: "data_struct2", file: !11, line: 12, size: 64, elements: !17)
!17 = !{!18}
!18 = !DIDerivedType(tag: DW_TAG_member, name: "f", scope: !16, file: !11, line: 13, baseType: !19, size: 64)
!19 = !DIDerivedType(tag: DW_TAG_pointer_type, baseType: !20, size: 64)
!20 = !DISubroutineType(types: !21)
!21 = !{!14, !22}
!22 = !DIDerivedType(tag: DW_TAG_pointer_type, baseType: !23, size: 64)
!23 = distinct !DICompositeType(tag: DW_TAG_structure_type, name: "data_struct", file: !11, line: 8, size: 32, elements: !24)
!24 = !{!25}
!25 = !DIDerivedType(tag: DW_TAG_member, name: "a", scope: !23, file: !11, line: 9, baseType: !14, size: 32)
!26 = !{}
!27 = !DILocalVariable(name: "a", arg: 1, scope: !10, file: !11, line: 16, type: !15)
!28 = !DILocation(line: 16, column: 30, scope: !10)
!29 = !DILocation(line: 17, column: 9, scope: !30)
!30 = distinct !DILexicalBlock(scope: !10, file: !11, line: 17, column: 9)
!31 = !DILocation(line: 17, column: 9, scope: !10)
!32 = !DILocalVariable(name: "b", scope: !33, file: !11, line: 18, type: !23)
!33 = distinct !DILexicalBlock(scope: !30, file: !11, line: 17, column: 12)
!34 = !DILocation(line: 18, column: 28, scope: !33)
!35 = !DILocation(line: 19, column: 11, scope: !33)
!36 = !DILocation(line: 19, column: 13, scope: !33)
!37 = !DILocation(line: 20, column: 16, scope: !33)
!38 = !DILocation(line: 20, column: 19, scope: !33)
!39 = !DILocation(line: 20, column: 9, scope: !33)
!40 = !DILocation(line: 22, column: 5, scope: !10)
!41 = !DILocation(line: 23, column: 1, scope: !10)
