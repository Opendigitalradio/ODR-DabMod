Required dependencies:
======================

* A C++11 capable compiler
* pkg-config
* FFTW 3.x
* Optional UHD for USRP
* Optional SoapySDR (see below)
* Optional ZeroMQ http://www.zeromq.org

Simple install procedure:
=========================

    % tar xjf odr-dabmod-X.Y.Z.tar.bz2      # Unpack the source
    % cd odr-dabmod-X.Y.Z                   # Change to the source directory
    % ./configure
                                            # Run the configure script
    % make                                  # Build ODR-DabMod
    [ as root ]
    % make install                          # Install ODR-DabMod

Configure options
=================

The configure script can be launched with a variety of options:

`--disable-zeromq`

Disable ZeroMQ input (to be used with ODR-DabMux), output and remotecontrol.

                        
`--disable-output-uhd`

Disable the binding to the UHD driver for USRPs


`--enable-fast-math`   

Compile using the `-ffast-math` option that gives a substantial speedup at the cost of floating point correctness.
 

`--disable-native`

**Remark:** Do not compile ODR-DabMod with `-march=native` compiler option. This is meant for distribution package maintainers who want to use their own march option, and for people running into compilation issues due to `-march=native`. (e.g. GCC bug 70132 on ARM systems)

**Debugging options:** You should not enable any debug option if you need good performance.


`--enable-trace`

Create debugging files for each DSP block for data analysis

For more information, call:

    % ./configure --help

Performance optimisation
------------------------
While the performance of modern systems is good enough in most cases to
run ODR-DabMod, it is sometimes necessary to increase the compilation
optimisation if all features are used or on slow systems.

Tricks for best performance:

* Do not use `--disable-native`
* Use `--enable-fast-math`
* Add `-O3` to compiler flags
* Disable assertions with `-DNDEBUG`

Applying all together:

    % ./configure CFLAGS="-O3 -DNDEBUG" CXXFLAGS="-O3 -DNDEBUG" --enable-fast-math

Checking for memory usage issues
--------------------------------
If your compiler supports it, you can enable the address sanitizer to check for memory
issues:

    % ./configure CFLAGS="-fsanitize=address -g -O2" CXXFLAGS="-fsanitize=address -g -O2"

The resulting binary will be instrumented with additional memory checks, which have a
measurable overhead. Please report if you get warnings or errors when using the sanitizer.

Nearly as simple install procedure using repository:
====================================================

* Download and install dependencies as above
* Clone the git repository
* Bootstrap autotools:
   
      % ./bootstrap.sh
      
  In case this fails, try:
  
      % aclocal && automake --gnu --add-missing && autoconf
        
* Then use `./configure` as above

SoapySDR support and required dependencies
==========================================

SoapySDR is a vendor-neutral library to drive SDR devices. It can be used to
drive the HackRF and the LimeSDR among others.

Required dependencies that need to be installed are, in order:

1. SoapySDR itself from https://github.com/pothosware/SoapySDR
2. The LimeSuite for the LimeSDR from https://github.com/myriadrf/LimeSuite
3. HackRF support for SoapySDR from https://github.com/pothosware/SoapyHackRF

ODR-DabMod will automatically recognise if the SoapySDR library is installed on
your system, and will print at the end of `./configure` if support is enabled or
not.

A configuration example is available in `doc/example.ini`
