from typing import Dict, List, Optional, Union, Any
import os 
import yaml
import glob
import subprocess
import pandas as pd 
import re
import random
import time
from dataclasses import dataclass
from pathlib import Path

# Constants
TIMEOUT: int = 900

@dataclass
class BenchmarkResult:
    """Data class to store benchmark results."""
    input_file: str
    property_file: Optional[str]
    expected: Optional[str]
    data_model: Optional[int]
    exit_code: Optional[Union[int, str]]
    last_line: Optional[str]
    runtime_solver: List[float]
    runtime_decision: List[float]
    number_of_variables: List[int]
    number_of_clauses: List[int]
    total_runtime: float

@dataclass
class BenchmarkSet:
    """Data class to store benchmark task information."""
    task_name: str
    set_file: str
    property_file: str

@dataclass
class SingleBenchmarkTask:
    """The dictionary that is parsed from the yml file."""
    task_dict: Dict[str, Any]


def parse_yml_file(yml_file: str, ignore_properties: bool = False) -> SingleBenchmarkTask:
    """
    Parse a YAML file containing benchmark configuration.
    
    Args:
        yml_file: Path to the YAML file
        ignore_properties: Whether to ignore property files in parsing
        
    Returns:
        Dictionary containing parsed configuration
    """
    base_dir = os.path.dirname(yml_file)
    base_dir_prop = os.path.dirname(base_dir)

    with open(yml_file, 'r') as file:
        data = yaml.safe_load(file)
    
    task: Dict[str, Any] = {"input_file": "", "data_model": None, "property_files": {}}
    
    # Extract input file name
    if 'input_files' in data:
        task['input_file'] = os.path.join(base_dir, data['input_files'])
    
    # Extract data model (32 or 64)
    if 'options' in data and 'data_model' in data['options']:
        data_model = data['options']['data_model']
        if 'ILP32' in data_model:
            task['data_model'] = 32
        elif 'ILP64' in data_model or 'LP64' in data_model:
            task['data_model'] = 64
    
    if ignore_properties:
        return SingleBenchmarkTask(task_dict=task)
    
    # Extract property files with expected results
    if 'properties' in data:
        property_files: Dict[str, str] = {}
        for prop in data['properties']:
            if 'property_file' in prop and 'expected_verdict' in prop:
                property_relative_path = prop['property_file'][3:]  # Remove the leading '../'
                property_files[os.path.join(base_dir_prop, property_relative_path)] = prop['expected_verdict']
        
        if property_files:
            task['property_files'] = property_files
    
    return SingleBenchmarkTask(task_dict=task)

def parse_set_file(set_file: str, ignore_properties: bool = False) -> List[SingleBenchmarkTask]:
    """
    Parse a set file containing paths to YAML files.
    
    Args:
        set_file: Path to the set file
        ignore_properties: Whether to ignore property files in parsing
        
    Returns:
        List of dictionaries containing parsed configurations
    """
    tasks: List[SingleBenchmarkTask] = []
    base_dir = os.path.dirname(set_file)
    
    with open(set_file, 'r') as file:
        lines = [line.strip() for line in file if line.strip() and not line.strip().startswith('#')]
    
    for line in lines:
        # Handle wildcards in file paths
        if line.startswith('#'):
            continue
        if '*' in line:
            pattern = os.path.join(base_dir, line)
            matching_files = glob.glob(pattern)
            for yml_file in matching_files:
                try:
                    parsed_data = parse_yml_file(yml_file, ignore_properties)
                    tasks.append(parsed_data)
                except Exception as e:
                    print(f"Error parsing {yml_file}: {str(e)}")
        else:
            # Handle direct file paths
            yml_file = os.path.join(base_dir, line)
            try:
                parsed_data = parse_yml_file(yml_file, ignore_properties)
                tasks.append(parsed_data)
            except Exception as e:
                print(f"Error parsing {yml_file}: {str(e)}")
    
    return tasks

def run_with_args(cmd_args: List[str], cwd: str) -> Dict[str, Any]:
    """
    Run a command with arguments and capture its output.
    
    Args:
        cmd_args: List of command arguments
        cwd: Working directory
        
    Returns:
        Dictionary containing command results and metrics
    """
    print(f"Running command: {' '.join(cmd_args)}")
    
    env = os.environ.copy()
    
    # Initialize default metrics
    metrics: Dict[str, Any] = {
        'runtime_solver': [],
        'runtime_decision': [], 
        'number_of_variables': [],
        'number_of_clauses': [],
        'exit_code': None,
        'last_line': None
    }
    
    result = None
    start_time = time.time()
    try:
        result = subprocess.run(
            cmd_args,
            capture_output=True,
            text=True,
            cwd=cwd,
            env=env
        )
    except Exception as e:
        print(f"Error running command: {' '.join(cmd_args)}")
        print(f"Error: {str(e)}")
        metrics['exit_code'] = "ERROR"
        metrics['last_line'] = f"Error: {str(e)}"
    end_time = time.time()
    total_time = end_time - start_time
    print(result)
    
    # Try to read stdout even if an exception occurred
    if result and result.stdout:
        output_lines = result.stdout.strip().split('\n')
        
        for line in output_lines:
            line = line.strip()
            
            if line.startswith('EC='):
                metrics['exit_code'] = int(line[3:])
            elif line.startswith('Runtime Solver:'):
                runtime = float(line.split(':')[1].strip().replace('s', ''))
                metrics['runtime_solver'].append(runtime)
            elif line.startswith('Runtime decision procedure:'):
                runtime = float(line.split(':')[1].strip().replace('s', ''))
                metrics['runtime_decision'].append(runtime)
            elif line and line[0].isdigit() and "variables" in line:
                metrics['number_of_variables'].append(int(line.split()[0]))
                metrics['number_of_clauses'].append(int(line.split()[2]))

        if output_lines:
            metrics['last_line'] = output_lines[-1]
            
    metrics['total_runtime'] = total_time
    
    return metrics

def run_wrapper(wrapper_path: str, tasks: List[SingleBenchmarkTask], global_property_file: Optional[str] = None) -> List[BenchmarkResult]:
    """
    Run the wrapper with given input configurations.
    
    Args:
        wrapper_path: Path to the wrapper executable
        input_dict: List of input configurations
        global_property_file: Optional global property file to use
        
    Returns:
        List of benchmark results
    """
    results: List[BenchmarkResult] = []

    for task_obj in tasks:
        task = task_obj.task_dict
        base_cmd = [wrapper_path]
        
        if 'data_model' in task:
            if task['data_model'] == 32:
                base_cmd.append('--32')
            elif task['data_model'] == 64:
                base_cmd.append('--64')
        
        if 'input_file' in task:
            base_cmd.append(task['input_file'])
        
        if global_property_file:
            cmd_with_global_prop = base_cmd.copy()
            cmd_with_global_prop.extend(['--propertyfile', global_property_file])
            
            result = run_with_args(cmd_with_global_prop, cwd=os.path.dirname(wrapper_path))
            results.append(BenchmarkResult(
                input_file=task.get('input_file', ''),
                property_file=global_property_file,
                expected=task['property_files'].get(global_property_file, ''),
                data_model=task.get('data_model', ''),
                exit_code=result.get('exit_code'),
                last_line=result.get('last_line'),
                runtime_solver=result.get('runtime_solver', []),
                runtime_decision=result.get('runtime_decision', []),
                number_of_variables=result.get('number_of_variables', []),
                number_of_clauses=result.get('number_of_clauses', []),
                total_runtime=result.get('total_runtime', 0)
            ))
        elif 'property_files' in task:
            for prop_file, expected in task['property_files'].items():
                cmd_with_prop = base_cmd.copy()
                cmd_with_prop.extend(['--propertyfile', prop_file])
                
                result = run_with_args(cmd_with_prop, cwd=os.path.dirname(wrapper_path))
                results.append(BenchmarkResult(
                    input_file=task.get('input_file', ''),
                    property_file=prop_file,
                    expected=expected,
                    data_model=task.get('data_model', ''),
                    exit_code=result.get('exit_code'),
                    last_line=result.get('last_line'),
                    runtime_solver=result.get('runtime_solver', []),
                    runtime_decision=result.get('runtime_decision', []),
                    number_of_variables=result.get('number_of_variables', []),
                    number_of_clauses=result.get('number_of_clauses', []),
                    total_runtime=result.get('total_runtime', 0)
                ))
        else:
            raise ValueError(f"No property files found for task {task}")
            result = run_with_args(base_cmd, cwd=os.path.dirname(wrapper_path))
            results.append(BenchmarkResult(
                input_file=task.get('input_file', ''),
                property_file=None,
                expected=None,
                data_model=task.get('data_model', ''),
                exit_code=result.get('exit_code'),
                last_line=result.get('last_line'),
                runtime_solver=result.get('runtime_solver', []),
                runtime_decision=result.get('runtime_decision', []),
                number_of_variables=result.get('number_of_variables', []),
                number_of_clauses=result.get('number_of_clauses', []),
                total_runtime=result.get('total_runtime', 0)
            ))
            
    return results

def dump_into_csv(results: List[BenchmarkResult], csv_file: str = 'results.csv') -> None:
    """
    Save benchmark results to a CSV file.
    
    Args:
        results: List of benchmark results
        csv_file: Path to output CSV file
    """
    file_exists = os.path.isfile(csv_file)
    
    for entry in results:
        row = {
            'input_file': entry.input_file.split('sv-benchmarks')[-1][1:] if 'sv-benchmarks' in entry.input_file else entry.input_file,
            'property_file': entry.property_file.split('sv-benchmarks')[-1][1:] if entry.property_file and 'sv-benchmarks' in entry.property_file else entry.property_file,
            'expected': entry.expected,
            'data_model': entry.data_model,
            'exit_code': entry.exit_code,
            'last_line': entry.last_line,
            'runtime_solver': entry.runtime_solver,
            'runtime_decision': entry.runtime_decision,
            'number_of_variables': entry.number_of_variables,
            'number_of_clauses': entry.number_of_clauses,
            'total_runtime': entry.total_runtime
        }
        
        df = pd.DataFrame([row])
        df.to_csv(csv_file, mode='a', header=not file_exists, index=False)
        file_exists = True
        
    print(f"Results have been saved incrementally to {csv_file}")

def extract_benchmark_set(benchmark_xml_path: str) -> List[BenchmarkSet]:
    """
    Extract benchmark task information from XML file.
    
    Args:
        benchmark_xml_path: Path to benchmark XML file
        
    Returns:
        List of benchmark tasks
    """
    with open(benchmark_xml_path, 'r') as file:
        benchmark_xml = file.read()
    
    pairs: List[BenchmarkSet] = []
    
    pattern = r'<tasks name="([^"]*)">\s*<includesfile>(.*?)</includesfile>\s*<propertyfile>(.*?)</propertyfile>\s*</tasks>'
    matches = re.findall(pattern, benchmark_xml, re.DOTALL)
    
    for task_name, set_file, prop_file in matches:
        clean_set_file = set_file.split('sv-benchmarks')[-1][1:] if 'sv-benchmarks' in set_file else set_file
        clean_prop_file = prop_file.split('sv-benchmarks')[-1][1:] if 'sv-benchmarks' in prop_file else prop_file
        
        pairs.append(BenchmarkSet(
            task_name=task_name,
            set_file=clean_set_file,
            property_file=clean_prop_file
        ))
    
    return pairs

def cleanup():
    commands = ["killall cbmc",
    "killall MainThread",
    "killall mpirun",
    "killall mallob",
    "killall mallob_sat_process"]   
    for command in commands:
        try:
            subprocess.run(command, shell=True, check=True)
        except subprocess.CalledProcessError as e:
            print(f"Error executing command: {command}. Error: {str(e)}")

def start_mallob_filesystem(path):
    mallob_cmd = ["build/mallob", "-t=16", "-compress-models"]
    mallob_process = subprocess.Popen(
        mallob_cmd, 
        cwd=path,
        stdout=subprocess.PIPE,
        stderr=subprocess.PIPE
    )
    print(f"Started Mallob process with PID {mallob_process.pid}")
    time.sleep(5)


def main(wrapper_path, set_file_path, global_property_file, result_csv_file):
    
    batch_size = 1
    file_per_set = 5

    parsed_set_data = parse_set_file(set_file_path)
    
    #random.seed(42)
    
    #random.shuffle(parsed_set_data)
    
    for i in range(0, min(file_per_set, len(parsed_set_data)), batch_size):
        current_batch = parsed_set_data[i:min(i+batch_size, len(parsed_set_data))]
        print(f"Processing batch {i//batch_size + 1} of {(len(parsed_set_data) + batch_size - 1) // batch_size}...")
        results = run_wrapper(wrapper_path, current_batch, global_property_file)   
        dump_into_csv(results, result_csv_file)


def run_benchmark_defs():

    BASE_DIR = '/home/oguz/Desktop/hiwi_code'
    BENCHMARK_DIR = BASE_DIR + '/benchmark/sv-benchmarks/'
    CBMC_WRAPPER = BASE_DIR + '/cbmc/cbmc-wrapper'
    MALLOB_WRAPPER = BASE_DIR + '/cbmc_mallob_monolithic/mallob/mallob-wrapper'
    FILESYSTEM_WRAPPER = BASE_DIR + '/cbmc_mallob_filesystem/cbmc/cbmc-wrapper'
    RESULTS_DIR = BASE_DIR + '/cbmc_mallob_monolithic/mallob/results/'
    MALLOB_DIR = BASE_DIR + '/mallob/'

    TOOLS = {"CBMC": CBMC_WRAPPER, "MALLOB": MALLOB_WRAPPER, "MALLOB-FILESYSTEM": FILESYSTEM_WRAPPER}

    benchmark_sets = (extract_benchmark_set(BASE_DIR + '/benchmark/benchmark-defs/cbmc.xml'))
    #print(pairs)

    to_test = ["NoOverflows-Main", "ReachSafety-ECA", "ReachSafety-Floats", "ReachSafety-Fuzzle", 
               "ReachSafety-Heap", "ReachSafety-Recursive", "ReachSafety-Sequentialized", "ReachSafety-XCSP", "SoftwareSystems-coreutils-MemSafety", 
               "SoftwareSystems-coreutils-NoOverflows", "SoftwareSystems-AWS-C-Common-ReachSafety", "SoftwareSystems-DeviceDriversLinux64-MemSafety",
               ]
    to_test = to_test[:1]
    

    benchmark_sets = [benchmark_set for benchmark_set in benchmark_sets if benchmark_set.task_name in to_test]
    benchmark_sets = benchmark_sets[:1]

    for benchmark_set in benchmark_sets:
        for tool in ['MALLOB-FILESYSTEM']: 
            set_file = BENCHMARK_DIR + benchmark_set.set_file
            prop_file = BENCHMARK_DIR + benchmark_set.property_file
            result_csv_file = RESULTS_DIR + tool + '_' + benchmark_set.task_name + '.csv'
            wrapper = TOOLS[tool]

            start_mallob_filesystem(MALLOB_DIR) if tool == 'MALLOB-FILESYSTEM' else None

            print(f"Running {tool} on {benchmark_set.task_name} with set file {set_file} and property file {prop_file}")
            try:
                main(wrapper, set_file, prop_file, result_csv_file)
            except KeyboardInterrupt:
                print("Process interrupted by user.")
                cleanup()
                break   
            # except Exception as e:
            #     print(f"Error running {tool} on {pair.task_name}: {str(e)}")
            #     cleanup()
            #     start_mallob_filesystem(wrapper) if tool == 'MALLOB-FILESYSTEM' else None
            #     continue
    
    cleanup()

if __name__ == "__main__":

    # BASE_DIR = '/home/oguz/Desktop/hiwi_code'
    # BENCHMARK_DIR = BASE_DIR + '/benchmark/sv-benchmarks/'
    # CBMC_WRAPPER = BASE_DIR + '/cbmc/cbmc-wrapper'
    # MALLOB_WRAPPER = BASE_DIR + '/cbmc_mallob_monolithic/mallob/mallob-wrapper'
    # FILESYSTEM_WRAPPER = BASE_DIR + '/cbmc_mallob_filesystem/cbmc/cbmc-wrapper'
    # RESULTS_DIR = BASE_DIR + '/cbmc_mallob_monolithic/mallob/results/'

    # TOOLS = {"CBMC": CBMC_WRAPPER, "MALLOB": MALLOB_WRAPPER, "MALLOB-FILESYSTEM": FILESYSTEM_WRAPPER}

    # to_test = ['c/floats-cdfpl/square_6.yml', 'c/hardware-verification-bv/btor2c-lazyMod.brp2.4.prop1-back-serstep.yml',
    #            'c/eca-rers2012/Problem14_label39.yml', 'c/hardware-verification-bv/btor2c-lazyMod.peg_solitaire.6.prop1-func-interl.yml']

    # yml_files = [os.path.join(BENCHMARK_DIR, file) for file in to_test]
    # #yml_data = [parse_yml_file(yml_file, ignore_properties=False) for yml_file in yml_files]
    # try:
    #     for yml_file in yml_files:
    #         print(f"Parsing {yml_file}")
    #         parsed_data = parse_yml_file(yml_file, ignore_properties=False)
    #         print(parsed_data)
    #         #results = run_wrapper(CBMC_WRAPPER, [parsed_data])   
    #         #print(results)
    #         #dump_into_csv(results, 'cbmc_handpicked_results.csv')
    #         results = run_wrapper(MALLOB_WRAPPER, [parsed_data])   
    #         print(results)
    #         dump_into_csv(results, 'mallob_handpicked_results.csv')
    # except KeyboardInterrupt:
    #     print("Process interrupted by user.")
    #     cleanup()
    # except Exception as e: 
    #     print(f"Error running on handpicked files: {str(e)}")
    #     cleanup()

    # start_mallob_filesystem(BASE_DIR + '/mallob')
    # try:
    #     for yml_file in yml_files:
    #         print(f"Parsing {yml_file}")
    #         parsed_data = parse_yml_file(yml_file, ignore_properties=False)
    #         print(parsed_data)
    #         results = run_wrapper(FILESYSTEM_WRAPPER, [parsed_data])   
    #         print(results)
    #         dump_into_csv(results, 'filesystem_handpicked_results.csv')
    # except KeyboardInterrupt:
    #     print("Process interrupted by user.")
    #     cleanup()
    # except Exception as e: 
    #     print(f"Error running on handpicked files: {str(e)}")
    #     cleanup()

    run_benchmark_defs()