// RUN: %HAKC_PYTHON_VENV
// RUN: %HAKC_START_POLICY_SERVER
// RUN: %HAKC_RUN_COMP_PASS
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

// CHECK-LABEL: void @HAKC_XFER_init_data(ptr noundef %0)
// CHECK: call i32 @get_hakc_address_color(ptr %0)
// CHECK: call ptr @hakc_transfer_to_clique(ptr %0, i64 128, i64 1, i64 13, i1 false)
// CHECK: call void @HAKC_ORIG_init_data(ptr %2)
// CHECK: call void @hakc_color_address(ptr %0, i32 %1, i64 128)
