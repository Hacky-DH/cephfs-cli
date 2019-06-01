#!/bin/env python
# cephfs-cli supports: config login upload download delete
# based on cephfstool
# 20180824
# 20181203 install easily, add env 

from __future__ import print_function
import sys
import os
from errno import *
import argparse
import json

__all__ = ["login","get_user_info","config_handler","upload_handler",
    "download_handler","remove_handler","mkdir_handler","pwd_handler",
    "chdir_handler","listdir_handler"]

def add_sys_path(path):
    if os.path.exists(path):
        if path in sys.path:
            sys.path.remove(path)
        sys.path.insert(1, path)

cur_dir = os.path.dirname(os.path.abspath(__file__))
home_dir = os.getenv('CEPH_CLI_HOME', cur_dir)
version = os.getenv('CEPH_CLI_VERSION', '0.0.1')
add_sys_path(os.path.join(home_dir, 'py_packages'))
default_log_dir = os.path.join(home_dir, 'logs/')
default_info_file = os.path.join(home_dir, 'conf', 'user.info')
last_work_dir = os.path.join(home_dir, ".cephcli.last.lwd")

if sys.version_info[0] == 2:
    import codecs
    open = codecs.open

try:
    import cephfstool as tool
except ImportError:
    print("cephcli may be not installed, please reinstall\n" + \
        "Or check which python version you use\n" + \
        "Or contact with administrator", file=sys.stderr)
    sys.exit(EAGAIN)

# -i user_info_file
user_info_file = default_info_file
cephfs_root_dir = None
cephfs_helper = None
verbose = False

def login(cephconf, cephaddr, name=None, key=None, root=None):
    configure = locals()
    if verbose:
        print("login arguments: ", configure)
    if not cephconf and not cephaddr:
        print("cephconf file or cephaddr is required\nconfig -h for help",\
            file=sys.stderr)
        return EINVAL
    global cephfs_helper
    cephfs_helper = tool.CephfsHelper()
    if cephconf:
        cephfs_helper.set_config_file(cephconf)
    if cephaddr:
        cephfs_helper.set_mon_addr(cephaddr)
    ret = cephfs_helper.login(name, key, root)
    if not ret:
        print("user login cephfs failed", file=sys.stderr)
        return EPERM
    try:
        with open(user_info_file, 'w', 'utf-8') as f:
            json.dump(configure, f, encoding='utf-8')
        if verbose:
            print("dump user info {0} to {1}".format(configure, user_info_file))
    except Exception as e:
        print('Warning: unable write configure to file {0} : {1}'\
            .format(user_info_file, e), file=sys.stderr)
    if os.path.exists(last_work_dir):
        with open(last_work_dir, 'r') as f:
            lwd = f.read()
        ret = cephfs_helper.chdir(lwd)
        if not ret:
            print("Warning: unable to change to last work dir " + lwd)
    if verbose:
        print("{0}:{1} login cephfs successfully".format(name if name else 'admin', 
            root if root else '/'))
    return 0

def get_user_info(): 
    try:
        with open(user_info_file, 'rb', 'utf-8') as f:
            #load will give you unicode str, so convert them to ascii
            info = json.load(f, encoding='utf-8', object_hook=ascii_encode_dict)
            if verbose:
                print("read user info {0} from {1}".format(info, user_info_file))
            return info, True
    except Exception as e:
        if verbose:
            print("read user info file error: ", e)
    return None, False

def check(func):
    def wrapper(*args, **kwargs):
        info, ok = get_user_info()
        if not ok:
            print("Not config correctly, please run install.sh firstly",\
                file=sys.stderr)
            return EPERM
        if cephfs_root_dir is not None:
            info["root"] = cephfs_root_dir
        ret = login(**info)
        if ret != 0:
            return ret
        return func(*args, **kwargs)
    return wrapper
    
def config_handler(args):
    #check user_info_file
    info, ok = get_user_info()
    if ok and not args.cephconf and not args.cephaddr:
        print("current user info:")
        print("User: " + info["name"])
        print("Root path: " + info["root"])
        print("use config -h for help to change config")
        return 0
    if not args.cephconf and not args.cephaddr:
        print("-c or -a is required for ceph addr\nconfig -h for help", file=sys.stderr)
        return EINVAL
    configure = {"cephconf":None,"cephaddr":None}
    if args.cephconf:
        configure["cephconf"] = args.cephconf.name
        args.cephconf.close()
    if args.cephaddr:
        configure["cephaddr"] = args.cephaddr
    if args.user:
        configure["name"] = args.user
    if args.keyfile:
        configure["key"] = args.keyfile.read()
        args.keyfile.close()
    if cephfs_root_dir:
        configure["root"] = cephfs_root_dir
    elif args.user:
        configure["root"] = "/" + args.user
    if verbose:
        print("config: ", configure)
    ret = login(**configure)
    if ret != 0:
        return ret
    print("config cephfs successfully\nYou can run upload or download command etc.")
    return 0

def ascii_encode_dict(data):
    ascii_encode = lambda x: x.encode('ascii') if isinstance(x, unicode) else x 
    return dict(map(ascii_encode, pair) for pair in data.items())

@check
def upload_handler(args):
    src_path, cephfs_path = args.src_path, args.cephfs_path
    if verbose:
        print('upload arguments: ', src_path, cephfs_path)
    for src in src_path:
        dst_path = cephfs_path
        if not os.path.exists(src):
            print("upload local path [{0}] No such file or directory".format(src),\
                file=sys.stderr)
            continue
        elif os.path.isfile(src):
            if dst_path == '.' or dst_path == '..':
                dst_path += '/'
            if dst_path[-1] == '/':
                dst_path = dst_path + os.path.basename(src)
        elif os.path.isdir(src):
            ret = cephfs_helper.stat(dst_path)
            if ret == 0: # file
                print("upload local dir [{0}] to cephfs exist file [{1}] is not allowed"\
                    .format(src, dst_path), file=sys.stderr)
                return EPERM
            else:
                if src[-1] == '/':
                    src = src[:-1]
                dirname = os.path.basename(src)
                dst_path = os.path.join(dst_path, dirname)
                if dst_path[-1] != '/':
                    dst_path += '/' 
        ret = cephfs_helper.write_tree(dst_path, src)
        if not ret:
            print("upload [{0}] failed".format(src), file=sys.stderr)
            return EPERM
        else:
            print("upload local path [{0}] to cephfs path [{1}] successfully".
                format(src, dst_path))
    return 0

@check
def download_handler(args):
    dst_path, cephfs_path = args.dst_path, args.cephfs_path
    if verbose:
        print('download arguments: ', cephfs_path, dst_path)
    ppath = os.path.dirname(dst_path)
    if len(ppath)>0 and not os.path.exists(ppath):
        os.makedirs(ppath)
    ret = cephfs_helper.stat(cephfs_path)
    if ret == -1:
        print("download path [{0}] No such file or directory".format(cephfs_path),\
            file=sys.stderr)
        return ENOENT
    elif ret == 0: # file
        if os.path.isdir(dst_path):
            dst_path = os.path.join(dst_path, os.path.basename(cephfs_path))
    elif ret == 1:
        print("download directory [{0}] is not supported".format(cephfs_path),\
            file=sys.stderr)
        return EPERM
    ret = cephfs_helper.read(cephfs_path, dst_path)
    if not ret:
        print("download [{0}] failed".format(cephfs_path), file=sys.stderr)
        return EPERM
    else:
        print("download to local path [{0}] from cephfs path [{1}] successfully".
            format(dst_path, cephfs_path))
    return 0

@check
def remove_handler(args):
    cephfs_path = args.cephfs_path
    if verbose:
        print('remove arguments: ', cephfs_path)
    for src in cephfs_path:
        st = cephfs_helper.stat(src)
        if st == 0:
            ret = cephfs_helper.remove(src)
        elif st == 1:
            ret = cephfs_helper.rmdir(src)
        else:
            print("remove path [{0}] No such file or directory".format(src),\
                file=sys.stderr)
            continue
        if not ret:
            print("remove path [{0}] failed".format(src), file=sys.stderr)
            return EPERM
        else:
            print("remove cephfs path [{0}] successfully".format(src))
    return 0

@check
def pwd_handler(args):
    print(cephfs_helper.getcwd())
    return 0

@check
def mkdir_handler(args):
    cephfs_path = args.cephfs_path
    if verbose:
        print('mkdir arguments: ', cephfs_path)
    for src in cephfs_path:
        if src[-1] != '/':
            src += '/'
        ret = cephfs_helper.get_safe_path(src)
        if not ret:
            print("mkdir path [{0}] failed".format(src), file=sys.stderr)
            return EPERM
        print("mkdir path [{0}] successfully".format(src))
    return 0

@check
def chdir_handler(args):
    cephfs_path = args.cephfs_path
    if verbose:
        print('chdir arguments: ', cephfs_path)
    if cephfs_path[-1] != '/':
        cephfs_path += '/'
    ret = cephfs_helper.chdir(cephfs_path)
    if not ret:
        print("chdir path [{0}] failed".format(cephfs_path), file=sys.stderr)
        return EPERM
    with open(last_work_dir, 'w') as f:
        f.write(cephfs_helper.getcwd())
    print("chdir path [{0}] successfully".format(cephfs_path))
    return 0

@check
def listdir_handler(args):
    cephfs_path = args.cephfs_path
    if verbose:
        print('listdir arguments: ', cephfs_path)
    if cephfs_path is None:
        cephfs_path = "./"
    ls = tool.StringVector()
    ret = cephfs_helper.listdir_buffer(cephfs_path, ls)
    if not ret:
        print("listdir path [{0}] failed".format(cephfs_path), file=sys.stderr)
        return EPERM
    if len(ls):
        for l in ls:
            print(l,end=' ')
        print()
    else:
        print("empty directory")
    return 0

def parse_cmdargs(args=None):
    parser = argparse.ArgumentParser(description='cephfs client tool')
    parser.add_argument('-v', '--version', action="store_true", help="display version")
    parser.add_argument('--verbose', '-vv', action="store_true", help="show verbose")
    parser.add_argument('-i', '--userfile', help='user info file',
        default=default_info_file)
    parser.add_argument('-r', '--root', help='root path in cephfs')
    sub = parser.add_subparsers(title='support subcommands')
    sub.required = False

    config = sub.add_parser('config', help='config cephfs and authentication')
    config.add_argument('-c', '--conf', dest='cephconf',
                        type=argparse.FileType('r'),
                        help='ceph configuration file')
    config.add_argument('-a', '--cephaddr', help='ceph mon addr')
    config.add_argument('-n', '--name', dest='user',
                        help='client name for authentication')
    config.add_argument('-k', '--keyfile', type=argparse.FileType('r'),
                        help='keyfile for authentication')
    config.set_defaults(func=config_handler)

    upload = sub.add_parser('upload', help='upload files to cephfs')
    upload.add_argument('src_path', help='local source path', nargs='+')
    upload.add_argument('cephfs_path', help='dst path in cephfs')
    upload.set_defaults(func=upload_handler)

    download = sub.add_parser('download', help='download files from cephfs')
    download.add_argument('cephfs_path', help='source path in cephfs')
    download.add_argument('dst_path', help='local dst path')
    download.set_defaults(func=download_handler)
    
    remove = sub.add_parser('remove', help='remove files from cephfs')
    remove.add_argument('cephfs_path', help='path in cephfs', nargs='+')
    remove.set_defaults(func=remove_handler)

    pwd = sub.add_parser('pwd', help='print working directory')
    pwd.set_defaults(func=pwd_handler)

    mkdir = sub.add_parser('mkdir', help='make directory')
    mkdir.add_argument('cephfs_path', help='path in cephfs', nargs='+')
    mkdir.set_defaults(func=mkdir_handler)

    chdir = sub.add_parser('cd', help='change directory')
    chdir.add_argument('cephfs_path', help='path in cephfs')
    chdir.set_defaults(func=chdir_handler)

    listdir = sub.add_parser('ls', help='list directory')
    listdir.add_argument('cephfs_path', help='path in cephfs',
        nargs='?')
    listdir.set_defaults(func=listdir_handler)

    parsed_args = parser.parse_args(args)
    return parser, parsed_args

def main():
    if len(sys.argv)>1 and "-v" in sys.argv: 
        print('cephcli', version + tool.version())
        return 0
    parser, parsed_args = parse_cmdargs()
    if parsed_args.verbose:
        global verbose
        verbose = parsed_args.verbose
    try:
        os.mkdir(default_log_dir)
    except OSError:
        pass
    tool.set_log_dir(default_log_dir)
    if verbose:
        print('log to path', default_log_dir)
    if parsed_args.userfile:
        global user_info_file 
        user_info_file = parsed_args.userfile
    global cephfs_root_dir
    if parsed_args.root:
        cephfs_root_dir = parsed_args.root if parsed_args.root[0]=='/' \
            else '/' + parsed_args.root
    else:
        cephfs_root_dir = None
    return parsed_args.func(parsed_args)

if __name__ == "__main__":
    sys.exit(main())

