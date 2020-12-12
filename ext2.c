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
void temp_test(){
   // printf("get in test\n");
   my_disk_read_block(1,buf);
    struct inode * test = (struct inode *)buf;
    printf("inode test :inode size = %d\n",test[0].size);
    printf("inode test :inode file_type = %d\n",test[0].file_type);
    printf("inode test :inode link = %d\n",test[0].link);
    for(int i = 0;i < 6;i ++){
        printf("inode test : inode block_point[%d] = %d\n",i,test[0].block_point[i]);
    }
    return ;

}
int my_disk_write_block(unsigned int block_num, char* buf){
    unsigned int block_num1 = 2*block_num;
    unsigned int block_num2 = block_num1 + 1;
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
    printf("get in creat_new_disk\n");
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
    for(int i = 0;i < 128;i ++){
        temp -> block_map[i] = 0;
    }
    for(int i = 1;i < 32;i ++){
        temp -> inode_map[i] = 0;
    }
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
                scanf("%c",&ans);
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
    if(length > 1){
        num ++;
    }
    return num;
}
unsigned int get_inode_num(char *name,int inode){
    /*函数用处：
        给出dir的inode,在其child中，找到孩子的name，返回name符合要求的Inode
    */
    //要先将这个标号的Inode从磁盘中取出来
    //因为：每个磁盘块存了32个inode的信息
    unsigned int inode_id_in_disk = inode/32 + INODE_BLOCK_BASE;
    unsigned int inode_id = inode % 32 - 1;//因为是从0开始计算的
    my_disk_read_block(inode_id_in_disk,buf);
    struct inode * myinode = (struct inode *)buf;
    for(int i = 0;i < BLOCK_IN_INODE;i ++){
        if(myinode[inode_id].block_point[i] == 0){
            break;
        }
        int block_id_in_disk = myinode[inode_id].block_point[i] + DATA_BLOCK_BASE;
        my_disk_read_block(block_id_in_disk,buf);
        struct dir_item * block_list = (struct dir_item *)buf;
        unsigned int block_num = BLOCK_SIZE / sizeof(struct dir_item);
        for(int j = 0;j < block_num;j ++){
            if((block_list[j].valid == 1)&&(strcmp(name,block_list[j].name) == 0)){
                return block_list[j].inode_id;
            }
        }
    }
    return -1;
}
int get_invaild_position(unsigned int num){
    int t = 0;
    for(uint32_t mask = 0x80000000;mask;mask = mask >> 1){
        if((num & mask) == 0){
            return t;
        }
        t ++;
    }
    return -1;
}
unsigned int get_new_block_id(int size){
    /*
    返回一个没有用过的，全新的inode的id
    */
   //在超级块中查询
   my_disk_read_block(0,buf);
   sp_block *super_block_temp = (sp_block*)buf;
   unsigned int ans = 0;
   for(int i = 0;i < size;i ++){
        if(size == 32)
            ans = get_invaild_position(super_block_temp->inode_map[i]);
        else if(size == 128)
            ans = get_invaild_position(super_block_temp->block_map[i]);
        if(ans >= 0)
            return ans;
    }
    return -1;

}
void setmap(unsigned int id,int type){
    /*
    函数作用：修改超级块里头的标志位
    type:1 data block 
         2 inode block
    */
    my_disk_read_block(0,buf);
    sp_block *super_block_temp = (sp_block*)buf;
    super_block_temp -> block_map[id / 32] = (1 << (32 - 1 - id % 32)) | (super_block_temp -> block_map[id / 32]);
    if(type == 1)
        super_block_temp ->free_block_count -= 1;
    else if(type == 2)
        super_block_temp ->free_inode_count -= 1;
    my_disk_write_block(0,(char *)&super_block_temp);
}
void init_new_data_block_for_use(unsigned int block_id){
/*
    函数作用：对于一个新的 data Block，对其进行初始化
    1 将其里面以前的数据清空
    2 将超级块里的标志位置1   
*/
    memset(buf,0,sizeof(buf));
    my_disk_write_block((block_id + DATA_BLOCK_BASE),buf);
    setmap(block_id,1);
}
void init_new_inode_block_for_use(unsigned int inode_id){
    /*
    函数作用：对于一个新的inode block,对其进行初始化
    1 将里面的数据清空
    2 将超级块里头的标志位置1
    */
    memset(buf,0,sizeof(buf));
    my_disk_write_block((inode_id + INODE_BLOCK_BASE),buf);
    setmap(inode_id,2);

}
int add_dir_data_block(unsigned int id_datablock,unsigned int id_inode,int type,char *name){
    /*
    1　将信息加到其父亲的data block中
    2  id 在这个data block中，我要将信息加在哪儿
    3　id_datablock　datablock 的id
    4  id_inode  inode 的id
    ５　type 类型 
    */
   my_disk_read_block((id_datablock + DATA_BLOCK_BASE),buf);
   struct dir_item * dir_list_block = (struct dir_item *)buf;
   for(int i = 0;i < 8;i ++){
       if(dir_list_block[i].valid == 0){
           //valid是０，可以用
           dir_list_block[i].inode_id = id_inode;
           dir_list_block[i].valid = 1;
           dir_list_block[i].type = type;
           strcpy(dir_list_block[i].name,name);
           my_disk_read_block((id_datablock + DATA_BLOCK_BASE),(char *)dir_list_block);
           return 1;
       }
   }
   return -1;
}

int find_place_for_dir(unsigned int id_block){
    /*
    在block里头找一块地
    */
    my_disk_read_block(id_block,buf);
    struct dir_item * dir_list_block = (struct dir_item *)buf;
    for(int i = 0;i < 8;i ++){
        if(dir_list_block[i].valid == 0){
            return i;
        }
    }
    return -1;
}
int add_dir_into_inode(char *name,int inode,char *dir_name){
    /*
    inode 父目录的inode号
    name  要新创的文件夹的名字
    */
    unsigned int inode_id_in_disk = inode/32 + INODE_BLOCK_BASE;
    unsigned int inode_id = inode % 32;//因为是从0开始计算的
    my_disk_read_block(inode_id_in_disk,buf);
    struct inode * myinode = (struct inode *)buf;
    unsigned int new_dir_indoe_id = get_new_block_id(32);//得到新的inode
    init_new_inode_block_for_use(new_dir_indoe_id);
    for(int i = 0;i < BLOCK_IN_INODE;i ++){
        //如果这个文件夹底下是空的,那么其block_num就会为0，那就可以直接为他创一个新的space
        if(myinode[inode_id].block_point[i] == 0){
            unsigned int new_block_id = get_new_block_id(128);
            init_new_data_block_for_use(new_block_id);    
            add_dir_data_block(new_block_id,new_dir_indoe_id,DIR_TYPE,dir_name);
            return 1;
        }
        else{
            //不空，来吧宝贝来找位置
            int pos = find_place_for_dir(myinode[inode_id].block_point[i]);
            if(pos >= 0){
                add_dir_data_block(myinode[inode_id].block_point[i],new_dir_indoe_id,DIR_TYPE,dir_name);
                return 1;
            }
        }
    }

}
void is_mkdir(){
    char address_temp[1024];
    char dir_name_in_inode[1024];
    scanf("%s",&address_temp);
    int num = my_address_split(address_temp);
    int inode_id = 0;//默认第1个inode是根目录
    for(int i = 1;i < num - 1;i ++){
        //默认第一个是/，然后从第二个开始遍历,而且最后一个是要创建的，所以不用遍历
        strcpy(dir_name_in_inode,address_list[i]);
        inode_id = get_inode_num(dir_name_in_inode,inode_id);
        if(inode_id == -1){
            //该找到的inode_id 没有找到
            printf("Can not create %s:No such file or dictionary\n",address_temp);
            return;
        }
    }
    if(add_dir_into_inode(address_list[num - 1],inode_id,&(address_list[num - 1])) == -1){
        printf("Can not create %s:space is not enough\n");
    }

}
void get_dir_from_inode(unsigned int id_block){
    my_disk_read_block((DATA_BLOCK_BASE + id_block),buf);
    struct dir_item * dir_list = (struct dir_item *)buf;
    for(int i = 0;i < 8;i ++){
        if(dir_list[i].valid == 1){
            printf("%s\n",dir_list[i].name);
        }
    }
    return ;
}
int show_dir(unsigned int inode){
    unsigned int inode_id_in_disk = inode/32 + INODE_BLOCK_BASE;
    unsigned int inode_id = inode % 32;//因为是从0开始计算的

    my_disk_read_block(inode_id_in_disk,buf);
    struct inode * myinode = (struct inode *)buf;
    for(int i = 0;i < BLOCK_IN_INODE;i ++){
        if(myinode[inode_id].block_point[i] == 0){
            return 1;
        }
        else{
            get_dir_from_inode(myinode[inode_id].block_point[i]);
        }
    }
    return;
}
void is_ls(){
    char address_temp[1024];
    char dir_name_in_inode[1024];
    scanf("%s",&address_temp);
    memset(address_list,0,sizeof(address_list));
    int num = my_address_split(address_temp);
    printf("num = %d\n",num);
    unsigned int inode_id = 0;//默认第1个inode是根目录
    for(int i = 1;i < num;i ++){
        strcpy(dir_name_in_inode,address_list[i]);
        inode_id = get_inode_num(dir_name_in_inode,inode_id);
        if(inode_id == -1){
            //该找到的inode_id 没有找到
            printf("No such file or dictionary\n",address_temp);
            return;
        }
    }
    printf(".\n..\n");
    show_dir(inode_id);


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
        printf("==> ");
        scanf("%s",&cmd);
    }
}

int main(){
    init();
    work();
    close_disk();
    exit(0);
}
// /home/dir1/dir2
