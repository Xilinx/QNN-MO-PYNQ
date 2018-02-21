# W1A2 overlay description

Hardware description of the overlay for 1 bit weights, 2 bit activation multi-layer offload

## Utilization reports

Utilization reports using Vivado Design Suite 2017.4:
```
------------------------------------------------------------------------------------------------
| Design Timing Summary
| ---------------------
------------------------------------------------------------------------------------------------

WNS(ns)      TNS(ns)  TNS Failing Endpoints  TNS Total Endpoints      WHS(ns)      THS(ns)  THS Failing Endpoints 
-------      -------  ---------------------  -------------------      -------      -------  ---------------------  
  0.139        0.000                      0               199765        0.016        0.000                      0  


All user specified timing constraints are met.

+----------------------------+-------+-------+-----------+-------+
|          Site Type         |  Used | Fixed | Available | Util% |
+----------------------------+-------+-------+-----------+-------+
| Slice LUTs                 | 43043 |     0 |     53200 | 80.91 |
|   LUT as Logic             | 29418 |     0 |     53200 | 55.30 |
|   LUT as Memory            | 13625 |     0 |     17400 | 78.30 |
|     LUT as Distributed RAM | 13060 |     0 |           |       |
|     LUT as Shift Register  |   565 |     0 |           |       |
| Slice Registers            | 38379 |     0 |    106400 | 36.07 |
|   Register as Flip Flop    | 38379 |     0 |    106400 | 36.07 |
|   Register as Latch        |     0 |     0 |    106400 |  0.00 |
| F7 Muxes                   |   252 |     0 |     26600 |  0.95 |
| F8 Muxes                   |     0 |     0 |     13300 |  0.00 |
+----------------------------+-------+-------+-----------+-------+
+-------------------+------+-------+-----------+-------+
|     Site Type     | Used | Fixed | Available | Util% |
+-------------------+------+-------+-----------+-------+
| Block RAM Tile    |  139 |     0 |       140 | 99.29 |
|   RAMB36/FIFO*    |  139 |     0 |       140 | 99.29 |
|     RAMB36E1 only |  139 |       |           |       |
|   RAMB18          |    0 |     0 |       280 |  0.00 |
+-------------------+------+-------+-----------+-------+
+----------------+------+-------+-----------+-------+
|    Site Type   | Used | Fixed | Available | Util% |
+----------------+------+-------+-----------+-------+
| DSPs           |   58 |     0 |       220 | 26.36 |
|   DSP48E1 only |   58 |       |           |       |
+----------------+------+-------+-----------+-------+
```
