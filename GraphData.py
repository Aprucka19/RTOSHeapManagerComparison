import pandas as pd
import matplotlib.pyplot as plt

# Load the CSV file
data = pd.read_csv('StatsThurough.csv')

# Convert time units from 12.5 ns to microseconds
data['AllocationTime'] *= 12.5e-3
data['FreeTime'] *= 12.5e-3
data['MixedOperationTime'] *= 12.5e-3

# Define heap type labels
heap_labels = {1: 'First Fit', 2: 'Best Fit', 3: 'Worst Fit', 4: 'Knuths'}

# Replace numeric heap types with labels
data['HeapType'] = data['HeapType'].map(heap_labels)

# Group data by 'HeapType' and calculate means
grouped = data.groupby('HeapType')
avg_alloc_time = grouped['AllocationTime'].mean()
avg_free_time = grouped['FreeTime'].mean()
avg_mixed_op_time = grouped['MixedOperationTime'].mean()
total_allocated = grouped['TotalAllocatedDuringTest'].mean()
avg_failed_allocs = grouped['AllocationsFailed'].mean()
avg_used_after_alloc = grouped['UsedAfterAlloc'].mean()
avg_free_after_alloc = grouped['FreeAfterAlloc'].mean()
avg_waste_after_alloc = grouped['WasteAfterAlloc'].mean()
avg_used_after_mixed = grouped['UsedAfterMixed'].mean()
avg_free_after_mixed = grouped['FreeAfterMixed'].mean()
avg_waste_after_mixed = grouped['WasteAfterMixed'].mean()

# Create subplots for each metric
fig, axs = plt.subplots(3, 3, figsize=(15, 15))
axs = axs.flatten()  # Flatten the axis array for easier indexing

# Define plot function
def plot_metric(ax, series, title, ylabel, color):
    series.plot(kind='bar', ax=ax, color=color)
    ax.set_title(title)
    ax.set_xlabel('Heap Type')
    ax.set_ylabel(ylabel)
    ax.set_xticklabels(series.index, rotation=0)
    ax.grid(True)

# Plot each metric
plot_metric(axs[0], avg_alloc_time, 'Average Allocation Time', 'Time (us)', 'skyblue')
plot_metric(axs[1], avg_free_time, 'Average Free Time', 'Time (us)', 'lightgreen')
plot_metric(axs[2], avg_mixed_op_time, 'Average Mixed Operation Time', 'Time (us)', 'salmon')
plot_metric(axs[3], total_allocated, 'Average Total Allocated During Test', 'Bytes', 'gold')
plot_metric(axs[4], avg_failed_allocs, 'Average Failed Allocations', 'Allocations', 'tomato')
plot_metric(axs[5], avg_used_after_alloc, 'Average Memory Used After Allocation', 'Memory (bytes)', 'blue')
plot_metric(axs[6], avg_free_after_alloc, 'Average Memory Free After Allocation', 'Memory (bytes)', 'green')
plot_metric(axs[7], avg_waste_after_alloc, 'Average Memory Waste After Allocation', 'Memory (bytes)', 'red')
plot_metric(axs[8], avg_used_after_mixed, 'Average Memory Used After Mixed Operations', 'Memory (bytes)', 'blue')

plt.tight_layout()
plt.show()
