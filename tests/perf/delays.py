#!/usr/bin/env python3

import matplotlib.pyplot as pp
import matplotlib.ticker as ticker
import numpy as np
import os, sys

files = sys.argv[1:]
prefix = os.path.commonprefix(files) # Common part of all files

signals = []
for file in files:
    name = file.partition(prefix)[2] # Uncommon part of the file
    data = np.fromfile(file, dtype=np.int64)
    signals.append((name, data))

samples = np.arange(len(signals[0][1])) # For the X axis, 1,2,3,...

fig, ax1 = pp.subplots(figsize=(8, 8))
fig.canvas.manager.set_window_title('Signals delays relative to ' + signals[0][0])

ns = ticker.EngFormatter(unit="s", sep="") # Delay in nanoseconds
ns.ENG_PREFIXES = {-9: 'f', -3: 'p', 0: 'n', 3: 'u', 6: 'm', 9: '', 12: 'k', 15: 'M', 18: 'G', 21: 'T', 24: 'P'}
sn = ticker.EngFormatter(sep="") # Sample number

for s in signals[1:]:
    delays = np.where(s[1] != 0, s[1] - signals[0][1], 0) # calculate deltas or keep zero for missing
    ax1.plot(samples, delays, label=s[0], linestyle='dashed', linewidth=0.5, marker='.', markersize=1.5)
ax1.xaxis.set_major_formatter(sn)
ax1.yaxis.set_major_formatter(ns)
ax1.set_xlabel('samples #')
ax1.set_ylabel('signals delay relative to ' + signals[0][0] + '(ns)', color='#69b3a2')
ax1.tick_params(axis="y", labelcolor='#69b3a2')
ax1.locator_params(axis='y', nbins=40, prune='lower', integer=True)
ax1.grid(True, axis='both')


#master = signals[0][1] - signals[0][1][0] # Relative to the first ts
master = np.diff(signals[0][1], n=1, prepend=signals[0][1][0]) # Relative to the previous ts
ax2 = ax1.twinx()
#ax2.plot([samples[0],samples[-1]], [master[0],master[-1]], label='Linear', linestyle='dashed', color='gray', alpha=0.2, linewidth=1.5)
ax2.plot(samples, master, label=signals[0][0], linestyle='solid', color='#3399e6', linewidth=1)
ax2.yaxis.set_major_formatter(ns)
ax2.set_ylabel(signals[0][0] + ' timestamp delay from the previous sample(ns)', color='#3399e6')
ax2.tick_params(axis="y", labelcolor='#3399e6')
ax2.locator_params(axis='both', nbins=40, prune='lower', integer=True)
legend = ax2.legend(handles=(ax1.lines + ax2.lines), loc='upper left')

ax2.format_coord = lambda x, y: '#%d delay: %dns' % (x, y)


# Will map legend lines to original lines.
legend = dict(zip(legend.get_texts(), (ax1.lines + ax2.lines)))

for label, line in legend.items():
   label.set_picker(5)  # Enable picking on the legend line.

def on_pick(event):
    if event.artist in legend:
        line = legend[event.artist]
        visible = not line.get_visible()
        line.set_visible(visible)
        event.artist.set_alpha(1.0 if visible else 0.3)
        fig.canvas.draw()

fig.canvas.mpl_connect('pick_event', on_pick)

pp.title(prefix + ','.join([s[0] for s in signals]))
pp.show()
