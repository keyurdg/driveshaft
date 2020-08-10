# Building Driveshaft

## Ubuntu 18.04

### Install Dependencies

```bash
sudo apt-get install build-essential \
                     automake \
                     autoconf \
                     cmake \
                     libgearman-dev \
                     libboost-dev \
                     libboost-system-dev \
                     libboost-program-options-dev \
                     libboost-filesystem-dev \
                     liblog4cxx-dev \
                     libcurl4-openssl-dev \
                     libtool
```

### Compilation

First, you'll need to compile and install [PrometheusC++](https://github.com/jupp0r/prometheus-cpp)

```bash

# checkout version 0.9.0
git clone --depth=50 git@github.com:jupp0r/prometheus-cpp.git jupp0r/prometheus-cpp
cd jupp0r/prometheus-cpp
git fetch origin refs/tags/v0.9.0:
git checkout -qf FETCH_HEAD

# fetch third-party dependencies
git submodule update --init && cmake -DBUILD_SHARED_LIBS=ON && sudo make install && popd

# run cmake
cmake -DBUILD_SHARED_LIBS=ON

# install prometheus-cpp library
sudo make install
```

Now, you're good to go on building driveshaft.

```bash
git clone https://github.com/keyurdg/driveshaft.git
cd driveshaft
mkdir build && cd build
cmake ..
```

Now you can `make driveshaft_unit_tests` and `make test`! Happy hacking!
