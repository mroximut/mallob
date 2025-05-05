import os 
import yaml  # Adding the missing import
import glob
import subprocess
import pandas as pd 
import re

def parse_yml_file(yml_file, ignore_properties=False):

    base_dir = os.path.dirname(yml_file)
    base_dir_prop = os.path.dirname(base_dir)

    with open(yml_file, 'r') as file:
        data = yaml.safe_load(file)
    
    result = {}
    
    # Extract input file name
    if 'input_files' in data:
        result['input_file'] = os.path.join(base_dir, data['input_files'])
    
    # Extract data model (32 or 64)
    if 'options' in data and 'data_model' in data['options']:
        data_model = data['options']['data_model']
        if 'ILP32' in data_model:
            result['data_model'] = 32
        elif 'ILP64' in data_model or 'LP64' in data_model:
            result['data_model'] = 64
    
    if ignore_properties:
        return result
    
    # Extract property files with expected results
    if 'properties' in data:
        property_files = {}
        for prop in data['properties']:
            if 'property_file' in prop and 'expected_verdict' in prop:
                property_relative_path = prop['property_file'][3:] # Remove the leading '../'
                property_files[os.path.join(base_dir_prop, property_relative_path)] = prop['expected_verdict']
        
        if property_files:
            result['property_files'] = property_files
    
    return result



def parse_set_file(set_file, ignore_properties=False):

    results = []
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
                    #parsed_data['yml_file'] = yml_file  # Include the source file path
                    results.append(parsed_data)
                except Exception as e:
                    print(f"Error parsing {yml_file}: {str(e)}")
        else:
            # Handle direct file paths
            yml_file = os.path.join(base_dir, line)
            try:
                parsed_data = parse_yml_file(yml_file, ignore_properties)
                #parsed_data['yml_file'] = yml_file  # Include the source file path
                results.append(parsed_data)
            except Exception as e:
                print(f"Error parsing {yml_file}: {str(e)}")
    
    return results


def run_with_args(cmd_args, cwd):
    print(f"Running command: {' '.join(cmd_args)}")
    
    env = os.environ.copy()
    # Use process group to control memory limits better
    #env["MALLOC_ARENA_MAX"] = "4"  # Limit memory arenas
    
    try:
        result = subprocess.run(
            cmd_args, 
            capture_output=True, 
            text=True, 
            cwd=cwd,  
            timeout=5, 
            env=env
        )
    except subprocess.TimeoutExpired:
        print(f"Command timed out: {' '.join(cmd_args)}")
        cleanup()
        return {}   
    print(result.stdout)
    
    output_lines = result.stdout.strip().split('\n')
    
    metrics = {}
    metrics['runtime_solver'] = []
    metrics['runtime_decision'] = []
    metrics['number_of_variables'] = []
    metrics['number_of_clauses'] = []

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

    return metrics


def run_wrapper(wrapper_path, input_dict, global_property_file=None):
    results = []

    for item in input_dict:
        base_cmd = [wrapper_path]
        
        if 'data_model' in item:
            if item['data_model'] == 32:
                base_cmd.append('--32')
            elif item['data_model'] == 64:
                base_cmd.append('--64')
        
        # Add input file
        if 'input_file' in item:
            base_cmd.append(item['input_file'])
        
        if global_property_file:
            # Run with global property file
            cmd_with_global_prop = base_cmd.copy()
            cmd_with_global_prop.append('--propertyfile')
            cmd_with_global_prop.append(global_property_file)
            
            # Run command and capture result
            result = run_with_args(cmd_with_global_prop, cwd=os.path.dirname(wrapper_path))
            results.append({
                'input_file': item.get('input_file', ''),
                'property_file': global_property_file,
                'expected': item['property_files'].get(global_property_file, ''),
                'result': result
            })

                # Add property files if they exist
        elif 'property_files' in item:
            for prop_file, expected in item['property_files'].items():
                cmd_with_prop = base_cmd.copy()
                cmd_with_prop.append('--propertyfile')
                cmd_with_prop.append(prop_file)
                
                # Run command and capture result
                result = run_with_args(cmd_with_prop, cwd=os.path.dirname(wrapper_path))
                results.append({
                    'input_file': item.get('input_file', ''),
                    'property_file': prop_file,
                    'expected': expected,
                    'result': result
                })
        
        else:
            # Run without property file
            result = run_with_args(base_cmd, cwd=os.path.dirname(wrapper_path))
            results.append({
                'input_file': item.get('input_file', ''),
                'result': result
            })
            
    return results

def dump_into_csv(results, csv_file='results.csv'): 
    file_exists = os.path.isfile(csv_file)
    
    # Process results into rows
    for entry in results:
        row = {
            'input_file': entry['input_file'].split('sv-benchmarks')[-1][1:] if 'sv-benchmarks' in entry['input_file'] else entry['input_file'],
            'property_file': entry.get('property_file', '').split('sv-benchmarks')[-1][1:] if entry.get('property_file', '') and 'sv-benchmarks' in entry.get('property_file', '') else entry.get('property_file', ''),
            'expected': entry.get('expected', '')
        }
        
        # Extract metrics from result dictionary
        if 'result' in entry:
            row.update({
                'exit_code': entry['result'].get('exit_code', ''),
                'runtime_solver': entry['result'].get('runtime_solver', []),
                'runtime_decision': entry['result'].get('runtime_decision', []),
                'number_of_variables': entry['result'].get('number_of_variables', []),
                'number_of_clauses': entry['result'].get('number_of_clauses', [])
            })
        
        # Create a single-row DataFrame and append to CSV
        df = pd.DataFrame([row])
        df.to_csv(csv_file, mode='a', header=not file_exists, index=False)
        file_exists = True  # Set to True after first write
        
    print(f"Results have been saved incrementally to {csv_file}")


def extract_benchmark_file_pairs(benchmark_xml_path):
    with open(benchmark_xml_path, 'r') as file:
        benchmark_xml = file.read()
    pairs = []
    
    # Extract all tasks with name, includesfile and propertyfile
    pattern = r'<tasks name="([^"]*)">\s*<includesfile>(.*?)</includesfile>\s*<propertyfile>(.*?)</propertyfile>\s*</tasks>'
    matches = re.findall(pattern, benchmark_xml, re.DOTALL)
    
    # Process each match
    for task_name, set_file, prop_file in matches:
        clean_set_file = set_file.split('sv-benchmarks')[-1][1:] if 'sv-benchmarks' in set_file else set_file
        clean_prop_file = prop_file.split('sv-benchmarks')[-1][1:] if 'sv-benchmarks' in prop_file else prop_file
        
        pairs.append({
            'task_name': task_name,
            'set_file': clean_set_file,
            'property_file': clean_prop_file
        })
    
    return pairs

def cleanup():
    commands = ["killall cbmc",
    "killall mpirun",
    "killall mallob",
    "killall mallob_sat_process"]   
    for command in commands:
        try:
            subprocess.run(command, shell=True, check=True)
        except subprocess.CalledProcessError as e:
            print(f"Error executing command: {command}. Error: {str(e)}")

def main(args):
    wrapper_path = args[0]
    set_file_path = args[1]
    global_property_file = args[2] 
    result_csv_file = args[3]
    batch_size = 1

    parsed_set_data = parse_set_file(set_file_path)
    for i in range(0, 100, batch_size):
        current_batch = parsed_set_data[i:min(i+batch_size, len(parsed_set_data))]
        print(f"Processing batch {i//batch_size + 1} of {(len(parsed_set_data) + batch_size - 1) // batch_size}...")
        results = run_wrapper(wrapper_path, current_batch, global_property_file)   
        dump_into_csv(results, result_csv_file)


if __name__ == "__main__":
    # # Example usage
    # yml_file = 'data_structures_set_multi_proc_ground-1.yml' 
    # parsed_data = parse_yml_file(yml_file)
    
    # # Print the parsed data
    # print("Parsed Data:")
    # for key, value in parsed_data.items():
    #     print(f"{key}: {value}")

    # Example usage of parse_set_file
    # set_file = '/home/oguz/Desktop/hiwi_code/benchmark/sv-benchmarks/c/MemSafety-Arrays.set'
    # parsed_set_data = parse_set_file(set_file, ignore_properties=True)
    # print("\nParsed Set Data:")
    # #for item in parsed_set_data[:2]:
    # #    print(item) 

    # # Process files in batches of 10
    # batch_size = 10
    # for i in range(0, 10, batch_size):
    #     current_batch = parsed_set_data[i:min(i+batch_size, len(parsed_set_data))]
    #     print(f"Processing batch {i//batch_size + 1} of {(len(parsed_set_data) + batch_size - 1) // batch_size}...")
    #     results = run_wrapper(current_batch, global_property_file="/home/oguz/Desktop/hiwi_code/benchmark/sv-benchmarks/c/properties/unreach-call.prp")
    #     for result in results:
    #         print(result)   
    #     dump_into_csv(results)
    BENCHMARK_DIR = '/home/oguz/Desktop/hiwi_code/benchmark/sv-benchmarks/'
    CBMC_WRAPPER = '/home/oguz/Desktop/hiwi_code/cbmc/cbmc-wrapper'
    MALLOB_WRAPPER = '/home/oguz/Desktop/hiwi_code/cbmc_mallob_monolithic/mallob/mallob-wrapper'
    FILESYSTEM_WRAPPER = '/home/oguz/Desktop/hiwi_code/cbmc_mallob_filesystem/cbmc/mallob-filesystem-wrapper'

    TOOLS = {"CBMC": CBMC_WRAPPER, "MALLOB": MALLOB_WRAPPER, "MALLOB-FILESYSTEM": FILESYSTEM_WRAPPER}

    pairs = (extract_benchmark_file_pairs('/home/oguz/Desktop/hiwi_code/benchmark/benchmark-defs/cbmc.xml'))
    #print(pairs)

    #pairs = [pair for pair in pairs if pair['task_name'] == 'SoftwareSystems-AWS-C-Common-ReachSafety']

    for pair in pairs[4:]:
        tool = "CBMC"
        set_file = BENCHMARK_DIR + pair['set_file']
        prop_file = BENCHMARK_DIR + pair['property_file']
        result_csv_file = tool + '_' + pair['task_name'] + '.csv'
        wrapper = TOOLS[tool]
        print(f"Running {tool} on {pair['task_name']} with set file {set_file} and property file {prop_file}")
        try:
            main([wrapper, set_file, prop_file, result_csv_file])
        except KeyboardInterrupt:
            print("Process interrupted by user.")
            cleanup()
            break   
        except Exception as e:
            print(f"Error running {tool} on {pair['task_name']}: {str(e)}")
            cleanup()
            continue


    # main(["/home/oguz/Desktop/hiwi_code/cbmc/cbmc-wrapper", 
    #       "/home/oguz/Desktop/hiwi_code/benchmark/sv-benchmarks/c/MemSafety-Arrays.set", 
    #       "/home/oguz/Desktop/hiwi_code/benchmark/sv-benchmarks/c/properties/unreach-call.prp", 
    #       "results_cbmc.csv"])
    


