## Description

This version of Xeus-Cling contains a Magic Command which translates the code content via NVRTC during execution. The kernel definition can then be executed directly with the CUDA Driver API in the notebook.

## Example Code

### Run Magic Command: 
In this case, the system has two GPUs and the kernel definition is generated for both devices. Compiler options are also used with the key '-co'.
![UseMagicCommand](https://github.com/user-attachments/assets/bf9b9775-0298-4252-8236-072ccfacf672)

### Run kernel with two GPUs in use:
![RunKernel](https://github.com/user-attachments/assets/68316e05-3f86-4b9f-8ca0-040958830dce)

### Redefine Kernel: 
Also used GPUInfo to get more information about the system:
![RedefineAndGPUInfo](https://github.com/user-attachments/assets/ac4e41aa-6c19-451e-99d2-9a632e721fcf)


### Installation from source

You will first need to create a new environment and install the dependencies:

```bash
mamba create -n xeus-cling -c conda-forge cmake xeus-zmq cling nlohmann_json=3.11.2 cppzmq xtl pugixml doctest cpp-argparse
source activate xeus-cling
```
Please refer to [environment-host.yml](https://github.com/jupyter-xeus/xeus-cling/blob/main/environment-host.yml) for packages specific versions to install if applicable.

You can then compile the sources. From the build directory, run:

```bash
cmake -D CMAKE_INSTALL_PREFIX=${CONDA_PREFIX} -D CMAKE_C_COMPILER=$CC -D CMAKE_CXX_COMPILER=$CXX -D CMAKE_INSTALL_LIBDIR=${CONDA_PREFIX}/lib ..
make && make install
```

If you don't have a frontend already installed (classic Jupyter Notebook or JupyterLab for instance), install one:

```bash
mamba install jupyterlab -c conda-forge
```





## Dependencies

 The installation of CUDA Driver and CUDA Toolkit is required to run the magic command nvrtc

``xeus-cling`` depends on

 - [xeus-zmq](https://github.com/jupyter-xeus/xeus-zmq)
 - [xtl](https://github.com/xtensor-stack/xtl)
 - [cling](https://github.com/root-project/cling)
 - [pugixml](https://github.com/zeux/pugixml)
 - [cpp-argparse](https://github.com/p-ranav/argparse)
 - [nlohmann_json](https://github.com/nlohmann/json)


| `xeus-cling` |   `xeus-zmq`    |      `xtl`      |     `cling`   |   `pugixml`   | `cppzmq` | `cpp-argparse`| `nlohmann_json` | `dirent` (windows only) |
|--------------|-----------------|-----------------|---------------|---------------|----------|---------------|-----------------|-------------------------|
|  main        |  >=1.1.0,<2.0.0 |  >=0.7.0,<0.8.0 | >=0.9,<0.10   | ~1.8.1        | ~4.3.0   |     ~3.0      | ~3.11.2         | >=2.3.2,<3              |
|  0.15.3      |  >=1.1.0,<2.0.0 |  >=0.7.0,<0.8.0 | >=0.9,<0.10   | ~1.8.1        | ~4.3.0   |     ~2.9      | >=3.6.1,<4.0    | >=2.3.2,<3              |
|  0.15.2      |  >=1.1.0,<2.0.0 |  >=0.7.0,<0.8.0 | >=0.9,<0.10   | ~1.8.1        | ~4.3.0   |     ~2.9      | >=3.6.1,<4.0    | >=2.3.2,<3              |
|  0.15.1      |  >=1.0.0,<2.0.0 |  >=0.7.0,<0.8.0 | >=0.9,<0.10   | ~1.8.1        | ~4.3.0   |     ~2.9      | >=3.6.1,<4.0    | >=2.3.2,<3              |
|  0.15.0      |  >=1.0.0,<2.0.0 |  >=0.7.0,<0.8.0 | >=0.9,<0.10   | ~1.8.1        | ~4.3.0   |     ~2.9      | >=3.6.1,<4.0    | >=2.3.2,<3              |


Prior to version 0.15, `xeus-cling` was depending on `cxxopts` instead of `cpp-argparse`.

| `xeus-cling` |   `xeus-zmq`    |      `xtl`      |     `cling`   |   `pugixml`   | `cppzmq` | `cxxopts`     | `nlohmann_json` | `dirent` (windows only) |
|--------------|-----------------|-----------------|---------------|---------------|----------|---------------|-----------------|-------------------------|
|  0.14.0      |  >=1.0.0,<2.0.0 |  >=0.7.0,<0.8.0 | >=0.6,<0.9    | ~1.8.1        | ~4.3.0   | >=2.1.1,<=2.2 | >=3.6.1,<4.0    | >=2.3.2,<3              |

Prior to version 0.14, `xeus-cling` was depending on `xeus` instead of `xeus-zmq`:

| `xeus-cling` |   `xeus`        |      `xtl`      |     `cling`   |   `pugixml`   | `cppzmq` | `cxxopts`     | `nlohmann_json` | `dirent` (windows only) |
|--------------|-----------------|-----------------|---------------|---------------|----------|---------------|-----------------|-------------------------|
|  0.13.0      |  >=2.0.0,<3.0.0 |  >=0.7.0,<0.8.0 | >=0.6,<0.9    | ~1.8.1        | ~4.3.0   | >=2.1.1,<=2.2 | >=3.6.1,<3.10   | >=2.3.2,<3              |
|  0.12.1      |  >=1.0.0,<2.0.0 |  >=0.7.0,<0.8.0 | >=0.6,<0.9    | ~1.8.1        | ~4.3.0   | >=2.1.1,<=2.2 | >=3.6.1,<4.0    | >=2.3.2,<3              |
|  0.12.0      |  >=1.0.0,<2.0.0 |  >=0.7.0,<0.8.0 | >=0.6,<0.9    | ~1.8.1        | ~4.3.0   | >=2.1.1,<=2.2 | >=3.6.1,<4.0    | >=2.3.2,<3              |

`xeus-cling` requires its dependencies to be built with the same compiler and same C runtime as the one used to build `cling`.

## License and Attribution

This project is based on [Xeus-Cling](https://github.com/jupyter-xeus/xeus-cling), which is licensed under the BSD 3-Clause License.

This modified version includes additional functionality, a special magic command that runs nvrtc compilation of the cell code.

In accordance with the original license, all copyright and license notices from the original project are retained.  
This derived work is also released under the BSD 3-Clause License.

---

**Original project:** [Xeus-Cling GitHub Repository](https://github.com/jupyter-xeus/xeus-cling)  
**License:** BSD 3-Clause License
