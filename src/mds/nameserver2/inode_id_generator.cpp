/*
 * Project: curve
 * Created Date: Thur March 28th 2019
 * Author: lixiaocui
 * Copyright (c) 2018 netease
 */

#include <glog/logging.h>
#include <string>
#include "src/mds/nameserver2/inode_id_generator.h"
#include "src/common/string_util.h"
#include "src/mds/nameserver2/namespace_helper.h"

namespace curve {
namespace mds {
bool InodeIdGeneratorImp::Init() {
    bool res = AllocateBundleIds(BUNDLEALLOCATED);
    return res;
}

bool InodeIdGeneratorImp::GenInodeID(InodeID *id) {
    ::curve::common::WriteLockGuard guard(lock_);
    if (nextId_ > bundleEnd_) {
        if (!AllocateBundleIds(BUNDLEALLOCATED)) {
            return false;
        }
    }

    *id = nextId_++;
    return true;
}

bool InodeIdGeneratorImp::AllocateBundleIds(int requiredNum) {
    // 获取已经allocate的最大值
    std::string out = "";
    uint64_t alloc;
    int errCode = client_->Get(INODESTOREKEY, &out);
    // 获取失败
    if (EtcdErrCode::OK != errCode && EtcdErrCode::KeyNotExist != errCode) {
        LOG(ERROR) << "get inode store key: " << INODESTOREKEY
                   << "err, errCode: " << errCode;
        return false;
    } else if (EtcdErrCode::KeyNotExist == errCode) {
        // key尚未存在，说明是初次分配
        alloc = INODEINITIALIZE;
    } else if (!NameSpaceStorageCodec::DecodeInodeID(out, &alloc)) {
        // key对应的value存在，但是decode失败，说明出现了internal err, 报警
        LOG(ERROR) << "decode inode id: " << out << "err";
        return false;
    }

    uint64_t target = alloc + BUNDLEALLOCATED;
    errCode = client_->CompareAndSwap(INODESTOREKEY, out,
        NameSpaceStorageCodec::EncodeInodeID(target));
    if (EtcdErrCode::OK != errCode) {
        LOG(ERROR) << "do CAS {preV: " << out << ", target: " << target
                   << "err, errCode: " << errCode;
        return false;
    }

    // 给next和end赋值
    bundleEnd_ = target;
    nextId_ = alloc + 1;
    return true;
}
}  // namespace mds
}  // namespace curve
