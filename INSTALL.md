You have 3 ways to install odr-dabmod on your host:

# Using binary debian packages
If your host is running a debian-based OS on amd64, arm64 or arm/v7, then you can install this tool using the standard debian packaging system:
1. Update the debian apt repository list:
   ```
   # Replace bullseye (debian-11) with bookworm (debian-12) if applicable

   curl -fsSL http://debian.opendigitalradio.org/opendigitalradio-bullseye.sources > /etc/apt/sources.list.d/opendigitalradio-bullseye.sources

   curl -fsSL http://debian.opendigitalradio.org/opendigitalradio.asc | gpg --dearmor > /etc/apt/trusted.gpg.d/opendigitalradio.gpg
   ```
1. Refresh the debian packages list:
   ```
   apt update
   ```
1. Install the package:
   ```
   sudo apt install --yes odr-dabmod
   ```

**Notice**: this package does not include the web-based GUI and Digital Predistortion Computation engine

# Using the dab-scripts
You can compile this tool as well as the other main components of the mmbTools set with an installation script:
1. Clone the dab-scripts repository:
   ```
   git clone https://github.com/opendigitalradio/dab-scripts.git
   ```
1. Follow the [instructions](https://github.com/Opendigitalradio/dab-scripts/tree/master/install)

# Compiling manually
Unlike the 2 previous options, this one allows you to compile odr-dabmod with the features you really need.

## Dependencies
### Debian Bullseye-based OS:
```
# Required packages
## C++11 compiler
sudo apt-get install --yes build-essential automake libtool

## FFTW 3.x
sudo apt-get install --yes libfftw3-dev

# optional packages
## ZeroMQ http://www.zeromq.org
sudo apt-get install --yes libzmq3-dev libzmq5

## UHD for USRP
sudo apt-get install --yes libuhd-dev

## LimeSuite for LimeSDR support
sudo apt-get install --yes liblimesuite-dev

## SoapySDR (see below)
sudo apt-get install --yes libsoapysdr-dev

## bladerf (see below)
sudo apt-get install --yes libbladerf-dev
```

## Compilation
1. Clone this repository:
   ```
   # stable version:
   git clone https://github.com/Opendigitalradio/ODR-DabMod.git

   # or development version (at your own risk):
   git clone https://github.com/Opendigitalradio/ODR-DabMod.git -b next
   ```
1. Configure the project
   ```
   cd ODR-DabMod
   ./bootstrap
   ./configure
   ```
1. Compile and install:
   ```
   make
   sudo make install
   ```

### Configure options
The configure script can be launched with a variety of options:
- Disable ZeroMQ input (to be used with ODR-DabMod), output and remotecontrol: `--disable-zeromq`
- Disable the binding to the UHD driver for USRPs: `--disable-output-uhd`
- Compile using the `-ffast-math` option that gives a substantial speedup at the cost of floating point correctness:  `--enable-fast-math`
- Do not pass `-march=native` to the compiler by using the argument: `--disable-native`

**Remark:** Do not compile ODR-DabMod with `-march=native` compiler option. This is meant for distribution package maintainers who want to use their own march option, and for people running into compilation issues due to `-march=native`. (e.g. GCC bug 70132 on ARM systems)

**Debugging options:** You should not enable any debug option if you need good performance.

Create debugging files for each DSP block for data analysis: `--enable-trace`

For more information, call:
```
./configure --help
```

#### Performance optimisation
While the performance of modern systems is good enough in most cases to
run ODR-DabMod, it is sometimes necessary to increase the compilation
optimisation if all features are used or on slow systems.

Tricks for best performance:

* Do not use `--disable-native`
* Use `--enable-fast-math`
* Add `-O3` to compiler flags
* Disable assertions with `-DNDEBUG`

Applying all together:
```
./configure CFLAGS="-O3 -DNDEBUG" CXXFLAGS="-O3 -DNDEBUG" --enable-fast-math
```

#### Checking for memory usage issues
If your compiler supports it, you can enable the address sanitizer to check for memory
issues:
```
./configure CFLAGS="-fsanitize=address -g -O2" CXXFLAGS="-fsanitize=address -g -O2"
```

The resulting binary will be instrumented with additional memory checks, which have a
measurable overhead. Please report if you get warnings or errors when using the sanitizer.

## SoapySDR support and required dependencies
SoapySDR is a vendor-neutral library to drive SDR devices. It can be used to
drive the HackRF and the LimeSDR among others.

Required dependencies that need to be installed are, in order:
1. SoapySDR itself from https://github.com/pothosware/SoapySDR
1. The LimeSuite for the LimeSDR from https://github.com/myriadrf/LimeSuite
1. HackRF support for SoapySDR from https://github.com/pothosware/SoapyHackRF

ODR-DabMod will automatically recognise if the SoapySDR library is installed on
your system, and will print at the end of `./configure` if support is enabled or
not.

A configuration example is available in `doc/example.ini`

## BladeRF support
In order to use `--enable-bladerf`, you need to install the `libbladerf2` including the -dev package.
