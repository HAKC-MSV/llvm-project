// RUN: %HAKC_EMIT_LLVM
// RUN: %HAKC_PROCESS_YAML_COMP_CONFIG
// RUN: %HAKC_PROCESS_YAML_COMP_DAG_CONFIG
// RUN: %HAKC_PROCESS_YAML_POLICY_CONFIG
// RUN: %HAKC_RUN_DAG_PASS
// RUN: %HAKC_PYTHON_VENV
// RUN: %HAKC_RUN_PYTHON_DAG
// RUN: %HAKC_START_POLICY_SERVER & sleep 1 &&\
// RUN: %HAKC_RUN_COMP_PASS
// RUN: %HAKC_EVALUATE

// testing never used struct types 
struct list_head{
    int a;
};

int foo(struct list_head * ListHead) {
    if(ListHead){
        return 1;
    }
    return 0;
}

// The type "list_head" is in the ignored types list, and should never be transferred
// CHECK-NOT: i32 @get_hakc_address_color(ptr %0)
// CHECK-NOT: ptr @hakc_transfer_to_clique(ptr %0, i64 32, i64 1, i64 13, i1 false)
