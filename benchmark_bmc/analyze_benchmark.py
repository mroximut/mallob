import pandas as pd
import os
import glob
import numpy as np
import matplotlib.pyplot as plt

def compare_csv_files(file1, file2):
    # Read CSV files
    df1 = pd.read_csv(file1)
    df2 = pd.read_csv(file2)

    # Make sure both dataframes have same rows ordered by input_file and property_file
    df1 = df1.sort_values(['input_file', 'property_file']).reset_index(drop=True)
    df2 = df2.sort_values(['input_file', 'property_file']).reset_index(drop=True)

    # Compare exit codes for each row
    for idx in range(len(df1)):
        val1 = df1.loc[idx, 'exit_code']
        val2 = df2.loc[idx, 'exit_code']

        if val1 != val2:
            print(f"\nDifference in row {idx} ({df1.loc[idx, 'input_file']}):")
            print(f"  exit_code: {val1} vs {val2}")

# Example usage:
# compare_csv_files('results/CBMC_MemSafety-Other.csv', 'results/MALLOB_MemSafety-Other.csv')

def compare_csv_files_with_same_suffix():
    total_cbmc_timeout_mallob_not = 0
    total_mallob_timeout_cbmc_not = 0
    total_both_timeout = 0

    # Get all CSV files in the results directory
    csv_files = glob.glob('mem-overflow-between10-30-timeout=900/*.csv')

    # Group files by their suffix (part after the underscore)
    file_groups = {}
    for file_path in csv_files:
        filename = os.path.basename(file_path)
        if '_' in filename:
            suffix = filename.split('_', 1)[1]
            if suffix not in file_groups:
                file_groups[suffix] = []
            file_groups[suffix].append(file_path)

    # Compare files with the same suffix
    for suffix, files in file_groups.items():
        if len(files) < 2:
            continue

        print(f"\n\nComparing files with suffix '{suffix}':")

        for i in range(len(files)):
            for j in range(i+1, len(files)):
                file1_name = os.path.basename(files[i]).split('_')[0]
                file2_name = os.path.basename(files[j]).split('_')[0]
                print(f"Comparing {file1_name} with {file2_name}")

                # Read CSV files
                df1 = pd.read_csv(files[i])
                df2 = pd.read_csv(files[j])

                difference = 0

                # Compare exit codes for each row
                for idx in range(min(len(df1), len(df2))):
                    val1 = df1.loc[idx, 'exit_code']
                    val2 = df2.loc[idx, 'exit_code']
                    lastline1 = df1.loc[idx, 'last_line']
                    lastline2 = df2.loc[idx, 'last_line']

                    both_invalid = (pd.isna(val1) or val1 in [42, 6, 137]) and (pd.isna(val2) or val2 in [42, 6, 137])

                    if (not both_invalid) and val1 != val2:
                    
                        difference += 1
                        print(f"\nDifference in row {idx} ({df1.loc[idx, 'input_file']}):")
                        print(f"  {file1_name} exit_code: {val1}, total_runtime: {df1.loc[idx, 'total_runtime']}")
                        print(f"  {file1_name} last_line: {lastline1}")
                       
                        print(f"  {file2_name} exit_code: {val2}, total_runtime: {df2.loc[idx, 'total_runtime']}")
                        print(f"  {file2_name} last_line: {lastline2}\n")

    #             # Count timeout differences
    #             cbmc_timeout_mallob_not = 0
    #             mallob_timeout_cbmc_not = 0
    #             both_timeout = 0

    #             for idx in range(min(len(df1), len(df2))):

    #                 val1_timeout = df1.loc[idx, 'exit_code'] == "TIMEOUT=200"
    #                 val2_timeout = df2.loc[idx, 'exit_code'] == "TIMEOUT=200"

    #                 if val1_timeout and not val2_timeout and file1_name.startswith("CBMC") and file2_name.startswith("MALLOB"):
    #                     cbmc_timeout_mallob_not += 1
    #                 elif val2_timeout and not val1_timeout and file1_name.startswith("CBMC") and file2_name.startswith("MALLOB"):
    #                     mallob_timeout_cbmc_not += 1
    #                 elif val1_timeout and not val2_timeout and file1_name.startswith("MALLOB") and file2_name.startswith("CBMC"):
    #                     mallob_timeout_cbmc_not += 1
    #                 elif val2_timeout and not val1_timeout and file1_name.startswith("MALLOB") and file2_name.startswith("CBMC"):
    #                     cbmc_timeout_mallob_not += 1

    #                 # Print the results
    #                 if val1_timeout != val2_timeout:
    #                     print(f"\nTimeout difference in row {idx} ({df1.loc[idx, 'input_file']}):")
    #                     print(f"  {file1_name} exit_code: {df1.loc[idx, 'exit_code']}, total_runtime: {df1.loc[idx, 'total_runtime']}")
    #                     print(f"  {file2_name} exit_code: {df2.loc[idx, 'exit_code']}, total_runtime: {df2.loc[idx, 'total_runtime']}")
    #                 if val1_timeout and val2_timeout:
    #                     both_timeout += 1

    #             print(f"CBMC timeout but MALLOB not: {cbmc_timeout_mallob_not}")
    #             print(f"MALLOB timeout but CBMC not: {mallob_timeout_cbmc_not}")
    #             print(f"Both timeout: {both_timeout}")
    #             total_cbmc_timeout_mallob_not += cbmc_timeout_mallob_not
    #             total_mallob_timeout_cbmc_not += mallob_timeout_cbmc_not
    #             total_both_timeout += both_timeout
    #             #print(f"Number of non-timeout differences:{difference - cbmc_timeout_mallob_not - mallob_timeout_cbmc_not}" )

    # print(f"Total CBMC timeout but MALLOB not: {total_cbmc_timeout_mallob_not}")
    # print(f"Total MALLOB timeout but CBMC not: {total_mallob_timeout_cbmc_not}")
    # print(f"Total both timeout: {total_both_timeout}")

def draw():
    """A graphic with runtime on x axis and number of solved instances with runtime less than x on y axis"""
    
    # Get all CSV files in the results directory
    csv_files = glob.glob('mem-overflow-between10-30-timeout=900/*.csv')
    
    # Group files by their suffix (benchmark category)
    categories = {}
    for file_path in csv_files:
        filename = os.path.basename(file_path)
        if '_' in filename:
            suffix = filename.split('_', 1)[1]
            if suffix not in categories:
                categories[suffix] = []
            categories[suffix].append(file_path)
    
    # Group files by their prefix (solver name)
    solvers = {}
    for file_path in csv_files:
        filename = os.path.basename(file_path)
        prefix = filename.split('_')[0]
        if prefix not in solvers:
            solvers[prefix] = []
        solvers[prefix].append(file_path)
    
    # Find common instances across all solvers
    instance_counts = {}
    for solver_name, file_paths in solvers.items():
        for file_path in file_paths:
            df = pd.read_csv(file_path)
            for _, row in df.iterrows():
                instance_key = f"{os.path.basename(file_path).split('_')[1]}_{row['input_file']}_{row['property_file']}"
                if instance_key not in instance_counts:
                    instance_counts[instance_key] = set()
                instance_counts[instance_key].add(solver_name)
    
    # Keep only instances that appear in all solvers
    common_instances = {k for k, v in instance_counts.items() if len(v) == len(solvers)}
    
    plt.figure(figsize=(10, 6))
    
    # Process each solver
    for solver_name, file_paths in solvers.items():
        # Collect all runtimes across files for this solver, but only for common instances
        all_runtimes = []
        
        for file_path in file_paths:
            df = pd.read_csv(file_path)
            category = os.path.basename(file_path).split('_', 1)[1]
            
            for _, row in df.iterrows():
                instance_key = f"{category}_{row['input_file']}_{row['property_file']}"
                
                # Only consider common instances and successful runs
                if instance_key in common_instances and row['exit_code'] in [0, 10]:
                    all_runtimes.append(row['total_runtime'])
        
        # Sort runtimes
        all_runtimes.sort()
        
        # Create x and y values for the plot
        x_values = np.linspace(0, 900, 100)  # From 0 to timeout (900s)
        y_values = [sum(1 for r in all_runtimes if r <= x) for x in x_values]
        
        # Plot the curve
        plt.plot(x_values, y_values, label=f"{solver_name} ({len(all_runtimes)} solved)")
    
    plt.xlabel('Runtime (s)')
    plt.ylabel('Number of solved instances')
    plt.title(f'Solver Performance: Instances Solved vs. Runtime ({len(common_instances)} common instances)')
    plt.legend()
    plt.grid(True)
    plt.savefig('solver_performance_common_instances.png')


def speedup_values(first_tool, second_tool, cutoff=30):
    """For each instance, calculate the speedup of Mallob over CBMC"""
    
    # Get all CSV files in the results directory
    csv_files = glob.glob('mem-overflow-between10-30-timeout=900/*.csv')
    
    # Group files by their suffix (benchmark category)
    file_groups = {}
    for file_path in csv_files:
        filename = os.path.basename(file_path)
        if '_' in filename:
            suffix = filename.split('_', 1)[1]
            if suffix not in file_groups:
                file_groups[suffix] = []
            file_groups[suffix].append(file_path)
    
    speedups = []
    both_solved = 0
    
    # For each benchmark category
    for suffix, files in file_groups.items():
        # Find CBMC and Mallob files
        cbmc_file = None
        mallob_file = None
        for file in files:
            if first_tool + '_' in file:
                cbmc_file = file
            elif second_tool + '_' in file:
                mallob_file = file
        
        if not cbmc_file or not mallob_file:
            continue
        
        # Read CSV files
        df_cbmc = pd.read_csv(cbmc_file)
        df_mallob = pd.read_csv(mallob_file)
        
        # Calculate speedup for each instance
        for idx in range(min(len(df_cbmc), len(df_mallob))):
            cbmc_time = df_cbmc.loc[idx, 'total_runtime']
            mallob_time = df_mallob.loc[idx, 'total_runtime']
            cbmc_exit = df_cbmc.loc[idx, 'exit_code']
            mallob_exit = df_mallob.loc[idx, 'exit_code']
            
            # Check if both solvers completed successfully
            cbmc_success = cbmc_exit in [0, 10]
            mallob_success = mallob_exit in [0, 10]
            
            if cbmc_success and mallob_success:
                solver_times_str = df_cbmc.loc[idx, 'runtime_solver'][1:-1].split(',')
                solver_times = list(map(float, solver_times_str)) if len(solver_times_str[0]) > 0 else []
                solver_time = sum(solver_times)

                speedup = cbmc_time / mallob_time if cbmc_time > cutoff else None
                if speedup:
                    print (f"Mallob time: {mallob_time:.2f}s, CBMC time: {cbmc_time:.2f}s")
                    print (f"Speedup for {df_cbmc.loc[idx, 'input_file']} ({df_cbmc.loc[idx, 'property_file']}): {speedup:.2f}x")
                    speedups.append(speedup)
                    if solver_time > 0:
                        solver_perc = solver_time / cbmc_time
                        print (f"{round(solver_perc, 3)} dominated by solver")
                    print("\n")
                else:
                    both_solved += 1
    
    print(len(speedups))
    print(f"Geometric mean of speedups: {np.exp(np.mean(np.log(speedups))):.2f}x")
    print(f"Total instances solved by both under {cutoff}s : {both_solved}")



#compare_csv_files_with_same_suffix()
speedup_values('CBMC', 'MALLOB', cutoff=20)
#draw()
