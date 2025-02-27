// RUN: %HAKC_PROCESS_YAML_COMP_CONFIG
// RUN: %HAKC_PROCESS_YAML_POLICY_CONFIG
// RUN: %HAKC_PYTHON_VENV
// RUN: %HAKC_EMIT_LLVM
// RUN: %HAKC_START_POLICY_SERVER & sleep 1 &&\
// RUN: %HAKC_RUN_PASS
// RUN: %HAKC_EVALUATE
struct linked_list {
    struct linked_list *next;
};

struct data {
    struct linked_list list;
    void *data;
};

void init_data(struct data *data) {
    data->data = 0;
    data->list.next = &data->list;
}

// ** update this test once check_data_access is working
// CHECK: call i8* @check_hakc_data_access(i8* %2, i32 6, i64 393218)
// CHECK-LABEL: @HAKC_XFER_init_data(ptr noundef %0)
// CHECK-LABEL: HAKCTransferEntry:
// CHECK: call ptr @hakc_transfer_to_clique(ptr %0, i64 16, i64 1, i64 13, i1 false)
// CHECK: call void @HAKC_ORIG_init_data(ptr %1)
