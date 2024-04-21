import pandas as pd
import matplotlib.pyplot as plt

# Load the CSV file
data = pd.read_csv('TestData.csv')

# Convert time units from 12.5 ns to microseconds
data['AllocationTime'] *= 12.5e-3
data['FreeTime'] *= 12.5e-3
data['MixedOperationTime'] *= 12.5e-3

# Define heap type labels
heap_labels = {1: 'First Fit', 2: 'Best Fit', 3: 'Worst Fit', 4: 'Knuths'}

# Replace numeric heap types with labels
data['HeapType'] = data['HeapType'].map(heap_labels)

# Filter data for the first three types and all four types
data_123 = data[data['HeapType'].isin(['First Fit', 'Best Fit', 'Worst Fit'])]
data_1234 = data  # Includes all types

# Function to plot metrics
def plot_metrics(dataframe):
    grouped = dataframe.groupby('HeapType').mean()
    colors = ['skyblue', 'lightgreen', 'salmon', 'gold', 'tomato', 'blue', 'green', 'red', 'purple']
    fig, axs = plt.subplots(3, 3, figsize=(15, 15))
    axs = axs.flatten()

    def plot_metric(ax, metric, title, ylabel, idx):
        series = grouped[metric]
        series.plot(kind='bar', ax=ax, color=colors[idx])
        ax.set_title(f'{title}')
        ax.set_ylabel(ylabel)
        ax.set_xlabel('Heap Type')
        ax.grid(True)
        # Set y-axis limits if necessary
        ax.set_ylim([max(0, series.min() - series.std()), series.max() + series.std()])

    plot_metric(axs[0], 'AllocationTime', 'Average Allocation Time', 'Time (us)', 0)
    plot_metric(axs[1], 'FreeTime', 'Average Free Time', 'Time (us)', 1)
    plot_metric(axs[2], 'MixedOperationTime', 'Average Mixed Operation Time', 'Time (us)', 2)
    plot_metric(axs[3], 'AllocationsFailed', 'Average Failed Allocations', 'Count', 3)
    plot_metric(axs[4], 'UsedAfterAlloc', 'Average Memory Used After Allocation', 'Bytes', 4)
    plot_metric(axs[5], 'FreeAfterAlloc', 'Average Memory Free After Allocation', 'Bytes', 5)
    plot_metric(axs[6], 'WasteAfterAlloc', 'Average Waste After Allocation', 'Bytes', 6)
    plot_metric(axs[7], 'UsedAfterMixed', 'Average Memory Used After Mixed Operations', 'Bytes', 7)
    plot_metric(axs[8], 'WasteAfterMixed', 'Average Waste After Mixed Operations', 'Bytes', 8)

    plt.tight_layout()
    plt.show()

# Plot for heap types 1, 2, and 3
plot_metrics(data_123)

# Plot for all heap types
plot_metrics(data_1234)
