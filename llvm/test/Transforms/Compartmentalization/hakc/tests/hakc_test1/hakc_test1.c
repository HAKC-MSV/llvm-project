// RUN: %HAKC_START_POLICY_SERVER & sleep 1 &&\
// RUN: clang -g -S -emit-llvm -mllvm --enable-hakc -mllvm --hakc-config %HAKC_CONFIG -o %t %s
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
// CHECK: call ptr @hakc_transfer_to_clique(ptr %0, i64 64, i64 1, i64 13, i1 false)
// CHECK: call i32 @HAKC_ORIG_foo(ptr %3)
// CHECK: call void @hakc_color_address(ptr %0, i32 %2, i64 64)
