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
void create_new_disk(sp_block *temp){

    //创建一个新的磁盘
    temp -> magic_num = MAGIC_NUM;
    temp -> free_block_count = 4096;
    temp -> free_inode_count = 1024;
    temp -> dir_inode_count = 0;
    //创建根目录的inode数组
    struct inode root[32];//一次创建一整个block 的inode，然后一起存进去
    root[0].size = 0;//现在根目录之下还没有文件
    root[0].file_type = DIR_TYPE;
    root[0].link = 1;//其默认值好像是1
    for(int i = 0;i < BLOCK_IN_INODE;i ++){
        root[0].block_point[i] = 0;
    }
    //修改超级块的内容
    temp -> free_block_count = temp -> free_block_count - 1;
    temp -> free_inode_count = temp -> free_inode_count - 1;
    temp -> inode_map[0] = (1 << 31) | (temp -> inode_map[0]);
    
    my_disk_write_block(0,(char *)temp);
    my_disk_write_block(1,(char*)& root);
    printf("Formating finished,good luck and have fun!\n");
}
void init(){
    if(open_disk() == 0){//打开文件无异常
        if(my_disk_read_block(0,buf) == 1){
            sp_block *temp = (sp_block*)buf;
            if((temp -> magic_num) != MAGIC_NUM){
                //打开时候的幻数显示不一样
                printf("Unknow file system found\n");
                printf("Format disk and rebuild a sucking file system?(y/n)\n");
                char ans;
                scanf("%c\n",&ans);
                if(ans == 'y'){
                    create_new_disk(temp);    
                }
                else{
                    printf("not your filesystem\n");
                    exit(0);
                }
            }
            else{
                //显示的幻数一样
                printf("file system found\ngood luck and have fun\n");
                printf("Format disk and rebuild a sucking file system?(y/n)\n");
                char ans;
                scanf("%c",&ans);
                if(ans == 'y'){
                    create_new_disk(temp);    
                }
                return;
            }
        }
        else{
            printf("read disk error \n");
            exit(-1);
        }
    }
    else{
        printf("open disk error\n");
        exit(-1);
    }
}
int my_address_split(char * address_cmd){
    //从命令行中读入的地址，现在将其分解
    int length = strlen(address_cmd);
    int num = 0;
    int temp = 0;
    address_list[0][0] = '/';
    address_list[0][1] = 0;
    num ++;
    for(int i = 1 ;i < length; i ++){//默认第一个一直为/，不做相对路径
        if(address_cmd[i] == '/'){
            address_list[num][temp ++] = 0;
            num ++;
            temp = 0;
        }
        else{
            address_list[num][temp ++] = address_cmd[i];
        }
    }
    num ++;
    return num;
}
int get_inode_num(char *name,int inode){
    /*函数用处：
        给出dir的inode,在其child中，找到孩子的name，返回name符合要求的Inode
    */
    //要先将这个标号的Inode从磁盘中取出来
    //因为：每个磁盘块存了32个inode的信息
    unsigned int inode_id_in_disk = inode/32 + INODE_BLOCK_BASE;
    unsigned int inode_id = inode % 32;
    printf("inode_id_in_disk = %d\n",inode_id_in_disk);
    my_disk_read_block(inode_id_in_disk,buf);
    struct inode * myinode = (struct inode *)buf;
    for(int i = 0;i < BLOCK_IN_INODE;i ++){
        printf("myinode[%d].block_point[%d] = %d\n",inode_id,i,myinode[inode_id].block_point[i]);
        if(myinode[inode_id].block_point[i] == 0){
            break;
        }
        int block_id_in_disk = myinode[inode_id].block_point[i] + DATA_BLOCK_BASE;
        
        my_disk_read_block(block_id_in_disk,buf);
        struct dir_item * block_list = (struct dir_item *)buf;
        unsigned int block_num = BLOCK_SIZE / sizeof(struct dir_item);
        printf("block_num = %d\n",block_num);
        for(int j = 0;j < block_num;j ++){
            if(strcmp(name,block_list[j].name) == 0){
                return block_list[j].inode_id;
            }
        }
    }
    return -1;
}
void is_mkdir(){
    char address_temp[1024];
    scanf("%s",&address_temp);
    printf("address repeat:%s\n",address_temp);
    int num = my_address_split(address_temp);
    int t = get_inode_num("/",1);
}
void work(){
    char cmd[1024];
    printf("==>");
    scanf("%s",&cmd);
    while(strcmp(cmd,"shutdown") != 0){
        if(strcmp("mkdir",cmd) == 0){
            is_mkdir();  
        }
    }
}
int main(){
    init();
    work();
    exit(0);
}
// /home/dir1/dir2
