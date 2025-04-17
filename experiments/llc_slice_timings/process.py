import numpy as np
import matplotlib.pyplot as plt
from matplotlib.ticker import FormatStrFormatter
import seaborn as sns

def read_data(file_path):
    data = {
        '0': [],
        '1': [],
        '2': [],
        '3': []
    }
    
    with open(file_path, 'r') as file:
        for line in file:
            parts = line.strip().split(',')
            if len(parts) == 2:
                slice_num, measurement = parts
                slice_num = slice_num.strip()
                measurement = float(measurement.strip())
                if slice_num in data:
                    data[slice_num].append(measurement)
    
    return data

def calculate_statistics(data):
    results = {}
    for slice_num, measurements in data.items():
        if measurements:
            mean = np.mean(measurements)
            stddev = np.std(measurements)
            results[slice_num] = {
                'mean': mean,
                'stddev': stddev,
                'count': len(measurements)
            }
        else:
            results[slice_num] = {
                'mean': None,
                'stddev': None,
                'count': 0
            }
    
    return results

def plot_data(data, results):
    plt.rcParams['font.family'] = 'Times New Roman'
    plt.rcParams['font.size'] = 32

    fig, ax = plt.subplots(figsize=(12, 4))
    
    # Plot bell curves with shading for each slice
    for slice_num, measurements in data.items():
        if measurements:
            mean = results[slice_num]['mean']
            stddev = results[slice_num]['stddev']
            
            # Define the range for the bell curve
            x = np.linspace(mean - 3*stddev, mean + 3*stddev, 1000)
            # Calculate the normal distribution
            y = (1/(stddev * np.sqrt(2 * np.pi))) * np.exp(-0.5 * ((x - mean) / stddev) ** 2)
            
            ax.plot(x, y, label=f'Slice {slice_num}', lw=2)
            ax.fill_between(x, y, alpha=0.2)  # Add shading under the curve

    plt.legend(ncol=2, fontsize=28)
    ax.yaxis.set_major_formatter(FormatStrFormatter('%.2f'))
    #ax.set_title('Bell Curve for Measurements per Slice')
    ax.set_xlabel('Access Time (Cycles)')
    ax.set_ylabel('Density')
    ax.set_xlim([0,65])
    plt.tight_layout(rect=[-0.03, -0.03, 1.03, 1])
    plt.savefig("llc_slice_timings.pdf")
    plt.show()

def main():
    file_path = './current/data.txt'  # Update with your file path
    data = read_data(file_path)
    results = calculate_statistics(data)
    
    # Print results
    for slice_num, result in results.items():
        print(f"Slice {slice_num}:")
        print(f"  Mean: {result['mean']:.3f}")
        print(f"  Standard Deviation: {result['stddev']:.3f}")
        print(f"  Number of Measurements: {result['count']}")
        print()
    
    plot_data(data, results)

if __name__ == '__main__':
    main()
