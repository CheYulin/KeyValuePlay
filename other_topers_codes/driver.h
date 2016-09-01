//
// Created by cheyulin on 9/1/16.
//

#ifndef KEYVALUESTORE_DRIVER_H
#define KEYVALUESTORE_DRIVER_H

#include <cstring>
#include <sys/mman.h>
#include <fcntl.h>
#include <unistd.h>
#include <string>

using namespace std;
hash<string> ha;
const int BUCKET_SIZE = 65535;
struct [[pack]] node {
    size_t key;
    unsigned int len = 0, offset;
};
const static string SN = "NULL";

class Answer {
    int dataFd, nodeFd;
    node *nodeBuf;
    unsigned int len;
    size_t t;
    char s[30010];
    unsigned int offset = 0;
    int pagesize = 1 << 24;
    int pagenum = 120;
    int nowpage;
    char *dataBuf[120];
    int pagelen = 16;
public:
    Answer() {
        nodeFd = open("redis.index", O_RDWR | O_CREAT, S_IRUSR | S_IWUSR);
        int sz = (BUCKET_SIZE + 1) * sizeof(node);
        ftruncate(nodeFd, sz);
        nodeBuf = (node *) mmap(0, sz, PROT_READ | PROT_WRITE, MAP_SHARED | MAP_POPULATE, nodeFd, 0);
        dataFd = open("redis.data", O_RDWR | O_CREAT, S_IRUSR | S_IWUSR);
        ftruncate(dataFd, pagesize * pagenum);
        for (int i = 0; i <= BUCKET_SIZE; i++) {
            if (offset > nodeBuf[i].offset + nodeBuf[i].len)continue;
            offset = nodeBuf[i].offset + nodeBuf[i].len;
        }
        nowpage = offset >> 24;
        for (int i = 0; i <= pagelen; i++) {
            if (nowpage - i < 0)break;
            dataBuf[nowpage - i] = (char *) mmap(0, pagesize, PROT_READ | PROT_WRITE, MAP_SHARED | MAP_POPULATE, dataFd,
                                                 (nowpage - i) << 24);
        }
    }

    string get(string key) {
        t = ha(key);
        int k = (t & BUCKET_SIZE + BUCKET_SIZE) & BUCKET_SIZE;
        while (nodeBuf[k].len) {
            if (nodeBuf[k].key == t) {
                int p = nodeBuf[k].offset >> 24;
                if (p + pagelen >= nowpage)
                    return string(dataBuf[p] + (nodeBuf[k].offset & (pagesize - 1)), nodeBuf[k].len);
                pread(dataFd, s, nodeBuf[k].len, nodeBuf[k].offset);
                return string(s, nodeBuf[k].len);
            }
            k = (k + 7) & BUCKET_SIZE;
        }
        return SN;
    }

    void put(string key, string value) {
        t = ha(key);
        len = value.size();
        if (((offset + len - 1) >> 24) != nowpage) {
            if (nowpage >= pagelen)
                munmap(dataBuf[nowpage - pagelen], pagesize);
            nowpage++;
            dataBuf[nowpage] = (char *) mmap(0, pagesize, PROT_READ | PROT_WRITE, MAP_SHARED, dataFd, nowpage << 24);
            offset = nowpage << 24;
        }
        memcpy((char *) (dataBuf[nowpage] + (offset & (pagesize - 1))), value.c_str(), len);
        int k = (t & BUCKET_SIZE + BUCKET_SIZE) & BUCKET_SIZE;
        while (nodeBuf[k].len && nodeBuf[k].key != t)k = (k + 7) & BUCKET_SIZE;
        nodeBuf[k].key = t;
        nodeBuf[k].len = len;
        nodeBuf[k].offset = offset;
        offset += len;
    }
};

#endif //KEYVALUESTORE_DRIVER_H