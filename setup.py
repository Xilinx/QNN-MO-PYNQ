#   Copyright (c) 2018, Xilinx, Inc.
#   All rights reserved.
#
#   Redistribution and use in source and binary forms, with or without
#   modification, are permitted provided that the following conditions are met:
#
#   1.  Redistributions of source code must retain the above copyright notice,
#       this list of conditions and the following disclaimer.
#
#   2.  Redistributions in binary form must reproduce the above copyright
#       notice, this list of conditions and the following disclaimer in the
#       documentation and/or other materials provided with the distribution.
#
#   3.  Neither the name of the copyright holder nor the names of its
#       contributors may be used to endorse or promote products derived from
#       this software without specific prior written permission.
#
#   THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
#   AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
#   THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
#   PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR
#   CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
#   EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
#   PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS;
#   OR BUSINESS INTERRUPTION). HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
#   WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR
#   OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF
#   ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

from setuptools import setup, find_packages
import subprocess
import sys
import shutil
import os
from glob import glob


import site

if 'BOARD' not in os.environ or not (os.environ['BOARD'] == 'Pynq-Z1' or os.environ['BOARD'] == 'Pynq-Z2'):
    print("Only supported on a Pynq Z1 or Z2 boards")
    exit(1)
    
package_data = []
data_files = []
package_include_data = [ 'bitstreams/', 'params/', 'libraries/' ]

if len(sys.argv) > 1 and sys.argv[1] == 'install':
    my_path = os.path.dirname(os.path.abspath(__file__)) + "/"
    print("Running pre installation scripts...")
    print("Building hardware library...")
    subprocess.check_output(["make", "-j2", "-C", my_path + "qnn/src/network/", "lib_hw"])
    shutil.copy2(my_path + "qnn/src/network/output/lib_hw.so", my_path + "qnn/libraries/")

    if os.environ.get('VIVADOHLS_INCLUDE_PATH') is not None:
        print("Building software libraries...")
        os.remove(my_path + "qnn/libraries/lib_sw_W1A2.so")
        os.remove(my_path + "qnn/libraries/lib_sw_W1A3.so")
        subprocess.check_output(["make", "-j2", "-C", my_path + "qnn/src/network/", "lib_sw_W1A2"])
        subprocess.check_output(["make", "-j2", "-C", my_path + "qnn/src/network/", "lib_sw_W1A3"])
        shutil.copy2(my_path + "qnn/src/network/output/lib_sw_W1A2.so", my_path + "qnn/libraries/")
        shutil.copy2(my_path + "qnn/src/network/output/lib_sw_W1A3.so", my_path + "qnn/libraries/")

    for root, dirs, files in os.walk(my_path + "qnn/"):
        for file in files:
            if file.endswith(".bz2"):
                print("Extracting " + os.path.join(root, file) + " ...")
                subprocess.check_output(["bzip2", "-d", os.path.join(root, file)])

    for dir in package_include_data:
        for root, dirs, files in os.walk(my_path + "qnn/" + dir):
            for file in files:
                package_data.append(os.path.join(root,file).replace(my_path + 'qnn/', ''));

    print ("Registering notebooks in data files...")
    data_files = [(os.path.join('/home/xilinx/jupyter_notebooks/qnn',root.replace('notebooks/','')), [os.path.join(root, f) for f in files]) for root, dirs, files in os.walk('notebooks/')]

    print ("Checking out darknet...")
    subprocess.check_output(["git", "clone", "https://github.com/giuliogamba/darknet", my_path + "darknet"])
    print ("Building darknet...")
    subprocess.check_output(["make", "-j2", "-C", my_path + "darknet", "all"])
    # print ("Moving darknet library...")
    # shutil.copy2(my_path + "darknet/libdarknet.so", my_path + "darknet/python/")
    # print ("Delete .git folder...")
    # shutil.rmtree(my_path + "darknet/.git")
    print ("Registering data files...")
    data_files.extend([(os.path.join('/opt/',root), [os.path.join(root, f) for f in files]) for root, dirs, files in os.walk('darknet/')])

setup(
    name = "qnn-loopback",
    version = 0.1,
    url = 'kwa/pynq',
    license = 'Apache Software License',
    author = "Nicholas Fraser, Giulio Gambardella, Björn Gottschall, Peter Ogden, Thomas Preusser, Andrea Solazzo",
    author_email = "pynq_support@xilinx.com",
    include_package_data = True,
    packages = ['qnn'],
    package_dir = { 'qnn' : 'qnn' },
    package_data = {
        'qnn' : package_data
    },
    data_files = data_files,
    description = "Classification using a hardware accelerated quantized neural network"
)
