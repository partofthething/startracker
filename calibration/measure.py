"""
Read angle vs. time from level and plot and analyze.
"""

import math

import matplotlib.pyplot as plt
import numpy as np

import level



DATA_FILE = 'data2.txt'

def measure(duration_in_seconds=20):
    t, x, y = [], [], []


    for seconds, xi, yi in level.read():
        t.append(seconds)
        x.append(xi)
        y.append(yi)
        if seconds > duration_in_seconds:
            break

    write(t, x, y)

    plt.plot(t, x, '.')
    plt.xlabel('Time (s)')
    plt.ylabel('Angle (degrees)')
    plt.title('Level data')
    plt.grid(alpha=0.3)
    plt.show()

def write(t, x, y):
    with open(DATA_FILE, 'w') as f:
        for ti, xi, yi in zip(t, x, y):
            f.write('{:<10.1f} {: 6.2f} {: 6.2f}\n'.format(ti, xi, yi))


def read():
    with open(DATA_FILE) as f:
        data = f.readlines()
    t, x, y = [], [], []
    for line in data:
        ti, xi, yi = (float(i) for i in line.split())
        t.append(ti)
        x.append(xi)
        y.append(yi)
    return t, x, y


def analyze(t, x, y):
    slope, intercept = np.polyfit(t, x, 1)
    result = slope * math.pi / 180.0
    fit = np.array(t) * slope + intercept
    plt.plot(t, x, '.', label='Data')
    plt.plot(t, fit, '-', label='Fit')

    plt.xlabel('Time (s)')
    plt.ylabel('Angle (degrees)')
    plt.title('Level data')
    plt.grid(alpha=0.3)
    plt.legend()
    ax = plt.gca()
    ax.text(0.1, 0.15, r'$\frac{{d\Theta}}{{dt}}$ = {:.3e} rad/s'.format(result),
            transform=ax.transAxes,
            color='green', fontsize=14)
    plt.show()

if __name__ == '__main__':
    t, x, y = read()
    analyze(t, x, y)
    # measure(10 * 60)
