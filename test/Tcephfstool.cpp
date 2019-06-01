#include "src/utils.h"
#include "src/cephfstool.h"
#include <gtest/gtest.h>

#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>

const char* user = "test_cephfs_user";
const char* root = "/pytestdir/test_cephfs_user";
const char* key = "AQBNzwhc8ru/IBAAef0NABDSntpt5Q8TQp4AWw==";
const char* adminkey = "AQCKECJbEQ+0BRAAQNxuSUsRXnsquluzGbb6SQ==";
const char* addr = "172.28.218.70,172.28.160.165,172.28.217.100";

class CephfsTool : public testing::Test {
protected:
    static CephfsHelper helper;
    random_generator rg;
    static void SetUpTestCase(){
        set_log_dir("./");
        helper.set_mon_addr(addr);
        ASSERT_TRUE(helper.login(user, key, root));
    }
    static void TearDownTestCase(){
        helper.shutdown();
    }
    virtual void SetUp(){
    }
    virtual void TearDown(){
    }

    void cmp_file_size(const char* tf, size_t size){
        struct stat st;
        stat(tf, &st);
        EXPECT_EQ(st.st_size, size);
    }

    void write_file(const char* fsize){
        const char* path = "/cephfs_tool_test_file";
        ASSERT_FALSE(helper.exists(path));

        size_t size = parse_obj_size(fsize);
        const char *tf = "/tmp/tmpfile";
        EXPECT_TRUE(mkTempFile(tf, size, rg));

        EXPECT_TRUE(helper.write(path, tf));
        remove(tf);
        
        EXPECT_TRUE(helper.read(path, tf));
        cmp_file_size(tf, size);
        remove(tf);
        EXPECT_TRUE(helper.remove(path));
    }
};

CephfsHelper CephfsTool::helper;

TEST_F(CephfsTool, rw_str){
    const char* path = "/cephfs_tool_test_file";
    const char* content = "hello cephfs";
    ASSERT_FALSE(helper.exists(path));
    EXPECT_TRUE(helper.write_str(path, content));
    char buffer[64];
    EXPECT_TRUE(helper.read_str(path, buffer, 64));
    EXPECT_STREQ(content, buffer);
    EXPECT_TRUE(helper.read_str(path, buffer, 64));
    EXPECT_STREQ(content, buffer);
    EXPECT_TRUE(helper.remove(path));
}

TEST_F(CephfsTool, rw_str_double_write){
    const char* path = "/cephfs_tool_test_file";
    const char* content = "hello cephfs";
    ASSERT_FALSE(helper.exists(path));
    EXPECT_TRUE(helper.write_str(path, content));
    char buffer[64];
    EXPECT_TRUE(helper.read_str(path, buffer, 64));
    EXPECT_STREQ(content, buffer);
    
    const char* short_str = "hello";
    EXPECT_TRUE(helper.write_str(path, short_str));
    memset(buffer, 0, 64);
    EXPECT_TRUE(helper.read_str(path, buffer, 64));
    EXPECT_STREQ(short_str, buffer);
    EXPECT_TRUE(helper.remove(path));
}

TEST_F(CephfsTool, write_dir){
    const char* path = "/cephfs_tool_test_dir/";
    const char* content = "hello cephfs";
    ASSERT_FALSE(helper.exists(path));
    EXPECT_FALSE(helper.write_str(path, content));
    EXPECT_TRUE(helper.rmdir(path));
}

TEST_F(CephfsTool, read_dir){
    const char* path = "/cephfs_tool_test_dir/";
    ASSERT_FALSE(helper.exists(path));
    char buff[64];
    EXPECT_FALSE(helper.read_str(path, buff, 64));
    EXPECT_TRUE(helper.rmdir(path));
}

TEST_F(CephfsTool, write_top_dir){
    const char* path = "../cephfs_tool_test_file";
    const char* content = "hello cephfs";
    ASSERT_FALSE(helper.exists(path));
    EXPECT_TRUE(helper.write_str(path, content));
    EXPECT_TRUE(helper.remove(path));
}

TEST_F(CephfsTool, write_top_top_dir){
    const char* path = "../cephfs_tool_test_dir/subdir/cephfs_tool_test_file";
    const char* content = "hello cephfs";
    ASSERT_FALSE(helper.exists(path));
    EXPECT_TRUE(helper.write_str(path, content));
    EXPECT_TRUE(helper.rmdir("/cephfs_tool_test_dir"));
}

TEST_F(CephfsTool, rm){
    const char* path = "/cephfs_tool_test_file";
    ASSERT_FALSE(helper.exists(path));
    EXPECT_TRUE(helper.write_str(path, "test"));
    ASSERT_TRUE(helper.exists(path));
    EXPECT_TRUE(helper.remove(path));
    ASSERT_FALSE(helper.exists(path));
}

TEST_F(CephfsTool, mktmpfile){
    size_t size = parse_obj_size("2018k");
    const char *tf = "/tmp/tmpfile";
    EXPECT_TRUE(mkTempFile(tf, size, rg));
    cmp_file_size(tf, size);
    remove(tf);
}

TEST_F(CephfsTool, write_file_lt_buf){
    write_file("68k");
}

TEST_F(CephfsTool, write_file_eq_buf){
    write_file("1m");
}

TEST_F(CephfsTool, write_file_gt_buf){
    write_file("2018k");
}

TEST_F(CephfsTool, write_5m_file){
    write_file("5m");
}

TEST_F(CephfsTool, write_exceed){
    size_t size = parse_obj_size("15m");
    const char *tf = "/tmp/tmpfile";
    EXPECT_TRUE(mkTempFile(tf, size, rg));

    const char* path = "/cephfs_tool_test_file";
    ASSERT_FALSE(helper.exists(path));
    // size depends on actual cephfs quota
    //EXPECT_FALSE(helper.write(path, tf));
    EXPECT_TRUE(helper.write(path, tf));
    remove(tf);
    EXPECT_TRUE(helper.remove(path));
}

TEST_F(CephfsTool, mkdir){
    const char* path = "/cephfs_tool_test_file/subdir/subdir2/";
    ASSERT_FALSE(helper.exists(path));
    EXPECT_TRUE(helper.get_safe_path(path));
    EXPECT_TRUE(helper.exists(path));
    EXPECT_TRUE(helper.rmdir("/cephfs_tool_test_file"));
}

TEST_F(CephfsTool, mkdir_with_file){
    const char* path = "/cephfs_tool_test_dir/subdir/cephfs_tool_test_file";
    EXPECT_FALSE(helper.exists(path));
    EXPECT_TRUE(helper.get_safe_path(path));
    EXPECT_TRUE(helper.exists("/cephfs_tool_test_dir/subdir"));
    EXPECT_TRUE(helper.rmdir("/cephfs_tool_test_dir"));
}

TEST_F(CephfsTool, write_path_with_noexistdir){
    const char* path = "/cephfs_tool_test_dir/cephfs_tool_test_file";
    const char* content = "hello cephfs";
    ASSERT_FALSE(helper.exists(path));
    EXPECT_TRUE(helper.write_str(path, content));
    EXPECT_TRUE(helper.rmdir("/cephfs_tool_test_dir"));
}

TEST_F(CephfsTool, write_one_path_twice){
    const char* path = "/cephfs_tool_test_dir/cephfs_tool_test_file";
    const char* content = "hello cephfs";
    ASSERT_FALSE(helper.exists(path));
    EXPECT_TRUE(helper.write_str(path, content));
    EXPECT_TRUE(helper.write_str(path, content));
    EXPECT_TRUE(helper.rmdir("/cephfs_tool_test_dir"));
}

TEST_F(CephfsTool, write_deep_path){
    const char* path = "/cephfs_tool_test_dir/subdir/subdir2/subdir3/deep_name";
    const char* content = "hello cephfs";
    ASSERT_FALSE(helper.exists(path));
    EXPECT_TRUE(helper.write_str(path, content));
    EXPECT_TRUE(helper.write_str("/cephfs_tool_test_dir/subdir/test_name", content));
    EXPECT_TRUE(helper.rmdir("/cephfs_tool_test_dir"));
}

/* will delete everything, comment this test
TEST_F(CephfsTool, rm_all){
    EXPECT_TRUE(helper.rmdir("/"));
}
*/

TEST_F(CephfsTool, file_length){
    size_t size = parse_obj_size("4894k");
    const char *tf = "/tmp/tmpfile";
    EXPECT_TRUE(mkTempFile(tf, size, rg));

    const char* path = "/cephfs_tool_test_file";
    ASSERT_FALSE(helper.exists(path));
    EXPECT_TRUE(helper.write(path, tf));
    remove(tf);
    uint64_t sz;
    EXPECT_TRUE(helper.length(path, sz));
    EXPECT_EQ(sz, size);
    EXPECT_TRUE(helper.remove(path));
}

TEST_F(CephfsTool, rename){
    const char* src = "/cephfs_tool_test_src_file";
    const char* dst = "/cephfs_tool_test_dst_file";
    const char* content = "hello cephfs";
    EXPECT_TRUE(helper.write_str(src, content));
    EXPECT_TRUE(helper.exists(src));
    EXPECT_FALSE(helper.exists(dst));
    EXPECT_TRUE(helper.rename(src, dst));
    EXPECT_TRUE(helper.exists(dst));
    EXPECT_FALSE(helper.exists(src));
    char buffer[64];
    EXPECT_TRUE(helper.read_str(dst, buffer, 64));
    EXPECT_STREQ(content, buffer);
    EXPECT_TRUE(helper.remove("/cephfs_tool_test_dst_file"));
}

TEST_F(CephfsTool, rename_no_src){
    const char* src = "/cephfs_tool_test_src_file";
    const char* dst = "/cephfs_tool_test_dst_file";
    EXPECT_FALSE(helper.exists(src));
    EXPECT_FALSE(helper.exists(dst));
    EXPECT_FALSE(helper.rename(src, dst));
    EXPECT_FALSE(helper.exists(dst));
    EXPECT_FALSE(helper.exists(src));
}

TEST_F(CephfsTool, rename_exist_dst){
    const char* src = "/cephfs_tool_test_src_file";
    const char* dst = "/cephfs_tool_test_dst_file";
    const char* content = "hello cephfs";
    EXPECT_TRUE(helper.write_str(src, content));
    EXPECT_TRUE(helper.write_str(dst, "dst file content"));
    EXPECT_TRUE(helper.exists(src));
    EXPECT_TRUE(helper.exists(dst));
    EXPECT_TRUE(helper.rename(src, dst));
    EXPECT_TRUE(helper.exists(dst));
    EXPECT_FALSE(helper.exists(src));
    char buffer[64];
    EXPECT_TRUE(helper.read_str(dst, buffer, 64));
    EXPECT_STREQ(content, buffer);
    EXPECT_TRUE(helper.remove("/cephfs_tool_test_dst_file"));
}

TEST_F(CephfsTool, rename_with_dir){
    const char* src = "/cephfs_tool_test_dir/subdir/cephfs_tool_test_src_file";
    const char* dst = "/cephfs_tool_test_dir/cephfs_tool_test_dst_file";
    const char* content = "hello cephfs";
    EXPECT_TRUE(helper.write_str(src, content));
    EXPECT_TRUE(helper.exists(src));
    EXPECT_FALSE(helper.exists(dst));
    EXPECT_TRUE(helper.rename(src, dst));
    EXPECT_TRUE(helper.exists(dst));
    EXPECT_FALSE(helper.exists(src));
    char buffer[64];
    EXPECT_TRUE(helper.read_str(dst, buffer, 64));
    EXPECT_STREQ(content, buffer);
    EXPECT_TRUE(helper.rmdir("/cephfs_tool_test_dir"));
}

TEST_F(CephfsTool, write_tree){
    system("mkdir -p /tmp/test/a/b; \
            echo 1 > /tmp/test/a/b/c; \
            echo 2 > /tmp/test/a/b/d; \
            mkdir -p /tmp/test/e; \
            echo 3 > /tmp/test/e/f;");
    EXPECT_TRUE(helper.write_tree("/cephfs_tool_test_tree/", "/tmp/test/"));
    EXPECT_TRUE(helper.rmdir("/cephfs_tool_test_tree"));
    system("/bin/rm -rf /tmp/test");
}

TEST_F(CephfsTool, write_tree_bad){
    system("echo abc > /tmp/a");
    EXPECT_FALSE(helper.write_tree("/cephfs_tool_test_baddir/", "/tmp/a"));
    EXPECT_TRUE(helper.rmdir("/cephfs_tool_test_baddir"));
    system("/bin/rm -f /tmp/a");
}

TEST_F(CephfsTool, write_big_file){
    //1g may take long time, 
    write_file("100m");
}

TEST_F(CephfsTool, list){
    helper.get_safe_path("/cephfs_tool_test_dir/a/");
    helper.get_safe_path("/cephfs_tool_test_dir/b/");
    helper.write_str("/cephfs_tool_test_dir/file","test");
    helper.write_str("/cephfs_tool_test_dir/file2","test");
    std::vector<std::string> list;
    helper.listdir("/cephfs_tool_test_dir",list);
    ASSERT_EQ(4, list.size());
    std::vector<std::string> expect{"a", "b", "file","file2"};
    for(auto e : expect){
        bool found = false;
        for(auto l : list){
            if(e == l){
                found = true;
                break;
            }
        }
        ASSERT_EQ(true, found);
    }
    helper.rmdir("/cephfs_tool_test_dir");
}

TEST_F(CephfsTool, list_big){
    int count = 1000;
    char path[256];
    for(int i = 0; i < count; ++i){
        std::snprintf(path, 256, "/cephfs_tool_test_dir/file%d", i+1);
        helper.write_str(path, "test");
    }
    std::vector<std::string> list;
    helper.listdir("/cephfs_tool_test_dir", list);
    ASSERT_EQ(count, list.size());
    helper.rmdir("/cephfs_tool_test_dir");
}

TEST_F(CephfsTool, list_big_buffer){
    int count = 1000;
    char path[256];
    for(int i = 0; i < count; ++i){
        std::snprintf(path, 256, "/cephfs_tool_test_dir/file%d", i+1);
        helper.write_str(path, "test");
    }
    std::vector<std::string> list;
    helper.listdir_buffer("/cephfs_tool_test_dir", list);
    ASSERT_EQ(count, list.size());
    helper.rmdir("/cephfs_tool_test_dir");
}

TEST_F(CephfsTool, rm_dir){
    const char* path = "/cephfs_tool_test_dir/subdir/";
    EXPECT_TRUE(helper.get_safe_path(path));
    EXPECT_TRUE(helper.exists(path));
    EXPECT_FALSE(helper.rm_dir("/cephfs_tool_test_dir"));
    EXPECT_TRUE(helper.rmdir("/cephfs_tool_test_dir"));
}

TEST_F(CephfsTool, chdir){
    const char* path = "/cephfs_tool_test_dir/subdir/";
    EXPECT_TRUE(helper.get_safe_path(path));
    EXPECT_TRUE(helper.exists(path));
    EXPECT_STREQ("/", helper.getcwd().c_str());
    EXPECT_TRUE(helper.chdir("/cephfs_tool_test_dir"));
    EXPECT_STREQ("/cephfs_tool_test_dir", helper.getcwd().c_str());
    EXPECT_TRUE(helper.rmdir("/cephfs_tool_test_dir"));
}

TEST_F(CephfsTool, statno){
    const char* path = "/cephfs_tool_test_dir/";
    EXPECT_FALSE(helper.exists(path));
    EXPECT_EQ(-1, helper.stat(path));
}

TEST_F(CephfsTool, statfile){
    const char* path = "/cephfs_tool_test_dir/cephfs_tool_test_file";
    ASSERT_TRUE(helper.chdir("/"));
    EXPECT_TRUE(helper.write_str(path, "test"));
    EXPECT_TRUE(helper.exists(path));
    EXPECT_EQ(0, helper.stat(path));
    EXPECT_TRUE(helper.rmdir("/cephfs_tool_test_dir"));
}

TEST_F(CephfsTool, statdir){
    const char* path = "/cephfs_tool_test_dir/";
    ASSERT_TRUE(helper.chdir("/"));
    EXPECT_TRUE(helper.get_safe_path(path));
    EXPECT_TRUE(helper.exists(path));
    EXPECT_EQ(1, helper.chdir(path));
    EXPECT_TRUE(helper.rmdir("/cephfs_tool_test_dir"));
}

/*
 * test cephfs with conf file
 */
class CephfsToolConf : public testing::Test {
protected:
    static CephfsHelper helper;
    static void SetUpTestCase(){
        set_log_dir("./");
    }
    static void TearDownTestCase(){
        system("/bin/rm -f /tmp/ceph.conf /tmp/keyfile");
        helper.shutdown();
    }
    virtual void SetUp(){
    }
    virtual void TearDown(){
    }
    void auto_conf(bool isadmin, bool keyfile, bool key_in_conf,
            bool has_addr = true, const char* mon = addr){
        std::stringstream cmd;
        const char* tkey = isadmin ? adminkey : key;
        if(keyfile){
            cmd<<"echo \""<<tkey<<"\" > /tmp/keyfile;";
        }
        if(has_addr){
            cmd<<"echo \"[global]\nmon host = "<<mon<<"\n";
        }else{
            cmd<<"echo \"invalid conf";
        }
        if(keyfile && key_in_conf){
            cmd<<"keyfile = /tmp/keyfile\"";
        }else{
            cmd<<"\"";
        }
        cmd<<" > /tmp/ceph.conf";
        system(cmd.str().c_str());
        helper.set_config_file("/tmp/ceph.conf");
    }
};

CephfsHelper CephfsToolConf::helper;

TEST_F(CephfsToolConf, conf_invalid_file){
    auto_conf(true, false, false, false);
    helper.set_config_file("/tmp/ceph.conf");
    ASSERT_FALSE(helper.login(nullptr, nullptr, nullptr));
}

TEST_F(CephfsToolConf, conf_admin_no_key){
    auto_conf(true, false, false);
    ASSERT_FALSE(helper.login(nullptr, nullptr, nullptr));
}

TEST_F(CephfsToolConf, conf_admin_with_key){
    auto_conf(true, false, false);
    helper.set_user_key(adminkey);
    ASSERT_TRUE(helper.login(nullptr, nullptr, nullptr));
}

TEST_F(CephfsToolConf, conf_admin_with_key_in_conf){
    auto_conf(true, true, true);
    ASSERT_TRUE(helper.login(nullptr, nullptr, nullptr));
}

TEST_F(CephfsToolConf, conf_admin_with_key_file){
    auto_conf(true, true, false);
    helper.set_user_key_file("/tmp/keyfile");
    ASSERT_TRUE(helper.login(nullptr, nullptr, nullptr));
}

TEST_F(CephfsToolConf, conf_admin_with_key_arg){
    auto_conf(true, false, false);
    ASSERT_TRUE(helper.login(nullptr, adminkey, nullptr));
}

TEST_F(CephfsToolConf, conf_admin_wrong_key){
    auto_conf(true, false, false);
    helper.set_user_key("fakekey");
    ASSERT_FALSE(helper.login(nullptr, nullptr, nullptr));
}

TEST_F(CephfsToolConf, conf_user_no_key){
    auto_conf(false, false, false);
    ASSERT_FALSE(helper.login(user, nullptr, nullptr));
}

TEST_F(CephfsToolConf, conf_user_with_key_in_conf_but_no_root){
    auto_conf(false, true, true);
    ASSERT_FALSE(helper.login(user, nullptr, nullptr));
}

TEST_F(CephfsToolConf, conf_user_with_key_in_conf){
    auto_conf(false, true, true);
    ASSERT_TRUE(helper.login(user, nullptr, root));
}

TEST_F(CephfsToolConf, conf_user_with_key_file){
    auto_conf(false, true, false);
    helper.set_user_key_file("/tmp/keyfile");
    ASSERT_TRUE(helper.login(user, nullptr, root));
}

TEST_F(CephfsToolConf, conf_user_with_key){
    auto_conf(false, false, false);
    helper.set_user_key(key);
    ASSERT_TRUE(helper.login(user, nullptr, root));
}

TEST_F(CephfsToolConf, conf_user_with_key_arg){
    auto_conf(false, false, false);
    ASSERT_TRUE(helper.login(user, key, root));
}

TEST_F(CephfsToolConf, conf_user_wrong_key){
    auto_conf(false, false, false);
    helper.set_user_key("fakekey");
    ASSERT_FALSE(helper.login(user, nullptr, root));
}
