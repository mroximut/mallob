import pandas as pd

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
compare_csv_files('results/CBMC_MemSafety-Other.csv', 'results/MALLOB_MemSafety-Other.csv')
