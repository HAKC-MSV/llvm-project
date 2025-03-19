// RUN: %HAKC_START_POLICY_SERVER
// RUN: clang -g -S -emit-llvm -mllvm --enable-hakc -mllvm --hakc-config=%HAKC_CONFIG -o %t %s
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
//        *v1++;
//        *v2++;
        struct data_struct b;
        b.a = 0;
        return a->f(&b); // this line is what causes the pass to fail 
    }
    return 0;
}

// CHECK-LABEL: i32 @HAKC_XFER_foo(ptr noundef %0, ptr noundef %1, ptr noundef %2)
// CHECK: call i32 @get_hakc_address_color(ptr %0)
// CHECK: call ptr @hakc_transfer_to_clique(ptr %0, i64 64, i64 1, i64 13, i1 false)
// CHECK: call i32 @get_hakc_address_color(ptr %1)
// CHECK: call ptr @hakc_transfer_to_clique(ptr %1, i64 32, i64 1, i64 13, i1 false)
// CHECK: call i32 @get_hakc_address_color(ptr %2)
// CHECK: call ptr @hakc_transfer_to_clique(ptr %2, i64 32, i64 1, i64 13, i1 false)
// CHECK: call i32 @HAKC_ORIG_foo(ptr %5, ptr %7, ptr %9)
// CHECK: call void @hakc_color_address(ptr %2, i32 %8, i64 32)
// CHECK: call void @hakc_color_address(ptr %1, i32 %6, i64 32)
// CHECK: call void @hakc_color_address(ptr %0, i32 %4, i64 64)
