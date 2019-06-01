/*
* cephfs tool
* header file
*
* 20180815
*/
#ifndef CEPHFSTOOL_H
#define CEPHFSTOOL_H

#include <string>
#include <cephfs/libcephfs.h>

//all function write the error msg to log file or stdout
class CephfsHelper {
private:
    struct ceph_mount_info *cmount;
    std::string config_file;
    std::string mon_addr;
    std::string user;
    std::string user_key;
    std::string user_key_file;
    std::string root;
private:
    void get_parent(const char* path, std::string &parent);
public:
    CephfsHelper():cmount(nullptr),
        config_file("/usr/local/cephfstool/conf/ceph.conf"){}
    CephfsHelper(const char *conf):cmount(nullptr),config_file(conf){}
    ~CephfsHelper(){ shutdown();}
    void shutdown();

    void set_config_file(const char* file);
    void set_mon_addr(const char* addr);
    void set_user_key(const char* key);
    void set_user_key_file(const char* key);
    const char* get_config_file() const{ return config_file.c_str();}
    const char* get_user() const{ return user.c_str();}
    const char* get_root() const{ return root.c_str();}

    //connect to cephfs
    bool login(const char* user, const char* key, const char* root);
    //write string to cephfs
    bool write_str(const char* path, const char* content);
    //read string from cephfs
    bool read_str(const char* path, char* buffer, size_t size);
    std::string read_str(const char* path);
    //write file to cephfs, path must be a file name, not a dir name
    bool write(const char* path, const char* local_path);
    //read from cephfs, then write to local file
    bool read(const char* path, const char* local_path);
    //write a whole dir tree to cephfs
    bool write_tree(const char* path, const char* local_path);
    //if path or parent is no exist, then mkdir
    bool get_safe_path(const char* path);
    //change cwd
    bool chdir(const char* path);
    //get cwd
    std::string getcwd();
    //remove a file from cephfs
    bool remove(const char* path);
    //get file size
    bool length(const char* path, uint64_t &sz);
    //remove empty dir
    bool rm_dir(const char* path);
    //recursive remove dir, also remove files
    bool rmdir(const char* path);
    //if file is exists or not
    bool exists(const char* path);
    //rename the file in cephfs
    bool rename(const char* src, const char* dst);
    //stat path, -1 not exists; 0 file; 1 dir; 2 other
    int stat(const char* path);
    //listdir  
    bool listdir(const char* path, std::vector<std::string>& list);
    //listdir use buffer
    bool listdir_buffer(const char* path, std::vector<std::string>& list);
};

extern void set_log_dir(const char* dir);
extern std::string version();

#endif

