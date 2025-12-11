// RUN: %HAKC_RUN_ENFORCEMENT_PASS
// RUN: %HAKC_EVALUATE


// testing never used struct types 
struct list_head {
    int a;
};

int foo(struct list_head * ListHead) {
    if(ListHead){
// CHECK-NOT: call ptr @check_hakc_data_access(ptr %1, i64 1, i64 73728)
        return ListHead->a;
    }
    return 0;
}
// The type "list_head" is in the ignored types list, and should never be transferred
// CHECK-LABEL: @HAKC_XFER_foo
// CHECK-NOT: i32 @get_hakc_address_color(ptr %0)
// CHECK-NOT: ptr @hakc_transfer_to_clique(ptr %0, i64 4, i64 1, i32 13, i1 false)
// CHECK: call i32 @HAKC_ORIG_foo(ptr %0)
// CHECK-NOT: call void @hakc_color_address
