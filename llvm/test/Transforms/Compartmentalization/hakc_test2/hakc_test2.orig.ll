; ModuleID = '/home/al32163/hakc/HAKC_CURR/llvm-project/llvm/test/Transforms/Compartmentalization/hakc_test2/hakc_test2.c'
source_filename = "/home/al32163/hakc/HAKC_CURR/llvm-project/llvm/test/Transforms/Compartmentalization/hakc_test2/hakc_test2.c"
target datalayout = "e-m:e-p270:32:32-p271:32:32-p272:64:64-i64:64-i128:128-f80:128-n8:16:32:64-S128"
target triple = "x86_64-unknown-linux-gnu"

%struct.data = type { %struct.linked_list, ptr }
%struct.linked_list = type { ptr }

; Function Attrs: noinline nounwind optnone uwtable
define dso_local void @init_data(ptr noundef %0) #0 !dbg !10 {
  %2 = alloca ptr, align 8
  store ptr %0, ptr %2, align 8
    #dbg_declare(ptr %2, !25, !DIExpression(), !26)
  %3 = load ptr, ptr %2, align 8, !dbg !27
  %4 = getelementptr inbounds %struct.data, ptr %3, i32 0, i32 1, !dbg !28
  store ptr null, ptr %4, align 8, !dbg !29
  %5 = load ptr, ptr %2, align 8, !dbg !30
  %6 = getelementptr inbounds %struct.data, ptr %5, i32 0, i32 0, !dbg !31
  %7 = load ptr, ptr %2, align 8, !dbg !32
  %8 = getelementptr inbounds %struct.data, ptr %7, i32 0, i32 0, !dbg !33
  %9 = getelementptr inbounds %struct.linked_list, ptr %8, i32 0, i32 0, !dbg !34
  store ptr %6, ptr %9, align 8, !dbg !35
  ret void, !dbg !36
}

attributes #0 = { noinline nounwind optnone uwtable "frame-pointer"="all" "min-legal-vector-width"="0" "no-trapping-math"="true" "stack-protector-buffer-size"="8" "target-cpu"="x86-64" "target-features"="+cmov,+cx8,+fxsr,+mmx,+sse,+sse2,+x87" "tune-cpu"="generic" }

!llvm.dbg.cu = !{!0}
!llvm.module.flags = !{!2, !3, !4, !5, !6, !7, !8}
!llvm.ident = !{!9}

!0 = distinct !DICompileUnit(language: DW_LANG_C11, file: !1, producer: "clang version 19.1.2 (git@g53gitlab.llan.ll.mit.edu:inherently-secure/llvm-project.git 6ac24fd1f99e0a3d7173c1e017764e588a5ed686)", isOptimized: false, runtimeVersion: 0, emissionKind: FullDebug, splitDebugInlining: false, nameTableKind: None)
!1 = !DIFile(filename: "/home/al32163/hakc/HAKC_CURR/llvm-project/llvm/test/Transforms/Compartmentalization/hakc_test2/hakc_test2.c", directory: "/home/al32163/hakc/HAKC_CURR/cmake-build-hakc-llvm/llvm-project/llvm/test", checksumkind: CSK_MD5, checksum: "cd398766be20bceadb6f212a9a07fa44")
!2 = !{i32 7, !"Dwarf Version", i32 5}
!3 = !{i32 2, !"Debug Info Version", i32 3}
!4 = !{i32 1, !"wchar_size", i32 4}
!5 = !{i32 8, !"PIC Level", i32 2}
!6 = !{i32 7, !"PIE Level", i32 2}
!7 = !{i32 7, !"uwtable", i32 2}
!8 = !{i32 7, !"frame-pointer", i32 2}
!9 = !{!"clang version 19.1.2 (git@g53gitlab.llan.ll.mit.edu:inherently-secure/llvm-project.git 6ac24fd1f99e0a3d7173c1e017764e588a5ed686)"}
!10 = distinct !DISubprogram(name: "init_data", scope: !11, file: !11, line: 18, type: !12, scopeLine: 18, flags: DIFlagPrototyped, spFlags: DISPFlagDefinition, unit: !0, retainedNodes: !24)
!11 = !DIFile(filename: "llvm-project/llvm/test/Transforms/Compartmentalization/hakc_test2/hakc_test2.c", directory: "/home/al32163/hakc/HAKC_CURR", checksumkind: CSK_MD5, checksum: "cd398766be20bceadb6f212a9a07fa44")
!12 = !DISubroutineType(types: !13)
!13 = !{null, !14}
!14 = !DIDerivedType(tag: DW_TAG_pointer_type, baseType: !15, size: 64)
!15 = distinct !DICompositeType(tag: DW_TAG_structure_type, name: "data", file: !11, line: 13, size: 128, elements: !16)
!16 = !{!17, !22}
!17 = !DIDerivedType(tag: DW_TAG_member, name: "list", scope: !15, file: !11, line: 14, baseType: !18, size: 64)
!18 = distinct !DICompositeType(tag: DW_TAG_structure_type, name: "linked_list", file: !11, line: 9, size: 64, elements: !19)
!19 = !{!20}
!20 = !DIDerivedType(tag: DW_TAG_member, name: "next", scope: !18, file: !11, line: 10, baseType: !21, size: 64)
!21 = !DIDerivedType(tag: DW_TAG_pointer_type, baseType: !18, size: 64)
!22 = !DIDerivedType(tag: DW_TAG_member, name: "data", scope: !15, file: !11, line: 15, baseType: !23, size: 64, offset: 64)
!23 = !DIDerivedType(tag: DW_TAG_pointer_type, baseType: null, size: 64)
!24 = !{}
!25 = !DILocalVariable(name: "data", arg: 1, scope: !10, file: !11, line: 18, type: !14)
!26 = !DILocation(line: 18, column: 29, scope: !10)
!27 = !DILocation(line: 19, column: 5, scope: !10)
!28 = !DILocation(line: 19, column: 11, scope: !10)
!29 = !DILocation(line: 19, column: 16, scope: !10)
!30 = !DILocation(line: 20, column: 24, scope: !10)
!31 = !DILocation(line: 20, column: 30, scope: !10)
!32 = !DILocation(line: 20, column: 5, scope: !10)
!33 = !DILocation(line: 20, column: 11, scope: !10)
!34 = !DILocation(line: 20, column: 16, scope: !10)
!35 = !DILocation(line: 20, column: 21, scope: !10)
!36 = !DILocation(line: 21, column: 1, scope: !10)
