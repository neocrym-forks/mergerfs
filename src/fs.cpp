/*
  Copyright (c) 2016, Antonio SJ Musumeci <trapexit@spawn.link>

  Permission to use, copy, modify, and/or distribute this software for any
  purpose with or without fee is hereby granted, provided that the above
  copyright notice and this permission notice appear in all copies.

  THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
  WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
  MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
  ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
  WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
  ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
  OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
*/

#include <string>
#include <vector>

#include <errno.h>
#include <fcntl.h>
#include <fcntl.h>
#include <glob.h>
#include <limits.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <sys/statvfs.h>
#include <sys/types.h>
#include <unistd.h>

#include "fs_attr.hpp"
#include "fs_path.hpp"
#include "fs_xattr.hpp"
#include "str.hpp"

using std::string;
using std::vector;

namespace fs
{
  void
  findallfiles(const vector<string> &srcmounts,
               const string         &fusepath,
               vector<string>       &paths)
  {
    for(size_t i = 0, ei = srcmounts.size(); i != ei; i++)
      {
        int rv;
        string fullpath;
        struct stat st;

        fs::path::make(srcmounts[i],fusepath,fullpath);

        rv = ::lstat(fullpath.c_str(),&st);
        if(rv == 0)
          paths.push_back(fullpath);
      }
  }

  int
  findonfs(const vector<string> &srcmounts,
           const string         &fusepath,
           const int             fd,
           string               &basepath)
  {
    int rv;
    string tmppath;
    unsigned long fsid;
    struct statvfs buf;

    rv = ::fstatvfs(fd,&buf);
    if(rv == -1)
      return -1;

    fsid = buf.f_fsid;
    for(int i = 0, ei = srcmounts.size(); i != ei; i++)
      {
        fs::path::make(srcmounts[i],fusepath,tmppath);

        rv = ::statvfs(tmppath.c_str(),&buf);
        if(rv == -1)
          continue;

        if(buf.f_fsid == fsid)
          {
            basepath = srcmounts[i];
            return 0;
          }
      }

    return (errno=ENOENT,-1);
  }

  void
  glob(const vector<string> &patterns,
       vector<string>       &strs)
  {
    int flags;
    glob_t gbuf = {0};

    flags = 0;
    for(size_t i = 0; i < patterns.size(); i++)
      {
        glob(patterns[i].c_str(),flags,NULL,&gbuf);
        flags = GLOB_APPEND;
      }

    for(size_t i = 0; i < gbuf.gl_pathc; ++i)
      strs.push_back(gbuf.gl_pathv[i]);

    globfree(&gbuf);
  }

  void
  realpathize(vector<string> &strs)
  {
    char *rv;
    char buf[PATH_MAX];

    for(size_t i = 0; i < strs.size(); i++)
      {
        rv = ::realpath(strs[i].c_str(),buf);
        if(rv == NULL)
          continue;

        strs[i] = buf;
      }
  }

  int
  getfl(const int fd)
  {
    return ::fcntl(fd,F_GETFL,0);
  }

  int
  mfs(const vector<string> &basepaths,
      const size_t          minfreespace,
      string               &path)
  {
    fsblkcnt_t mfs;
    ssize_t    mfsidx;

    mfs    = 0;
    mfsidx = -1;
    for(size_t i = 0, ei = basepaths.size(); i != ei; i++)
      {
        int rv;
        struct statvfs fsstats;
        const string &basepath = basepaths[i];

        rv = ::statvfs(basepath.c_str(),&fsstats);
        if(rv == 0)
          {
            fsblkcnt_t spaceavail;

            spaceavail = (fsstats.f_frsize * fsstats.f_bavail);
            if((spaceavail > mfs) && (spaceavail >= minfreespace))
              {
                mfs    = spaceavail;
                mfsidx = i;
              }
          }
      }

    if(mfsidx == -1)
      return (errno=ENOENT,-1);

    path = basepaths[mfsidx];

    return 0;
  }
};
