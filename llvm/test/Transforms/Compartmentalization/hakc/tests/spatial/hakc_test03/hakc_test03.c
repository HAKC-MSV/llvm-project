// RUN: %HAKC_RUN_ENFORCEMENT_PASS
// RUN: %HAKC_EVALUATE

struct data_struct {
    int a;
};

int bar(struct data_struct *);

int foo(struct data_struct *a) {
    if (a) {
        (a->a)++;
        return bar(a);
    }
    return 0;
}

// CHECK-LABEL: i32 @HAKC_XFER_foo(ptr noundef %0) #0
// CHECK: call i32 @get_hakc_address_color(ptr %0)
// CHECK: call ptr @hakc_transfer_to_clique(ptr %0, i64 4, i64 1, i32 13, i1 false)
// CHECK: call i32 @HAKC_ORIG_foo(ptr %2)
// CHECK: call void @hakc_color_address(ptr %0, i32 %1, i64 4)
