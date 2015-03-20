/*
   Copyright Jakub Kruszona-Zawadzki, Core Technology Sp. z o.o.

   This file is part of MooseFS.

   READ THIS BEFORE INSTALLING THE SOFTWARE. BY INSTALLING,
   ACTIVATING OR USING THE SOFTWARE, YOU ARE AGREEING TO BE BOUND BY
   THE TERMS AND CONDITIONS OF MooseFS LICENSE AGREEMENT FOR
   VERSION 1.7 AND HIGHER IN A SEPARATE FILE. THIS SOFTWARE IS LICENSED AS
   THE PROPRIETARY SOFTWARE. YOU NOT ACQUIRE
   ANY OWNERSHIP RIGHT, TITLE OR INTEREST IN OR TO ANY INTELLECTUAL
   PROPERTY OR OTHER PROPRIETARY RIGHTS.
 */

#ifndef _MFS_FUSE_H_
#define _MFS_FUSE_H_

#include <fuse_lowlevel.h>

// int mfs_rootnode_setup(char *path);

#if FUSE_USE_VERSION >= 26
void mfs_statfs(fuse_req_t req, fuse_ino_t ino);
#else
void mfs_statfs(fuse_req_t req);
#endif
void mfs_access(fuse_req_t req, fuse_ino_t ino, int mask);
void mfs_lookup(fuse_req_t req, fuse_ino_t parent, const char *name);
void mfs_getattr(fuse_req_t req, fuse_ino_t ino, struct fuse_file_info *fi);
void mfs_setattr(fuse_req_t req, fuse_ino_t ino, struct stat *stbuf, int to_set, struct fuse_file_info *fi);
void mfs_mknod(fuse_req_t req, fuse_ino_t parent, const char *name, mode_t mode, dev_t rdev);
void mfs_unlink(fuse_req_t req, fuse_ino_t parent, const char *name);
void mfs_mkdir(fuse_req_t req, fuse_ino_t parent, const char *name, mode_t mode);
void mfs_rmdir(fuse_req_t req, fuse_ino_t parent, const char *name);
void mfs_symlink(fuse_req_t req, const char *path, fuse_ino_t parent, const char *name);
void mfs_readlink(fuse_req_t req, fuse_ino_t ino);
void mfs_rename(fuse_req_t req, fuse_ino_t parent, const char *name, fuse_ino_t newparent, const char *newname);
void mfs_link(fuse_req_t req, fuse_ino_t ino, fuse_ino_t newparent, const char *newname);
void mfs_opendir(fuse_req_t req, fuse_ino_t ino, struct fuse_file_info *fi);
void mfs_readdir(fuse_req_t req, fuse_ino_t ino, size_t size, off_t off, struct fuse_file_info *fi);
void mfs_releasedir(fuse_req_t req, fuse_ino_t ino, struct fuse_file_info *fi);
void mfs_create(fuse_req_t req, fuse_ino_t parent, const char *name, mode_t mode, struct fuse_file_info *fi);
void mfs_open(fuse_req_t req, fuse_ino_t ino, struct fuse_file_info *fi);
void mfs_release(fuse_req_t req, fuse_ino_t ino, struct fuse_file_info *fi);
void mfs_read(fuse_req_t req, fuse_ino_t ino, size_t size, off_t off, struct fuse_file_info *fi);
void mfs_write(fuse_req_t req, fuse_ino_t ino, const char *buf, size_t size, off_t off, struct fuse_file_info *fi);
void mfs_flush(fuse_req_t req, fuse_ino_t ino, struct fuse_file_info *fi);
void mfs_fsync(fuse_req_t req, fuse_ino_t ino, int datasync, struct fuse_file_info *fi);
#if defined(__APPLE__)
void mfs_setxattr (fuse_req_t req, fuse_ino_t ino, const char *name, const char *value, size_t size, int flags, uint32_t position);
void mfs_getxattr (fuse_req_t req, fuse_ino_t ino, const char *name, size_t size, uint32_t position);
#else
void mfs_setxattr (fuse_req_t req, fuse_ino_t ino, const char *name, const char *value, size_t size, int flags);
void mfs_getxattr (fuse_req_t req, fuse_ino_t ino, const char *name, size_t size);
#endif /* __APPLE__ */
void mfs_listxattr (fuse_req_t req, fuse_ino_t ino, size_t size);
void mfs_removexattr (fuse_req_t req, fuse_ino_t ino, const char *name);
#if FUSE_USE_VERSION >= 26
//void mfs_getlk(fuse_req_t req, fuse_ino_t ino, struct fuse_file_info *fi, struct flock *lock);
//void mfs_setlk(fuse_req_t req, fuse_ino_t ino, struct fuse_file_info *fi, struct flock *lock, int sl);
#endif
void mfs_init(int debug_mode_in,int keep_cache_in,double direntry_cache_timeout_in,double entry_cache_timeout_in,double attr_cache_timeout_in,double xattr_cache_timeout_in,double groups_cache_timeout,int mkdir_copy_sgid_in,int sugid_clear_mode_in,int xattr_acl_support_in);

#endif
