import pandas as pd
import seaborn as sns
import matplotlib.pyplot as plt
from mpl_toolkits.axes_grid1 import make_axes_locatable
import numpy as np

def parse_confusion_matrix(file_path):
    with open(file_path, 'r') as file:
        data = file.read()
        
    matrices = []
    lines = data.splitlines()
    idx = 0

    while idx < len(lines):
        if "Confusion Matrix:" in lines[idx]:
            idx += 1  # Move to the next line after the header
            matrix = []
            while idx < len(lines) and lines[idx].strip():  # Read until an empty line
                if "evsets_create" in lines[idx]:  # Stop condition
                    break
                parts = lines[idx].strip().split()
                # Convert parts to integers, ignoring the first element (row index)
                row = [int(part) for part in parts[1:]]  # Skip the first part
                matrix.append(row)
                idx += 1
            if matrix:
                matrices.append(pd.DataFrame(matrix))
        else:
            idx += 1
    
    return matrices

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

import matplotlib.pyplot as plt
import seaborn as sns
from mpl_toolkits.axes_grid1 import make_axes_locatable

def plot_confusion_matrices(averages, stddevs):
    if not averages:
        print("No averaged matrices found.")
        return

    plt.rcParams['font.family'] = 'Times New Roman'
    plt.rcParams['font.size'] = 32

    titles = ['RDTSCP', 'Fixed-Delay Chain']
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

    fig.text(0.5, 0.05, 'Predicted Slice Index', ha='center', va='center', fontsize=32)
    
    plt.tight_layout(rect=[0, 0.03, 1, 1])  # Adjust layout to make room for shared labels

    plt.savefig("llc_rdtscp_NOT_gate_quiet_predictions.pdf")
    plt.show()

# Parse the matrices
matrices_quiet_rdtscp = parse_confusion_matrix('./current/data_quiet_rdtscp.txt')
print(f"Parsed {len(matrices_quiet_rdtscp)} confusion matrices.")
matrices_quiet_NOT_gate = parse_confusion_matrix('./current/data_quiet_NOT_gate.txt')
print(f"Parsed {len(matrices_quiet_NOT_gate)} confusion matrices.")

normalised_quiet_rdtscp = normalise_to_percentage_per_group(matrices_quiet_rdtscp)
normalised_quiet_NOT_gate = normalise_to_percentage_per_group(matrices_quiet_NOT_gate)

averaged_quiet_rdtscp, stddev_quiet_rdtscp = calculate_avg_stddev(normalised_quiet_rdtscp)
print(averaged_quiet_rdtscp)
print(stddev_quiet_rdtscp)

averaged_quiet_NOT_gate, stddev_quiet_NOT_gate = calculate_avg_stddev(normalised_quiet_NOT_gate)
print(averaged_quiet_NOT_gate)
print(stddev_quiet_NOT_gate)

plot_confusion_matrices([averaged_quiet_rdtscp, averaged_quiet_NOT_gate], [stddev_quiet_rdtscp, stddev_quiet_NOT_gate])

