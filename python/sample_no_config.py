#!/bin/env python
import sys
import cephfstool as tool

tool.set_log_dir(None)
fs = tool.CephfsHelper()
fs.set_mon_addr("172.28.217.102,172.18.178.106")
key = "AQCiNnVbMEXFChAAdbI90BUZMGlDCeVl9QvPNA=="
ret = fs.login("test_cephfs_user",key,"/test_cephfs_user")
if not ret: sys.exit(1)
print("user: " + fs.get_user())
print("root: " + fs.get_root())

ret = fs.write_str("/hello","hello from python")
if not ret: sys.exit(1)
#write to testfile
fs.read("/hello","testfile")
str = fs.read_str("/hello")
print(str)
fs.remove("/hello")

