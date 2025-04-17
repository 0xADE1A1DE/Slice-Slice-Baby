import matplotlib.pyplot as plt
import numpy as np

def plot_data(data):
    plt.rcParams['font.family'] = 'Times New Roman'
    plt.rcParams['font.size'] = 32

    """Function to plot the data."""
    plt.figure(figsize=(12, 4))

    # Create plots for each slice (0 to 3)
    for slice_num in range(len(data)):
        if slice_num in data:
            x_values = data[slice_num][0]
            y_values = data[slice_num][1]
            plt.plot(x_values, y_values, label=f'Slice {slice_num}')  # No markers


    # Add labels and title
    plt.xlabel('Delay Chain Length')
    plt.ylabel('Win Probability')

    plt.xlim(0,50)
    plt.yticks([0, 0.5, 1])

    plt.legend(ncol=2, fontsize=28)
    plt.grid()
    
    # Show the plot
    plt.tight_layout(rect=[-0.03, -0.03, 1.03, 1])
    plt.savefig("llc_NOT_gate_probabilities.pdf")
    plt.show()

def main():
    # File path
    file_path = './current/data.txt'

    # Initialize data storage
    data = {}

    # Read the data from the file
    with open(file_path, 'r') as f:
        for line in f:
            # Ignore lines that do not match the expected format
            if line.startswith('evsets_create'):
                continue
            
            # Parse the data line
            try:
                slice_num, delay_length, win_percentage = map(float, line.strip().split(','))
                slice_num = int(slice_num)  # Convert slice number to integer
                win_probability = win_percentage / 100.0
                if slice_num not in data:
                    data[slice_num] = ([], [])  # Initialize lists for x and y
                data[slice_num][0].append(delay_length)  # Append delay length
                data[slice_num][1].append(win_probability)  # Append win percentage as an integer
            except ValueError:
                # Handle any lines that don't conform to the expected format
                continue

    # Call the plot function
    plot_data(data)

if __name__ == '__main__':
    main()
