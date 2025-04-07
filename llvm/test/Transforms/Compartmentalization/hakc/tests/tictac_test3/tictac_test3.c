// RUN: %HAKC_PYTHON_VENV
// RUN: %HAKC_START_POLICY_SERVER
// RUN: %HAKC_RUN_COMP_PASS
// RUN: %HAKC_EVALUATE

typedef struct {
  int portid;
  int socket_cnt;
  char buf[2];
} nftables_data_t;

void setup(nftables_data_t* data){
  data->portid = 0;
  data->socket_cnt = 0;
  data->buf[0] = 'a';
  data->buf[1] = 'z';
}

void steady_state(nftables_data_t* data){
  ++data->socket_cnt;
}

void teardown(nftables_data_t* data){
  data->portid = -1;
  data->socket_cnt = -1;
  data->buf[0] = '\0';
  data->buf[1] = '\0';
}

void exit();

// Note: perform both the post dom analysis and the compartmentalization in this test
// TODO: add checks -> write post dom analysis result to text file, then use file check for result
void entry(int a, int b) {
  nftables_data_t data;
  setup(&data);
  if(b > 0){
    steady_state(&data);
  }
  else{
    steady_state(&data);
  }
  teardown(&data);
  exit();
}
