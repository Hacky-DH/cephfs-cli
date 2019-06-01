# cephfs-cli

cephfs client for upload and download large files and directory tree.

# require
g++ >= 4.8.5

# make rpm
```
bash make-rpm.sh
```

# help
```
usage: cephfs-cli.py [-h] [-v] [--verbose] [-i USERFILE] [-r ROOT]
                     {config,upload,download,remove,pwd,mkdir,cd,ls} ...

cephfs client tool

optional arguments:
  -h, --help            show this help message and exit
  -v, --version         display version
  --verbose, -V         show verbose
  -i USERFILE, --userfile USERFILE
                        user info file
  -r ROOT, --root ROOT  root path in cephfs

support subcommands:
  {config,upload,download,remove,pwd,mkdir,cd,ls}
    config              config cephfs and authentication
    upload              upload files to cephfs
    download            download files from cephfs
    remove              remove files from cephfs
    pwd                 print working directory
    mkdir               make directory
    cd                  change directory
    ls                  list directory
```

