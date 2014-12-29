VERSION=1.12
autoreconf
./configure
make distclean
tar cavf swapspace-$VERSION.tar.gz --exclude-vcs .
gpg --sign swapspace-$VERSION.tar.gz
