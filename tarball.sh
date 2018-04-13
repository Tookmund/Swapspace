VERSION=1.12
autoreconf
./configure
make distclean
tar cavf swapspace-$VERSION.tar.gz --exclude-vcs .
echo "Signing swapspace-$VERSION.tar.gz"
gpg --sign swapspace-$VERSION.tar.gz
