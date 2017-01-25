import math
import numpy as np
import os

# Compute free delta time values
DELTA_TIME_FREE_TAB_LEN=65
dtf_pts=[x/float(DELTA_TIME_FREE_TAB_LEN-1) for x in
        xrange(DELTA_TIME_FREE_TAB_LEN)]
dtf_vls=[(math.pow(2.,-6*(1.-x)) - math.pow(2.,-6))/(1. - math.pow(2.,-6.))
            for x in dtf_pts]

# Compute swing values
# The number of divisions per note
SWING_SET_SIZE=3
SWING_DIV_MAX=3
N_SWING_DIVS=2
N_SWING_PTS=N_SWING_DIVS*SWING_DIV_MAX
swing_pts=[]
for x1 in np.arange(0,SWING_DIV_MAX+1./N_SWING_DIVS,1./N_SWING_DIVS):
    for x2 in np.arange(0,SWING_DIV_MAX+1./N_SWING_DIVS-x1,1./N_SWING_DIVS):
        x3 = SWING_DIV_MAX - x2 - x1
        swing_pts.append((x1, x2, x3))

if os.environ.get('TEST') == None:
    with open('constants/tables.c','w') as f:
        f.write("/* This file generated by constants/tables.py */\n\n")
        # Write free delta time values
        f.write("const float tables_delta_time_free[] = {\n")
        for x in dtf_vls:
            f.write("    %f,\n" % (x,))
        f.write("};\n\n")
        # Write swing points
        f.write("const float tables_swing_pts[] = {\n")
        for x in swing_pts:
            f.write("    %f, %f, %f,\n" % x)
        f.write("};\n\n")
    
    with open('constants/tables.h','w') as f:
        f.write("/* This file generated by constants/tables.py */\n\n")
        f.write("#ifndef TABLES_H\n#define TABLES_H\n")
        # Declare free delta time values
        f.write("extern const float tables_delta_time_free[];\n")
        f.write(("#define TABLES_DELTA_TIME_FREE_LEN "
                + "%d\n" % (DELTA_TIME_FREE_TAB_LEN,)))
        # Declare swing points
        f.write("extern const float tables_swing_pts[];\n")
        f.write("#define TABLES_SWING_PTS_LOOKUP(n,x1,x2,x3) \\\n"
                "   do { \\\n"
                "       *x1 = tables_swing_pts[(%d) * (n)]; \\\n"
                "       *x2 = tables_swing_pts[(%d) * (n) + 1]; \\\n"
                "       *x3 = tables_swing_pts[(%d) * (n) + 2]; \\\n"
                "   while (0)\n" % (SWING_SET_SIZE,SWING_SET_SIZE,SWING_SET_SIZE))
        f.write("#define TABLES_N_SWING_SETS %d\n" % (len(swing_pts),))
        f.write("#endif /* TABLES_H */\n")


