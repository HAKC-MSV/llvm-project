// RUN: %HAKC_PYTHON_VENV
// RUN: %HAKC_START_POLICY_SERVER
// RUN: %HAKC_RUN_COMP_PASS
// RUN: %HAKC_EVALUATE

// testing static branch
struct data_struct {
    int a;
};

int static_branch_foo(struct data_struct* ds) {
// CHECK-NOT: call void @check_hakc_data_access
    if(ds){
        return 1; 
    }
    return 0;
}

// CHECK-LABEL: i32 @HAKC_XFER_static_branch_foo(ptr noundef %0) #0 section ".hakc.1.text" {
// CHECK: call i32 @get_hakc_address_color(ptr %0)
// CHECK: call ptr @hakc_transfer_to_clique(ptr %0, i64 4, i64 1, i64 13, i1 false)
// CHECK: call i32 @HAKC_ORIG_static_branch_foo(ptr %2)
// CHECK: call void @hakc_color_address(ptr %0, i32 %1, i64 4)
