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
        
        # Normalize while avoiding division by zero
        normalised_df = df.div(row_sums, axis=0).fillna(0) * 100
        
        # Append the normalized DataFrame
        normalised_dfs.append(normalised_df)
    
    return normalised_dfs

def process(matrices):
    normalise_to_percentage_per_group

def plot_confusion_matrices(averages, stddevs):
    if not averages:
        print("No averaged matrices found.")
        return

    plt.rcParams['font.family'] = 'Times New Roman'
    plt.rcParams['font.size'] = 32

    titles = ['Quiet Scenario', 'Busy Scenario']
    cmap = 'rocket'
    
    fig, axes = plt.subplots(1, len(averages), figsize=(12, 6), squeeze=False)
    
    for i, (ax, avg, stddev, title) in enumerate(zip(axes[0], averages, stddevs, titles)):
        # Create heatmap for mean values
        heatmap = sns.heatmap(avg, annot=avg.round(0).astype(int), fmt='d', vmin=0, vmax=100, ax=ax, cbar=False, cmap=cmap)

        # Overlay standard deviation as text annotations
        for row in range(avg.shape[0]):
            for col in range(avg.shape[1]):
                mean_value = avg.iloc[row, col]
                # Set text color based on the mean value
                text_color = 'black' if mean_value >= 80 else 'white'
                
                ax.text(col + 0.5, row + 0.5, f"\n\n± {stddev.iloc[row, col]:.0f}", 
                        ha='center', va='center', color=text_color, fontsize=20)

        ax.set_title(title, fontsize=32)
        ax.tick_params(axis='y', labelrotation=0)
        
        if i == 0:
            ax.set_ylabel('Actual Slice Index')
        if i == 1:  # Only add color bar for the second matrix
            # Create a divider for the existing axes instance
            divider = make_axes_locatable(ax)
            cax = divider.append_axes("right", size="5%", pad=0.1)
            ax.set_yticks([])
            
            # Add color bar to the new axes
            cbar = fig.colorbar(heatmap.collections[0], cax=cax)
            cbar.set_ticks([0, 25, 50, 75, 100])
            cbar.set_ticklabels(['0%', '25%', '50%', '75%', '100%'])
            cbar.ax.tick_params(labelsize=28)  # Customize tick labels

    fig.text(0.5, 0.04, 'Predicted Slice Index (Comparator Gate)', ha='center', va='center', fontsize=32)
    
    plt.tight_layout(rect=[0, 0.03, 1, 1])  # Adjust layout to make room for shared labels

    plt.savefig("llc_COMPARATOR_gate_predictions.pdf")
    plt.show()

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

def main():
    folder_path = './current'
    per_core_matrices = []
    per_core_speeds = []

    # Read all data files in the folder
    for filename in sorted(os.listdir(folder_path)):
        if filename.startswith(f"data_{sys.argv[1]}_") and filename.endswith('.txt'):
            file_path = os.path.join(folder_path, filename)
            with open(file_path, 'r') as file:
                data = file.read()
            matrices, speed_avg, speed_stddev = parse_confusion_matrix(data)
            matrices = normalise_to_percentage_per_group(matrices)
            matrices = calculate_avg_stddev(matrices)
            per_core_matrices.append(matrices)
            print(speed_avg, speed_stddev)
            per_core_speeds.append([speed_avg, speed_stddev])

    # Process the matrices and calculate true positive rates
    per_core_avg_matrices = []
    per_core_stddev_matrices = []
    for c in per_core_matrices:
        per_core_avg_matrices.append(c[0])
        per_core_stddev_matrices.append(c[1])

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

    print("Accuracy", round(avgavgavg / float(len(per_core_matrices))), round(avgavgstd / float(len(per_core_matrices))))

    speed_avg, speed_stddev = average_per_core_speeds(per_core_speeds)
    print("Speed", speed_avg, speed_stddev)

    #plot_confusion_matrices(per_core_avg_matrices, per_core_stddev_matrices)

if __name__ == '__main__':
    main()
