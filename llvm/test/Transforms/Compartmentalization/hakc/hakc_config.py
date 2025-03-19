import os
import subprocess
import sys

HAKC_DEFAULT_COMPARTMENT = "0"
HAKC_DEFAULT_DIVISION = "0"
HAKC_SERVER_TIMEOUT = "10"


def HAKC_PROCESS_YAML_CONFIGS(HAKC_TEST_SOURCE, HAKC_BASE_FNAME, HAKC_BUILD_PATH, HAKC_BACKING_TYPE):
    global HAKC_DEFAULT_COMPARTMENT, HAKC_DEFAULT_DIVISION, HAKC_SERVER_TIMEOUT
    # process the first compression yaml
    try:
        yaml_comp_in = os.path.dirname(os.path.dirname(HAKC_TEST_SOURCE)) + "/configs/hakc_comp_config.yml.in"
        print(yaml_comp_in)

        with open(yaml_comp_in, 'r') as file:
            content = file.read()

        content = content.replace("@HAKC_BUILD_PATH@", HAKC_BUILD_PATH)
        content = content.replace("@HAKC_SOURCE_PATH_ENTRY@", os.path.dirname(HAKC_TEST_SOURCE))
        content = content.replace("@HAKC_DAG_ANALYSIS_PATH@", HAKC_BUILD_PATH + "/dag-analysis")
        content = content.replace("@HAKC_PASS_MODE@", "RunCompartmentalization")
        yaml_comp_out = HAKC_BUILD_PATH + "/hakc_comp_config.yml"
        with open(yaml_comp_out, 'w') as file:
            file.write(content)
    except FileNotFoundError as e:
        print(f"An error occurred: {e}")
        print(f"Error type: {type(e)}")
        print(f"File path: {e.filename}")  # e.filename may not be available in all exception scenarios
    except Exception as e:
        print(f"An unexpected error occurred: {e}")
        print(f"Error type: {type(e)}")

    # process the second compression yaml
    try:
        yaml_comp_dag_in = os.path.dirname(os.path.dirname(HAKC_TEST_SOURCE)) + "/configs/hakc_comp_config.yml.in"
        yaml_comp_dag_out = HAKC_BUILD_PATH + "/hakc_comp_dag_config.yml"
        with open(yaml_comp_dag_in, 'r') as file:
            content = file.read()

        content = content.replace("@HAKC_BUILD_PATH@", HAKC_BUILD_PATH)
        content = content.replace("@HAKC_SOURCE_PATH_ENTRY@", os.path.dirname(HAKC_TEST_SOURCE))
        content = content.replace("@HAKC_DAG_ANALYSIS_PATH@", HAKC_BUILD_PATH + "/dag-analysis")
        content = content.replace("@HAKC_PASS_MODE@", "RunDataAccessGraphAnalysis")

        with open(yaml_comp_dag_out, 'w') as file:
            file.write(content)
    except FileNotFoundError as e:
        print(f"An error occurred: {e}")
        print(f"Error type: {type(e)}")
        print(f"File path: {e.filename}")  # e.filename may not be available in all exception scenarios
    except Exception as e:
        print(f"An unexpected error occurred: {e}")
        print(f"Error type: {type(e)}")

    # process the policy yaml
    try:
        yaml_policy_in = os.path.dirname(os.path.dirname(HAKC_TEST_SOURCE)) + "/configs/hakc_policy_config.yml.in"
        yaml_policy_out = HAKC_BUILD_PATH + "/hakc_policy_config.yml"
        with open(yaml_policy_in, 'r') as file:
            content = file.read()
        # enums too much work for this simple case
        if (HAKC_BACKING_TYPE == "yaml"):
            content = content.replace("@HAKC_BACKING_STORE@",
                                      str(HAKC_BUILD_PATH + "/backing_" + HAKC_BASE_FNAME + ".yml"))
        elif (HAKC_BACKING_TYPE == "kuzu"):
            content = content.replace("@HAKC_BACKING_STORE@", str(HAKC_BUILD_PATH + "/hakc-db"))

        content = content.replace("@HAKC_DEFAULT_COMPARTMENT@", HAKC_DEFAULT_COMPARTMENT)
        content = content.replace("@HAKC_DEFAULT_DIVISION@", HAKC_DEFAULT_DIVISION)
        content = content.replace("@HAKC_SERVER_TIMEOUT@", HAKC_SERVER_TIMEOUT)

        with open(yaml_policy_out, 'w') as file:
            file.write(content)
    except FileNotFoundError as e:
        print(f"An error occurred: {e}")
        print(f"Error type: {type(e)}")
        print(f"File path: {e.filename}")  # e.filename may not be available in all exception scenarios
    except Exception as e:
        print(f"An unexpected error occurred: {e}")
        print(f"Error type: {type(e)}")


def run_command(command):
    result = subprocess.run(command, capture_output=True, text=True, shell=True)
    if result.returncode == 0:
        return result.stdout.strip()
    else:
        return f"Error: {result.stderr.strip()}"


def process(HAKC_TEST_SOURCE, HAKC_BACKING_TYPE):
    HAKC_BASE_FNAME = os.path.basename(os.path.dirname(HAKC_TEST_SOURCE)).replace(".c", "")
    HAKC_BUILD_PATH = os.path.dirname(HAKC_TEST_SOURCE).replace("llvm-project", "cmake-build-hakc-llvm/llvm-project")

    HAKC_PROCESS_YAML_CONFIGS(HAKC_TEST_SOURCE, HAKC_BASE_FNAME, HAKC_BUILD_PATH, HAKC_BACKING_TYPE)


# Only process files here that rely on the hakc_test_n file
if __name__ == "__main__":
    print("Configuring ")

    n = len(sys.argv)
    if n == 2:
        TEST_SOURCE = sys.argv[1]
        print(TEST_SOURCE)
        process(TEST_SOURCE)
    else:
        print("ERROR Incorrect Args")
        exit(1)
