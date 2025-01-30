; ModuleID = '/home/al32163/hakc/HAKC_CURR/llvm-project/llvm/test/Transforms/Compartmentalization/hakc_test5/hakc_test5.c'
source_filename = "/home/al32163/hakc/HAKC_CURR/llvm-project/llvm/test/Transforms/Compartmentalization/hakc_test5/hakc_test5.c"
target datalayout = "e-m:e-p270:32:32-p271:32:32-p272:64:64-i64:64-i128:128-f80:128-n8:16:32:64-S128"
target triple = "x86_64-unknown-linux-gnu"

; Function Attrs: noinline nounwind optnone uwtable
define dso_local i32 @foo() #0 !dbg !23 {
  %1 = alloca ptr, align 8
  %2 = alloca i32, align 4
    #dbg_declare(ptr %1, !28, !DIExpression(), !29)
  %3 = call ptr @kmalloc(i64 noundef 512, i32 noundef 3264), !dbg !30
  store ptr %3, ptr %1, align 8, !dbg !31
    #dbg_declare(ptr %2, !32, !DIExpression(), !34)
  store i32 0, ptr %2, align 4, !dbg !34
  br label %4, !dbg !35

4:                                                ; preds = %13, %0
  %5 = load i32, ptr %2, align 4, !dbg !36
  %6 = icmp slt i32 %5, 512, !dbg !38
  br i1 %6, label %7, label %16, !dbg !39

7:                                                ; preds = %4
  %8 = load i32, ptr %2, align 4, !dbg !40
  %9 = load ptr, ptr %1, align 8, !dbg !42
  %10 = load i32, ptr %2, align 4, !dbg !43
  %11 = sext i32 %10 to i64, !dbg !44
  %12 = getelementptr inbounds i32, ptr %9, i64 %11, !dbg !44
  store i32 %8, ptr %12, align 4, !dbg !45
  br label %13, !dbg !46

13:                                               ; preds = %7
  %14 = load i32, ptr %2, align 4, !dbg !47
  %15 = add nsw i32 %14, 1, !dbg !47
  store i32 %15, ptr %2, align 4, !dbg !47
  br label %4, !dbg !48, !llvm.loop !49

16:                                               ; preds = %4
  ret i32 0, !dbg !52
}

declare ptr @kmalloc(i64 noundef, i32 noundef) #1

attributes #0 = { noinline nounwind optnone uwtable "frame-pointer"="all" "min-legal-vector-width"="0" "no-trapping-math"="true" "stack-protector-buffer-size"="8" "target-cpu"="x86-64" "target-features"="+cmov,+cx8,+fxsr,+mmx,+sse,+sse2,+x87" "tune-cpu"="generic" }
attributes #1 = { "frame-pointer"="all" "no-trapping-math"="true" "stack-protector-buffer-size"="8" "target-cpu"="x86-64" "target-features"="+cmov,+cx8,+fxsr,+mmx,+sse,+sse2,+x87" "tune-cpu"="generic" }

!llvm.dbg.cu = !{!0}
!llvm.module.flags = !{!15, !16, !17, !18, !19, !20, !21}
!llvm.ident = !{!22}

!0 = distinct !DICompileUnit(language: DW_LANG_C11, file: !1, producer: "clang version 19.1.2 (git@g53gitlab.llan.ll.mit.edu:inherently-secure/llvm-project.git 3302becd236de1832c655b222b179974c9a2fd47)", isOptimized: false, runtimeVersion: 0, emissionKind: FullDebug, enums: !2, retainedTypes: !11, splitDebugInlining: false, nameTableKind: None)
!1 = !DIFile(filename: "/home/al32163/hakc/HAKC_CURR/llvm-project/llvm/test/Transforms/Compartmentalization/hakc_test5/hakc_test5.c", directory: "/home/al32163/hakc/HAKC_CURR/cmake-build-hakc-llvm/llvm-project/llvm/test", checksumkind: CSK_MD5, checksum: "c0c9493c67bba08f25283daa819893ef")
!2 = !{!3}
!3 = !DICompositeType(tag: DW_TAG_enumeration_type, file: !4, line: 19, baseType: !5, size: 32, elements: !6)
!4 = !DIFile(filename: "linux/tools/include/linux/types.h", directory: "/home/al32163/hakc/HAKC_CURR", checksumkind: CSK_MD5, checksum: "aa9efaacfd4da30b8515144de63bf038")
!5 = !DIBasicType(name: "unsigned int", size: 32, encoding: DW_ATE_unsigned)
!6 = !{!7, !8, !9, !10}
!7 = !DIEnumerator(name: "GFP_KERNEL", value: 0)
!8 = !DIEnumerator(name: "GFP_ATOMIC", value: 1)
!9 = !DIEnumerator(name: "__GFP_HIGHMEM", value: 2)
!10 = !DIEnumerator(name: "__GFP_HIGH", value: 3)
!11 = !{!12, !14, !13}
!12 = !DIDerivedType(tag: DW_TAG_pointer_type, baseType: !13, size: 64)
!13 = !DIBasicType(name: "int", size: 32, encoding: DW_ATE_signed)
!14 = !DIDerivedType(tag: DW_TAG_typedef, name: "gfp_t", file: !4, line: 24, baseType: !3)
!15 = !{i32 7, !"Dwarf Version", i32 5}
!16 = !{i32 2, !"Debug Info Version", i32 3}
!17 = !{i32 1, !"wchar_size", i32 4}
!18 = !{i32 8, !"PIC Level", i32 2}
!19 = !{i32 7, !"PIE Level", i32 2}
!20 = !{i32 7, !"uwtable", i32 2}
!21 = !{i32 7, !"frame-pointer", i32 2}
!22 = !{!"clang version 19.1.2 (git@g53gitlab.llan.ll.mit.edu:inherently-secure/llvm-project.git 3302becd236de1832c655b222b179974c9a2fd47)"}
!23 = distinct !DISubprogram(name: "foo", scope: !24, file: !24, line: 14, type: !25, scopeLine: 14, spFlags: DISPFlagDefinition, unit: !0, retainedNodes: !27)
!24 = !DIFile(filename: "llvm-project/llvm/test/Transforms/Compartmentalization/hakc_test5/hakc_test5.c", directory: "/home/al32163/hakc/HAKC_CURR", checksumkind: CSK_MD5, checksum: "c0c9493c67bba08f25283daa819893ef")
!25 = !DISubroutineType(types: !26)
!26 = !{!13}
!27 = !{}
!28 = !DILocalVariable(name: "mem", scope: !23, file: !24, line: 15, type: !12)
!29 = !DILocation(line: 15, column: 11, scope: !23)
!30 = !DILocation(line: 16, column: 19, scope: !23)
!31 = !DILocation(line: 16, column: 9, scope: !23)
!32 = !DILocalVariable(name: "i", scope: !33, file: !24, line: 17, type: !13)
!33 = distinct !DILexicalBlock(scope: !23, file: !24, line: 17, column: 5)
!34 = !DILocation(line: 17, column: 13, scope: !33)
!35 = !DILocation(line: 17, column: 9, scope: !33)
!36 = !DILocation(line: 17, column: 20, scope: !37)
!37 = distinct !DILexicalBlock(scope: !33, file: !24, line: 17, column: 5)
!38 = !DILocation(line: 17, column: 22, scope: !37)
!39 = !DILocation(line: 17, column: 5, scope: !33)
!40 = !DILocation(line: 18, column: 28, scope: !41)
!41 = distinct !DILexicalBlock(scope: !37, file: !24, line: 17, column: 34)
!42 = !DILocation(line: 18, column: 11, scope: !41)
!43 = !DILocation(line: 18, column: 17, scope: !41)
!44 = !DILocation(line: 18, column: 15, scope: !41)
!45 = !DILocation(line: 18, column: 20, scope: !41)
!46 = !DILocation(line: 19, column: 5, scope: !41)
!47 = !DILocation(line: 17, column: 30, scope: !37)
!48 = !DILocation(line: 17, column: 5, scope: !37)
!49 = distinct !{!49, !39, !50, !51}
!50 = !DILocation(line: 19, column: 5, scope: !33)
!51 = !{!"llvm.loop.mustprogress"}
!52 = !DILocation(line: 20, column: 5, scope: !23)
