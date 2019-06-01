#/bin/bash
user=$1
[[ -z $user ]] && { echo "ERROR: Need user name, try your erp name"; exit 1; }

cwd=$(cd $(dirname $0); pwd)
pushd $cwd > /dev/null

sed -i "s#@CEPH_CLI_HOME@#$cwd#" bin/cephcli

if [[ $(whoami) == "root" ]];then
    ln -rsf bin/* /usr/bin/
else
    mkdir -p $HOME/bin
    ln -rsf bin/* $HOME/bin/
fi

wget http://$url/$user -qO conf/user.info
[[ $? -ne 0 ]] && { echo "ERROR: $user NOT found, please apply user name";  exit 1; }

popd > /dev/null
echo "install cephcli DONE"

