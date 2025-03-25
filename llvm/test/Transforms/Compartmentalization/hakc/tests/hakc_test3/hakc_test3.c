// RUN: %HAKC_PYTHON_VENV
// RUN: %HAKC_START_POLICY_SERVER
// RUN: %HAKC_RUN_COMP_PASS
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
// CHECK: call ptr @hakc_transfer_to_clique(ptr %0, i64 32, i64 2, i64 13, i1 false)
// CHECK: call i32 @HAKC_ORIG_foo(ptr %2)
// CHECK: call void @hakc_color_address(ptr %0, i32 %1, i64 32)
