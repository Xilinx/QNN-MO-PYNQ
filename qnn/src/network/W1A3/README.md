# W1A3 overlay description

Hardware description of the overlay for 1 bit weights, 3 bit activation multi-layer offload

## Utilization reports

Utilization reports using Vivado Design Suite 2017.4:
```
------------------------------------------------------------------------------------------------
| Design Timing Summary
| ---------------------
------------------------------------------------------------------------------------------------

WNS(ns)      TNS(ns)  TNS Failing Endpoints  TNS Total Endpoints      WHS(ns)      THS(ns)  THS Failing Endpoints 
-------      -------  ---------------------  -------------------      -------      -------  ---------------------  
  0.071        0.000                      0               177008        0.015        0.000                      0  


All user specified timing constraints are met.

+----------------------------+-------+-------+-----------+-------+
|          Site Type         |  Used | Fixed | Available | Util% |
+----------------------------+-------+-------+-----------+-------+
| Slice LUTs                 | 46888 |     0 |     53200 | 88.14 |
|   LUT as Logic             | 38764 |     0 |     53200 | 72.86 |
|   LUT as Memory            |  8124 |     0 |     17400 | 46.69 |
|     LUT as Distributed RAM |  7380 |     0 |           |       |
|     LUT as Shift Register  |   744 |     0 |           |       |
| Slice Registers            | 43139 |     0 |    106400 | 40.54 |
|   Register as Flip Flop    | 43139 |     0 |    106400 | 40.54 |
|   Register as Latch        |     0 |     0 |    106400 |  0.00 |
| F7 Muxes                   |  1652 |     0 |     26600 |  6.21 |
| F8 Muxes                   |   768 |     0 |     13300 |  5.77 |
+----------------------------+-------+-------+-----------+-------+
+-------------------+------+-------+-----------+--------+
|     Site Type     | Used | Fixed | Available |  Util% |
+-------------------+------+-------+-----------+--------+
| Block RAM Tile    |  140 |     0 |       140 | 100.00 |
|   RAMB36/FIFO*    |  140 |     0 |       140 | 100.00 |
|     RAMB36E1 only |  140 |       |           |        |
|   RAMB18          |    0 |     0 |       280 |   0.00 |
+-------------------+------+-------+-----------+--------+
+----------------+------+-------+-----------+-------+
|    Site Type   | Used | Fixed | Available | Util% |
+----------------+------+-------+-----------+-------+
| DSPs           |   57 |     0 |       220 | 25.91 |
|   DSP48E1 only |   57 |       |           |       |
+----------------+------+-------+-----------+-------+
```
