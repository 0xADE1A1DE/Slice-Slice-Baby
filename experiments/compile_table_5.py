import os
import re
import numpy as np
import pandas as pd
from collections import defaultdict
import matplotlib.pyplot as plt

pname_lookup = {
"11700k": "i7-11700KF & 8",
"13700h": "i7-13700H & 10",
"6700k": "i7-6700K & 4",
"10710u": "i7-10710U & 6",
"9850h": "i7-9850H & 6",
"8700": "i7-8700 & 6",
"10900k": "i9-10900K & 10",
"12900kf": "i9-12900KF & 10",
"13900kf": "i9-13900KF & 12",
"14900k": "i9-14900K & 12"
}

wildcard_re = r""

def compute_stats(data):
    Q1 = np.percentile(data, 25)
    Q3 = np.percentile(data, 75)
    IQR = Q3 - Q1
    
    return {
        'mean': np.mean(data),
        'median': np.median(data),
        'std': np.std(data),
        'min': np.min(data),
        'max': np.max(data),
        'IQR': IQR,
    }

def compute_stats2(data, filter_zero):
    # Helper function to compute statistics
    def compute_statistics(values):
        if values:
            return {
                'mean': np.mean(values),
                'median': np.median(values),
                'std': np.std(values),
                'min': np.min(values),
                'max': np.max(values),
            }
        else:
            return {
                'mean': None,
                'median': None,
                'std': None,
                'min': None,
                'max': None,
            }
    
    if filter_zero:
        # Filter data to include only those where item[0] == '0'
        values = [float(item[1]) * 1000 for item in data if int(item[0]) == 0]
        return compute_statistics(values)
    else:
        stats = {
                'mean': 0.0,
                'median': 0.0,
                'std': 0.0,
                'min': 0.0,
                'max': 0.0,
            }
        values = []
        for s in range(1000):
            values.append(0.0)
            for p in range(64):
                values[-1] += float(data[s * 64 + p][1]) * 1000
        stats = compute_statistics(values)
        return stats

# Gets total found, total duplicate and the number of expected eviction sets (i.e. total) from the data.
def compute_stats3(data, filter_zero, idx):
    # Helper function to compute statistics
    def compute_statistics(values):
        if values:
            return {
                'mean': np.mean(values),
                'median': np.median(values),
                'std': np.std(values),
                'min': np.min(values),
                'max': np.max(values),
            }
        else:
            return {
                'mean': None,
                'median': None,
                'std': None,
                'min': None,
                'max': None,
            }
    
    if filter_zero:
        # Filter data to include only those where item[0] == '0'
        values = [float(item[idx]) for item in data]
        return compute_statistics(values)
    else:
        stats = {
                'mean': 0.0,
                'median': 0.0,
                'std': 0.0,
                'min': 0.0,
                'max': 0.0,
            }
        values = []
        for s in range(1000):
            values.append(0.0)
            for p in range(64):
                values[-1] += float(data[s * 64 + p][idx])
        stats = compute_statistics(values)
        return stats

# Found: 126
# Duplicate: 1
# Total: 192
# Errors: 0

def parse_confusion_matrix(data):
    matrices = []
    addresses_per_second = []

    lines = data.splitlines()
    idx = 0

    while idx < len(lines):
        # Parse confusion matrix
        if lines[idx].startswith("Bin"):
            matrix = []
            while idx < len(lines) and lines[idx].startswith("Bin"):
                temp_parts = lines[idx].split("%)")[1].strip()
                parts = []
                for i in temp_parts.split():
                    if "%" not in i:
                        parts.append(int(i.replace(',', '')))
                matrix.append(parts)
                idx += 1
            if matrix:
                matrices.append(pd.DataFrame(matrix))
        
        # Parse addresses per second
        if "evsp_run(): Addresses / second: " in lines[idx]:
            address_line = lines[idx]
            addresses = float(address_line.split("Addresses / second: ")[1])
            addresses_per_second.append(addresses)

        idx += 1

    # Calculate average and standard deviation
    if addresses_per_second:
        avg_addresses = np.mean(addresses_per_second)
        stddev_addresses = np.std(addresses_per_second)
    else:
        avg_addresses = stddev_addresses = 0.0

    avg_mbs = (avg_addresses * 64) / (1024 * 1024)
    stddev_mbs = (stddev_addresses * 64) / (1024 * 1024)

    return matrices, avg_mbs, stddev_mbs

def normalise_to_percentage_per_group(dfs):
    normalised_dfs = []
    
    for df in dfs:
        # Calculate row sums
        row_sums = df.sum(axis=1)
        
        # Normalize while avoiding division by zero
        normalised_df = df.div(row_sums, axis=0).fillna(0) * 100
        
        # Append the normalized DataFrame
        normalised_dfs.append(normalised_df)
    
    return normalised_dfs

def calculate_avg_stddev(confusion_matrices):
    # Stack the DataFrames to calculate element-wise sums
    total_sum = sum(confusion_matrices)
    
    # Calculate the average
    avg = total_sum / len(confusion_matrices)
    
    # Calculate the standard deviation
    variance = sum((cm - avg) ** 2 for cm in confusion_matrices) / len(confusion_matrices)
    stddev = variance ** 0.5
    return avg, stddev

# def plot_slice_results_with_trendlines(overall_slice_results):
#     plt.figure(figsize=(10, 6))
    
#     for i, (processor_name, slice_results) in enumerate(overall_slice_results):
#         # Sort the slice results
#         slice_results.sort()
        
#         # Extract x and y coordinates
#         x, y = zip(*slice_results)

#         # Normalize x values to range [0, 1]
#         x_min, x_max = min(x), max(x)
#         x_normalized = [(value - x_min) / (x_max - x_min) for value in x]

#         # Fit a linear trendline
#         coefficients = np.polyfit(x_normalized, y, 1)  # 1 for linear fit
#         trendline = np.polyval(coefficients, x_normalized)

#         # Plot the scatter points
#         plt.scatter(x_normalized, y, alpha=0.7, label=f'{processor_name}')

#         # Plot the trendline
#         plt.plot(x_normalized, trendline, linestyle='--', label=f'Trendline {processor_name}')

#     # Add titles and labels
#     plt.title('Slice Results with Trendlines')
#     plt.xlabel('Eviction Set Generation Time (Normalized)')
#     plt.ylabel('Slice Classification True Positive Rate')
#     plt.legend()
#     plt.grid(True)

#     # Show the plot
#     plt.show()

def plot_slice_results_per_processor(overall_slice_results):
    # Determine the number of processors and create subplots
    num_processors = len(overall_slice_results)
    fig, axes = plt.subplots(1, num_processors, figsize=(5 * num_processors, 10), sharey=True)

    for i, (processor_name, slice_results) in enumerate(overall_slice_results):
        # Sort the slice results
        slice_results.sort()
        
        # Extract x and y coordinates
        x, y = zip(*slice_results)

        # Z-score normalization for x values (eviction set generation times)
        x_mean = np.mean(x)
        x_std = np.std(x)
        x_zscore = [(value - x_mean) / x_std for value in x]

        # Z-score normalization for y values (slice classification true positive rates)
        y_mean = np.mean(y)
        y_std = np.std(y)
        y_zscore = [(value - y_mean) / y_std for value in y]

        # Fit a linear trendline
        coefficients = np.polyfit(x_zscore, y, 1)  # 1 for linear fit
        trendline = np.polyval(coefficients, x_zscore)

        # Plot the scatter points on the corresponding axis
        axes[i].scatter(x_zscore, y, alpha=0.7, label='Data Points')
        axes[i].plot(x_zscore, trendline, linestyle='--', color='red', label='Trendline')

        # Set titles and labels for each subplot
        axes[i].set_title(f'Slice Results for {processor_name}')
        axes[i].set_xlabel('Z-Score of Eviction Set Generation Time')
        axes[i].set_ylabel('Slice Classification True Positive Rate')
        axes[i].legend()
        axes[i].grid(True)

    # Adjust layout
    plt.tight_layout()
    plt.show()

def get_slice_partitioning_results(filename, build_times):
    with open(filename, 'r') as file:
        data = file.read()
    matrices, speed_avg, speed_stddev = parse_confusion_matrix(data)
    matrices = normalise_to_percentage_per_group(matrices)

    slice_averages = []
    for df in matrices:
        true_positive_value = df.iloc[1, 1]
        slice_averages.append(true_positive_value)

    slice_results = []
    for a in range(0, len(slice_averages)):
        slice_results.append((build_times[a], slice_averages[a]))
    slice_results.sort()

    return slice_results


def parse_log_file(filename):
    with open(filename, 'r') as file:
        lines = file.readlines()

    setup_time_re = re.compile(r"evsets_create\(LLC\): Setup took: (\d+\.\d+)ms")
    build_time_re = re.compile(r"evsets_create\(LLC\): Build took: (\d+\.\d+)ms")
    total_time_re = re.compile(r"evsets_create\(LLC\): Total time: (\d+\.\d+)ms")
    test_total_re = re.compile(wildcard_re + r"Total\s+:\s+(\d+) /\s+(\d+) \[([\d\.]+)%\]" + wildcard_re)
    test_successful_re = re.compile(wildcard_re + r"Successful\s+:\s+(\d+) /\s+(\d+) \[([\d\.]+)%\]" + wildcard_re)
    test_duplicates_re = re.compile(wildcard_re + r"Duplicates\s+:\s+(\d+) /\s+(\d+) \[([\d\.]+)%\]" + wildcard_re)
    test_missing_re = re.compile(wildcard_re + r"Missing\s+:\s+(\d+) /\s+(\d+) \[([\d\.]+)%\]" + wildcard_re)

    setup_times = []
    build_times = []
    total_times = []

    total_summary = {'mean': [], 'median': [], 'stddev': []}
    successful_summary = {'mean': [], 'median': [], 'stddev': []}
    duplicates_summary = {'mean': [], 'median': [], 'stddev': []}
    missing_summary = {'mean': [], 'median': [], 'stddev': []}

    for line in lines:
        setup_match = setup_time_re.search(line)
        build_match = build_time_re.search(line)
        total_time_match = total_time_re.search(line)
        total_match = test_total_re.search(line)
        successful_match = test_successful_re.search(line)
        duplicates_match = test_duplicates_re.search(line)
        missing_match = test_missing_re.search(line)

        if setup_match:
            setup_times.append(float(setup_match.group(1)))
        if build_match:
            build_times.append(float(build_match.group(1)))
        if total_time_match:
            total_times.append(float(total_time_match.group(1)))
        if total_match:
            total_summary['mean'].append(float(total_match.group(3)))
        if successful_match:
            successful_summary['mean'].append(float(successful_match.group(3)))
        if duplicates_match:
            duplicates_summary['mean'].append(float(duplicates_match.group(3)))
        if missing_match:
            missing_summary['mean'].append(float(missing_match.group(3)))

    setup_stats = compute_stats(setup_times)
    build_stats = compute_stats(build_times)
    total_time_stats = compute_stats(total_times)
    total_stats = compute_stats(total_summary['mean'])
    successful_stats = compute_stats(successful_summary['mean'])
    duplicates_stats = compute_stats(duplicates_summary['mean'])
    missing_stats = compute_stats(missing_summary['mean'])

    slice_results = None
    if "no_slice" not in os.path.basename(filename):
        slice_results = get_slice_partitioning_results(filename, build_times)

    return setup_stats, build_stats, total_time_stats, total_stats, successful_stats, duplicates_stats, missing_stats, slice_results

def process_all_logs_in_dir(d):
    data = {}

    overall_slice_results = []

    for root, dirs, files in os.walk(d):
        for file in files:
            if file.endswith('.txt'):
                file_path = os.path.join(root, file)
                processor_name = os.path.basename(file).split("config_")[1].split(".txt")[0]
                if "no_slice" in os.path.basename(file):
                    processor_name = processor_name + "_l2_only"
                setup_stats, build_stats, total_time_stats, total_stats, successful_stats, duplicates_stats, missing_stats, slice_results = parse_log_file(file_path)
                if(slice_results != None):
                    overall_slice_results.append((pname_lookup[processor_name.split('_l2_only')[0]].split(" &")[0], slice_results))
                data[processor_name] = {
                    'setup_mean': setup_stats['mean'],
                    'setup_median': setup_stats['median'],
                    'setup_std': setup_stats['std'],
                    'setup_min': setup_stats['min'],
                    'setup_max': setup_stats['max'],
                    'build_mean': build_stats['mean'],
                    'build_median': build_stats['median'],
                    'build_std': build_stats['std'],
                    'build_min': build_stats['min'],
                    'build_max': build_stats['max'],
                    'total_time_mean': total_time_stats['mean'],
                    'total_time_median': total_time_stats['median'],
                    'total_time_std': total_time_stats['std'],
                    'total_time_min': total_time_stats['min'],
                    'total_time_max': total_time_stats['max'],
                    'total_time_IQR': total_time_stats['IQR'],
                    'total_mean': total_stats['mean'],
                    'total_median': total_stats['median'],
                    'total_stddev': total_stats['std'],
                    'successful_mean': successful_stats['mean'],
                    'successful_median': successful_stats['median'],
                    'successful_stddev': successful_stats['std'],
                    'duplicates_mean': duplicates_stats['mean'],
                    'duplicates_median': duplicates_stats['median'],
                    'duplicates_stddev': duplicates_stats['std'],
                    'missing_mean': missing_stats['mean'],
                    'missing_median': missing_stats['median'],
                    'missing_stddev': missing_stats['std'],
                }
                print(processor_name, data[processor_name]['total_time_mean'], data[processor_name]['total_time_median'], data[processor_name]['total_time_std'], data[processor_name]['total_time_min'], data[processor_name]['total_time_max'], data[processor_name]['total_time_IQR'])
    return data, overall_slice_results

def parse_plumtree_logs_in_dir(d):
    data = {}

    for root, dirs, files in os.walk(d):
        for file in files:
            temp_data = []
            err_data = []
            if file.endswith('.txt'):
                print(file)
                with open(d+'/'+file, 'r') as f:
                    lines = f.readlines()

                offset_re = re.compile(r"Page Offset: \d")
                total_time_re = re.compile(r"mistakes and the mapping took ")

                found_re = re.compile(r"Found: \d")
                duplicate_re = re.compile(r"Duplicate: \d")
                total_re = re.compile(r"Total: \d")

                current = [-1,-1]
                err_current = [0,0,0]

                for line in lines:
                    offset_match = offset_re.search(line)
                    total_time_match = total_time_re.search(line)

                    found_match = found_re.search(line)
                    duplicate_match = duplicate_re.search(line)
                    total_match = total_re.search(line)

                    if offset_match:
                        current[0] = line.strip().split("Page Offset: ")[1]
                    if total_time_match:
                        current[1] = line.strip().split("mistakes and the mapping took ")[1].split(" seconds (")[0]
                        temp_data.append(current)
                        current = [-1,-1]
                    if(found_match):
                        err_current[0] = line.strip().split("Found: ")[1]
                    if(duplicate_match):
                        err_current[1] = line.strip().split("Duplicate: ")[1]
                    if(total_match):
                        err_current[2] = line.strip().split("Total: ")[1]
                        err_data.append(err_current)
                        err_current = [0,0,0]

            processor_name = os.path.basename(file).split("data_main_")[1].split(".txt")[0]
            processor_name = processor_name + "_plumtree"

            po_total_time_stats = None
            fs_total_time_stats = None

            po_total_time_stats = compute_stats2(temp_data, True)
            po_total_found_stats = compute_stats3(err_data, True, 0)
            po_total_duplicate_stats = compute_stats3(err_data, True, 1)
            po_total_total_stats = compute_stats3(err_data, True, 2)

            print("po_total_found_stats", po_total_found_stats)
            print("po_total_duplicate_stats", po_total_duplicate_stats)
            print("po_total_total_stats", po_total_total_stats)

            if "6700" in processor_name or "11700" in processor_name:
                fs_total_time_stats = compute_stats2(temp_data, True)

                fs_total_found_stats = compute_stats3(err_data, True, 0)
                fs_total_duplicate_stats = compute_stats3(err_data, True, 1)
                fs_total_total_stats = compute_stats3(err_data, True, 2)

                print("fs_total_found_stats", fs_total_found_stats)
                print("fs_total_duplicate_stats", fs_total_duplicate_stats)
                print("fs_total_total_stats", fs_total_total_stats, "\n")
            else:
                fs_total_time_stats = compute_stats2(temp_data, False)

                fs_total_found_stats = compute_stats3(err_data, False, 0)
                fs_total_duplicate_stats = compute_stats3(err_data, False, 1)
                fs_total_total_stats = compute_stats3(err_data, False, 2)

                print("fs_total_found_stats", fs_total_found_stats)
                print("fs_total_duplicate_stats", fs_total_duplicate_stats)
                print("fs_total_total_stats", fs_total_total_stats, "\n")

            data[processor_name] = {
                'po_total_time_mean': po_total_time_stats['mean'],
                'po_total_time_median': po_total_time_stats['median'],
                'po_total_time_std': po_total_time_stats['std'],
                'fs_total_time_mean': fs_total_time_stats['mean'],
                'fs_total_time_median': fs_total_time_stats['median'],
                'fs_total_time_std': fs_total_time_stats['std'],
            }
    return data

if __name__ == "__main__":

    directories = ["./llc_evsets_po/current", "./llc_evsets_fs/current"]
    datas = []
    overall_slice_results = []
    for d in directories:
        data, temp_slice_results = process_all_logs_in_dir(d)
        datas.append(data)
        overall_slice_results.append(temp_slice_results)

    plumtree_data = parse_plumtree_logs_in_dir("./prune_plumtree_mod/data");
   
    for data in datas:
        for processor, stats in data.items():
            print(f"Processor: {processor}")
            print(f"Total Evsets: Mean={stats['total_mean']:8.1f}%, Median={stats['total_median']:8.1f}%, Stddev={stats['total_stddev']:8.1f}%")
            print(f"Successful Evsets: Mean={stats['successful_mean']:8.1f}%, Median={stats['successful_median']:8.1f}%, Stddev={stats['successful_stddev']:8.1f}%")
            print(f"Duplicates Evsets: Mean={stats['duplicates_mean']:8.1f}%, Median={stats['duplicates_median']:8.1f}%, Stddev={stats['duplicates_stddev']:8.1f}%")
            print(f"Missing Evsets: Mean={stats['missing_mean']:8.1f}%, Median={stats['missing_median']:8.1f}%, Stddev={stats['missing_stddev']:8.1f}%")
            
            if "l2_only" not in processor:
                for processor2, stats2 in data.items():
                    if processor in processor2 and processor != processor2:
                        print((stats2['setup_mean'] + stats2['build_mean']) / (stats['setup_mean'] + stats['build_mean']))
            print()

    # for r in overall_slice_results:
    #     plot_slice_results_per_processor(r)

    latex_table = ""
    # Initialize processor_row as a dictionary
    processor_row = {}

    # Combine data from datasets

    idx = 0
    for data in datas:

        for processor, stats in data.items():
                if '_l2_only' in processor:
                    if processor.split('_l2_only')[0] not in processor_row:
                        processor_row[processor.split('_l2_only')[0]] = f"{pname_lookup[processor.split('_l2_only')[0]].ljust(20)} & ${stats['total_time_mean']:8.0f} $ & $ {stats['total_time_std']:8.0f}$"
                    else:
                        processor_row[processor.split('_l2_only')[0]] += f" & ${stats['total_time_mean']:8.0f} $ & $ {stats['total_time_std']:8.0f}$"

        for processor, stats in plumtree_data.items():
            if '_plumtree' in processor:
                if processor.split('_plumtree')[0] not in processor_row:
                    if idx == 0:
                        processor_row[processor.split('_plumtree')[0]] = f"{processor.split('_plumtree')[0].ljust(20)} & ${stats['po_total_time_mean']:8.0f} $ & $ {stats['po_total_time_std']:8.0f}$"
                    else:
                        processor_row[processor.split('_plumtree')[0]] = f"{processor.split('_plumtree')[0].ljust(20)} & ${stats['fs_total_time_mean']:8.0f} $ & $ {stats['fs_total_time_std']:8.0f}$"
                else:
                    if idx == 0:
                        processor_row[processor.split('_plumtree')[0]] += f" & ${stats['po_total_time_mean']:8.0f} $ & $ {stats['po_total_time_std']:8.0f}$"
                    else:
                        processor_row[processor.split('_plumtree')[0]] += f" & ${stats['fs_total_time_mean']:8.0f} $ & $ {stats['fs_total_time_std']:8.0f}$"
        idx += 1

        for processor, stats in data.items():
                if '_l2_only' not in processor:
                    processor_row[processor] += f" & ${stats['total_time_mean']:8.0f} $ & $ {stats['total_time_std']:8.0f}$"
        
    # Build the final LaTeX table from the dictionary
    for row in processor_row.values():
        latex_table += row + "   \\\\\n"

    print(latex_table)
    
