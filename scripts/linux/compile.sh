# we're at: /home/hagen/data/luametatex
#
# we assume this has been done: 
#
# git clone https://github.com/contextgarden/luametatex
# cd luametatex
# git checkout work
#
# git config pull.rebase true 

mkdir -p luametatex
mkdir -p texbinaries

cd luametatex

git checkout .
git reset
git pull

chmod 755 ./build.sh

./build.sh

cp build/native/luametatex   ../texbinaries/texmf-linux-64