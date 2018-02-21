# QNN-MO-PYNQ PIP INSTALL Package

This repo contains the pip install package for Quantized Neural Network (QNN) on PYNQ using a Multi-Layer Offload (MO) architecture.
Two different overlays are here included, namely W1A2 (1 bit weights, 2 bit activations) and W1A3 (1 bit weights, 3 bit activations), executing in the Programmable Logic 1 Convolutional layer and 1 (optional) Max Pool layer.

## Quick Start

In order to install it your PYNQ, connect to the board, open a terminal and type:

```shell
# (on PYNQ v2.1)
sudo pip3.6 install git+https://github.com/Xilinx/QNN-MO-PYNQ.git
```

**The installation can take up to 10 minutes, since dependencies must be resolved and sources compiled.**

This will install the QNN package to your board, and create a **QNN** directory in the Jupyter home area. You will find the Jupyter notebooks to test the QNN overlays in this directory.

In order to build the shared object during installation, the user should copy the include folder from VIVADO HLS on the PYNQ board (in windows in vivado-path/Vivado_HLS/201x.y/include, /vivado-path/Vidado_HLS/201x.y/include in unix) and set the environment variable *VIVADOHLS_INCLUDE_PATH* to the location in which the folder has been copied.
If the env variable is not set, the precompiled version will be used instead.

## Repo organization

The repo is organized as follows:

-	qnn: contains the qnn class description as well as the classes for the test networks
	-	src: contains the sources of the 2 overlays and the libraries to rebuild them:
		- library: FINN library for HLS QNN-MO descriptions, host code, script to rebuilt and drivers for the PYNQ (please refer to README for more details)
		- network: overlay topologies (W1A2 and w1A3) HLS top functions, host code and make script for HW and SW built (please refer to README for more details)
	-	bitstreams: with the bitstream for the 2 overlays
	-	libraries: pre-compiled shared objects for low-level driver of the 2 overlays
	-	params: set of trained parameters for the 2 overlays:
		- A pruned version of [DoReFa-Net](https://arxiv.org/abs/1606.06160) network, trained on the [ImageNet](http://www.image-net.org/) dataset with 1 bit weights and 2 bit activations
		- A modified version of [Tiny Yolo](https://pjreddie.com/darknet/yolo/), namely Tinier-Yolo, trained on [PASCAL VOC](http://host.robots.ox.ac.uk/pascal/VOC/) dataset with 1 bit weights and 3 bit activations
-	notebooks: lists a set of python notebooks examples, that during installation will be moved in `/home/xilinx/jupyter_notebooks/qnn/` folder
-	tests: contains test scripts and test images

## Hardware design rebuilt

In order to rebuild the hardware designs, the repo should be cloned in a machine with installation of the Vivado Design Suite (tested with 2017.4).
Following the step-by-step instructions:

1.	Clone the repository on your linux machine: git clone https://github.com/Xilinx/QNN-MO-PYNQ.git;
2.	Move to `clone_path/QNN-MO-PYNQ/qnn/src/network/`
3.	Set the XILINX_QNN_ROOT environment variable to `clone_path/QNN-MO-PYNQ/qnn/src/`
4.	Launch the shell script make-hw.sh with parameters the target network, target platform and mode, with the command `./make-hw.sh {network} {platform} {mode}` where:
	- network can be W1A2 or W1A3;
	- platform is pynq;
	- mode can be `h` to launch Vivado HLS synthesis, `b` to launch the Vivado project (needs HLS synthesis results), `a` to launch both.
5.	The results will be visible in `clone_path/QNN-MO-PYNQ/qnn/src/network/output/` that is organized as follows:
	- bitstream: contains the generated bitstream(s);
	- hls-syn: contains the Vivado HLS generated RTL and IP (in the subfolder named as the target network);
	- report: contains the Vivado and Vivado HLS reports;
	- vivado: contains the Vivado project.
6.	Copy the generated bitstream and tcl script on the PYNQ board `pip_installation_path/qnn/bitstreams/`
