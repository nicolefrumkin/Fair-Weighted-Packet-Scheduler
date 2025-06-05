import pandas as pd
import matplotlib.pyplot as plt

df = pd.read_csv("schedule.csv")

fig, ax = plt.subplots(figsize=(10, 4))

unique_names = df['name'].unique()
name_to_y = {name: i for i, name in enumerate(unique_names)}
bar_height = 0.8

for _, row in df.iterrows():
    y = name_to_y[row['name']]
    ax.barh(y=y, width=row['length'], left=row['start'], height=bar_height,
            color=row['color'], edgecolor='black')

ax.set_yticks(range(len(unique_names)))
ax.set_yticklabels(unique_names)
ax.set_xlabel("Time")
ax.set_title("WFQ Schedule Visualization")
plt.tight_layout()
plt.grid(True)
plt.show()
