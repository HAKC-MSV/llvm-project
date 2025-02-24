// RUN: %HAKC_PROCESS_YAML_COMP_CONFIG
// RUN: %HAKC_PROCESS_YAML_POLICY_CONFIG
// RUN: %HAKC_PYTHON_VENV
// RUN: %HAKC_EMIT_LLVM
// RUN: %HAKC_START_POLICY_SERVER & sleep 1 &&\
// RUN: %HAKC_RUN_PASS
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

// note: incomplete test. need derricks help for expected behavior 
// CHECK-LABEL: HAKC_XFER_static_branch_foo(ptr noundef %0)
// CHECK-LABEL: HAKCTransferEntry:
// CHECK: call ptr @hakc_transfer_to_clique(ptr %0, i64 1, i64 1, i64 13, i1 false)
// CHECK: call i32 @HAKC_ORIG_static_branch_foo(ptr %1)
