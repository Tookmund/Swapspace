# Swapspace release process
1. TEST! See below.
2. Update version number in NEWS with new features and changes.
3. Commit just the NEWS file and the configure.ac version bump as "Version x.xx"
4. Tag the previous commit as "vx.xx" and sign it using: `git tag -s vx.xx`
	Include the changelog entries in the tag message

## Swapspace test scenarios

This list contains all the tests that should be run before a release
of Swapspace, to avoid situations like the 1.15-1.16 release snafu.

Make sure to bump the version number and `make distcheck`,
then move that tarball to the test machine.

### Scenarios
1. `hog` should be used to trigger swapspace to eventually create a swapfile.

Eventually, I would like to test more scenarios with more complexity,
but this would all need to be automated first.

Make sure to run `swapspace -e` between tests!

### Filesystems
Use `blkid` to check which partition is which.

1. btrfs (first thing to test, since it requires more complex support)
	1. Set compression flag set on the swapfile directory
	2. Unset NOCOW flag on the swapfile directory
2. ext4
3. xfs
4. f2fs
