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

First, you'll need to compile and install [Snyder](https://github.com/mrtazz/snyder)

```bash
git clone https://github.com/mrtazz/snyder
cd snyder
./autogen.sh
./configure
make install
```

Now, you're good to go on building driveshaft.

```bash
git clone https://github.com/keyurdg/driveshaft.git
cd driveshaft
mkdir build && cd build
cmake ..
```

Now you can `make driveshaft_unit_tests` and `make test`! Happy hacking!
