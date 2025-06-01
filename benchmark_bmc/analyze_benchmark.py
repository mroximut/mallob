import pandas as pd
import os
import glob

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
    csv_files = glob.glob('mem-overflow-between10-30-timeout=200/*.csv')

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

                    if val1 != val2:
                        difference += 1
                        print(f"\nDifference in row {idx} ({df1.loc[idx, 'input_file']}):")
                        print(f"  {file1_name} exit_code: {val1}, total_runtime: {df1.loc[idx, 'total_runtime']}")
                        print(f"  {file2_name} exit_code: {val2}, total_runtime: {df2.loc[idx, 'total_runtime']}\n")

                # Count timeout differences
                cbmc_timeout_mallob_not = 0
                mallob_timeout_cbmc_not = 0
                both_timeout = 0

                for idx in range(min(len(df1), len(df2))):

                    val1_timeout = df1.loc[idx, 'exit_code'] == "TIMEOUT=200"
                    val2_timeout = df2.loc[idx, 'exit_code'] == "TIMEOUT=200"

                    if val1_timeout and not val2_timeout and file1_name.startswith("CBMC") and file2_name.startswith("MALLOB"):
                        cbmc_timeout_mallob_not += 1
                    elif val2_timeout and not val1_timeout and file1_name.startswith("CBMC") and file2_name.startswith("MALLOB"):
                        mallob_timeout_cbmc_not += 1
                    elif val1_timeout and not val2_timeout and file1_name.startswith("MALLOB") and file2_name.startswith("CBMC"):
                        mallob_timeout_cbmc_not += 1
                    elif val2_timeout and not val1_timeout and file1_name.startswith("MALLOB") and file2_name.startswith("CBMC"):
                        cbmc_timeout_mallob_not += 1

                    # Print the results
                    if val1_timeout != val2_timeout:
                        print(f"\nTimeout difference in row {idx} ({df1.loc[idx, 'input_file']}):")
                        print(f"  {file1_name} exit_code: {df1.loc[idx, 'exit_code']}, total_runtime: {df1.loc[idx, 'total_runtime']}")
                        print(f"  {file2_name} exit_code: {df2.loc[idx, 'exit_code']}, total_runtime: {df2.loc[idx, 'total_runtime']}")
                    if val1_timeout and val2_timeout:
                        both_timeout += 1

                print(f"CBMC timeout but MALLOB not: {cbmc_timeout_mallob_not}")
                print(f"MALLOB timeout but CBMC not: {mallob_timeout_cbmc_not}")
                print(f"Both timeout: {both_timeout}")
                total_cbmc_timeout_mallob_not += cbmc_timeout_mallob_not
                total_mallob_timeout_cbmc_not += mallob_timeout_cbmc_not
                total_both_timeout += both_timeout
                #print(f"Number of non-timeout differences:{difference - cbmc_timeout_mallob_not - mallob_timeout_cbmc_not}" )

    print(f"Total CBMC timeout but MALLOB not: {total_cbmc_timeout_mallob_not}")
    print(f"Total MALLOB timeout but CBMC not: {total_mallob_timeout_cbmc_not}")
    print(f"Total both timeout: {total_both_timeout}")


compare_csv_files_with_same_suffix()
