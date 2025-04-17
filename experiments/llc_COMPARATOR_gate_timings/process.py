import matplotlib.pyplot as plt
import seaborn as sns
import numpy as np
from mpl_toolkits.axes_grid1 import make_axes_locatable

# Read data from file
def read_data(file_path):
    with open(file_path, 'r') as file:
        lines = file.readlines()
    
    # Extract relevant data
    data = []
    for line in lines:
        if '|' in line:
            parts = line.strip().split('|')
            if len(parts) == 2:
                yx, signal = parts
                y, x = map(int, yx.split(':'))
                signal = float(signal)
                data.append((y, x, signal))
    return data

# Organize data into a 2D array (matrix)
def organise_data(data):
    matrix = np.zeros((4, 4))  # Initialize a 4x4 matrix with zeros

    for y, x, signal in data:
        if 0 <= y < 4 and 0 <= x < 4:
            matrix[y, x] = signal

    # Normalize the matrix by dividing by the maximum value if the max is not zero
    max_value = 10
    if max_value != 0:
        matrix = matrix / max_value

    return matrix

# Plot the data as multiple heatmaps in a 2x2 grid with shared axes
def plot_multiple_data(matrices):
    plt.rcParams['font.family'] = 'Times New Roman'
    plt.rcParams['font.size'] = 32

    # Create 1x4 subplots (one row, four columns), share y-axis
    fig, axes = plt.subplots(1, 4, figsize=(12, 4), sharey=True)
    titles = ['Core 0', 'Core 1', 'Core 2', 'Core 3']
    
    # Create a color map with the full range of the data
    norm = plt.Normalize(vmin=0, vmax=1)  # Adjust min and max values as per your data range
    cmap = "Reds"  # Color map for the heatmap
    
    # Generate each subplot
    for idx, (ax, matrix) in enumerate(zip(axes, matrices)):        
        # Create heatmap for each normalized matrix
        heatmap = sns.heatmap(matrix, ax=ax, cmap=cmap, cbar=False, square=True, linewidths=0.5, linecolor='black', norm=norm)
        
        # Set title and labels for each subplot
        ax.set_title(titles[idx], fontsize=32)
        ax.set_xlabel('')
        ax.set_ylabel('')
        ax.tick_params(axis='y', labelrotation=0)

        ax.set_xticks([0.5, 1.5, 2.5, 3.5])
        ax.set_xticklabels(['0', '1', '2', '3'])

        # Set custom y-tick labels for the first subplot
        if idx == 0:
            ax.set_ylabel('Input Slice', fontsize=32)
            ax.set_yticklabels(['0', '1', '2', '3'])

    # Add a color bar (legend) outside the plots
    cbar_ax = fig.add_axes([0.94, 0.245, 0.02, 0.515])  # Adjusted position of the color bar
    sm = plt.cm.ScalarMappable(cmap=cmap, norm=norm)
    sm.set_array([])  # Empty array, required for the colorbar to work
    cbar = fig.colorbar(sm, cax=cbar_ax, orientation="vertical", pad=0.04)
    cbar.ax.tick_params(labelsize=28)  # Customize tick labels

    # Set axis labels outside the subplots
    fig.text(0.5, 0.05, 'Compare Slice', ha='center', va='center', fontsize=32)

    # Adjust layout to prevent overlap
    plt.subplots_adjust(right=0.9)  # This ensures there's room for the color bar
    plt.tight_layout(rect=[-0.03, 0, 0.97, 1])
    # Save and show the figure
    plt.savefig("llc_COMPARATOR_gate_timings.pdf", bbox_inches="tight")
    plt.show()  # Uncomment to show the plot

# Main execution
if __name__ == "__main__":
    file_paths = ['./current/data_0.txt', './current/data_1.txt', './current/data_2.txt', './current/data_3.txt']
    
    # Read, organize, normalize, and store each matrix
    matrices = [organise_data(read_data(file_path)) for file_path in file_paths]
    
    # Plot all normalized matrices as subplots with shared axes
    plot_multiple_data(matrices)
