// RUN: %HAKC_PROCESS_YAML_COMP_CONFIG
// RUN: %HAKC_PROCESS_YAML_POLICY_CONFIG
// RUN: %HAKC_PYTHON_VENV
// RUN: %HAKC_EMIT_LLVM
// RUN: %HAKC_START_POLICY_SERVER & sleep 1 &&\
// RUN: %HAKC_RUN_PASS
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

// note: incomplete test. need derricks help for expected behavior
// CHECK: call ptr @check_hakc_data_access(ptr %8, i64 2, i64 139264)
// CHECK: getelementptr inbounds %struct.data_struct, ptr %9, i32 0, i32 0
// CHECK: getelementptr inbounds %struct.data_struct, ptr %8, i32 0, i32 0

// CHECK: define dso_local i32 @HAKC_XFER_bar(ptr noundef %0)
// CHECK: HAKCTransferEntry:
// CHECK: call ptr @hakc_transfer_to_clique(ptr %0, i64 4, i64 1, i64 13, i1 false)
// CHECK: call i32 @HAKC_ORIG_bar(ptr %1)

// CHECK: define dso_local i32 @HAKC_XFER_foo(ptr noundef %0)
// CHECK: HAKCTransferEntry:
// CHECK: call ptr @hakc_transfer_to_clique(ptr %0, i64 4, i64 2, i64 13, i1 false)
// CHECK: call i32 @HAKC_ORIG_foo(ptr %1)
