// RUN: %HAKC_START_POLICY_SERVER & sleep 1 &&\
// RUN: clang -g -S -emit-llvm -mllvm --enable-hakc -mllvm --hakc-config %HAKC_CONFIG -o %t %s
// RUN: %HAKC_EVALUATE
// testing static branch
struct data_struct {
    int a;
};

int static_branch_foo(struct data_struct* ds) {
    if(ds){
        return 1; 
    }
    return 0;
}

// CHECK-LABEL: i32 @HAKC_XFER_static_branch_foo(ptr noundef %0) #0 section ".hakc.1.text" {
// CHECK: call i32 @get_hakc_address_color(ptr %0)
// CHECK: call ptr @hakc_transfer_to_clique(ptr %0, i64 32, i64 1, i64 13, i1 false)
// CHECK: call i32 @HAKC_ORIG_static_branch_foo(ptr %3)
// CHECK: call void @hakc_color_address(ptr %0, i32 %2, i64 32)
