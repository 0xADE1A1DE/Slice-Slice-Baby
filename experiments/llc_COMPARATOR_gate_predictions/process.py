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
        if lines[idx].startswith("Bin"):
            matrix = []
            while idx < len(lines) and lines[idx].startswith("Bin"):
                parts = lines[idx].split("%)")[1].strip().split()[:4]
                parts = [int(part.replace(',', '')) for part in parts]
                matrix.append(parts)
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
    
def plot_confusion_matrices(averages, stddevs):
    if not averages:
        print("No averaged matrices found.")
        return

    plt.rcParams['font.family'] = 'Times New Roman'
    plt.rcParams['font.size'] = 32

    titles = ['Quiet System', 'Busy System']
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

    fig.text(0.5, 0.05, 'Predicted Slice Index (Comparator Gate)', ha='center', va='center', fontsize=32)
    
    plt.tight_layout(rect=[0, 0.03, 1, 1])  # Adjust layout to make room for shared labels

    plt.savefig("llc_COMPARATOR_gate_predictions.pdf")
    plt.show()

# Parse the matrices
matrices_quiet = parse_confusion_matrix('../per_core_llc_COMPARATOR_gate_predictions/current/data_config_6700k_llc_timings_0.txt')
print(f"Parsed {len(matrices_quiet)} confusion matrices.")
matrices_busy = parse_confusion_matrix('./current/data_busy.txt')
print(f"Parsed {len(matrices_busy)} confusion matrices.")

normalised_quiet = normalise_to_percentage_per_group(matrices_quiet)
normalised_busy = normalise_to_percentage_per_group(matrices_busy)

averaged_quiet, stddev_quiet = calculate_avg_stddev(normalised_quiet)
print(averaged_quiet)
print(stddev_quiet)

averaged_busy, stddev_busy = calculate_avg_stddev(normalised_busy)
print(averaged_busy)
print(stddev_busy)

plot_confusion_matrices([averaged_quiet, averaged_busy], [stddev_quiet, stddev_busy])

