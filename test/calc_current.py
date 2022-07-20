#!/usr/bin/python

import numpy as np
import matplotlib.pyplot as plt

g_arr = list()

for k in range(128):
    hc = (k & 0x40) >> 6

    # Datasheet MBI5029, page 22
    # this is misleading, it is meant to mean
    # cc0 * 2^5 + cc1 * 2^4 + cc2 * 2^3 + cc3 * 2^2 + cc4 * 2^1 + cc5 * 2^0
    d = k & 0x3f


    # Datasheet MBI5029, page 21
    g = ((1 + 2 * hc)/3)*(1+d/32)/3

    g_arr.append((k, g))

g_arr = np.array(g_arr)

fig, ax = plt.subplots()
ax.set_xlabel('Configuration Code')
ax.set_ylabel('Gain')
ax.plot(g_arr[:, 0], g_arr[:, 1])
plt.show()
