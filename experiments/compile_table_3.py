import pandas as pd
import seaborn as sns
import matplotlib.pyplot as plt
from mpl_toolkits.axes_grid1 import make_axes_locatable
import numpy as np
import os
import sys

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

def calculate_avg_stddev(confusion_matrices):
    # Stack the DataFrames to calculate element-wise sums
    total_sum = sum(confusion_matrices)
    
    # Calculate the average
    avg = total_sum / len(confusion_matrices)
    
    # Calculate the standard deviation
    variance = sum((cm - avg) ** 2 for cm in confusion_matrices) / len(confusion_matrices)
    stddev = variance ** 0.5
    return avg, stddev

def normalise_to_percentage_per_group(dfs):
    normalised_dfs = []
    
    for df in dfs:
        # Calculate row sums
        row_sums = df.sum(axis=1)
        
        # Normalise while avoiding division by zero
        normalised_df = df.div(row_sums, axis=0).fillna(0) * 100
        
        # Append the normalised DataFrame
        normalised_dfs.append(normalised_df)
    
    return normalised_dfs

def plot_processor_performance(data):
    # Prepare data for plotting
    experiments = ['COMPARATOR', 'CLOSEST_MATCH', 'BAYESIAN_INFERENCE', 'DECISION_TREE']
    processors = set()
    processor_data = {exp: {} for exp in experiments}

    # Collecting data
    for exp_index, experiment in enumerate(data):
        for entry in experiment:
            processor_name = entry[0]
            processors.add(processor_name)
            avg_accuracy = entry[2]
            stddev_accuracy = entry[3]
            avg_speed = entry[4]
            stddev_speed = entry[5]
            
            # Store data in a structured way
            if processor_name not in processor_data[experiments[exp_index]]:
                processor_data[experiments[exp_index]][processor_name] = {
                    'accuracy': avg_accuracy,
                    'stddev_accuracy': stddev_accuracy,
                    'speed': avg_speed,
                    'stddev_speed': stddev_speed
                }

    # Convert processors set to sorted list for consistent ordering
    processors = sorted(processors)

    # Set up the scatter plot
    plt.figure(figsize=(10, 6))

    # Plot each experiment
    for exp in experiments:
        x = []
        y = []
        x_err = []
        y_err = []
        
        for processor in processors:
            if processor in processor_data[exp]:
                x.append(processor_data[exp][processor]['accuracy'])
                y.append(processor_data[exp][processor]['speed'])
                x_err.append(processor_data[exp][processor]['stddev_accuracy'])
                y_err.append(processor_data[exp][processor]['stddev_speed'])
        
        # Create scatter plot with error bars
        plt.errorbar(x, y, xerr=x_err, yerr=y_err, 
                     fmt='o', label=exp, alpha=0.7, capsize=5)

    # Labeling
    plt.title('Processor Performance: Accuracy vs Speed with Stddev')
    plt.xlabel('Average True Positive Accuracy')
    plt.ylabel('Average Speed')
    plt.legend()
    plt.grid(True)
    
    # Show plot
    plt.tight_layout()
    plt.show()

def generate_latex_table(data):
    print()
    # Define the headers
    headers = ['Processor Name', 'Accuracy', 'Speed', 'Accuracy', 'Speed', 'Accuracy', 'Speed', 'Accuracy', 'Speed']
    # Start the LaTeX table
    latex_table = r'\begin{table}[htbp]' + '\n'
    latex_table += r'\centering' + '\n'
    latex_table += r'\begin{tabular}{|l|' + 'c|' * 8 + '}' + '\n'
    latex_table += r'\hline' + '\n'
    latex_table += ' & '.join(headers) + r' \\ \hline' + '\n'
    
    # Collecting and formatting the data for the table
    processor_groups = {}

    for exp_index, experiment in enumerate(data):
        for entry in experiment:
            processor_name = entry[0]
            experiment_name = entry[1].split('_')[2]  # Extracting experiment name
            avg_accuracy = entry[2]
            stddev_accuracy = entry[3]
            avg_speed = entry[4]
            stddev_speed = entry[5]
            
            # Group data by processor
            if processor_name not in processor_groups:
                processor_groups[processor_name] = []
            processor_groups[processor_name].append((experiment_name, avg_accuracy, stddev_accuracy, avg_speed, stddev_speed))

    # Generating the table rows, with experiments horizontal
    for processor_name, results in processor_groups.items():
        row = [processor_name]  # Start with the processor name
        for experiment in results:
            experiment_name, avg_accuracy, stddev_accuracy, avg_speed, stddev_speed = experiment
            row.append(f'{avg_accuracy:3d} & {stddev_accuracy:3d}')
            row.append(f'{avg_speed:3d} & {stddev_speed:3d}')
        # Fill in the row with empty cells if less than 4 experiments
        while len(row) < len(headers):
            row.append('')
        latex_table += ' & '.join(row) + r' \\' + '\n'

    latex_table += r'\end{tabular}' + '\n'
    latex_table += r'\caption{Processor Performance Results}' + '\n'
    latex_table += r'\label{tab:processor_performance}' + '\n'
    latex_table += r'\end{table}' + '\n'
    
    print(latex_table)

    return latex_table


def average_per_core_speeds(per_core_speeds):
    if not per_core_speeds:
        return []  # Return empty if there's nothing to average

    total_avg = 0.0
    total_stddev = 0.0
    count = len(per_core_speeds)

    for speed_avg, speed_stddev in per_core_speeds:
        total_avg += speed_avg
        total_stddev += speed_stddev

    # Calculate the averages
    avg_speed_avg = total_avg / count
    avg_speed_stddev = total_stddev / count

    return [round(avg_speed_avg), round(avg_speed_stddev)]

pname_lookup = {
"11700k" : "i7-11700KF ",
"13700h" : "i7-13700H  ",
"6700k"  : "i7-6700K   ",
"10710u" : "i7-10710U  ",
"9850h"  : "i7-9850H   ",
"8700"   : "i7-8700    ",
"10900k" : "i9-10900K  ",
"12900k": "i9-12900KF ",
"13900kf": "i9-13900KF ",
"14900k" : "i9-14900K  "
}

def process_predictions(processor_data):
    data = []
    # Iterate over each processor's data
    for p in processor_data:
        processor_name = p[0]

        experiment = p[3]

        per_core_matrices = p[1]
        per_core_speeds = p[2][0]

        per_core_avg_matrices = []
        per_core_stddev_matrices = []

        # Collate per-core matrices and speeds
        for core_matrices in per_core_matrices:
            per_core_avg_matrices.append(core_matrices[0][0])
            per_core_stddev_matrices.append(core_matrices[0][1])

        avgavgavg = 0
        avgavgstd = 0
        for s in range(0, len(per_core_avg_matrices)):
            print(f"{s} ", end="")
            avgavg = 0
            avgstd = 0
            for i in range(0, len(per_core_matrices)):
                avgavg += per_core_avg_matrices[s][i][i]
                avgstd += per_core_stddev_matrices[s][i][i]
                print(f"& ${per_core_avg_matrices[s][i][i]:8.0f} $ & $ {per_core_stddev_matrices[s][i][i]:2.0f}$ ", end="")
            print(f"& ${avgavg / float(len(per_core_matrices)):8.0f} $ & $ {avgstd / float(len(per_core_matrices)):2.0f}$ ", end="\\\\\n")
            avgavgavg += avgavg / float(len(per_core_matrices))
            avgavgstd += avgstd / float(len(per_core_matrices))


        accuracy_avg = round(avgavgavg / float(len(per_core_matrices)))
        accuracy_std_avg = round(avgavgstd / float(len(per_core_matrices)))

        speed_avg, speed_stddev = average_per_core_speeds(per_core_speeds)

        d = (processor_name, experiment, accuracy_avg, accuracy_std_avg, speed_avg, speed_stddev)
        #print(d)
        data.append(d)
    return data

def main():
    directories = [
        "./per_core_llc_COMPARATOR_gate_predictions/current", 
        "./per_core_llc_closest_match_predictions/current", 
        "./per_core_llc_bayesian_inference_predictions/current", 
        "./per_core_llc_decision_tree_predictions/current"
    ]
    
    experiment_data = []

    for d in directories:
        processor_data = {}
        experiment = d.split("/")[1]
        print("Processing experiment", experiment)
        # Read all data files in the folder
        for filename in sorted(os.listdir(d)):
            if filename.startswith("data_") and filename.endswith('.txt'):
                per_core_matrices = []
                per_core_speeds = []
                processor_name = pname_lookup[filename.split("_")[2]]
                print("Processing ", processor_name, "Core", filename.split("_")[3].split(".txt")[0])
                
                file_path = os.path.join(d, filename)
                with open(file_path, 'r') as file:
                    data = file.read()
                
                matrices, speed_avg, speed_stddev = parse_confusion_matrix(data)
                matrices = normalise_to_percentage_per_group(matrices)
                matrices = calculate_avg_stddev(matrices)
                
                per_core_matrices.append(matrices)
                per_core_speeds.append([speed_avg, speed_stddev])
                
                # Collate data in the dictionary
                if processor_name not in processor_data:
                    processor_data[processor_name] = {
                        'matrices': [],
                        'speeds': []
                    }

                processor_data[processor_name]['matrices'].append(per_core_matrices)
                processor_data[processor_name]['speeds'].append(per_core_speeds)

        # Convert the collated data to the desired format
        formatted_processor_data = [
            (processor_name, data['matrices'], data['speeds'], experiment) 
            for processor_name, data in processor_data.items()
        ]
        experiment_data.append(process_predictions(formatted_processor_data))

    generate_latex_table(experiment_data)

if __name__ == '__main__':
    main()
