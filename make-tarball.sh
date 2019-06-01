#/bin/bash
set -e

version=$1
[ -z $version ] && version="0.0.4"

echo "cephcli version $version"

cwd=$(cd $(dirname $0); pwd)
pushd $cwd > /dev/null

#build
mkdir -p build
pushd build > /dev/null
cmake -DCMAKE_INSTALL_PREFIX=/cephcli ..
make
test/cephfstooltest --gtest_throw_on_failure
popd > /dev/null

#install 
buildroot=tarball_build
/bin/rm -fr $buildroot
mkdir -p $buildroot
pushd build > /dev/null
make DESTDIR=$cwd/$buildroot install
popd > /dev/null
rootdir=$cwd/$buildroot/cephcli
/bin/cp -af bin cephfs-cli.py lib install.sh $rootdir
pushd $rootdir > /dev/null

/bin/rm -fr lib/libcephfstool.a include sample
/bin/rm -fr bin/.*swp conf/.*swp
/bin/chmod 0755 bin/* install.sh
/bin/mkdir conf
/bin/touch conf/user.info
sed -i "s#@CEPH_CLI_VERSION@#$version#" bin/cephcli

libs=(libceph-common.so.0 libcephfs.so.2.0.0 libibverbs.so.1.1.15 libnl-3.so.200.23.0 libnl-route-3.so.200.23.0)
alibs=(libceph-common.so libcephfs.so.2 libibverbs.so.1 libnl-3.so.200 libnl-route-3.so.200)
for(( i=0;i<${#libs[@]};i++)); do
    ln -srf lib/${libs[i]} lib/${alibs[i]}
done
popd > /dev/null

tarball="$cwd/cephcli-$version.tar.gz"
pushd $cwd/$buildroot > /dev/null
tar -czf $tarball cephcli
popd > /dev/null
/bin/rm -fr $cwd/$buildroot
#sshpass -p user scp $tarball url || true

echo "build $tarball done"

