/*
 * Project: curve
 * Created Date: Friday September 7th 2018
 * Author: hzsunjianliang
 * Copyright (c) 2018 netease
 */

#ifndef SRC_MDS_NAMESERVER2_CURVEFS_H_
#define SRC_MDS_NAMESERVER2_CURVEFS_H_

#include <vector>
#include <string>
#include <thread>  //NOLINT
#include "proto/nameserver2.pb.h"
#include "src/mds/nameserver2/namespace_storage.h"
#include "src/mds/nameserver2/inode_id_generator.h"
#include "src/mds/common/mds_define.h"
#include "src/mds/nameserver2/chunk_allocator.h"
#include "src/mds/nameserver2/clean_manager.h"
#include "src/mds/nameserver2/async_delete_snapshot_entity.h"
#include "src/mds/nameserver2/session.h"

#include "src/common/authenticator.h"

using curve::common::Authenticator;

namespace curve {
namespace mds {

struct RootAuthOption {
    std::string rootOwner;
    std::string rootPassword;
};


const uint64_t ROOTINODEID = 0;
const char ROOTFILENAME[] = "/";
const uint64_t INTERALGARBAGEDIR = 1;

using ::curve::mds::DeleteSnapShotResponse;

class CurveFS {
 public:
    // singleton, supported in c++11
    static CurveFS &GetInstance() {
        static CurveFS curvefs;
        return curvefs;
    }

    /**
     *  @brief CurveFS初始化
     *  @param NameServerStorage:
     *         InodeIDGenerator：
     *         ChunkSegmentAllocator：
     *         CleanManagerInterface:
     *         sessionManager：
     *         sessionOptions ：初始化所session需要的参数
     *         authOptions : 对root用户进行认认证的参数
     *  @return 初始化是否成功
     */
    bool Init(NameServerStorage*, InodeIDGenerator*, ChunkSegmentAllocator*,
              std::shared_ptr<CleanManagerInterface>,
              SessionManager *sessionManager,
              const struct SessionOptions &sessionOptions,
              const struct RootAuthOption &authOptions);

    /**
     *  @brief CurveFS Uninit
     *  @param
     *  @return
     */
    void Uninit();

    // namespace ops
    /**
     *  @brief 创建文件
     *  @param fileName: 文件名
     *         owner: 文件的拥有者
     *         filetype：文件类型
     *         length：文件长度
     *  @return 是否成功，成功返回StatusCode::kOK
     */
    StatusCode CreateFile(const std::string & fileName,
                          const std::string& owner,
                          FileType filetype,
                          uint64_t length);
    /**
     *  @brief 获取文件信息。
     *         如果文件不存在返回StatusCode::kFileNotExists
     *         如果文件元数据获取错误，返回StatusCode::kStorageError
     *  @param filename：文件名
     *         inode：返回获取到的文件系统
     *  @return 是否成功，成功返回StatusCode::kOK
     */
    StatusCode GetFileInfo(const std::string & filename,
                           FileInfo * inode) const;

    /**
     *  @brief 删除文件
     *  @param filename：文件名
     *  @return 是否成功，成功返回StatusCode::kOK
     */
    StatusCode DeleteFile(const std::string & filename);

    /**
     *  @brief 获取目录下所有文件信息
     *  @param dirname：目录名字
     *         files：返回查到的结果
     *  @return 是否成功，成功返回StatusCode::kOK
     */
    StatusCode ReadDir(const std::string & dirname,
                       std::vector<FileInfo> * files) const;

    /**
     *  @brief 重命名文件
     *  @param oldFileName：旧文件名
     *         newFileName：希望重命名的新文件名
     *  @return 是否成功，成功返回StatusCode::kOK
     */
    // TODO(hzsunjianliang): 添加源文件的inode的参数，用于检查
    StatusCode RenameFile(const std::string & oldFileName,
                          const std::string & newFileName);

    /**
     *  @brief 扩容文件
     *  @param filename：文件名
     *         newSize：希望扩容后的文件大小
     *  @return 是否成功，成功返回StatusCode::kOK
     */
    // extent size minimum unit 1GB ( segement as a unit)
    StatusCode ExtendFile(const std::string &filename,
                          uint64_t newSize);


    // segment(chunk) ops
    /**
     *  @brief 查询segment信息，如果segment不存在，根据allocateIfNoExist决定是否
     *         创建新的segment
     *  @param filename：文件名
     *         offset: segment的偏移
     *         allocateIfNoExist：如果segment不存在，是否需要创建新的segment
     *         segment：返回查询到的segment信息
     *  @return 是否成功，成功返回StatusCode::kOK
     */
    StatusCode GetOrAllocateSegment(
        const std::string & filename,
        offset_t offset,
        bool allocateIfNoExist, PageFileSegment *segment);

    /**
     *  @brief 获取root文件信息
     *  @param
     *  @return 返回获取到的root文件信息
     */
    FileInfo GetRootFileInfo(void) const {
        return rootFileInfo_;
    }

    /**
     *  @brief 创建快照，如果创建成功，返回创建的快照文件info
     *  @param filename：文件名
     *         snapshotFileInfo: 返回创建的快照文件info
     *  @return 是否成功，成功返回StatusCode::kOK
     */
    StatusCode CreateSnapShotFile(const std::string &fileName,
                            FileInfo *snapshotFileInfo);

    /**
     *  @brief 获取文件的所有快照info
     *  @param filename：文件名
     *         snapshotFileInfos: 返回文件的所有的快照info
     *  @return 是否成功，成功返回StatusCode::kOK
     */
    StatusCode ListSnapShotFile(const std::string & fileName,
                            std::vector<FileInfo> *snapshotFileInfos) const;
    // async interface
    /**
     *  @brief 删除快照文件，删除文件指定seq的快照文件
     *  @param filename：文件名
     *         seq: 快照的seq
     *         entity: 异步删除快照entity
     *  @return 是否成功，成功返回StatusCode::kOK
     */
    StatusCode DeleteFileSnapShotFile(const std::string &fileName,
                            FileSeqType seq,
                            std::shared_ptr<AsyncDeleteSnapShotEntity> entity);

    /**
     *  @brief 获取快照的状态，如果状态是kFileDeleting，额外返回删除进度
     *  @param fileName : 文件名
     *         seq : 快照的sequence
     *         [out] status : 文件状态 kFileCreated, kFileDeleting
     *         [out] progress : 如果状态是kFileDeleting，这个参数额外返回删除进度
     *  @return 是否成功，成功返回StatusCode::kOK
     */
    StatusCode CheckSnapShotFileStatus(const std::string &fileName,
                            FileSeqType seq, FileStatus * status,
                            uint32_t * progress) const;

    /**
     *  @brief 获取快照info信息
     *  @param filename：文件名
     *         seq: 快照的seq
     *         snapshotFileInfo: 返回查询到的快照信息
     *  @return 是否成功，成功返回StatusCode::kOK
     */
    StatusCode GetSnapShotFileInfo(const std::string &fileName,
                            FileSeqType seq, FileInfo *snapshotFileInfo) const;

    /**
     *  @brief 获取快照的segment信息
     *  @param filename：文件名
     *         seq: 快照的seq
     *         offset：
     *         segment: 返回查询到的快照segment信息
     *  @return 是否成功，成功返回StatusCode::kOK
     */
    StatusCode GetSnapShotFileSegment(
            const std::string & filename,
            FileSeqType seq,
            offset_t offset,
            PageFileSegment *segment);

    // session ops
    /**
     *  @brief 打开文件
     *  @param filename：文件名
     *         clientIP：clientIP
     *         session：返回创建的session信息
     *         fileInfo：返回打开的文件信息
     *  @return 是否成功，成功返回StatusCode::kOK
     */
    StatusCode OpenFile(const std::string &fileName,
                        const std::string &clientIP,
                        ProtoSession *protoSession,
                        FileInfo  *fileInfo);

    /**
     *  @brief 关闭文件
     *  @param fileName: 文件名
     *         sessionID：sessionID
     *  @return 是否成功，成功返回StatusCode::kOK
     */
    StatusCode CloseFile(const std::string &fileName,
                         const std::string &sessionID);

    /**
     *  @brief 更新session的有效期
     *  @param filename：文件名
     *         sessionid：sessionID
     *         date: 请求的时间，用来防止重放攻击
     *         signature: 用来进行请求的身份验证
     *         clientIP: clientIP
     *         fileInfo: 返回打开的文件信息
     *  @return 是否成功，成功返回StatusCode::kOK
     */
    StatusCode RefreshSession(const std::string &filename,
                              const std::string &sessionid,
                              const uint64_t date,
                              const std::string &signature,
                              const std::string &clientIP,
                              FileInfo  *fileInfo);

    /**
     * @breif 创建克隆文件，当前克隆文件的创建只有root用户能够创建
     * @param filename 文件名
     * @param owner 调用接口的owner信息
     * @param filetype 文件的类型
     * @param length 克隆文件的长度
     * @param seq 版本号
     * @param ChunkSizeType 创建克隆文件的chunk大小
     * @param[out] fileInfo 创建成功克隆文件的fileInfo
     * @return 成功返回StatusCode:kOK
     */
    StatusCode CreateCloneFile(const std::string &filename,
                            const std::string& owner,
                            FileType filetype,
                            uint64_t length,
                            FileSeqType seq,
                            ChunkSizeType chunksize,
                            FileInfo *fileInfo);

    /**
     * @brief 设置克隆文件的状态
     * @param filename 文件名
     * @param fileID 设置文件的inodeid
     * @param fileStatus 需要设置的状态
     *
     * @return  是否成功，成功返回StatusCode::kOK
     *
     */
    StatusCode SetCloneFileStatus(const std::string &filename,
                            uint64_t fileID,
                            FileStatus fileStatus);
    /**
     *  @brief 检查的文件owner
     *  @param: filename：文件名
     *  @param: owner：文件的拥有者
     *  @param: signature是用户侧传过来的签名信息
     *  @param: date是用于计算signature的时间
     *  @return 是否成功，成功返回StatusCode::kOK
     *          验证失败返回StatusCode::kOwnerAuthFail
     *          其他失败
     */
    StatusCode CheckFileOwner(const std::string &filename,
                              const std::string &owner,
                              const std::string &signature,
                              uint64_t date);

    /**
     *  @brief 检查的文件各级目录的owner
     *  @param: filename：文件名
     *  @param: owner：文件的拥有者
     *  @param: signature是用户侧传过来的签名信息
     *  @param: date是用于计算signature的时间
     *  @return 是否成功，成功返回StatusCode::kOK
     *          验证失败返回StatusCode::kOwnerAuthFail
     *          其他失败，kFileNotExists，kStorageError，kNotDirectory
     */
    StatusCode CheckPathOwner(const std::string &filename,
                              const std::string &owner,
                              const std::string &signature,
                              uint64_t date);

    /**
     *  @brief 检查的RenameNewfile文件各级目录的owner
     *  @param: filename：文件名
     *  @param: owner：文件的拥有者
     *  @param: signature是用户侧传过来的签名信息
     *  @param: date是用于计算signature的时间
     *  @return 是否成功，成功返回StatusCode::kOK
     *          验证失败返回StatusCode::kOwnerAuthFail
     *          其他失败，kFileNotExists，kStorageError，kNotDirectory
     */
    StatusCode CheckDestinationOwner(const std::string &filename,
                              const std::string &owner,
                              const std::string &signature,
                              uint64_t date);

 private:
    CurveFS() = default;

    void InitRootFile(void);

    StatusCode WalkPath(const std::string &fileName,
                        FileInfo *fileInfo, std::string  *lastEntry) const;

    StatusCode LookUpFile(const FileInfo & parentFileInfo,
                          const std::string & fileName,
                          FileInfo *fileInfo) const;

    StatusCode PutFile(const FileInfo & fileInfo);

    /**
     * @brief 执行一次fileinfo的snapshot快照事务
     * @param originalFileInfo: 原文件对于fileInfo
     * @param SnapShotFile: 生成的snapshot文件对于fileInfo
     * @return StatusCode: 成功或者失败
     */
    StatusCode SnapShotFile(const FileInfo * originalFileInfo,
        const FileInfo * SnapShotFile) const;

    std::string GetRootOwner() {
        return rootAuthOptions_.rootOwner;
    }

    /**
     * @brief: 检查当前请求date是否合法，与当前时间前后15分钟内为合法
     * @param: date请求的时间点
     * @return: 合法返回true，否则false
     */
    bool CheckDate(uint64_t date);
    /**
     *  @brief 检查请求的signature是否合法
     *  @param: owner：文件的拥有者
     *  @param: signature是用户侧传过来的签名信息
     *  @param: date是用于计算signature的时间
     *  @return: 签名合法为true，否则false
     */
    bool CheckSignature(const std::string& owner,
                        const std::string& signature,
                        uint64_t date);

    StatusCode CheckPathOwnerInternal(const std::string &filename,
                              const std::string &owner,
                              const std::string &signature,
                              std::string *lastEntry,
                              uint64_t *parentID);

    /**
     *  @brief 删除文件时，把待删除的文件放到回收站
     *  @param: fileInfo：待删除的文件
     *  @param[out]: waitDelteFileInfo：生成的待删除文件信息，一方面作为出参
     *               一方面持久化到回收站
     *  @return: 是否成功
     */
    StatusCode MoveFileToRecycle(const FileInfo &fileInfo,
                                                FileInfo *waitDelteFileInfo);

    /**
     *  @brief 判断一个目录下是否没有文件
     *  @param: fileInfo：目录的fileInfo
     *  @param: result: 目录下是否是空的，true表示目录为空，false表示目录非空
     *  @return: 是否成功，成功返回StatusCode::kOK
     */
    StatusCode isDirectoryEmpty(const FileInfo &fileInfo, bool *result);

    /**
     *  @brief 判断文件是否有有效的session
     *  @param: fileName
     *  @return: true表示文件有有效session，false表示文件无有效session
     */
    bool isFileHasValidSession(const std::string &fileName);

 private:
    FileInfo rootFileInfo_;
    NameServerStorage*          storage_;
    InodeIDGenerator*           InodeIDGenerator_;
    ChunkSegmentAllocator*      chunkSegAllocator_;
    SessionManager *            sessionManager_;
    std::shared_ptr<CleanManagerInterface> cleanManager_;
    struct RootAuthOption       rootAuthOptions_;
};
extern CurveFS &kCurveFS;
}   // namespace mds
}   // namespace curve
#endif   // SRC_MDS_NAMESERVER2_CURVEFS_H_

