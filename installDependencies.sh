### install Anaconda
mkdir deps/
wget -O deps/anaconda.sh https://repo.continuum.io/archive/Anaconda3-5.0.0-Linux-x86_64.sh
bash deps/anaconda.sh -b -p ./extern/anaconda

# Other Deps
sudo apt-get install build-essential curl git cmake unzip autoconf autogen automake libtool mlocate zlib1g-dev g++-7 python python3-numpy python3-dev python3-pip python3-wheel wget libboost-all-dev pkg-config zip g++ zlib1g-dev unzip python -y
sudo updatedb



### For tensorflow cc
# Bazel
wget -O deps/bazel.sh https://github.com/bazelbuild/bazel/releases/download/0.10.0/bazel-0.10.0-installer-linux-x86_64.sh
sudo bash deps/bazel.sh

# Cuda toolkit 10.0
# follow instructions from https://developer.nvidia.com/cuda-10.0-download-archive

# cuDNN
# follow instructions from https://docs.nvidia.com/deeplearning/cudnn/install-guide/index.html#installcuda;
# download cuDNN 7.6.5 for cuda 10.0

# build and install my version of tensorflow
# remove the author's tensorflow_cc
rm -rf extern/tensorflow_cc/
# clone Eric's tensorflow_cc
cd extern/
git clone https://github.com/ericchen321/tensorflow_cc.git
cd tensorflow_cc/tensorflow_cc
mkdir build
cd build
# build and install with CUDA
cmake -DTENSORFLOW_STATIC=OFF -DTENSORFLOW_SHARED=ON -DALLOW_CUDA=ON ..
make -j8 -k 2>&1 | tee build.log
sudo make install
cd tensorflow
./bazel-bin/tensorflow/tools/pip_package/build_pip_package ./pip_package_build
cd ../../../../../ # back to AutoDef/
# upgrade pip3
pip3 install --upgrade pip
extern/anaconda/bin/pip install --upgrade extern/tensorflow_cc/tensorflow_cc/build/tensorflow/pip_package_build/tensorflow-1.8.0-cp36-cp36m-linux_x86_64.whl

# Install Keras
extern/anaconda/bin/pip install keras==2.0.8 

# Now I need to set the default in keras
mkdir ~/.keras/
cp ./keras.json ~/.keras/


### Now the rest of the related code
# remove the author's GAUSS
rm -rf extern/GAUSS/
# clone Eric's GAUSS recursively
cd extern/
git clone --recursive https://github.com/ericchen321/GAUSS.git
# Build GAUSS
cd GAUSS/
bash InstallGAUSS_Ubuntu_noqt.sh
cd ../../

# Build Libigl Python bindings
rm -rf extern/libigl/
git clone -b AutoDef --recursive git@github.com:lawsonfulton/libigl-legacy.git extern/libigl
cd extern/libigl/python
mkdir build
cd build
cmake ..
make -j8
cd ../../../../

# Build nlohmann's json
rm -rf extern/json/
git clone --recursive git@github.com:nlohmann/json.git extern/json/
cd extern/json/
git reset --hard 9294e25
# build tests (probably not necessary?)
mkdir build
cd build
cmake ..
cmake --build .
cd ../../../

# Build Cubacode
cd src/cubacode
mkdir build
cd build
cmake ..
make -j8
cd ../../../
