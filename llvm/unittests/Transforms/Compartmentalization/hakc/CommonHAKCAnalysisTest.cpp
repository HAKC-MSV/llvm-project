//
// Created by de29664 on 3/24/26.
//

#include "llvm/Transforms/Compartmentalization/hakc/HAKCAnalysis/CommonHAKCAnalysis.h"
#include "llvm/AsmParser/Parser.h"
#include "llvm/IR/InstIterator.h"

#include "gtest/gtest.h"

namespace llvm {
namespace {

TEST(HAKCUnitTests, IndirectCallGetDefTest) {
  LLVMContext Ctx;
  ModuleAnalysisManager MAM;

  SMDiagnostic Error;
  StringRef Text = R"(
target datalayout = "e-m:e-p270:32:32-p271:32:32-p272:64:64-i64:64-i128:128-f80:128-n8:16:32:64-S128"
target triple = "x86_64-unknown-linux-gnu"
%struct.kiocb = type { ptr, i64, ptr, ptr, i32, i16, %union.anon.33 }
%union.anon.33 = type { ptr }
%struct.iov_iter = type { i8, i8, i8, i64, %union.anon.34, %union.anon.37 }
%union.anon.34 = type { %struct.iovec }
%struct.iovec = type { ptr, i64 }
%union.anon.37 = type { i64 }
%struct.file_operations = type { ptr, i32, ptr, ptr, ptr, ptr, ptr, ptr, ptr, ptr, ptr, ptr, ptr, ptr, ptr, ptr, ptr, ptr, ptr, ptr, ptr, ptr, ptr, ptr, ptr, ptr, ptr, ptr, ptr, ptr, ptr, ptr, ptr }
%struct.pcpu_hot = type { %union.anon.38 }
%union.anon.38 = type { %struct.anon.39, [16 x i8] }
%struct.anon.39 = type { ptr, i32, i32, i64, i64, ptr, i16, i8 }

@.str = private unnamed_addr constant [16 x i8] c"fs/read_write.c\00", align 1
@pcpu_hot = external dso_local global %struct.pcpu_hot, section ".data..percpu..shared_aligned", align 64 #0

; Function Attrs: fn_ret_thunk_extern noredzone nounwind null_pointer_is_valid sspstrong
define dso_local i64 @vfs_read(ptr noundef %file, ptr noundef %buf, i64 noundef %count, ptr noundef %pos) local_unnamed_addr #2 align 16 {
entry:
  %kiocb.i = alloca %struct.kiocb, align 8
  %iter.i = alloca %struct.iov_iter, align 8
  %f_mode = getelementptr inbounds i8, ptr %file, i64 12
  %0 = load i32, ptr %f_mode, align 4
  %and = and i32 %0, 1
  %tobool.not = icmp eq i32 %and, 0
  br i1 %tobool.not, label %cleanup, label %if.end

if.end:                                           ; preds = %entry
  %and2 = and i32 %0, 131072
  %tobool3.not = icmp eq i32 %and2, 0
  br i1 %tobool3.not, label %cleanup, label %__access_ok.exit

__access_ok.exit:                                 ; preds = %if.end
  %1 = ptrtoint ptr %buf to i64
  %add.i = add i64 %1, %count
  %2 = tail call i64 asm "mov $1,$0\0A1:\0A.pushsection runtime_ptr_USER_PTR_MAX,\22a\22\0A\09.long 1b - ${2:c} - .\0A.popsection", "=r,i,i,~{dirflag},~{fpsr},~{flags}"(i64 81985529216486895, i64 8) #17
  %cmp7.i = icmp ule i64 %add.i, %2
  %cmp9.i = icmp uge i64 %add.i, %1
  %3 = and i1 %cmp9.i, %cmp7.i
  br i1 %3, label %if.end18, label %cleanup

if.end18:                                         ; preds = %__access_ok.exit
  %cmp1.i = icmp slt i64 %count, 0
  br i1 %cmp1.i, label %if.then22, label %if.end.i

if.end.i:                                         ; preds = %if.end18
  %tobool3.not.i = icmp eq ptr %pos, null
  br i1 %tobool3.not.i, label %rw_verify_area.exit, label %if.then4.i

if.then4.i:                                       ; preds = %if.end.i
  %4 = load i64, ptr %pos, align 8
  %cmp5.i = icmp slt i64 %4, 0
  br i1 %cmp5.i, label %if.then14.i, label %if.else.i65

if.then14.i:                                      ; preds = %if.then4.i
  %f_op.i.i = getelementptr inbounds i8, ptr %file, i64 16
  %5 = load ptr, ptr %f_op.i.i, align 8
  %fop_flags.i.i = getelementptr inbounds i8, ptr %5, i64 8
  %6 = load i32, ptr %fop_flags.i.i, align 8
  %and.i.i = and i32 %6, 32
  %tobool.i.not.i = icmp eq i32 %and.i.i, 0
  br i1 %tobool.i.not.i, label %if.then22, label %if.end16.i

if.end16.i:                                       ; preds = %if.then14.i
  %sub.i = sub i64 0, %4
  %cmp17.not.i = icmp ugt i64 %sub.i, %count
  br i1 %cmp17.not.i, label %rw_verify_area.exit, label %if.then22

if.else.i65:                                      ; preds = %if.then4.i
  %add.i66 = add nuw i64 %4, %count
  %cmp21.i = icmp slt i64 %add.i66, 0
  br i1 %cmp21.i, label %if.then30.i, label %rw_verify_area.exit

if.then30.i:                                      ; preds = %if.else.i65
  %f_op.i57.i = getelementptr inbounds i8, ptr %file, i64 16
  %7 = load ptr, ptr %f_op.i57.i, align 8
  %fop_flags.i58.i = getelementptr inbounds i8, ptr %7, i64 8
  %8 = load i32, ptr %fop_flags.i58.i, align 8
  %and.i59.i = and i32 %8, 32
  %tobool.i60.not.i = icmp eq i32 %and.i59.i, 0
  br i1 %tobool.i60.not.i, label %if.then22, label %rw_verify_area.exit

rw_verify_area.exit:                              ; preds = %if.end.i, %if.end16.i, %if.else.i65, %if.then30.i
  %call37.i = tail call i32 @security_file_permission(ptr noundef %file, i32 noundef 4) #16
  %tobool21.not = icmp eq i32 %call37.i, 0
  br i1 %tobool21.not, label %if.end23, label %if.then22

if.then22:                                        ; preds = %if.then14.i, %if.end16.i, %if.then30.i, %if.end18, %rw_verify_area.exit
  %retval.1.i75 = phi i32 [ %call37.i, %rw_verify_area.exit ], [ -22, %if.then14.i ], [ -75, %if.end16.i ], [ -22, %if.then30.i ], [ -22, %if.end18 ]
  %conv20 = sext i32 %retval.1.i75 to i64
  br label %cleanup

if.end23:                                         ; preds = %rw_verify_area.exit
  %spec.store.select = tail call i64 @llvm.umin.i64(i64 %count, i64 2147479552)
  %f_op = getelementptr inbounds i8, ptr %file, i64 16
  %9 = load ptr, ptr %f_op, align 8
  %read = getelementptr inbounds i8, ptr %9, i64 24
  %10 = load ptr, ptr %read, align 8
  %tobool27.not = icmp eq ptr %10, null
  br i1 %tobool27.not, label %if.else, label %if.then28

if.then28:                                        ; preds = %if.end23
  %call31 = tail call i64 %10(ptr noundef %file, ptr noundef %buf, i64 noundef %spec.store.select, ptr noundef %pos) #16
  br label %if.end38

if.else:                                          ; preds = %if.end23
  %read_iter = getelementptr inbounds i8, ptr %9, i64 40
  %11 = load ptr, ptr %read_iter, align 8
  %tobool33.not = icmp eq ptr %11, null
  br i1 %tobool33.not, label %if.end43, label %if.then34

if.then34:                                        ; preds = %if.else
  call void @llvm.lifetime.start.p0(i64 48, ptr nonnull %kiocb.i) #14
  %12 = getelementptr inbounds i8, ptr %kiocb.i, i64 32
  store i64 0, ptr %12, align 8
  call void @llvm.lifetime.start.p0(i64 40, ptr nonnull %iter.i) #14
  %f_iocb_flags.i.i = getelementptr inbounds i8, ptr %file, i64 52
  %13 = load i32, ptr %f_iocb_flags.i.i, align 4
  %14 = tail call i64 asm "movq %gs:${1:a}, $0", "=r,i,~{dirflag},~{fpsr},~{flags}"(ptr nonnull @pcpu_hot) #17
  %15 = inttoptr i64 %14 to ptr
  %io_context.i.i.i.i = getelementptr inbounds i8, ptr %15, i64 2248
  %16 = load ptr, ptr %io_context.i.i.i.i, align 8
  %tobool.not.i.i.i.i = icmp eq ptr %16, null
  br i1 %tobool.not.i.i.i.i, label %init_sync_kiocb.exit.i, label %if.end.i.i.i.i

if.end.i.i.i.i:                                   ; preds = %if.then34
  %ioprio.i.i.i.i = getelementptr inbounds i8, ptr %16, i64 12
  %17 = load i16, ptr %ioprio.i.i.i.i, align 4
  %cmp6.i.i.i.i = icmp ult i16 %17, 8192
  br i1 %cmp6.i.i.i.i, label %if.then8.i.i.i.i, label %init_sync_kiocb.exit.i

if.then8.i.i.i.i:                                 ; preds = %if.end.i.i.i.i
  %policy.i.i.i.i.i = getelementptr inbounds i8, ptr %15, i64 964
  %18 = load i32, ptr %policy.i.i.i.i.i, align 4
  %cmp.i.i.i.i.i = icmp eq i32 %18, 5
  br i1 %cmp.i.i.i.i.i, label %task_nice_ioclass.exit.i.i.i.i, label %if.else.i.i.i.i.i

if.else.i.i.i.i.i:                                ; preds = %if.then8.i.i.i.i
  %19 = add i32 %18, -1
  %or.cond.i.i.i.i.i.i = icmp ult i32 %19, 2
  %cmp3.i.i.i.i.i.i = icmp eq i32 %18, 6
  %spec.select.i.i.i.i.i.i = or i1 %cmp3.i.i.i.i.i.i, %or.cond.i.i.i.i.i.i
  %..i.i.i.i.i = select i1 %spec.select.i.i.i.i.i.i, i32 8192, i32 16384
  br label %task_nice_ioclass.exit.i.i.i.i

task_nice_ioclass.exit.i.i.i.i:                   ; preds = %if.else.i.i.i.i.i, %if.then8.i.i.i.i
  %retval.0.i21.i.i.i.i = phi i32 [ 24576, %if.then8.i.i.i.i ], [ %..i.i.i.i.i, %if.else.i.i.i.i.i ]
  %static_prio.i.i.i.i.i.i = getelementptr inbounds i8, ptr %15, i64 112
  %20 = load i32, ptr %static_prio.i.i.i.i.i.i, align 16
  %add.i.i.i.i.i = add i32 %20, -100
  %div.i.i.i.i.i = sdiv i32 %add.i.i.i.i.i, 5
  %or.cond13.i.i.i.i.i = icmp ugt i32 %div.i.i.i.i.i, 7
  %or11.i.i.i.i.i = or disjoint i32 %div.i.i.i.i.i, %retval.0.i21.i.i.i.i
  %conv.i.i.i.i.i = trunc i32 %or11.i.i.i.i.i to i16
  %retval.0.i.i.i.i.i = select i1 %or.cond13.i.i.i.i.i, i16 -8192, i16 %conv.i.i.i.i.i
  br label %init_sync_kiocb.exit.i

init_sync_kiocb.exit.i:                           ; preds = %task_nice_ioclass.exit.i.i.i.i, %if.end.i.i.i.i, %if.then34
  %retval.0.in.i.i.i.i = phi i16 [ %retval.0.i.i.i.i.i, %task_nice_ioclass.exit.i.i.i.i ], [ %17, %if.end.i.i.i.i ], [ 0, %if.then34 ]
  store ptr %file, ptr %kiocb.i, align 8
  %.compoundliteral.sroa.2.0..sroa_idx.i.i = getelementptr inbounds i8, ptr %kiocb.i, i64 8
  %21 = getelementptr inbounds i8, ptr %kiocb.i, i64 16
  call void @llvm.memset.p0.i64(ptr noundef align 8 dereferenceable(24) %21, i8 0, i64 16, i1 false)
  store i32 %13, ptr %12, align 8
  %.compoundliteral.sroa.6.0..sroa_idx.i.i = getelementptr inbounds i8, ptr %kiocb.i, i64 36
  store i16 %retval.0.in.i.i.i.i, ptr %.compoundliteral.sroa.6.0..sroa_idx.i.i, align 4
  %.compoundliteral.sroa.7.sroa.1.0..compoundliteral.sroa.7.0..sroa_idx.sroa_idx.i.i = getelementptr inbounds i8, ptr %kiocb.i, i64 40
  store i64 0, ptr %.compoundliteral.sroa.7.sroa.1.0..compoundliteral.sroa.7.0..sroa_idx.sroa_idx.i.i, align 8
  br i1 %tobool3.not.i, label %cond.end.i, label %cond.true.i

cond.true.i:                                      ; preds = %init_sync_kiocb.exit.i
  %22 = load i64, ptr %pos, align 8
  br label %cond.end.i

cond.end.i:                                       ; preds = %cond.true.i, %init_sync_kiocb.exit.i
  %cond.i = phi i64 [ %22, %cond.true.i ], [ 0, %init_sync_kiocb.exit.i ]
  store i64 0, ptr %iter.i, align 8
  store i64 %cond.i, ptr %.compoundliteral.sroa.2.0..sroa_idx.i.i, align 8
  %.compoundliteral.sroa.420.0..sroa_idx.i.i = getelementptr inbounds i8, ptr %iter.i, i64 8
  store i64 0, ptr %.compoundliteral.sroa.420.0..sroa_idx.i.i, align 8
  %.compoundliteral.sroa.5.0..sroa_idx.i19.i = getelementptr inbounds i8, ptr %iter.i, i64 16
  store ptr %buf, ptr %.compoundliteral.sroa.5.0..sroa_idx.i19.i, align 8
  %.compoundliteral.sroa.6.0..sroa_idx.i20.i = getelementptr inbounds i8, ptr %iter.i, i64 24
  store i64 %spec.store.select, ptr %.compoundliteral.sroa.6.0..sroa_idx.i20.i, align 8
  %.compoundliteral.sroa.7.0..sroa_idx.i.i = getelementptr inbounds i8, ptr %iter.i, i64 32
  store i64 1, ptr %.compoundliteral.sroa.7.0..sroa_idx.i.i, align 8
  %call.i = call i64 %11(ptr noundef nonnull %kiocb.i, ptr noundef nonnull %iter.i) #16
  %cmp.i67 = icmp eq i64 %call.i, -529
  br i1 %cmp.i67, label %do.body3.i, label %do.end8.i

do.body3.i:                                       ; preds = %cond.end.i
  call void asm sideeffect "597: nop\0A\09.pushsection .discard.annotate_insn,\22M\22,@progbits,8\0A\09.long 597b - .\0A\09.long 3\0A\09.popsection\0A\09", "i,~{dirflag},~{fpsr},~{flags}"(i32 597) #14
  call void asm sideeffect "1:\09.byte 0x0f, 0x0b\0A.pushsection __bug_table,\22aw\22\0A2:\09.long 1b - .\09# bug_entry::bug_addr\0A\09.long ${0:c} - .\09# bug_entry::file\0A\09.word ${1:c}\09# bug_entry::line\0A\09.word ${2:c}\09# bug_entry::flags\0A\09.org 2b+${3:c}\0A.popsection\0A", "i,i,i,i,~{dirflag},~{fpsr},~{flags}"(ptr nonnull @.str, i32 485, i32 0, i64 12) #14
  unreachable

do.end8.i:                                        ; preds = %cond.end.i
  br i1 %tobool3.not.i, label %new_sync_read.exit, label %if.then10.i

if.then10.i:                                      ; preds = %do.end8.i
  %23 = load i64, ptr %.compoundliteral.sroa.2.0..sroa_idx.i.i, align 8
  store i64 %23, ptr %pos, align 8
  br label %new_sync_read.exit

new_sync_read.exit:                               ; preds = %do.end8.i, %if.then10.i
  call void @llvm.lifetime.end.p0(i64 40, ptr nonnull %iter.i) #14
  call void @llvm.lifetime.end.p0(i64 48, ptr nonnull %kiocb.i) #14
  br label %if.end38

if.end38:                                         ; preds = %new_sync_read.exit, %if.then28
  %ret.0 = phi i64 [ %call31, %if.then28 ], [ %call.i, %new_sync_read.exit ]
  %cmp39 = icmp sgt i64 %ret.0, 0
  br i1 %cmp39, label %if.then41, label %if.end43

if.then41:                                        ; preds = %if.end38
  %24 = load i32, ptr %f_mode, align 4
  %and.i.i68 = and i32 %24, 100663296
  %cmp.i.i = icmp eq i32 %and.i.i68, 33554432
  br i1 %cmp.i.i, label %fsnotify_access.exit, label %if.end.i.i

if.end.i.i:                                       ; preds = %if.then41
  %f_path.i.i = getelementptr inbounds i8, ptr %file, i64 64
  %dentry.i.i.i = getelementptr inbounds i8, ptr %file, i64 72
  %25 = load ptr, ptr %dentry.i.i.i, align 8
  %d_inode.i.i.i.i.i = getelementptr inbounds i8, ptr %25, i64 48
  %26 = load ptr, ptr %d_inode.i.i.i.i.i, align 8
  %i_sb.i.i.i.i = getelementptr inbounds i8, ptr %26, i64 40
  %27 = load ptr, ptr %i_sb.i.i.i.i, align 8
  %s_fsnotify_info.i.i.i.i.i.i.i = getelementptr inbounds i8, ptr %27, i64 912
  %28 = load volatile ptr, ptr %s_fsnotify_info.i.i.i.i.i.i.i, align 16
  %tobool.not.i.i.i.i.i.i = icmp eq ptr %28, null
  br i1 %tobool.not.i.i.i.i.i.i, label %fsnotify_access.exit, label %fsnotify_sb_has_watchers.exit.i.i.i.i

fsnotify_sb_has_watchers.exit.i.i.i.i:            ; preds = %if.end.i.i
  %watched_objects.i.i.i.i.i.i = getelementptr inbounds i8, ptr %28, i64 8
  %29 = load volatile i64, ptr %watched_objects.i.i.i.i.i.i, align 8
  %tobool2.i.i.not.i.i.i.i = icmp eq i64 %29, 0
  br i1 %tobool2.i.i.not.i.i.i.i, label %fsnotify_access.exit, label %if.end.i.i.i.i69

if.end.i.i.i.i69:                                 ; preds = %fsnotify_sb_has_watchers.exit.i.i.i.i
  %30 = load i16, ptr %26, align 8
  %31 = and i16 %30, -4096
  %cmp.i.i.i.i = icmp eq i16 %31, 16384
  br i1 %cmp.i.i.i.i, label %if.then3.i.i.i.i, label %if.end8.i.i.i.i

if.then3.i.i.i.i:                                 ; preds = %if.end.i.i.i.i69
  %32 = load i32, ptr %25, align 8
  %33 = and i32 %32, 16384
  %tobool.not.i.i.i.i70 = icmp eq i32 %33, 0
  br i1 %tobool.not.i.i.i.i70, label %notify_child.i.i.i.i, label %if.end8.i.i.i.i

if.end8.i.i.i.i:                                  ; preds = %if.then3.i.i.i.i, %if.end.i.i.i.i69
  %mask.addr.0.i.i.i.i = phi i32 [ 1073741825, %if.then3.i.i.i.i ], [ 1, %if.end.i.i.i.i69 ]
  %d_parent.i.i.i.i = getelementptr inbounds i8, ptr %25, i64 24
  %34 = load ptr, ptr %d_parent.i.i.i.i, align 8
  %cmp9.i.i.i.i = icmp eq ptr %34, %25
  br i1 %cmp9.i.i.i.i, label %notify_child.i.i.i.i, label %if.end12.i.i.i.i

if.end12.i.i.i.i:                                 ; preds = %if.end8.i.i.i.i
  %call13.i.i.i.i = call i32 @__fsnotify_parent(ptr noundef %25, i32 noundef %mask.addr.0.i.i.i.i, ptr noundef %f_path.i.i, i32 noundef 2) #16
  br label %fsnotify_access.exit

notify_child.i.i.i.i:                             ; preds = %if.end8.i.i.i.i, %if.then3.i.i.i.i
  %mask.addr.1.i.i.i.i = phi i32 [ %mask.addr.0.i.i.i.i, %if.end8.i.i.i.i ], [ 1073741825, %if.then3.i.i.i.i ]
  %call14.i.i.i.i = call i32 @fsnotify(i32 noundef %mask.addr.1.i.i.i.i, ptr noundef %f_path.i.i, i32 noundef 2, ptr noundef null, ptr noundef null, ptr noundef %26, i32 noundef 0) #16
  br label %fsnotify_access.exit

fsnotify_access.exit:                             ; preds = %if.then41, %if.end.i.i, %fsnotify_sb_has_watchers.exit.i.i.i.i, %if.end12.i.i.i.i, %notify_child.i.i.i.i
  %35 = call i64 asm "movq %gs:${1:a}, $0", "=r,i,~{dirflag},~{fpsr},~{flags}"(ptr nonnull @pcpu_hot) #17
  %36 = inttoptr i64 %35 to ptr
  %ioac.i = getelementptr inbounds i8, ptr %36, i64 2280
  %37 = load i64, ptr %ioac.i, align 8
  %add.i71 = add i64 %37, %ret.0
  store i64 %add.i71, ptr %ioac.i, align 8
  br label %if.end43

if.end43:                                         ; preds = %if.else, %fsnotify_access.exit, %if.end38
  %ret.078 = phi i64 [ %ret.0, %fsnotify_access.exit ], [ %ret.0, %if.end38 ], [ -22, %if.else ]
  %38 = call i64 asm "movq %gs:${1:a}, $0", "=r,i,~{dirflag},~{fpsr},~{flags}"(ptr nonnull @pcpu_hot) #17
  %39 = inttoptr i64 %38 to ptr
  %syscr.i = getelementptr inbounds i8, ptr %39, i64 2296
  %40 = load i64, ptr %syscr.i, align 8
  %inc.i = add i64 %40, 1
  store i64 %inc.i, ptr %syscr.i, align 8
  br label %cleanup

cleanup:                                          ; preds = %__access_ok.exit, %if.end, %entry, %if.end43, %if.then22
  %retval.0 = phi i64 [ %conv20, %if.then22 ], [ %ret.078, %if.end43 ], [ -9, %entry ], [ -22, %if.end ], [ -14, %__access_ok.exit ]
  ret i64 %retval.0
}

; Function Attrs: noredzone null_pointer_is_valid
declare dso_local i32 @__fsnotify_parent(ptr noundef, i32 noundef, ptr noundef, i32 noundef) local_unnamed_addr #3

; Function Attrs: noredzone null_pointer_is_valid
declare dso_local i32 @fsnotify(i32 noundef, ptr noundef, i32 noundef, ptr noundef, ptr noundef, ptr noundef, i32 noundef) local_unnamed_addr #3

; Function Attrs: noredzone null_pointer_is_valid
declare dso_local i32 @security_file_permission(ptr noundef, i32 noundef) local_unnamed_addr #3
  )";

  std::unique_ptr<Module> M = parseAssemblyString(Text, Error, Ctx);
  if (!M) {
    FAIL() << "Failed to parse assembly string: " << Error.getMessage();
  }

  auto *F = M->getFunction("vfs_read");
  ASSERT_TRUE(F);

  hakc::CommonHAKCAnalysis CommonHAKCAnalysis(*M, MAM);

  for (auto it = inst_begin(F); it != inst_end(F); ++it) {
    Instruction *I = &*it;
    if (auto *CallI = dyn_cast<CallInst>(I)) {
      if (CallI->isIndirectCall()) {
        SmallVector<Value *> DefChain;
        auto *IndirectCallOp = CallI->getCalledOperand();
        CommonHAKCAnalysis.findDefChain(IndirectCallOp, true, DefChain);

        int LoadInstCount = 0;
        for (Value *V : DefChain) {
          if (isa<LoadInst>(V)) {
            LoadInstCount++;
          }
        }
        ASSERT_EQ(LoadInstCount, 2);

        ASSERT_TRUE(isa_and_nonnull<Argument>(
            CommonHAKCAnalysis.getDef(IndirectCallOp, true)));
      }
    }
  }
}

} // namespace
} // namespace llvm
