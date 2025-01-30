; ModuleID = '/home/al32163/hakc/HAKC_CURR/llvm-project/llvm/test/Transforms/Compartmentalization/hakc_test8/hakc_test8.c'
source_filename = "/home/al32163/hakc/HAKC_CURR/llvm-project/llvm/test/Transforms/Compartmentalization/hakc_test8/hakc_test8.c"
target datalayout = "e-m:e-p270:32:32-p271:32:32-p272:64:64-i64:64-i128:128-f80:128-n8:16:32:64-S128"
target triple = "x86_64-unknown-linux-gnu"

%struct.data_struct = type { i32 }
%struct.data_struct2 = type { ptr }

@kmalloc_caches = dso_local global i32 53, align 4, !dbg !0
@somevar = dso_local global i32 53, align 4, !dbg !5

; Function Attrs: noinline nounwind optnone uwtable
define dso_local i32 @foo(ptr noundef %0, ptr noundef %1, ptr noundef %2) #0 !dbg !17 {
  %4 = alloca i32, align 4
  %5 = alloca ptr, align 8
  %6 = alloca ptr, align 8
  %7 = alloca ptr, align 8
  %8 = alloca %struct.data_struct, align 4
  store ptr %0, ptr %5, align 8
    #dbg_declare(ptr %5, !33, !DIExpression(), !34)
  store ptr %1, ptr %6, align 8
    #dbg_declare(ptr %6, !35, !DIExpression(), !36)
  store ptr %2, ptr %7, align 8
    #dbg_declare(ptr %7, !37, !DIExpression(), !38)
  %9 = load ptr, ptr %5, align 8, !dbg !39
  %10 = icmp ne ptr %9, null, !dbg !39
  br i1 %10, label %11, label %23, !dbg !41

11:                                               ; preds = %3
  %12 = load ptr, ptr %6, align 8, !dbg !42
  %13 = getelementptr inbounds i32, ptr %12, i32 1, !dbg !42
  store ptr %13, ptr %6, align 8, !dbg !42
  %14 = load i32, ptr %12, align 4, !dbg !44
  %15 = load ptr, ptr %7, align 8, !dbg !45
  %16 = getelementptr inbounds i32, ptr %15, i32 1, !dbg !45
  store ptr %16, ptr %7, align 8, !dbg !45
  %17 = load i32, ptr %15, align 4, !dbg !46
    #dbg_declare(ptr %8, !47, !DIExpression(), !48)
  %18 = getelementptr inbounds %struct.data_struct, ptr %8, i32 0, i32 0, !dbg !49
  store i32 0, ptr %18, align 4, !dbg !50
  %19 = load ptr, ptr %5, align 8, !dbg !51
  %20 = getelementptr inbounds %struct.data_struct2, ptr %19, i32 0, i32 0, !dbg !52
  %21 = load ptr, ptr %20, align 8, !dbg !52
  %22 = call i32 %21(ptr noundef %8), !dbg !51
  store i32 %22, ptr %4, align 4, !dbg !53
  br label %24, !dbg !53

23:                                               ; preds = %3
  store i32 0, ptr %4, align 4, !dbg !54
  br label %24, !dbg !54

24:                                               ; preds = %23, %11
  %25 = load i32, ptr %4, align 4, !dbg !55
  ret i32 %25, !dbg !55
}

attributes #0 = { noinline nounwind optnone uwtable "frame-pointer"="all" "min-legal-vector-width"="0" "no-trapping-math"="true" "stack-protector-buffer-size"="8" "target-cpu"="x86-64" "target-features"="+cmov,+cx8,+fxsr,+mmx,+sse,+sse2,+x87" "tune-cpu"="generic" }

!llvm.dbg.cu = !{!2}
!llvm.module.flags = !{!9, !10, !11, !12, !13, !14, !15}
!llvm.ident = !{!16}

!0 = !DIGlobalVariableExpression(var: !1, expr: !DIExpression())
!1 = distinct !DIGlobalVariable(name: "kmalloc_caches", scope: !2, file: !7, line: 10, type: !8, isLocal: false, isDefinition: true)
!2 = distinct !DICompileUnit(language: DW_LANG_C11, file: !3, producer: "clang version 19.1.2 (git@g53gitlab.llan.ll.mit.edu:inherently-secure/llvm-project.git 3302becd236de1832c655b222b179974c9a2fd47)", isOptimized: false, runtimeVersion: 0, emissionKind: FullDebug, globals: !4, splitDebugInlining: false, nameTableKind: None)
!3 = !DIFile(filename: "/home/al32163/hakc/HAKC_CURR/llvm-project/llvm/test/Transforms/Compartmentalization/hakc_test8/hakc_test8.c", directory: "/home/al32163/hakc/HAKC_CURR/cmake-build-hakc-llvm/llvm-project/llvm/test", checksumkind: CSK_MD5, checksum: "d52aca8c72c745729f68ddbd62ec7687")
!4 = !{!0, !5}
!5 = !DIGlobalVariableExpression(var: !6, expr: !DIExpression())
!6 = distinct !DIGlobalVariable(name: "somevar", scope: !2, file: !7, line: 11, type: !8, isLocal: false, isDefinition: true)
!7 = !DIFile(filename: "llvm-project/llvm/test/Transforms/Compartmentalization/hakc_test8/hakc_test8.c", directory: "/home/al32163/hakc/HAKC_CURR", checksumkind: CSK_MD5, checksum: "d52aca8c72c745729f68ddbd62ec7687")
!8 = !DIBasicType(name: "int", size: 32, encoding: DW_ATE_signed)
!9 = !{i32 7, !"Dwarf Version", i32 5}
!10 = !{i32 2, !"Debug Info Version", i32 3}
!11 = !{i32 1, !"wchar_size", i32 4}
!12 = !{i32 8, !"PIC Level", i32 2}
!13 = !{i32 7, !"PIE Level", i32 2}
!14 = !{i32 7, !"uwtable", i32 2}
!15 = !{i32 7, !"frame-pointer", i32 2}
!16 = !{!"clang version 19.1.2 (git@g53gitlab.llan.ll.mit.edu:inherently-secure/llvm-project.git 3302becd236de1832c655b222b179974c9a2fd47)"}
!17 = distinct !DISubprogram(name: "foo", scope: !7, file: !7, line: 21, type: !18, scopeLine: 21, flags: DIFlagPrototyped, spFlags: DISPFlagDefinition, unit: !2, retainedNodes: !32)
!18 = !DISubroutineType(types: !19)
!19 = !{!8, !20, !31, !31}
!20 = !DIDerivedType(tag: DW_TAG_pointer_type, baseType: !21, size: 64)
!21 = distinct !DICompositeType(tag: DW_TAG_structure_type, name: "data_struct2", file: !7, line: 17, size: 64, elements: !22)
!22 = !{!23}
!23 = !DIDerivedType(tag: DW_TAG_member, name: "f", scope: !21, file: !7, line: 18, baseType: !24, size: 64)
!24 = !DIDerivedType(tag: DW_TAG_pointer_type, baseType: !25, size: 64)
!25 = !DISubroutineType(types: !26)
!26 = !{!8, !27}
!27 = !DIDerivedType(tag: DW_TAG_pointer_type, baseType: !28, size: 64)
!28 = distinct !DICompositeType(tag: DW_TAG_structure_type, name: "data_struct", file: !7, line: 13, size: 32, elements: !29)
!29 = !{!30}
!30 = !DIDerivedType(tag: DW_TAG_member, name: "a", scope: !28, file: !7, line: 14, baseType: !8, size: 32)
!31 = !DIDerivedType(tag: DW_TAG_pointer_type, baseType: !8, size: 64)
!32 = !{}
!33 = !DILocalVariable(name: "a", arg: 1, scope: !17, file: !7, line: 21, type: !20)
!34 = !DILocation(line: 21, column: 30, scope: !17)
!35 = !DILocalVariable(name: "v1", arg: 2, scope: !17, file: !7, line: 21, type: !31)
!36 = !DILocation(line: 21, column: 38, scope: !17)
!37 = !DILocalVariable(name: "v2", arg: 3, scope: !17, file: !7, line: 21, type: !31)
!38 = !DILocation(line: 21, column: 47, scope: !17)
!39 = !DILocation(line: 22, column: 9, scope: !40)
!40 = distinct !DILexicalBlock(scope: !17, file: !7, line: 22, column: 9)
!41 = !DILocation(line: 22, column: 9, scope: !17)
!42 = !DILocation(line: 23, column: 12, scope: !43)
!43 = distinct !DILexicalBlock(scope: !40, file: !7, line: 22, column: 12)
!44 = !DILocation(line: 23, column: 9, scope: !43)
!45 = !DILocation(line: 24, column: 12, scope: !43)
!46 = !DILocation(line: 24, column: 9, scope: !43)
!47 = !DILocalVariable(name: "b", scope: !43, file: !7, line: 25, type: !28)
!48 = !DILocation(line: 25, column: 28, scope: !43)
!49 = !DILocation(line: 26, column: 11, scope: !43)
!50 = !DILocation(line: 26, column: 13, scope: !43)
!51 = !DILocation(line: 27, column: 16, scope: !43)
!52 = !DILocation(line: 27, column: 19, scope: !43)
!53 = !DILocation(line: 27, column: 9, scope: !43)
!54 = !DILocation(line: 29, column: 5, scope: !17)
!55 = !DILocation(line: 30, column: 1, scope: !17)
