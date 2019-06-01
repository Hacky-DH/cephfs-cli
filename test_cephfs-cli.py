import pytest
import os
import sys
from errno import *
import re
import json

cephfs_cli = pytest.importorskip("cephfs-cli")
addr = '172.28.218.70,172.28.160.165,172.28.217.100'
key = 'AQBNzwhc8ru/IBAAef0NABDSntpt5Q8TQp4AWw=='
user = 'test_cephfs_user'
root = '/pytestdir/test_cephfs_user'

@pytest.yield_fixture(scope='function')
def suit():
    try:
        os.remove(cephfs_cli.last_work_dir)
    except OSError:
        pass
    sys.argv = ["cephfs_cli_test"]
    yield

def test_zero(suit, capsys):
    with pytest.raises(SystemExit) as err:
        cephfs_cli.main()
    assert 2 == err.value.code
    _, err = capsys.readouterr()
    assert "error: too few arguments" in err, err

def test_version(suit, capsys):
    sys.argv.append("-v")
    assert 0 == cephfs_cli.main()
    out, _ = capsys.readouterr()
    assert "0.0.1.2018" in out, out

def test_config_zero(suit, capsys):
    sys.argv.extend(["-vv","config"])
    assert 0 == cephfs_cli.main()
    out, err = capsys.readouterr()
    assert "current user info" in out
    assert not err

def test_config_not_exist_file(suit, capsys):
    sys.argv.extend(["-vv","config","-c", "/tmp/notexist/conf/ceph.conf"])
    with pytest.raises(SystemExit) as err:
        cephfs_cli.main()
    assert 2 == err.value.code
    out, err = capsys.readouterr()
    assert "error: argument -c/--conf: can't open '/tmp/notexist/conf/ceph.conf': " + \
        "[Errno 2] No such file or directory: '/tmp/notexist/conf/ceph.conf'" in err, err

def test_config_invalid_file(suit, capfd, tmpdir):
    conf = tmpdir.mkdir("conf").join("ceph.conf")
    conf.write("ceph invalid conf")
    sys.argv.extend(["-vv","config","-c",str(conf)])
    assert EPERM == cephfs_cli.main()
    conf.remove()
    _, err = capfd.readouterr()
    assert "no monitors specified to connect to" in err, err
    assert "user login cephfs failed" in err, err

def test_config_valid_file_no_key(suit, capfd, tmpdir):
    conf = tmpdir.mkdir("conf").join("ceph.conf")
    conf.write("[global]\nmon host = {}\n".format(addr))
    sys.argv.extend(["-vv","config","-c",str(conf)])
    assert EPERM == cephfs_cli.main()
    conf.remove()
    out, err = capfd.readouterr()
    #assert "[ERROR] Unable to open cephfs : (95) Operation not supported" in out, out
    assert "user login cephfs failed" in err, err

def test_config_file_admin_with_key_file(suit, capfd, tmpdir):
    conf = tmpdir.mkdir("conf").join("ceph.conf")
    k = tmpdir.join("conf").join("key")
    k.write(key)
    conf.write("[global]\nmon host = {}\nkeyfile = {}".format(addr, str(k)))
    sys.argv.extend(["-vv","config","-c",str(conf)])
    assert 0 == cephfs_cli.main()
    conf.remove()
    key.remove()
    out, _ = capfd.readouterr()
    assert "connect cephfs admin:/ successfully" in out, out
    assert "admin:/ login cephfs successfully" in out, out

def test_config_file_user_with_key_file(suit, capfd, tmpdir):
    conf = tmpdir.mkdir("conf").join("ceph.conf")
    key = tmpdir.join("conf").join("key")
    key.write("AQCiNnVbMEXFChAAdbI90BUZMGlDCeVl9QvPNA==")
    conf.write("[global]\nmon host = 172.28.217.102,172.18.178.106\nkeyfile = " + \
        str(key)+"\n")
    sys.argv.extend(["-vv","config","-c",str(conf),"-n","test_cephfs_user"])
    assert 0 == cephfs_cli.main()
    conf.remove()
    key.remove()
    out, _ = capfd.readouterr()
    assert "connect cephfs test_cephfs_user:/test_cephfs_user successfully" in out, out
    assert "test_cephfs_user:/test_cephfs_user login cephfs successfully" in out, out

def test_config_file_user_with_key_file_with_root(suit, capfd, tmpdir):
    conf = tmpdir.mkdir("conf").join("ceph.conf")
    key = tmpdir.join("conf").join("key")
    key.write("AQCiNnVbMEXFChAAdbI90BUZMGlDCeVl9QvPNA==")
    conf.write("[global]\nmon host = 172.28.217.102,172.18.178.106\nkeyfile = " + \
        str(key)+"\n")
    sys.argv.extend(["-vv","-r","/test_cephfs_user","config","-c",str(conf),\
        "-n","test_cephfs_user"])
    assert 0 == cephfs_cli.main()
    out, _ = capfd.readouterr()
    assert "connect cephfs test_cephfs_user:/test_cephfs_user successfully" in out, out
    assert "test_cephfs_user:/test_cephfs_user login cephfs successfully" in out, out

def test_config_file_user_with_key_file_with_invalid_root(suit, capfd, tmpdir):
    conf = tmpdir.mkdir("conf").join("ceph.conf")
    key = tmpdir.join("conf").join("key")
    key.write("AQCiNnVbMEXFChAAdbI90BUZMGlDCeVl9QvPNA==")
    conf.write("[global]\nmon host = 172.28.217.102,172.18.178.106\nkeyfile = " + \
        str(key)+"\n")
    sys.argv.extend(["-vv","-r","/invalid_root","config","-c",str(conf),\
        "-n","test_cephfs_user"])
    assert EPERM == cephfs_cli.main()
    out, err = capfd.readouterr()
    conf.remove()
    key.remove()
    assert "[ERROR] Unable to open cephfs /invalid_root: (1) " + \
        "Operation not permitted" in out, out
    assert "user login cephfs failed" in err, err

def test_config_args_user_a_no_key(suit, capfd):
    sys.argv.extend(["-vv","-r","/test_cephfs_user","config","-a",
        "172.28.217.102,172.18.178.106","-n",
        "test_cephfs_user"])
    assert EPERM == cephfs_cli.main()
    out, err = capfd.readouterr()
    assert "[ERROR] Unable to open cephfs /test_cephfs_user: (95) " +\
        "Operation not supported" in out, out
    assert "user login cephfs failed" in err, err

def test_config_args_user_a_with_key(suit, capfd, tmpdir):
    key = tmpdir.mkdir("conf").join("key")
    key.write("AQCiNnVbMEXFChAAdbI90BUZMGlDCeVl9QvPNA==")
    sys.argv.extend(["-vv","config","-a","172.28.217.102,172.18.178.106","-n",\
        "test_cephfs_user","-k",str(key)])
    assert 0 == cephfs_cli.main()
    out, _ = capfd.readouterr()
    key.remove()
    assert "connect cephfs test_cephfs_user:/test_cephfs_user successfully" in out, out
    assert "test_cephfs_user:/test_cephfs_user login cephfs successfully" in out, out

def test_config_args_user_a_with_invalid_key(suit, capfd, tmpdir):
    key = tmpdir.mkdir("conf").join("key")
    key.write("invalid key")
    sys.argv.extend(["-vv","config","-a","172.28.217.102,172.18.178.106","-n",\
        "test_cephfs_user","-k",str(key)])
    assert EPERM == cephfs_cli.main()
    out, err = capfd.readouterr()
    key.remove()
    assert "[ERROR] Unable to open cephfs /test_cephfs_user: (22) " + \
        "Invalid argument" in out, out
    assert "user login cephfs failed" in err, err

def test_config_args_user_a_with_invalid_root(suit, capfd, tmpdir):
    key = tmpdir.mkdir("conf").join("key")
    key.write("AQCiNnVbMEXFChAAdbI90BUZMGlDCeVl9QvPNA==")
    sys.argv.extend(["-vv","-r","/invalid_root","config","-a",\
        "172.28.217.102,172.18.178.106","-n",\
        "test_cephfs_user","-k",str(key)])
    assert EPERM == cephfs_cli.main()
    out, err = capfd.readouterr()
    assert "[ERROR] Unable to open cephfs /invalid_root: (1) " + \
        "Operation not permitted" in out, out
    assert "user login cephfs failed" in err, err

def test_config_args_user_a_with_group(suit, capfd, tmpdir):
    key = tmpdir.mkdir("conf").join("key")
    key.write("AQCiNnVbMEXFChAAdbI90BUZMGlDCeVl9QvPNA==")
    sys.argv.extend(["-vv","-r","/test_group","config","-a",\
        "172.28.217.102,172.18.178.106","-n",\
        "test_cephfs_user","-k",str(key)])
    assert 0 == cephfs_cli.main()
    key.remove()
    out, _ = capfd.readouterr()
    assert "connect cephfs test_cephfs_user:/test_group successfully" in out, out
    assert "test_cephfs_user:/test_group login cephfs successfully" in out, out

def test_user_info_file_config(suit, capfd, tmpdir):
    key = tmpdir.mkdir("conf").join("key")
    info = tmpdir.join("conf").join("user.info")
    key.write("AQCiNnVbMEXFChAAdbI90BUZMGlDCeVl9QvPNA==")
    sys.argv.extend(["-vv","-i",str(info),"config","-a",\
        "172.28.217.102,172.18.178.106","-n",\
        "test_cephfs_user","-k",str(key)])
    assert 0 == cephfs_cli.main()
    out, _ = capfd.readouterr()
    assert "connect cephfs test_cephfs_user:/test_cephfs_user successfully" in out, out
    assert "test_cephfs_user:/test_cephfs_user login cephfs successfully" in out, out
    assert os.path.exists(str(info))
    #load user info
    sys.argv = ["cephfs_cli_test","-vv","-i",str(info),"config"]
    assert 0 == cephfs_cli.main()
    out, _ = capfd.readouterr()
    assert "Root path: /test_cephfs_user" in out, out
    key.remove()
    info.remove()

info = "/tmp/test_user.info"
test_dir = "/pytest_dir/"
test_file = "/pytest_file"

@pytest.fixture(scope='function')
def config(suit, tmpdir):
    with open(info,'w') as f:
        data = {"cephconf": None, "root": "/test_cephfs_user",
            "name": "test_cephfs_user",
            "key": "AQCiNnVbMEXFChAAdbI90BUZMGlDCeVl9QvPNA==",
            "cephaddr": "172.28.217.102,172.18.178.106"
        }
        json.dump(data, f)
    assert os.path.exists(info)
    sys.argv = ["cephfs_cli_test"]

def test_upload_zero(suit, capsys):
    sys.argv.extend(["-vv","upload"])
    with pytest.raises(SystemExit) as err:
        cephfs_cli.main()
    assert 2 == err.value.code
    _, err = capsys.readouterr()
    assert "error: too few arguments" in err, err

def test_upload_one_arg(suit, capsys):
    sys.argv.extend(["-vv","-i",info,"upload","src"])
    with pytest.raises(SystemExit) as err:
        cephfs_cli.main()
    assert 2 == err.value.code
    _, err = capsys.readouterr()
    assert "error: too few arguments" in err, err

def test_upload_one_not_exist_file(suit, capfd):
    sys.argv.extend(["-vv","-i",info,"upload","noexistfile",test_file])
    assert 0 == cephfs_cli.main()
    _, err = capfd.readouterr()
    assert "upload local path [noexistfile] No such file or directory" in err, err

def remove(f, capfd):
    sys.argv = ["cephfs_cli_test","-i",info,"remove",f]
    assert 0 == cephfs_cli.main()
    capfd.readouterr()

def test_upload_file_to_file(config, capfd, tmpdir):
    src = tmpdir.join("src_file")
    src.write("hello string from pytest")
    sys.argv.extend(["-vv","-i",info,"upload",str(src),test_file])
    assert 0 == cephfs_cli.main()
    src.remove()
    out, err = capfd.readouterr()
    assert re.search(r"upload local path \[.*src_file\] " +\
        "to cephfs path \[/pytest_file\] successfully",out), out
    assert len(err) == 0
    remove(test_file, capfd)

def test_upload_file_to_dir(config, capfd, tmpdir):
    src = tmpdir.join("src_file")
    src.write("hello string from pytest")
    sys.argv.extend(["-vv","-i",info,"upload",str(src),test_dir])
    assert 0 == cephfs_cli.main()
    src.remove()
    out, err = capfd.readouterr()
    assert re.search(r"upload local path \[.*src_file\] " +\
        "to cephfs path \[/pytest_dir/src_file\] successfully",out), out
    assert len(err) == 0
    remove(test_dir, capfd)

def test_upload_dir_to_dir(config, capfd, tmpdir):
    src = tmpdir.mkdir("folder").join("src_file")
    src.write("hello string from pytest")
    sys.argv.extend(["-vv","-i",info,"upload",src.dirname,test_dir])
    assert 0 == cephfs_cli.main()
    src.remove()
    out, err = capfd.readouterr()
    assert re.search(r"upload local path \[.*folder\] " +\
        "to cephfs path \[/pytest_dir/\] successfully",out), out
    assert len(err) == 0
    remove(test_dir, capfd)

def test_upload_dir_to_file(config, capfd, tmpdir):
    src = tmpdir.mkdir("folder").join("src_file")
    src.write("hello string from pytest")
    #upload a file
    sys.argv = ["cephfs_cli_test","-i",info,"upload",str(src),test_file]
    assert 0 == cephfs_cli.main()
    capfd.readouterr()
    #upload dir to a exist file
    sys.argv = ["cephfs_cli_test","-i",info,"upload",src.dirname,test_file]
    assert EPERM == cephfs_cli.main()
    src.remove()
    _, err = capfd.readouterr()
    assert "to cephfs exist file [/pytest_file] is not allowed" in err, err
    remove(test_file, capfd)

def test_upload_multi_file(config, capfd, tmpdir):
    src = tmpdir.join("src_file")
    src.write("hello string from pytest")
    src2 = tmpdir.join("src_file2")
    src2.write("hello string from pytest again")
    sys.argv.extend(["-vv","-i",info,"upload",str(src),str(src2),test_dir])
    assert 0 == cephfs_cli.main()
    src.remove()
    src2.remove()
    out, err = capfd.readouterr()
    assert re.search(r"upload local path \[.*src_file\] " +\
        "to cephfs path \[/pytest_dir/src_file\] successfully",out), out
    assert re.search(r"upload local path \[.*src_file2\] " +\
        "to cephfs path \[/pytest_dir/src_file\] successfully",out), out
    assert len(err) == 0
    remove(test_dir, capfd)

def test_upload_one_file_with_invalid_group(config, capfd, tmpdir):
    src = tmpdir.join("src_file")
    src.write("hello string from pytest")
    sys.argv.extend(["-vv","-i",info,"-r","test_invalid","upload",str(src),test_file])
    assert EPERM == cephfs_cli.main()
    src.remove()
    out, err = capfd.readouterr()
    assert "[ERROR] Unable to open cephfs /test_invalid: (1) " + \
        "Operation not permitted" in out
    assert "user login cephfs failed" in err

def test_upload_one_file_with_group(config, capfd, tmpdir):
    src = tmpdir.join("src_file")
    src.write("hello string from pytest")
    sys.argv.extend(["-vv","-i",info,"-r","test_group","upload",str(src),test_file])
    assert 0 == cephfs_cli.main()
    src.remove()
    out, err = capfd.readouterr()
    assert re.search(r"upload local path \[.*src_file\] " +\
        "to cephfs path \[/pytest_file\] successfully",out), out
    assert len(err) == 0
    remove(test_file, capfd)

def test_download_zero(suit, capsys):
    sys.argv.extend(["-vv","download"])
    with pytest.raises(SystemExit) as err:
        cephfs_cli.main()
    assert 2 == err.value.code
    _, err = capsys.readouterr()
    assert "error: too few arguments" in err, err

def test_download_one_arg(suit, capsys):
    sys.argv.extend(["-vv","download","abc"])
    with pytest.raises(SystemExit) as err:
        cephfs_cli.main()
    assert 2 == err.value.code
    _, err = capsys.readouterr()
    assert "error: too few arguments" in err, err

def test_download_not_exist_file(config, capfd):
    sys.argv.extend(["-i",info,"download","abc","local_file"])
    assert ENOENT == cephfs_cli.main()
    _, err = capfd.readouterr()
    assert "download path [abc] No such file or directory" in err, err

def upload(capfd, tmpdir, dst = test_dir):
    #upload a file
    src = tmpdir.join("src_file")
    src.write("hello string from pytest")
    sys.argv = ["cephfs_cli_test","-i",info,"upload",str(src),dst]
    assert 0 == cephfs_cli.main()
    capfd.readouterr()

def test_download_file_to_file(config, capfd, tmpdir):
    upload(capfd, tmpdir)
    sys.argv = ["cephfs_cli_test","-i",info,"download",test_dir+"src_file","local_file"]
    assert 0 == cephfs_cli.main()
    out, err = capfd.readouterr()
    assert "download to local path [local_file] from cephfs path " +\
        "[/pytest_dir/src_file] successfully" in out, out
    assert len(err) == 0 

def test_download_file_to_dir(config, capfd, tmpdir):
    upload(capfd, tmpdir)
    os.mkdir("local_dir")
    sys.argv = ["cephfs_cli_test","-i",info,"download",test_dir+"src_file","local_dir"]
    assert 0 == cephfs_cli.main()
    out, err = capfd.readouterr()
    assert "download to local path [local_dir/src_file] from cephfs path " +\
        "[/pytest_dir/src_file] successfully" in out, out
    assert len(err) == 0
    import shutil
    shutil.rmtree("local_dir")

def test_download_file_to_dir2(config, capfd, tmpdir):
    upload(capfd, tmpdir)
    sys.argv = ["cephfs_cli_test","-i",info,"download",test_dir+"src_file",
        "local_dir/dir2/"]
    assert 0 == cephfs_cli.main()
    out, err = capfd.readouterr()
    assert "download to local path [local_dir/dir2/src_file] from cephfs path " +\
        "[/pytest_dir/src_file] successfully" in out, out
    assert len(err) == 0
    import shutil
    shutil.rmtree("local_dir")

def test_download_dir_to_file(config, capfd, tmpdir):
    upload(capfd, tmpdir)
    sys.argv = ["cephfs_cli_test","-i",info,"download",test_dir,"local_file"]
    assert EPERM == cephfs_cli.main()
    _, err = capfd.readouterr()
    assert "download directory [/pytest_dir/] is not supported" in err, err

def test_download_dir_to_dir(config, capfd, tmpdir):
    upload(capfd, tmpdir)
    sys.argv = ["cephfs_cli_test","-i",info,"download",test_dir,"local_dir/dir2/"]
    assert EPERM == cephfs_cli.main()
    _, err = capfd.readouterr()
    assert "download directory [/pytest_dir/] is not supported" in err, err
    import shutil
    shutil.rmtree("local_dir")

def test_remove_zero(suit, capsys):
    sys.argv.extend(["-vv","remove"])
    with pytest.raises(SystemExit) as err:
        cephfs_cli.main()
    assert 2 == err.value.code
    _, err = capsys.readouterr()
    assert "error: too few arguments" in err, err

def test_remove_multi(config, capfd, tmpdir):
    upload(capfd, tmpdir, "/test_file1")
    upload(capfd, tmpdir, "/test_file2")
    sys.argv = ["cephfs_cli_test","-i",info,"remove","/test_file1","/test_file2"]
    assert 0 ==cephfs_cli.main()
    out, err = capfd.readouterr()
    assert "remove cephfs path [/test_file1] successfully" in out, out
    assert "remove cephfs path [/test_file2] successfully" in out, out
    assert len(err) == 0

def test_pwd(config, capfd):
    sys.argv.extend(["-i",info,"pwd"])
    assert 0 == cephfs_cli.main()
    out, _ = capfd.readouterr()
    assert "/" in out, out

def test_mkdir_zero(config, capsys):
    sys.argv.extend(["-i",info,"mkdir"])
    with pytest.raises(SystemExit) as err:
        cephfs_cli.main()
    assert 2 == err.value.code
    _, err = capsys.readouterr()
    assert "error: too few arguments" in err, err

def test_mkdir_one(config, capfd):
    sys.argv.extend(["-i",info,"mkdir",test_dir])
    assert 0 == cephfs_cli.main()
    out, _ = capfd.readouterr()
    assert "mkdir path [/pytest_dir/] successfully" in out
    # remove dir
    sys.argv = ["cephfs_cli_test","-i",info,"remove",test_dir]
    assert 0 == cephfs_cli.main()
    out, _ = capfd.readouterr()
    assert "remove cephfs path [/pytest_dir/] successfully" in out, out

def test_mkdir_multi(config, capfd):
    sys.argv.extend(["-i",info,"mkdir",test_dir,"/other_dir"])
    assert 0 == cephfs_cli.main()
    out, _ = capfd.readouterr()
    assert "mkdir path [/pytest_dir/] successfully" in out
    assert "mkdir path [/other_dir/] successfully" in out
    # remove dirs
    sys.argv = ["cephfs_cli_test","-i",info,"remove",test_dir,"/other_dir"]
    assert 0 == cephfs_cli.main()
    out, _ = capfd.readouterr()
    assert "remove cephfs path [/pytest_dir/] successfully" in out, out
    assert "remove cephfs path [/other_dir] successfully" in out, out

def test_chdir_zero(suit, capsys):
    sys.argv = ["cephfs_cli_test","-i",info,"cd"]
    with pytest.raises(SystemExit) as err:
        cephfs_cli.main()
    assert 2 == err.value.code
    _, err = capsys.readouterr()
    assert "error: too few arguments" in err, err

def test_chdir(config, capfd):
    sys.argv.extend(["-i",info,"mkdir",test_dir])
    assert 0 == cephfs_cli.main()
    capfd.readouterr()

    sys.argv = ["cephfs_cli_test","-i",info,"pwd"]
    assert 0 == cephfs_cli.main()
    capfd.readouterr()

    sys.argv = ["cephfs_cli_test","-i",info,"cd",test_dir]
    assert 0 == cephfs_cli.main()
    out, _ = capfd.readouterr()
    assert "chdir path [/pytest_dir/] successfully" in out, out

    sys.argv = ["cephfs_cli_test","-i",info,"pwd"]
    assert 0 == cephfs_cli.main()
    out, _ = capfd.readouterr()
    assert test_dir[0:-1] in out, out

    sys.argv = ["cephfs_cli_test","-i",info,"remove",test_dir]
    assert 0 == cephfs_cli.main()
    capfd.readouterr()

def test_listdir_zero(config, capfd):
    sys.argv = ["cephfs_cli_test","-i",info,"ls"]
    assert 0 == cephfs_cli.main()
    capfd.readouterr()

def test_listdir_empty(config, capfd):
    sys.argv = ["cephfs_cli_test","-i",info,"mkdir",test_dir]
    assert 0 == cephfs_cli.main()
    capfd.readouterr()

    sys.argv = ["cephfs_cli_test","-i",info,"ls",test_dir]
    assert 0 == cephfs_cli.main()
    out, err = capfd.readouterr()
    assert "empty directory" in out
    assert len(err) == 0
    remove(test_dir, capfd)

def test_listdir(config, capfd, tmpdir):
    upload(capfd, tmpdir)

    sys.argv = ["cephfs_cli_test","-i",info,"ls",test_dir]
    assert 0 == cephfs_cli.main()
    out, err = capfd.readouterr()
    assert "src_file" in out
    assert len(err) == 0
    remove(test_dir, capfd)
    
