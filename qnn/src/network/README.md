# Build Software

The software represents the interface to the neuronal network on the fpga. There are two ways the software can be builded, as testbench and as library. The testbench can be directly called from the command line to test the neuronal network. The library is used from the python jupyter notebooks to offload images to the hardware.

### Dependencies

* rapidjson libraries under ../library/rapidjson  
    This dependency gets automatically resolved if an internet connection is available, otherwise clone the library from https://github.com/Tencent/rapidjson

* VIVADO HLS Libraries for the pure software implemenation  
    The user should copy the include folder from VIVADO HLS on the PYNQ board (in windows in vivado-path/Vivado_HLS/201x.y/include, /vivado-path/Vidado_HLS/201x.y/include in unix) and set the environment variable **VIVADOHLS_INCLUDE_PATH** to the location in which the folder has been copied.  

### Build Steps

> The user can find the builded libraries and applications in the folder output/

* ``` make help ```  
    Gives the user an overview of all possible targets, options and dependencies
* ``` make lib_hw ```  
    Builds the hardware library for the python jupyter notebooks.
* ``` make lib_sw_W1A2 lib_sw_W1A3 ```  
    Builds the pure software implementation libraries for the python jupyter notebooks. These libraries behave exactly like lib_hw, but are only compatible with the specified network.
* ``` make app_hw app_sw_W1A2 app_sw_W1A3 ```  
    Builds the testbenches for hardware and software implementations. These can be used with the network and layer json files to test the neuronal network implementation.

> The building automatically recognize the platform on which the command is launched (Zynq or Zynq Ultrascale) and adapts the low-level drivers address accordingly

# Build Hardware

Please read the *Hardware design rebuilt* chapter in [qnn-loopback/README.md](../../../README.md).
