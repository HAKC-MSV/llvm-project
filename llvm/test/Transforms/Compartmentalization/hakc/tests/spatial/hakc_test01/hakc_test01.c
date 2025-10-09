// RUN: %HAKC_PYTHON_VENV
// RUN: %HAKC_START_ANALYSIS_SERVER
// RUN: %HAKC_RUN_ANALYSIS_PASS
// sleep for n seconds to let server finish constructing dag
// RUN: sleep 15
// RUN: %HAKC_RUN_ENFORCEMENT_PASS
// RUN: %HAKC_EVALUATE


struct data_struct {
    int a;
};

struct data_struct2 {
    int (*f)(struct data_struct *);
};

int foo(struct data_struct2 *a) {
    if (a) {
        struct data_struct b;
        b.a = 0;
        return a->f(&b);
    }
    return 0;
}

// CHECK-LABEL: i32 @HAKC_XFER_foo(ptr noundef %0)
// CHECK: call i32 @get_hakc_address_color(ptr %0)
// CHECK: call ptr @hakc_transfer_to_clique(ptr %0, i64 8, i64 1, i32 13, i1 false)
// CHECK: call i32 @HAKC_ORIG_foo(ptr %2)
// CHECK: call void @hakc_color_address(ptr %0, i32 %1, i64 8)
