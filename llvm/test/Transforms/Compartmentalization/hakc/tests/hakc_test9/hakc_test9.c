// RUN: %HAKC_START_POLICY_SERVER
// RUN: clang -g -S -emit-llvm -mllvm --enable-hakc -mllvm --hakc-config=%HAKC_CONFIG -o %t %s
// RUN: %HAKC_EVALUATE
// test of nested compartment calls

struct data_struct {
    int a;
};

int bar(struct data_struct *b){
    if(b){
        return (b->a)++;
    }
    return (b->a)--; 
}

int foo(struct data_struct *a) {
    if (a) {
        struct data_struct * b;
        b->a = 0;
        return bar(b);
    }
    return 0;
}

// CHECK-LABEL: i32 @HAKC_XFER_bar(ptr noundef %0)
// CHECK: call i32 @get_hakc_address_color(ptr %0)
// CHECK: call ptr @hakc_transfer_to_clique(ptr %0, i64 32, i64 1, i64 13, i1 false)
// CHECK: call i32 @HAKC_ORIG_bar(ptr %3)
// CHECK: call void @hakc_color_address(ptr %0, i32 %2, i64 32)

// CHECK-LABEL: i32 @HAKC_XFER_foo(ptr noundef %0)
// CHECK: call i32 @get_hakc_address_color(ptr %0)
// CHECK: call ptr @hakc_transfer_to_clique(ptr %0, i64 32, i64 2, i64 13, i1 false)
// CHECK: call i32 @HAKC_ORIG_foo(ptr %3)
// CHECK: call void @hakc_color_address(ptr %0, i32 %2, i64 32)
