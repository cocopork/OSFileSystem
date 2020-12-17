#include "global.h"
#include "disk.h"

/**********************     硬盘     ******************************/
//硬盘初始化
int disk_init(){
    if(open_disk()==-1){
        printf("disk open error\n");
        return -1;
    }
    if (disk_read_block(SP_BLK_INDEX,disk_buf)==-1)
    {
        printf("read disk No.%d error\n",SP_BLK_INDEX);
        return -1;
    }
    //初始化根目录结构体
    root_dir_item.inode_id = ROOT_INODE_IDX;
    strcpy(root_dir_item.name,"root");
    root_dir_item.type = DIR_TYPE;
    root_dir_item.valid = 1;
    //获取幻数
    sp_read();
    int32_t magic_num=super_block.magic_num;
    

    if(magic_num==0){
        printf("start disk init...\n");
        root_init();
    }
    else if(magic_num!=MAGICNUM){
        printf("disk has been destroyed\n");
        return -1;
    }
    else//不是第一次进入系统
    {
        printf("welcome \n");
        current_path_inode = get_root();
        current_path_inode_idx = ROOT_INODE_IDX;
        cur_path_level = 1;
        strcpy(cur_path[0],"root");
    }
    
}

//读取超级块
void sp_read()
{
    // char *p = disk_buf;
    // int32_t temp;
    char *sp_blk_p = &super_block;
    int disk_buf_idx = 0;//缓冲区写头
    memset(disk_buf,0,sizeof(disk_buf));
    disk_read_block(SP_DISKBLK_IDX,disk_buf);
    read_buf(disk_buf,sp_blk_p,0,4*4);
    disk_buf_idx +=sizeof(uint32_t)*4;
    sp_blk_p += (4+128)*sizeof(uint32_t);//先写最后的inode_map
    read_buf(disk_buf,sp_blk_p,disk_buf_idx,32*sizeof(uint32_t));
    //后半部
    memset(disk_buf,0,sizeof(disk_buf));
    disk_read_block(SP_DISKBLK_IDX+1,disk_buf);
    sp_blk_p = &super_block;
    sp_blk_p +=4*sizeof(uint32_t);
    disk_buf_idx = 0;
    read_buf(disk_buf,sp_blk_p,disk_buf_idx,sizeof(uint32_t)*128);
    
}

//超级块写入磁盘
void sp_write()
{
    // char *p = disk_buf;
    char *sp_blk_p = &super_block;
    int disk_buf_idx = 0;//缓冲区写头
    memset(disk_buf,disk_buf_idx,sizeof(disk_buf));

    write_buf(disk_buf,sp_blk_p,0,4*4);
    disk_buf_idx +=sizeof(uint32_t)*4;
    sp_blk_p += (4+128)*sizeof(uint32_t);//先写最后的inode_map
    write_buf(disk_buf,sp_blk_p,disk_buf_idx,32*sizeof(uint32_t));

    disk_write_block(SP_DISKBLK_IDX,disk_buf);

    sp_blk_p = &super_block;
    sp_blk_p +=4*sizeof(uint32_t);
    disk_buf_idx = 0;
    memset(disk_buf,0,sizeof(disk_buf));
    write_buf(disk_buf,sp_blk_p,disk_buf_idx,sizeof(uint32_t)*128);
    disk_write_block(SP_DISKBLK_IDX+1,disk_buf);    
}

//根目录和超级块初始化
void root_init()
{
    //先修改inode所在磁盘块
    memset(disk_buf,0,sizeof(disk_buf));
    //初始化root inode
    struct inode root_inode;
    int block_num = DATA_DT_IDX;
    root_inode = inode_init(0,DIR_TYPE);

    //记录root为当前工作路径
    current_path_inode = root_inode;
    current_path_inode_idx = ROOT_INODE_IDX;
    cur_path_level = 1;
    strcpy(cur_path[0],"root");

    //写入inode
    write_buf(disk_buf,&root_inode,0,sizeof(struct inode));
    disk_write_block(inode_to_disk(root_dir_item.inode_id),disk_buf);


    //再修改数据块
    memset(disk_buf,0,sizeof(disk_buf));
    struct dir_item a,b;// .和..
    a.inode_id = root_dir_item.inode_id;
    strcpy(a.name,".");
    a.type = DIR_TYPE;
    a.valid = 1;
    write_buf(disk_buf,&a,0,sizeof(dir_item_t));

    b.inode_id = NULL;
    strcpy(b.name,"..");
    b.type = DIR_TYPE;
    b.valid = 1;
    write_buf(disk_buf,&b,sizeof(dir_item_t),sizeof(dir_item_t));
    disk_write_block(DATA_DISKBLK_IDX,disk_buf);

    //修改super_block
    set_inode_map(root_dir_item.inode_id,1);
    set_block_map(INODE_DT_IDX,1);
    set_block_map(DATA_DT_IDX,1);
    set_block_map(SP_BLK_INDEX,1);
    super_block.dir_inode_count = 1;
    super_block.magic_num = MAGICNUM;
    super_block.free_block_count = BLKNUM-3;
    super_block.free_inode_count = INODENUM-1;
    sp_write();
    disk_read_block(DATA_DISKBLK_IDX,disk_buf);
}

//初始化inode，返回一个新的inode，
//若不成功，Inode.type = ERROR
//free_inode 新inode序号
struct inode inode_init(int free_inode,int type)
{
    struct inode new;
    new.file_type = DIR_TYPE;  
    new.link = 1;
    if (type==DIR_TYPE)
    {
        new.size = sizeof(struct dir_item)*2;// .和..两个目录项的大小
        for (size_t i = 0; i < 6; i++)
        {
            new.block_point[i] = 0;
        }    

        int blk_id = get_free_blk();//找新block保存.和..
        set_block_map(blk_id,1);
        if (blk_id==-1)
        {
            new.file_type = ERROR;
            return new;
        }
        new.block_point[0] = blk_id;
        //写入.和..目录项
        disk_read_block(blk_id*2,disk_buf);
        struct dir_item a,b;// .和..
        a.inode_id = root_dir_item.inode_id;
        strcpy(a.name,".");
        a.type = DIR_TYPE;
        a.valid = 1;
        write_buf(disk_buf,&a,0,sizeof(dir_item_t));

        b.inode_id = NULL;
        strcpy(b.name,"..");
        b.type = DIR_TYPE;
        b.valid = 1;
        write_buf(disk_buf,&b,sizeof(dir_item_t),sizeof(dir_item_t));
        disk_write_block(blk_id*2,disk_buf);
    }
    else if (type==FILE_TYPE)
    {
        new.size = 0;
        for (size_t i = 0; i < 6; i++)
        {
            new.block_point[i] = 0;
        }   
        int blk_id = get_free_blk(); 
        set_block_map(blk_id,1);
        if (blk_id==-1)
        {
            new.file_type = ERROR;
            return new;
        }
        new.block_point[0] = blk_id;    
    }
    return new;
      
}

//获取根目录inode
struct inode get_root()
{
    struct inode root_inode;
    disk_read_block(INODE_DISKBLK_IDX,disk_buf);
    read_buf(disk_buf,&root_inode,ROOT_INODE_IDX,sizeof(struct inode));
    return root_inode;
}

//获取一个空闲block的数据块id
int get_free_blk()
{
    uint32_t temp;
    for(int i=0;i<128;i++)
    {
        temp = super_block.block_map[i];
        for(int j=0;j<32;j++)
        {
            if(i==0&&j==0)//前面几块被占用了
                j += DATA_DT_IDX;
            if ((temp&(1<<(31-j)))==0)
            {
                return i*128+j;
            }
            
        }
    }
    return -1;
}
//获取空闲inode
int get_free_inode()
{
    uint32_t temp;
    for(int i=0;i<32;i++)
    {
        temp = super_block.inode_map[i];
        for(int j=0;j<32;j++)
        {
            if ((temp&(1<<(31-j)))==0)
            {
                return i*32+j;
            }   
        }
    }
    return 0;
}

//获取current_path_inode空闲的目录项
//有同名文件，则dir_1.type=ERROR
void get_free_item(dir_item_t *dir,int *i,int *j,int *disk_blk_part,int type,int check_same)
{
    dir_item_t dir_1;
    dir_1.valid = 1;
    int i_1,j_1;//i是在哪一个block_point，j是在磁盘块中第几条
    int disk_blk_part_1;//指明那个空的dir在第一个磁盘块还是第二个磁盘块
    //再检查当前目录下找一个放目录的地方
    for(i_1=0;i_1<6;i_1++)
    {
        int blk_id = current_path_inode.block_point[i_1];
        if (blk_id==0)
            break;
        //前半部
        disk_read_block(blk_id*2,disk_buf);
        for(j_1=0;j_1<ITEM_PER_DISBLK;j_1++)
        {
            read_buf(disk_buf,&dir_1,
                    j_1*sizeof(dir_item_t),
                    sizeof(dir_item_t));
            //有同名文件
            if (strcmp(dir_1.name,path[0])==0&&type==dir_1.type&&dir_1.valid==1&&check_same==1)
            {
                dir->type = ERROR;
                return;
            }
            if (dir_1.valid==0){
                disk_blk_part_1 = 0;
                break;
            }       
        }
        if (dir_1.valid==0)
            break;
        //后半部
        disk_read_block(blk_id*2+1,disk_buf);
        for(j_1=0;j_1<ITEM_PER_DISBLK;j_1++)
        {
            read_buf(disk_buf,&dir_1,
                    j_1*sizeof(dir_item_t),
                    sizeof(dir_item_t));
            //有同名文件
            if (strcmp(dir_1.name,path[0])==0&&type==dir_1.type&&dir_1.valid==1&&check_same==1)
            {
                dir->type = ERROR;
                return;
            }
            if (dir_1.valid==0){
                disk_blk_part_1 = 1;
                break;
            }    
        }
        if (dir_1.valid==0)
            break;
    } 
    *dir = dir_1;
    *i = i_1;
    *j = j_1;
    *disk_blk_part = disk_blk_part_1;  
}

//file_name文件名，
//block_point_idx在当前目录哪一个block_point，
//item_idx在哪一个目录项
//disk_blk_part在哪一个磁盘块
//type文件类型还是目录类型
int file_name_exist(char file_name[121],int *block_point_idx,int *item_idx,int *disk_blk_part,int type)
{
    dir_item_t dir_1;
    dir_1.valid = 1;
    int i_1,j_1;//i是在哪一个block_point，j是在磁盘块中第几条
    int disk_blk_part_1;//指明那个dir在第一个磁盘块还是第二个磁盘块
    //查找同名item是否存在
    for(i_1=0;i_1<6;i_1++)
    {
        int blk_id = current_path_inode.block_point[i_1];
        if (blk_id==0)
            break;
        //前半部
        disk_read_block(blk_id*2,disk_buf);
        for(j_1=0;j_1<ITEM_PER_DISBLK;j_1++)
        {
            read_buf(disk_buf,&dir_1,
                    j_1*sizeof(dir_item_t),
                    sizeof(dir_item_t));
            //有同名文件
            if (strcmp(dir_1.name,file_name)==0&&dir_1.valid==1&&dir_1.type==type)
            {
                *block_point_idx = i_1;
                *item_idx = j_1;
                *disk_blk_part = 0;
                return 1;
            }      
        }
        //后半部
        disk_read_block(blk_id*2+1,disk_buf);
        for(j_1=0;j_1<ITEM_PER_DISBLK;j_1++)
        {
            read_buf(disk_buf,&dir_1,
                    j_1*sizeof(dir_item_t),
                    sizeof(dir_item_t));
            //有同名文件
            if (strcmp(dir_1.name,file_name)==0&&dir_1.valid==1&&dir_1.type==type)
            {
                *block_point_idx = i_1;
                *item_idx = j_1;
                *disk_blk_part = 0;
                return 1;
            }    
        }
    }
    return -1; 
}

/**********************     字节     ******************************/
//按字节写入，从dst的start开始写到src的开头，一共写size个字节
void write_buf(char *buf,void *src,int buf_start,int size)
{
    char *src_1 = (char*)src;
    for (size_t i = 0; i < size; i++)
    {
        buf[i+buf_start] = src_1[i];
    }
}
//按字节读出buf
void read_buf(char *buf,void* dst,int buf_start,int size)
{
    char *dst_1 = (char*)dst;
    for (size_t i = 0; i < size; i++)
    {
        dst_1[i] = buf[i+buf_start];
    }
}

//写32位字节
void write_buf_32(char *dst,int32_t src)
{
    char temp;
    for (size_t i = 3; i > 0; i--)
    {
        temp = (src>>8*i)&&0x0ff;
        *dst = temp;
        dst++;
    }
    temp = src&&0x0ff;
    *dst = temp;
}

//从buf读取4个字节
int32_t read_buf_32(char *p)
{
    int32_t temp = 0;  
    for (size_t i = 0; i < 3; i++)
    {
        temp |= *p;
        p++;
        temp = temp<<8;
    }
    temp |= *p;
    return temp;
}

//输入inode索引
void set_inode_map(int inode_idx,int bit)
{
    int bytes_index = inode_idx/32;
    int bit_index   = inode_idx%32;   
    if (bit==1)
    {
        byte_set(&super_block.inode_map[bytes_index],bit_index);
    }
    else if (bit==0)
    {
        byte_reset(&super_block.inode_map[bytes_index],bit_index);
    }
    
}

//输入数据块索引，设置相应位置为1
void set_block_map(int blk_idx,int bit)
{
    int bytes_index = blk_idx/32;
    int bit_index   = blk_idx%32;   
    if (bit==1)
    {
        byte_set(&super_block.block_map[bytes_index],bit_index);
    }
    else if (bit==0)
    {
        byte_reset(&super_block.block_map[bytes_index],bit_index);
    }    
}

//字节位置1
void byte_set(uint32_t *bytes,int index)
{//从左往右，从小到大
    uint32_t temp = *bytes;
    temp |= (1<<(31-index));
    *bytes = temp;
}
//字节位置0
void byte_reset(uint32_t *bytes,int index)
{
    uint32_t temp = *bytes;
    temp &= ~(1<<(31-index));
    *bytes = temp;    
}

/**********************     索引转换     ******************************/
//inode_id对应磁盘号
int inode_to_disk(int inode_idx)
{
    return inode_idx/16+INODE_DISKBLK_IDX;
}

//inode_id在磁盘号中的位置(0~41)
int inode_in_disk(int inode_idx)
{
    return inode_idx%16;
}

//整数转换
char* itoa(int num,char* str,int radix)
{
    char index[]="0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZ";//索引表
    unsigned unum;//存放要转换的整数的绝对值,转换的整数可能是负数
    int i=0,j,k;//i用来指示设置字符串相应位，转换之后i其实就是字符串的长度；转换后顺序是逆序的，有正负的情况，k用来指示调整顺序的开始位置;j用来指示调整顺序时的交换。
 
    //获取要转换的整数的绝对值
    if(radix==10&&num<0)//要转换成十进制数并且是负数
    {
        unum=(unsigned)-num;//将num的绝对值赋给unum
        str[i++]='-';//在字符串最前面设置为'-'号，并且索引加1
    }
    else unum=(unsigned)num;//若是num为正，直接赋值给unum
 
    //转换部分，注意转换后是逆序的
    do
    {
        str[i++]=index[unum%(unsigned)radix];//取unum的最后一位，并设置为str对应位，指示索引加1
        unum/=radix;//unum去掉最后一位
 
    }while(unum);//直至unum为0退出循环
 
    str[i]='\0';//在字符串最后添加'\0'字符，c语言字符串以'\0'结束。
 
    //将顺序调整过来
    if(str[0]=='-') k=1;//如果是负数，符号不用调整，从符号后面开始调整
    else k=0;//不是负数，全部都要调整
 
    char temp;//临时变量，交换两个值时用到
    for(j=k;j<=(i-1)/2;j++)//头尾一一对称交换，i其实就是字符串的长度，索引最大值比长度少1
    {
        temp=str[j];//头部赋值给临时变量
        str[j]=str[i-1+k-j];//尾部赋值给头部
        str[i-1+k-j]=temp;//将临时变量的值(其实就是之前的头部值)赋给尾部
    }
 
    return str;//返回转换后的字符串
 
}