# we're at: /Users/hagen/data/luametatex
#
# we assume this has been done: 
#
# git clone https://github.com/contextgarden/luametatex
# cd luametatex
# git checkout work

mkdir -p luametatex
mkdir -p texbinaries

cd luametatex

git checkout .
git reset
git pull

chmod 755 ./build.sh

./build.sh osx-arm
./build.sh osx-intel
./build.sh osx-universal

cp build/osx-arm/luametatex   ../texbinaries/texmf-osx-arm64
cp build/osx-intel/luametatex ../texbinaries/texmf-osx-64
cp build/osx/luametatex       ../texbinaries/texmf-osx-universal