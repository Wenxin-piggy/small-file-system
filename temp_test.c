#include <string.h>
#include "disk.h"
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>

#define BLOCK_SIZE 1024 //一个块的字节数
#define ADDRESS_SIZE 20//命令中一共有多少个地址
#define BLOCK_IN_INODE 6//表示一个inode可以表示block的数量
#define MAGIC_NUM 0xdec0da
#define SUPER_BLOCK_BASE 0
#define INODE_BLOCK_BASE 1
#define DATA_BLOCK_BASE 33

#define FILE_TYPE 1
#define DIR_TYPE 2

char address_list[ADDRESS_SIZE][128];
char buf[BLOCK_SIZE];
typedef struct super_block {
    int32_t magic_num;                  // 幻数
    int32_t free_block_count;           // 空闲数据块数
    int32_t free_inode_count;           // 空闲inode数
    int32_t dir_inode_count;            // 目录inode数
    uint32_t block_map[128];            // 数据块占用位图
    uint32_t inode_map[32];             // inode占用位图
} sp_block;

struct inode {
    uint32_t size;              // 文件大小
    uint16_t file_type;         // 文件类型（文件/文件夹）
    uint16_t link;              // 连接数
    uint32_t block_point[6];    // 数据块指针
};

struct dir_item { //其长度为128B,则一个Block中有8块
    // 目录项一个更常见的叫法是 dirent(directory entry)
    uint32_t inode_id;          // 当前目录项表示的文件/目录的对应inode
    uint16_t valid;             // 当前目录项是否有效 
    uint8_t type;               // 当前目录项类型（文件/目录）
    char name[121];             // 目录项表示的文件/目录的文件名/目录名
};

int my_disk_read_block(unsigned int block_num, char* buf){
    unsigned int block_num1 = 2*block_num;
    unsigned int block_num2 = block_num1 + 1;
    if(disk_read_block(block_num1,buf) == 0){
        if(disk_read_block(block_num2,buf+512) == 0){
            return 1;
        }
    }
    else{
        printf("open disk error\n");
    }
}
int my_disk_write_block(unsigned int block_num, char* buf){
    unsigned int block_num1 = 2*block_num;
    unsigned int block_num2 = block_num1 + 1;
    struct inode * test = (struct inode *)buf;
    printf("inode test :inode size = %d\n",test[0].size);
    printf("inode test :inode file_type = %d\n",test[0].file_type);
    printf("inode test :inode link = %d\n",test[0].link);
    for(int i = 0;i < 6;i ++){
        printf("inode test : inode block_point[%d] = %d\n",i,test[0].block_point[i]);
    }
    if(disk_write_block(block_num1,buf) == 0){
        if(disk_write_block(block_num2,buf + 512) == 0){
            return 1;
        }
    }
    else{
        printf("write disk error\n");
        exit(-1);
    }


}

int main(){
    struct inode root[32];//一次创建一整个block 的inode，然后一起存进去
    root[0].size = 0;//现在根目录之下还没有文件
    root[0].file_type = DIR_TYPE;
    root[0].link = 1;//其默认值好像是1
    for(int i = 0;i < BLOCK_IN_INODE;i ++){
        root[0].block_point[i] = 0;
    }
    my_disk_write_block(1,(char*)& root);
    exit(0);
}
// /home/dir1/dir2
