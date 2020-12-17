#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include "disk.h"

//常量
#define BLKSIZE             1024    //数据块大小，单位字节
#define BLKNUM              4096
#define INODENUM            1024
#define SP_BLK_INDEX        0
#define MAGICNUM            0x12345678
#define BUF_MAX_SIZE        256
#define CMD_SIZE            10
#define ARGV_SIZE           240
#define ITEM_PER_DISBLK     4
//磁盘块索引
#define SP_DISKBLK_IDX      0
#define INODE_DISKBLK_IDX   2
#define DATA_DISKBLK_IDX    128

//数据块索引
#define SP_DT_IDX           0
#define INODE_DT_IDX        1
#define DATA_DT_IDX         64

//命令标号
#define LS          1//展示读取文件夹内容
#define MKDIR       2//创建文件夹
#define TOUCH       3//创建文件
#define CP          4//复制文件
#define SHUTDOWN    5//关闭系统
#define RM          6//删除文件
#define CD          7//更改路径
//目录
#define ROOT_INODE_IDX      0//根目录inode索引号
#define FILE_TYPE           1//文件类
#define DIR_TYPE            2//目录类
#define ERROR               3//错误        
#define NAME_LEN            121



//结构体定义
//超级块
typedef struct super_block {
    int32_t magic_num;                  // 幻数
    int32_t free_block_count;           // 空闲数据块数
    int32_t free_inode_count;           // 空闲inode数
    int32_t dir_inode_count;            // 目录inode数
    uint32_t block_map[128];            // 数据块占用位图
    uint32_t inode_map[32];             // inode占用位图
} sp_block;

//inode索引
struct inode {
    uint32_t size;              // 文件大小
    uint16_t file_type;         // 文件类型（文件/文件夹）
    uint16_t link;              // 连接数
    uint32_t block_point[6];    // 数据块指针
};

//目录项
typedef struct dir_item {               // 目录项一个更常见的叫法是 dirent(directory entry)
    uint32_t inode_id;          // 当前目录项表示的文件/目录的对应inode
    uint16_t valid;             // 当前目录项是否有效 
    uint8_t type;               // 当前目录项类型（文件/目录）
    char name[121];             // 目录项表示的文件/目录的文件名/目录名
}dir_item_t;


//全局变量
struct super_block super_block;     //超级块
char disk_buf[512];                 //磁盘读写缓冲区
char data_buf[1024];                //一个数据块缓冲区，读写磁盘分两次
char cmd_buf[BUF_MAX_SIZE];         //shell命令缓冲区
char command[CMD_SIZE];             //命令内容
char argv[ARGV_SIZE];               //命令变量，即路径名称
char path[10][121];                 //路径名称
struct dir_item root_dir_item;      //根目录结构体

char cur_path[10][121];
int cur_path_level;
struct inode current_path_inode;    //当前工作路径下inode节点
uint32_t current_path_inode_idx;    //当前工作路径下inode索引号

//shell
int get_cmd(char *buf,int nbuf);
struct inode get_inode(struct inode last_inode,int level);
int parse_cmd(char *buf);
int get_path();
void exec_ls();
void exec_mkdir();
void exec_touch();
void exec_cp();
void exec_shutdown();
void exec_rm();
void exec_cd();
//utils
//硬盘
int disk_init();                 //硬盘初始化，将超级块记录数据写入虚拟盘
void root_init();               //根目录和超级块初始化
struct inode inode_init(int free_inode,int type);
void sp_read();
void sp_write();
//其他工具
char* get_token(char *buf,char *token,int max_size);
struct inode get_root();
int get_free_blk();
int get_free_inode();
void get_free_item(dir_item_t *dir,int *i,int *j,int *disk_blk_part,int type,int check_same);
int file_name_exist(char file_name[121],int *block_point_idx,int *item_idx,int *disk_blk_part,int type);
void write_buf(char *buf,void *src,int buf_start,int size);
void read_buf(char *buf,void* dst,int buf_start,int size);
//字节
void set_inode_map(int inode_idx,int bit);
void set_block_map(int blk_idx,int bit);
//索引转换
int inode_to_disk(int inode_idx);
int inode_in_disk(int inode_idx);
//整数转字符串
char* itoa(int num,char* str,int radix);