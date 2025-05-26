
## Dependencies

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

This modified version includes additional functionality, specifically an extended magic command system.

In accordance with the original license, all copyright and license notices from the original project are retained.  
This derived work is also released under the BSD 3-Clause License.

---

**Original project:** [Xeus-Cling GitHub Repository](https://github.com/jupyter-xeus/xeus-cling)  
**License:** BSD 3-Clause License
