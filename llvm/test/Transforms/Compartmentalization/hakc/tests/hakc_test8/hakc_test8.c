// RUN: %HAKC_PYTHON_VENV
// RUN: %HAKC_START_POLICY_SERVER
// RUN: %HAKC_RUN_COMP_PASS
// RUN: %HAKC_EVALUATE


// test of GetIgnoredGlobals

int kmalloc_caches = 53;
int somevar = 53;

struct data_struct {
    int a;
};

struct data_struct2 {
    int (*f)(struct data_struct *);
};

int foo(struct data_struct2 *a, int* v1, int* v2) {
    if (a) {
// CHECK-LABEL: if.then
// CHECK: call ptr @check_hakc_data_access(ptr %1, i64 4, i64 270336)
        *v1++;
// CHECK: call ptr @check_hakc_data_access(ptr %4, i64 4, i64 270336)
        *v2++;
        struct data_struct b;
        b.a = 0;
// CHECK: all ptr @check_hakc_data_access(ptr %7, i64 4, i64 270336)
// CHECK: call ptr @check_hakc_code_access(ptr %10, i64 4, i64 270336, ptr @entry_tokens_4, i64 1)
        return a->f(&b);
    }
    return 0;
}

// CHECK-LABEL: i32 @HAKC_XFER_foo(ptr noundef %0, ptr noundef %1, ptr noundef %2)
// CHECK: call i32 @get_hakc_address_color(ptr %0)
// CHECK: call ptr @hakc_transfer_to_clique(ptr %0, i64 64, i64 4, i64 13, i1 false)
// CHECK: call i32 @get_hakc_address_color(ptr %1)
// CHECK: call ptr @hakc_transfer_to_clique(ptr %1, i64 32, i64 4, i64 13, i1 false)
// CHECK: call i32 @get_hakc_address_color(ptr %2)
// CHECK: call ptr @hakc_transfer_to_clique(ptr %2, i64 32, i64 4, i64 13, i1 false)
// CHECK: call i32 @HAKC_ORIG_foo(ptr %4, ptr %6, ptr %8)
// CHECK: call void @hakc_color_address(ptr %2, i32 %7, i64 32)
// CHECK: call void @hakc_color_address(ptr %1, i32 %5, i64 32)
// CHECK: call void @hakc_color_address(ptr %0, i32 %3, i64 64)
