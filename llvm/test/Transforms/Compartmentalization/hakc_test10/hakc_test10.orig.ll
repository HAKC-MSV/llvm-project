; ModuleID = '/home/al32163/hakc/HAKC_CURR/llvm-project/llvm/test/Transforms/Compartmentalization/hakc_test10/hakc_test10.c'
source_filename = "/home/al32163/hakc/HAKC_CURR/llvm-project/llvm/test/Transforms/Compartmentalization/hakc_test10/hakc_test10.c"
target datalayout = "e-m:e-p270:32:32-p271:32:32-p272:64:64-i64:64-i128:128-f80:128-n8:16:32:64-S128"
target triple = "x86_64-unknown-linux-gnu"

@kmalloc_caches = dso_local global i32 53, align 4, !dbg !0
@somevar = dso_local global i32 53, align 4, !dbg !5

; Function Attrs: noinline nounwind optnone uwtable
define dso_local i32 @foo(ptr noundef %0, ptr noundef %1) #0 !dbg !17 {
  %3 = alloca i32, align 4
  %4 = alloca ptr, align 8
  %5 = alloca ptr, align 8
  store ptr %0, ptr %4, align 8
    #dbg_declare(ptr %4, !22, !DIExpression(), !23)
  store ptr %1, ptr %5, align 8
    #dbg_declare(ptr %5, !24, !DIExpression(), !25)
  %6 = icmp ne ptr %4, null, !dbg !26
  br i1 %6, label %7, label %13, !dbg !28

7:                                                ; preds = %2
  %8 = load ptr, ptr %5, align 8, !dbg !29
  %9 = getelementptr inbounds i32, ptr %8, i32 1, !dbg !29
  store ptr %9, ptr %5, align 8, !dbg !29
  %10 = load i32, ptr %8, align 4, !dbg !31
  %11 = load ptr, ptr %5, align 8, !dbg !32
  %12 = load i32, ptr %11, align 4, !dbg !33
  store i32 %12, ptr %3, align 4, !dbg !34
  br label %14, !dbg !34

13:                                               ; preds = %2
  store i32 0, ptr %3, align 4, !dbg !35
  br label %14, !dbg !35

14:                                               ; preds = %13, %7
  %15 = load i32, ptr %3, align 4, !dbg !36
  ret i32 %15, !dbg !36
}

; Function Attrs: noinline nounwind optnone uwtable
define dso_local i32 @bar() #0 !dbg !37 {
  %1 = alloca ptr, align 8
  %2 = alloca ptr, align 8
    #dbg_declare(ptr %1, !40, !DIExpression(), !41)
  store ptr @kmalloc_caches, ptr %1, align 8, !dbg !42
    #dbg_declare(ptr %2, !43, !DIExpression(), !44)
  store ptr @somevar, ptr %2, align 8, !dbg !45
  %3 = load ptr, ptr %1, align 8, !dbg !46
  %4 = load ptr, ptr %2, align 8, !dbg !47
  %5 = call i32 @foo(ptr noundef %3, ptr noundef %4), !dbg !48
  ret i32 %5, !dbg !49
}

attributes #0 = { noinline nounwind optnone uwtable "frame-pointer"="all" "min-legal-vector-width"="0" "no-trapping-math"="true" "stack-protector-buffer-size"="8" "target-cpu"="x86-64" "target-features"="+cmov,+cx8,+fxsr,+mmx,+sse,+sse2,+x87" "tune-cpu"="generic" }

!llvm.dbg.cu = !{!2}
!llvm.module.flags = !{!9, !10, !11, !12, !13, !14, !15}
!llvm.ident = !{!16}

!0 = !DIGlobalVariableExpression(var: !1, expr: !DIExpression())
!1 = distinct !DIGlobalVariable(name: "kmalloc_caches", scope: !2, file: !7, line: 11, type: !8, isLocal: false, isDefinition: true)
!2 = distinct !DICompileUnit(language: DW_LANG_C11, file: !3, producer: "clang version 19.1.2 (git@g53gitlab.llan.ll.mit.edu:inherently-secure/llvm-project.git 6ac24fd1f99e0a3d7173c1e017764e588a5ed686)", isOptimized: false, runtimeVersion: 0, emissionKind: FullDebug, globals: !4, splitDebugInlining: false, nameTableKind: None)
!3 = !DIFile(filename: "/home/al32163/hakc/HAKC_CURR/llvm-project/llvm/test/Transforms/Compartmentalization/hakc_test10/hakc_test10.c", directory: "/home/al32163/hakc/HAKC_CURR/cmake-build-hakc-llvm/llvm-project/llvm/test", checksumkind: CSK_MD5, checksum: "6af0782f793278498318bc320f436830")
!4 = !{!0, !5}
!5 = !DIGlobalVariableExpression(var: !6, expr: !DIExpression())
!6 = distinct !DIGlobalVariable(name: "somevar", scope: !2, file: !7, line: 12, type: !8, isLocal: false, isDefinition: true)
!7 = !DIFile(filename: "llvm-project/llvm/test/Transforms/Compartmentalization/hakc_test10/hakc_test10.c", directory: "/home/al32163/hakc/HAKC_CURR", checksumkind: CSK_MD5, checksum: "6af0782f793278498318bc320f436830")
!8 = !DIBasicType(name: "int", size: 32, encoding: DW_ATE_signed)
!9 = !{i32 7, !"Dwarf Version", i32 5}
!10 = !{i32 2, !"Debug Info Version", i32 3}
!11 = !{i32 1, !"wchar_size", i32 4}
!12 = !{i32 8, !"PIC Level", i32 2}
!13 = !{i32 7, !"PIE Level", i32 2}
!14 = !{i32 7, !"uwtable", i32 2}
!15 = !{i32 7, !"frame-pointer", i32 2}
!16 = !{!"clang version 19.1.2 (git@g53gitlab.llan.ll.mit.edu:inherently-secure/llvm-project.git 6ac24fd1f99e0a3d7173c1e017764e588a5ed686)"}
!17 = distinct !DISubprogram(name: "foo", scope: !7, file: !7, line: 14, type: !18, scopeLine: 14, flags: DIFlagPrototyped, spFlags: DISPFlagDefinition, unit: !2, retainedNodes: !21)
!18 = !DISubroutineType(types: !19)
!19 = !{!8, !20, !20}
!20 = !DIDerivedType(tag: DW_TAG_pointer_type, baseType: !8, size: 64)
!21 = !{}
!22 = !DILocalVariable(name: "v1", arg: 1, scope: !17, file: !7, line: 14, type: !20)
!23 = !DILocation(line: 14, column: 14, scope: !17)
!24 = !DILocalVariable(name: "v2", arg: 2, scope: !17, file: !7, line: 14, type: !20)
!25 = !DILocation(line: 14, column: 23, scope: !17)
!26 = !DILocation(line: 15, column: 9, scope: !27)
!27 = distinct !DILexicalBlock(scope: !17, file: !7, line: 15, column: 9)
!28 = !DILocation(line: 15, column: 9, scope: !17)
!29 = !DILocation(line: 16, column: 12, scope: !30)
!30 = distinct !DILexicalBlock(scope: !27, file: !7, line: 15, column: 14)
!31 = !DILocation(line: 16, column: 9, scope: !30)
!32 = !DILocation(line: 17, column: 17, scope: !30)
!33 = !DILocation(line: 17, column: 16, scope: !30)
!34 = !DILocation(line: 17, column: 9, scope: !30)
!35 = !DILocation(line: 19, column: 5, scope: !17)
!36 = !DILocation(line: 20, column: 1, scope: !17)
!37 = distinct !DISubprogram(name: "bar", scope: !7, file: !7, line: 22, type: !38, scopeLine: 22, spFlags: DISPFlagDefinition, unit: !2, retainedNodes: !21)
!38 = !DISubroutineType(types: !39)
!39 = !{!8}
!40 = !DILocalVariable(name: "v1", scope: !37, file: !7, line: 23, type: !20)
!41 = !DILocation(line: 23, column: 10, scope: !37)
!42 = !DILocation(line: 24, column: 8, scope: !37)
!43 = !DILocalVariable(name: "v2", scope: !37, file: !7, line: 25, type: !20)
!44 = !DILocation(line: 25, column: 10, scope: !37)
!45 = !DILocation(line: 26, column: 8, scope: !37)
!46 = !DILocation(line: 27, column: 16, scope: !37)
!47 = !DILocation(line: 27, column: 20, scope: !37)
!48 = !DILocation(line: 27, column: 12, scope: !37)
!49 = !DILocation(line: 27, column: 5, scope: !37)
