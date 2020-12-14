#include <string.h>
#include "disk.h"
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>

#define BLOCK_SIZE 1024 //一个块的字节数
#define ADDRESS_SIZE 20//命令中一共有多少个地址
#define BLOCK_IN_INODE 6//表示一个inode可以block_point的数量
#define MAGIC_NUM 0xabcdcba
#define DIR_TYPE 1
#define FILE_TYPE 2

#define SUPER_BLOCK_BASE 0
#define INODE_BLOCK_BASE 1
#define DATA_BLOCK_BASE 33

char address_list[ADDRESS_SIZE][128];

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
void finished(){
    close_disk();
}
void my_disk_read_block(unsigned int block_num, char* buf){
    unsigned int block_num1 = 2*block_num;
    unsigned int block_num2 = block_num1 + 1;
    disk_read_block(block_num1,buf);
    disk_read_block(block_num2,buf+512);
    return;
}
void my_disk_write_block(unsigned int block_num, char* buf){
    unsigned int block_num1 = 2*block_num;
    unsigned int block_num2 = block_num1 + 1;
    disk_write_block(block_num1,buf);
    disk_write_block(block_num2,buf + 512);
    return;
}
void init_new_disk(){//1
    char buf[1024];
    my_disk_read_block(0,buf);
    sp_block *sp_b = (sp_block*)buf;
    //先初始化超级块
    sp_b -> magic_num = MAGIC_NUM;
    sp_b -> free_block_count = 4096;
    sp_b -> free_inode_count = 1024 - 1;
    sp_b -> dir_inode_count = 1;
    for(int i = 0;i < 128;i ++)
        sp_b -> block_map[i] = 0;
    for(int i = 0;i < 32;i ++){
         sp_b -> inode_map[i] = 0;
    }
    sp_b -> inode_map[0] = (1 << 31) | (sp_b -> inode_map[0]);
    my_disk_write_block(0,(char *)sp_b);
    //初始化根目录inode
    struct inode dir_r[32];
    dir_r[0].size = 0;
    dir_r[0].file_type = DIR_TYPE;
    dir_r[0].link = 1;
    for(int i = 0;i < BLOCK_IN_INODE;i ++)
        dir_r[0].block_point[i] = 0;
    my_disk_write_block(1,(char *)dir_r);
}
int init_cmd(){
    printf("Format disk and rebuild a sucking file system?(y/n)\n");
    char c;
    scanf("%c",&c);
    if(c == 'y'||c == 'Y'){
        init_new_disk();
        return 1;
    }
    return 0;
}
void init(){
    open_disk();
    char buf[1024];
    my_disk_read_block(0,buf);
    sp_block *sp_b = (sp_block*)buf;
    if(sp_b -> magic_num == MAGIC_NUM){
        printf("file system found\n");
        init_cmd();
    }
    else{
        printf("UNKNOW file system found\n");
        if(init_cmd() == 0){
            printf("file system error\n");
            close_disk();
            exit(0);
        }
    }
    printf("Init finished,good luck and have fun!\n");
}
int my_address_split(char * address_cmd){
    int length = strlen(address_cmd);
    strcpy(address_list[0],"/");
    int num = 1;
    int temp = 0;
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
    if(length > 1){
        address_list[num][temp ++] = 0;
        num ++;
    }
    return num;
}
int get_inodeid_by_name(char * dir_name,unsigned int inode_id){
    //利用Inode_id找到Inode块
    char buf[1024];
    unsigned int in_disk = inode_id/32 + INODE_BLOCK_BASE;
    my_disk_read_block(in_disk,buf);
    struct inode * myinode = (struct inode *)buf;
    unsigned int id = inode_id % 32;//因为是从0开始计算的
    for(int i = 0;i < BLOCK_IN_INODE;i ++){
        if(myinode[id].block_point[i] == 0){
            break;
        }
        int block_id_in_disk = myinode[id].block_point[i] + DATA_BLOCK_BASE - 1;//这个point的标记从1开始，0用来标记没有东西了
        char buf2[1024];
        my_disk_read_block(block_id_in_disk,buf2);
        struct dir_item * block_list = (struct dir_item *)buf2;
        unsigned int block_num = BLOCK_SIZE / sizeof(struct dir_item);
        for(int j = 0;i < block_num;j ++){
            if((block_list[j].valid == 1)&&(strcmp(dir_name,block_list[j].name) == 0)){
                return block_list[j].inode_id;
            }
        }
    }
    return -1;

}
void show_dir(unsigned int inode){
    char buf[1024];
    unsigned int inode_id_in_disk = inode/32 + INODE_BLOCK_BASE;
    unsigned int inode_id = inode % 32;//因为是从0开始计算的
    my_disk_read_block(inode_id_in_disk,buf);
    struct inode * myinode = (struct inode *)buf;
    for(int i = 0;i < BLOCK_IN_INODE;i ++){
        if(myinode[inode_id].block_point[i] == 0){
            return ;
        }
        else{
            int block_id_in_disk = myinode[inode_id].block_point[i] + DATA_BLOCK_BASE - 1;
            char buf2[1024];
            my_disk_read_block(block_id_in_disk,buf2);
            struct dir_item * block_list = (struct dir_item *)buf2;
            unsigned int block_num = BLOCK_SIZE / sizeof(struct dir_item);
            for(int j = 0;j < block_num;j ++){
                if(block_list[j].valid == 1){
                    printf("%s\n",block_list[j].name);
                }
            }
        }
    }
    return ;
}
void is_ls(){
    char address_cmd[1024];
    scanf("%s",&address_cmd);
    memset(address_list,0,sizeof(address_list));
    int num = my_address_split(address_cmd);
    unsigned int inode_id = 0;//从根目录开始找起
    for(int i = 1;i < num;i ++){
        //  printf("before get inode id:%s\n",address_list[i]);
        inode_id = get_inodeid_by_name(address_list[i],inode_id);//穿父亲的id进去，找到孩子的id
        if(inode_id == -1){
            printf("No such file or dictionary\n",address_cmd);
            return;
        }
    }
    printf(".\n..\n");
    show_dir(inode_id);
}

int get_invaild_position(unsigned int num,int base){
    int t = 0;
    for(uint32_t mask = 0x80000000;mask;mask = mask >> 1){
        if((num & mask) == 0){
            return t + base*32;
        }
        t ++;
    }
    return -1;
}
unsigned int get_new_block_id(int size){
    char buf[1024];
    unsigned int ans = 0;
    my_disk_read_block(0,buf);
    sp_block *sp_b = (sp_block*)buf;
    for(int i = 0;i < size;i ++){
        if(size == 32){
            ans = get_invaild_position(sp_b->inode_map[i],i);
        }
        else if(size == 128){
            ans = get_invaild_position(sp_b->block_map[i],i);
        }
        if(ans >= 0){
            return ans;
        }
    }
    return -1;
}
void setmap(unsigned int id,int type){
    char buf[1024];
    my_disk_read_block(0,buf);
    sp_block *sp_b = (sp_block*)buf;
    if(type == 1){
        //待完善
        sp_b ->free_block_count -= 1;
        sp_b ->block_map[id/32] |= (1 << (32 - id%32 - 1));
    }
       
    else if(type == 2){
        sp_b ->free_inode_count -= 1;
        sp_b ->dir_inode_count += 1; 
        sp_b ->inode_map[id/32] |= (1 << (32 - id%32 - 1));
    }  
    my_disk_write_block(0,(char *)sp_b);
}
void init_new_inode_block_for_use(unsigned int inode,int type){
    char buf[1024];
    unsigned int inode_id_in_disk = inode/32 + INODE_BLOCK_BASE;
    unsigned int inode_id = inode % 32;

    my_disk_read_block(inode_id_in_disk,buf);
    struct inode * inode_list = (struct inode *)buf;
    inode_list[inode_id].size = 0;
    inode_list[inode_id].file_type = type;
    inode_list[inode_id].link = 1;
    for(int i = 0;i < BLOCK_IN_INODE;i ++){
        inode_list[inode_id].block_point[i] = 0;
    }
    my_disk_write_block(inode_id_in_disk,(char *)inode_list);
   
    setmap(inode,2);
}
void init_new_data_block_for_use(unsigned int block_id){
    char buf[1024];
    memset(buf,0,sizeof(buf));
    my_disk_write_block((block_id + DATA_BLOCK_BASE),buf);
    setmap(block_id,1);
}
void add_dir_data_block(unsigned int id_datablock,unsigned int id_inode,int type,char *name){
    char buf[1024];
    my_disk_read_block((id_datablock + DATA_BLOCK_BASE),buf);
    struct dir_item * dir_list_block = (struct dir_item *)buf;
    for(int i = 0;i < 8;i ++){
        if(dir_list_block[i].valid == 0){
            dir_list_block[i].inode_id = id_inode;
            dir_list_block[i].valid = 1;
            dir_list_block[i].type = type;
            strcpy(dir_list_block[i].name,name);
            my_disk_write_block((id_datablock + DATA_BLOCK_BASE),(char *)dir_list_block);
            return ;
        }
    }
}
int find_place_for_dir(unsigned int id_block_pointed){
    char buf[1024];
    int id_block = id_block_pointed + DATA_BLOCK_BASE;//传进来的时候已经-1了
    my_disk_read_block(id_block,buf);
    struct dir_item * dir_list_block = (struct dir_item *)buf;
    for(int i = 0;i < 8;i ++){
        if(dir_list_block[i].valid == 0){
            return i;
        }
    }
    return -1;
}
void add_dir(unsigned int inode, char *dir_name){
    char buf[1024];
    unsigned int inode_id_in_disk = inode/32 + INODE_BLOCK_BASE;
    // unsigned int inode_id_in_disk = 1;
    unsigned int inode_id = inode % 32;
    
    struct inode * myinode = (struct inode *)buf;
    unsigned int new_dir_indoe_id = get_new_block_id(32);//得到新的inode
    init_new_inode_block_for_use(new_dir_indoe_id,DIR_TYPE);
    my_disk_read_block(inode_id_in_disk,buf);

    for(int i = 0;i < BLOCK_IN_INODE;i ++){
        if(myinode[inode_id].block_point[i] == 0){
            unsigned int new_block_id = get_new_block_id(128);
            init_new_data_block_for_use(new_block_id);   
            add_dir_data_block(new_block_id,new_dir_indoe_id,DIR_TYPE,dir_name);
            myinode[inode_id].block_point[i] = 1 + new_block_id ;
            my_disk_write_block(inode_id_in_disk,(char *)myinode);
            return;
        }
        else{
            int id_data_blokc = myinode[inode_id].block_point[i] - 1;
            int pos = find_place_for_dir(id_data_blokc);
            if(pos >= 0){
                add_dir_data_block(id_data_blokc,new_dir_indoe_id,DIR_TYPE,dir_name);
                my_disk_write_block(inode_id_in_disk,(char *)myinode);
                return;
            }
        }
    }
    printf("Can not create:No space\n");
}
void is_mkdir(){
    char address_temp[1024];
    scanf("%s",&address_temp);
    int num = my_address_split(address_temp);
    
    char buf[1024];
    my_disk_read_block(1,buf);
    struct inode * test = (struct inode *)buf;

    unsigned int inode_id = 0;//默认第1个inode是根目录
    for(int i = 1;i < num - 1;i ++){
        //默认第一个是/，然后从第二个开始遍历,而且最后一个是要创建的，所以不用遍历
        inode_id = get_inodeid_by_name(address_list[i],inode_id);
        if(inode_id == -1){
            //该找到的inode_id 没有找到
            printf("Can not create %s:No such file or dictionary\n",address_temp);
            return;
        }
    }
    //找到了他父亲的
    add_dir(inode_id,address_list[num-1]);
}
void is_touch(){
    printf("test is touch\n");
}
void work(){
    char cmd[1024];
    printf("==> ");
    scanf("%s",&cmd);
    while(strcmp(cmd,"shutdown") != 0){
        if(strcmp("mkdir",cmd) == 0){
            is_mkdir(); 
        }
        else if(strcmp("ls",cmd) == 0){
            is_ls();
        }
        else if (strcmp("touch",cmd) == 0){
            is_touch();
        }
        printf("==> ");
        scanf("%s",&cmd);
    }
}
int main(){
    init();
    work();
    finished();

}