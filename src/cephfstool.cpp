/*
* cephfs tool
*
* 20180815
*/

#include "utils.h"
#include "cephfstool.h"

bool log_to_file = true;
std::string log_dir_prefix = "./";
std::ofstream log_stream;

static constexpr size_t BUFFER_SIZE = 1024*1024; //1MB

void set_log_dir(const char* dir){
    if(dir != nullptr){
        struct stat st;
        if(stat(dir, &st) == 0){
            if(S_ISDIR(st.st_mode)){
                //dir is exist
                log_to_file = true;
                log_dir_prefix = dir;
                return;
            }
        }
    }
    log_to_file = false;
    log_dir_prefix = "";
}

std::string version(){
    std::string date = __DATE__, time = __TIME__;
    std::stringstream ss;
    ss<<"."<<date.substr(date.length()-4);
    //Jan, Feb, Mar, Apr, May, Jun, Jul, Aug, Sep, Oct, Nov, Dec
    std::string month = date[2] == 'n' ? (date[1] == 'a' ? "01" : "06") :
        date[2] == 'b' ? "02" : date[2] == 'r' ? (date[1] == 'a' ? "03" : "04") :
        date[2] == 'y' ? "05" : date[2] == 'l' ? "07" : date[2] == 'g' ? "08" : 
        date[2] == 'p' ? "09" : date[2] == 't' ? "10" : date[2] == 'v' ? "11" : 
        date[2] == 'c' ? "12" : "99";
    std::remove(time.begin(),time.end(),':');
    ss<<month<<date.substr(4,2)<<time.substr(0,6);
    return ss.str();
}

void CephfsHelper::shutdown(){
    if(cmount){
        ceph_shutdown(cmount);
        cmount = nullptr;
    }
}

void CephfsHelper::set_config_file(const char* file) {
    if(file != nullptr && *file != '\0'){
        config_file = file;
        mon_addr = "";
        user_key = "";
    }
}

void CephfsHelper::set_mon_addr(const char* addr) {
    if(addr != nullptr && *addr != '\0'){
        mon_addr = addr;
        config_file = "";
    }
}

void CephfsHelper::set_user_key(const char* key) {
    if(key != nullptr && *key != '\0'){
        user_key = key;
    }
}

void CephfsHelper::set_user_key_file(const char* key) {
    if(key != nullptr && *key != '\0'){
        user_key_file = key;
    }
}

bool CephfsHelper::login(const char* user, const char* key, const char* root){
    //user and root can be nullptr, then use default (admin and /)
    if(key != nullptr && *key != '\0'){
        user_key = key;
    }
    int ret = 0;
    ret = ceph_create(&cmount, user);
    if(ret < 0){
        error("Unable to create cephfs with ", user, -ret);
        return false;
    }
    if(!config_file.empty()){
        ret = ceph_conf_read_file(cmount, config_file.c_str());
        if(ret < 0){
            error("Unable to read conf file ", config_file.c_str(), -ret);
            return false;
        }
    }
    if(!mon_addr.empty()){
        ret = ceph_conf_set(cmount, "mon host", mon_addr.c_str());
        if(ret < 0){
            error("Unable to set cephfs config ", "", -ret);
            return false;
        }
    }
    if(!user_key.empty()){
        ret = ceph_conf_set(cmount, "key", user_key.c_str());
        if(ret < 0){
            error("Unable to set cephfs config ", "", -ret);
            return false;
        }
    } else if(!user_key_file.empty()){
        ret = ceph_conf_set(cmount, "keyfile", user_key_file.c_str());
        if(ret < 0){
            error("Unable to set cephfs config ", "", -ret);
            return false;
        } 
    }

    ret = ceph_mount(cmount, root);
    if(ret < 0){
        error("Unable to open cephfs ", root, -ret);
        return false;
    }
    if(user)
        this->user = user;
    else
        this->user = "admin";
    if(root)
        this->root = root;
    else
        this->root = "/";
    log("INFO")<<"connect cephfs "<<this->user<<":"<<this->root<<" successfully."<<std::endl;
    return true;
}

bool CephfsHelper::write_str(const char* path, const char* content){
    if(path == nullptr || *path == '\0' ||
        content == nullptr || *content == '\0') return false;
    if(cmount == nullptr){
        log("ERROR")<<"No user log in cephfs"<<std::endl;
        return false;
    }
    if(!get_safe_path(path)) return false;
    int fd = ceph_open(cmount, path, O_WRONLY|O_CREAT|O_TRUNC, 0644);
    if(fd <= 0){
        error("Unable to open cephfs file ", path, -fd);
        return false;
    }
    size_t size = strlen(content);
    int ret = ceph_write(cmount, fd, content, size, 0);
    if(ret < 0){
        error("Unable to write data to cephfs, path: ", path, -ret);
        ceph_close(cmount, fd);
        return false;
    }
    if(ret < (int)size){
        log("ERROR")<<"cephfs actual write "<<ret<<" bytes, but request is "
            <<size<<" bytes."<<std::endl;
        ceph_close(cmount, fd);
        return false;
    }
    ceph_close(cmount, fd);
    log("INFO")<<"cephfs write to "<<path<<", "<<ret<<" bytes"<<std::endl;
    return true;
}

bool CephfsHelper::read_str(const char* path, char* buffer, size_t size){
    if(path == nullptr || *path == '\0' ||
        buffer == nullptr || size == 0) return false;
    if(cmount == nullptr){
        log("ERROR")<<"No user log in cephfs"<<std::endl;
        return false;
    }
    if(!get_safe_path(path)) return false;
    int fd = ceph_open(cmount, path, O_RDONLY, 0644);
    if(fd <= 0){
        error("Unable to open cephfs file ", path, -fd);
        return false;
    }
    //maybe not read all content when size is smaller than the size of fd
    int ret = ceph_read(cmount, fd, buffer, size, 0);
    if(ret < 0){
        error("Unable to read data from cephfs, path: ", path, -ret);
        ceph_close(cmount, fd);
        return false;
    }
    ceph_close(cmount, fd);
    log("INFO")<<"cephfs read from "<<path<<", "<<ret<<" bytes"<<std::endl;
    return true;
}

std::string CephfsHelper::read_str(const char* path){
    char buffer[1024];
    if(!read_str(path, buffer, 1024)) return "cephfs occurs an error";
    return std::string(buffer);
}

bool CephfsHelper::remove(const char* path){
    if(path == nullptr || *path == '\0') return false;
    if(cmount == nullptr){
        log("ERROR")<<"No user log in cephfs"<<std::endl;
        return false;
    }
    int ret = ceph_unlink(cmount, path);
    if(ret < 0){
        error("Unable to remove from cephfs, path: ", path, -ret);
        return false;
    }
    log("INFO")<<"cephfs remove "<<path<<std::endl;
    return true;
}

bool CephfsHelper::write(const char* path, const char* local_path){
    if(path == nullptr || *path == '\0' || 
        local_path == nullptr || *local_path == '\0') return false;
    if(cmount == nullptr){
        log("ERROR")<<"No user log in cephfs"<<std::endl;
        return false;
    }
    std::ifstream is(local_path);
    if(!is){
        error("Unable to open local file ", local_path, 0);
        return false;
    }
    if(!get_safe_path(path)) return false;
    int fd = ceph_open(cmount, path, O_WRONLY|O_CREAT|O_TRUNC, 0644);
    if(fd <= 0){
        error("Unable to open cephfs file ", path, -fd);
        return false;
    }
   
    //buffer write
    char buffer[BUFFER_SIZE] = {0};
    size_t offset = 0;
    while(is){
        is.read(buffer, BUFFER_SIZE);
        int read_count = (int)is.gcount(), write_count;
        if(read_count <= 0) break;
        //retry write to ceph
        int cur_write_count = read_count, buffer_offset = 0;
        size_t cur_offset = offset;
        while(true){
            write_count = ceph_write(cmount, fd, buffer + buffer_offset, 
                    cur_write_count, cur_offset);
            if(write_count < 0){
                error("Unable to write data to ceph, path ", path, -write_count);
                ceph_close(cmount, fd); 
                return false;
            }
            if(write_count >= cur_write_count) break;
            log("WARN")<<"cephfs actual write "<<write_count
                <<" bytes, but the request is "
                <<cur_write_count<<" bytes, will retry"<<std::endl;
            cur_write_count -= write_count;
            cur_offset += write_count;
            buffer_offset += write_count;
        }
        offset += read_count;
    }
    log("INFO")<<"cephfs write to "<<path<<", "<<offset<<" bytes"<<std::endl;
    ceph_close(cmount, fd); 
    return true;
}

bool CephfsHelper::read(const char* path, const char* local_path){
    if(path == nullptr || *path == '\0' || 
        local_path == nullptr || *local_path == '\0') return false;
    if(cmount == nullptr){
        log("ERROR")<<"No user log in cephfs"<<std::endl;
        return false;
    }
    std::ofstream os(local_path);
    if(!os){
        error("Unable to open local file ", local_path, 0);
        return false;
    }
    if(!get_safe_path(path)) return false;
    int fd = ceph_open(cmount, path, O_RDONLY, 0644);
    if(fd <= 0){
        error("Unable to open cephfs file ", path, -fd);
        return false;
    }
    //buffer read
    char buffer[BUFFER_SIZE];
    int read_count;
    size_t offset = 0;
    while(true){
        memset(buffer, 0, BUFFER_SIZE);
        read_count = ceph_read(cmount, fd, buffer, BUFFER_SIZE, offset);
        if(read_count < 0){
            error("Unable to read data from cephfs ", path, -read_count);
            ceph_close(cmount, fd);
            return false;
        }
        os.write(buffer, read_count);
        offset += read_count;
        if(read_count < (int)BUFFER_SIZE) break;
    }
    ceph_close(cmount, fd);
    log("INFO")<<"cephfs read from "<<path<<", "<<offset<<" bytes"<<std::endl;
    return true;
}

void CephfsHelper::get_parent(const char* path, std::string &parent){
    if(path == nullptr || *path == '\0'){
        parent = "/";
        return;
    }
    const char *endp;
    endp = path + strlen(path) - 1;
    //Strip trailing slashes
    while (endp > path && *endp == '/')
        endp--;
    //Find the start of the dir
    while (endp > path && *endp != '/')
        endp--;
    if(endp == path){
        parent = (*endp == '/')?"/":".";
        return;
    }
    do {
        endp--;
    } while (endp > path && *endp == '/');    
    parent = std::string(path, endp-path+1);
    return;
}

bool CephfsHelper::get_safe_path(const char* path){
    if(path == nullptr || *path == '\0') return false;
    if(cmount == nullptr){
        log("ERROR")<<"No user log in cephfs"<<std::endl;
        return false;
    }
    //cephfs strip leading .. auto
    const char* _path = path;
    if(path[strlen(path)-1] != '/'){
        std::string tp;
        get_parent(path, tp);
        _path = tp.c_str();
    }
    if(strlen(_path) == 1 && *_path == '/') return true;
    int ret = ceph_mkdirs(cmount, _path, 0777);
    if(ret < 0 && ret != -EEXIST){
        error("Unable to mkdir: ", _path, -ret);
        return false;
    }
    if(ret == 0) log("INFO")<<"cephfs mkdir "<<_path<<std::endl;
    return true;
}

bool CephfsHelper::length(const char* path, uint64_t &sz){
    if(path == nullptr || *path == '\0') return false;
    if(cmount == nullptr){
        log("ERROR")<<"No user log in cephfs"<<std::endl;
        return false;
    }
    struct ceph_statx stx;
    int ret = ceph_statx(cmount, path, &stx, CEPH_STATX_SIZE, AT_SYMLINK_NOFOLLOW);
    if(ret < 0){
        error("Unable to get file size, path: ", path, -ret);
        return false;
    }
    sz = stx.stx_size;
    log("INFO")<<"cephfs get file length "<<path<<" "<<sz<<" bytes"<<std::endl;
    return true;
}

bool CephfsHelper::rm_dir(const char* path){
    if(path == nullptr || *path == '\0') return false;
    if(cmount == nullptr){
        log("ERROR")<<"No user log in cephfs"<<std::endl;
        return false;
    }
    int ret = ceph_rmdir(cmount, path);
    if(ret < 0){
        error("Unable to rm dir, path: ", path, -ret);
        return false;
    }
    log("INFO")<<"cephfs rm dir "<<path<<std::endl;
    return true;
}

//rmdir recursive
bool CephfsHelper::rmdir(const char* path){
    if(path == nullptr || *path == '\0') return false;
    if(cmount == nullptr){
        log("ERROR")<<"No user log in cephfs"<<std::endl;
        return false;
    }
    struct ceph_dir_result *dirp;
    struct dirent de;
    struct ceph_statx stx;
    int ret;
    ret = ceph_opendir(cmount, path, &dirp);
    if(ret < 0){
        error("Unable to open path: ", path, -ret);
        return false;
    }
    while ((ret = ceph_readdirplus_r(cmount, dirp, &de, &stx, 
        CEPH_STATX_INO, AT_NO_ATTR_SYNC, nullptr)) > 0) {
        std::string new_dir = de.d_name;
        if(new_dir != "." && new_dir != "..") {
            new_dir = path;
            new_dir += '/';
            new_dir += de.d_name;
            if(S_ISDIR(stx.stx_mode)) {
                if(!rmdir(new_dir.c_str())) return false;
            } else {
                if(!remove(new_dir.c_str())) return false;
            }
        }
    }
    if(ret < 0) {
        error("Unable to open path: ", path, -ret);
        return false;
    }
    ret = ceph_closedir(cmount, dirp);
    if(ret < 0) {
        error("Unable to close path: ", path, -ret);
        return false;
    }
    if(strlen(path) == 1 && *path == '/') return true;
    ret = ceph_rmdir(cmount, path);
    if(ret < 0) {
        error("Unable to remove path: ", path, -ret);
        return false;
    }
    log("INFO")<<"cephfs remove dir "<<path<<std::endl;
    return true;
}

bool CephfsHelper::exists(const char* path){
    if(path == nullptr || *path == '\0') return false;
    if(cmount == nullptr){
        log("ERROR")<<"No user log in cephfs"<<std::endl;
        return false;
    }
    struct ceph_statx stx;
    int ret = ceph_statx(cmount, path, &stx, 0, AT_SYMLINK_NOFOLLOW);
    return (ret == 0);
}

bool CephfsHelper::rename(const char* src, const char* dst){
    if(src == nullptr || *src == '\0' || 
        dst == nullptr || *dst == '\0') return false;
    if(cmount == nullptr){
        log("ERROR")<<"No user log in cephfs"<<std::endl;
        return false;
    }
    if(!get_safe_path(dst)) return false;
    int ret = ceph_rename(cmount, src, dst);
    if(ret < 0){
        error("Unable to rename file, src: ", src, -ret);
        return false;
    }
    log("INFO")<<"cephfs rename from "<<src<<" to "<<dst<<std::endl;
    return true;
}

bool CephfsHelper::write_tree(const char* path, const char* local_path){
    if(path == nullptr || *path == '\0' ||
        local_path == nullptr || *local_path == '\0') return false;
    if(cmount == nullptr){
        log("ERROR")<<"No user log in cephfs"<<std::endl;
        return false;
    }
    int ret;
    struct stat st;
    ret = ::stat(local_path, &st);
    if(ret < 0){
        error("Unable to get stat local path ", local_path, errno);
        return false;
    }
    if(S_ISREG(st.st_mode)){
        //regular file, just write to cephfs
        //if path is a dir, write will be failed
        if(!write(path, local_path)) return false;
    }else if(S_ISDIR(st.st_mode)){
        DIR *dp;
        struct dirent *de;
        dp = opendir(local_path);
        if(dp == nullptr){
            error("Unable to open local dir ", local_path, errno);
            return false;
        }
        while((de = readdir(dp)) != nullptr){
            //skip .  ..  .*
            if(de->d_name[0] == '.') continue;
            std::string new_path = path;
            std::string new_local_path = local_path;
            if(new_path[new_path.size()-1] != '/') new_path += '/';
            new_path += de->d_name;
            if(new_local_path[new_local_path.size()-1] != '/')
                new_local_path += '/';
            new_local_path += de->d_name;
            if(!write_tree(new_path.c_str(), new_local_path.c_str())){
                closedir(dp);
                return false;
            }
        }
        closedir(dp);
    }
    return true;
}

bool CephfsHelper::chdir(const char* path){
    if(path == nullptr || *path == '\0') return false;
    if(cmount == nullptr){
        log("ERROR")<<"No user log in cephfs"<<std::endl;
        return false;
    }
    int ret = ceph_chdir(cmount, path);
    if(ret < 0){
        error("Unable to cd, path: ", path, -ret);
        return false;
    }
    log("INFO")<<"cephfs chdir "<<path<<std::endl;
    return true;
}

std::string CephfsHelper::getcwd(){
    if(cmount == nullptr){
        log("ERROR")<<"No user log in cephfs"<<std::endl;
        return false;
    }
    return std::string(ceph_getcwd(cmount));
}

int CephfsHelper::stat(const char* path){
    if(path == nullptr || *path == '\0') return false;
    if(cmount == nullptr){
        log("ERROR")<<"No user log in cephfs"<<std::endl;
        return false;
    }
    struct ceph_statx stx;
    int ret = ceph_statx(cmount, path, &stx, CEPH_STATX_MODE, AT_SYMLINK_NOFOLLOW);
    if(ret == 0){
        if(S_ISREG(stx.stx_mode))
            return 0;
        else if(S_ISDIR(stx.stx_mode))
            return 1;
        else return 2;
    }
    //error("Unable to stat, path: ", path, -ret);
    return -1;
}

bool CephfsHelper::listdir(const char* path,
    std::vector<std::string>& list){
    if(path == nullptr || *path == '\0') return false;
    if(cmount == nullptr){
        log("ERROR")<<"No user log in cephfs"<<std::endl;
        return false;
    }
    struct ceph_dir_result *dirp;
    struct dirent de;
    int ret;
    ret = ceph_opendir(cmount, path, &dirp);
    if(ret < 0){
        error("Unable to open path: ", path, -ret);
        return false;
    }
    list.clear();
    while ((ret = ceph_readdir_r(cmount, dirp, &de)) > 0) {
        std::string name = de.d_name;
        if(name != "." && name != "..") {
            list.push_back(name);
        }
    }
    if(ret < 0) {
        error("Unable to read path: ", path, -ret);
        return false;
    }
    ret = ceph_closedir(cmount, dirp);
    if(ret < 0) {
        error("Unable to close path: ", path, -ret);
        return false;
    }
    return true;
}

bool CephfsHelper::listdir_buffer(const char* path,
    std::vector<std::string>& list){
    if(path == nullptr || *path == '\0') return false;
    if(cmount == nullptr){
        log("ERROR")<<"No user log in cephfs"<<std::endl;
        return false;
    }
    struct ceph_dir_result *dirp;
    int ret;
    ret = ceph_opendir(cmount, path, &dirp);
    if(ret < 0){
        error("Unable to open path: ", path, -ret);
        return false;
    }
    list.clear();
    int buflen = 512, pos;
    char *buf = new char[buflen];
    if(buf == nullptr){
        error("bad alloc", path, 0);
        return false;
    }
    while(true){
        ret = ceph_getdnames(cmount, dirp, buf, buflen);
        if(ret == -ERANGE) { //expand the buffer
            delete [] buf;
            buflen *= 2;
            buf = new char[buflen];
            if(buf == nullptr){
                error("bad alloc", path, 0);
                return false;
            }
            continue;
        }
        if(ret <= 0) break;
        pos = 0;
        while(pos < ret){
            std::string name(buf + pos);
            if(name != "." && name != "..") {
                list.push_back(name);
            }
            pos += name.size() + 1;
        }
    }
    delete [] buf;
    if(ret < 0) {
        error("Unable to read path: ", path, -ret);
        return false;
    }
    ret = ceph_closedir(cmount, dirp);
    if(ret < 0) {
        error("Unable to close path: ", path, -ret);
        return false;
    }
    return true;
}

