#include "global.h"
#include <stdio.h>
#include <stdlib.h>

//从输入获取用户命令
int get_cmd(char *buf,int nbuf)
{
    printf("> ");
    memset(buf,0,sizeof(buf));
    gets(buf);
    if(buf[0]==0)
        return 0;
    return 1;
}

//解析用户命令，返回指令码
int parse_cmd(char *buf)
{   
    char *cur_p=buf;
    //指令保存到command
    int i = 0;
    memset(command,0,sizeof(command));
    cur_p = get_token(cur_p,command,CMD_SIZE);
    if (cur_p==0)
        return -1;
    //获取命令
    int cmd_code;
    if (!strcmp(command,"ls"))
        cmd_code = LS;
    else if (!strcmp(command,"mkdir"))
        cmd_code = MKDIR;
    else if (!strcmp(command,"touch"))
        cmd_code = TOUCH;
    else if(!strcmp(command,"cp"))
        cmd_code = CP;
    else if(!strcmp(command,"shutdown"))
        cmd_code = SHUTDOWN; 
    else if(!strcmp(command,"rm"))
        cmd_code = RM; 
    else
    {
        printf("no such command\n");
        return -1;
    }

    //获取变量（文件路径）
    memset(argv,0,sizeof(argv));
    cur_p = get_token(cur_p,argv,ARGV_SIZE);  
    if (cur_p==0)
        return -1;    
    return cmd_code;
}

//从buf中获取一个单词，保存到token下，max_size限制了获取单词的大小，正确返回当前读头位置，否则返回0
char* get_token(char *buf,char *token,int max_size)
{
    char *p=buf;
    while (*p==' '||*p=='\t')
    {
        if (p<buf+BUF_MAX_SIZE)
            p++;
        else
        {
            printf("token too long!\n");
            return 0;            
        }     
    }
    int i = 0;
    while (*p!=' '&&*p!='\t'&&*p!='\0')
    {
        if (i>=max_size-1||p>=(buf+BUF_MAX_SIZE-1))
        {
            printf("token too long!\n");
            return 0;
        }
        token[i] = *p;
        p++;
        i++;  
    }
    token[i] = '\0';
    return p;
}

//从argv获取分段好的path，返回路径的深度
int get_path()
{
   int i = 0;
   int j = 0;
   while (argv[i]!='\0')
   {
        if (j>=10)
            return 0;
        while (argv[i]!='/'&&argv[i]!='\\'&&argv[i]!='\0')
        {
            path[j][i] = argv[i];
            i++;
        }
        path[j][i] = '\0';
        if(path[j][0]!='\0')// 形如 “/home”，path[0]可能会为空，得处理
            j++;   
   } 
   for (size_t i = 0; i < NAME_LEN; i++)
   {
       path[j][i]=0;
   }
   
   return j;
}

//获取节点
struct inode get_inode(struct inode last_inode,int level)
{
    struct inode tag_inode;
    int tag_inode_id;
    int disblk_id;
    struct dir_item cur_dir_item;
    int block;

    for (size_t i = 0; i < 6; i++)
    {
        block = last_inode.block_point[i];
        if (block==0)//注意block从1开始
            continue;
        disk_read_block(block*2,disk_buf);
        for (size_t j = 0; j < ITEM_PER_DISBLK; j++)
        {
            read_buf(disk_buf,&cur_dir_item,
                        j*(sizeof(struct dir_item)),
                        sizeof(struct dir_item));
            if(cur_dir_item.type!=DIR_TYPE)
                continue;
            if(cur_dir_item.valid==0)
                continue;
            //找到路径当前层级相应的dir_item，读取对应inode
            if (strcmp(cur_dir_item.name,path[level])==0)
            {
                tag_inode_id = cur_dir_item.inode_id;
                disblk_id = inode_to_disk(tag_inode_id);
                disk_read_block(disblk_id,disk_buf);
                read_buf(disk_buf,&tag_inode,
                            inode_in_disk(tag_inode_id),
                            sizeof(struct inode));
                return tag_inode;
            }          
        }
    }
    //如果当前目录下没有这个指定的路径，则返回size为0的inode
    tag_inode.size = 0;
    return tag_inode;
}

/**********************     shell命令     ******************************/
//显示文件目录
void exec_ls()
{
    struct inode cur_inode = current_path_inode;
    //读取找到的inode的对应block
    int dt_blk_id;
    struct dir_item dir;
    char list[48][121];
    int dir_num=0;
    int file_num = 0;
    for(int i=0;i<6;i++)
    {
        dt_blk_id = cur_inode.block_point[i];
        //前半部磁盘块
        disk_read_block(dt_blk_id*2,disk_buf);
        for(int j=0;j<ITEM_PER_DISBLK;j++)
        {
            read_buf(disk_buf,&dir,
                    j*(sizeof(struct dir_item)),
                    sizeof(struct dir_item));
            if (dir.valid==1)
            {
                if (dir.type==DIR_TYPE)   
                {
                    strcpy(list[dir_num],dir.name);
                    dir_num++;
                }
                else if (dir.type==FILE_TYPE)  
                {   
                    strcpy(list[47-file_num],dir.name);
                    file_num++;
                }
                             
            } 
        }
        //后半部磁盘块
        disk_read_block(dt_blk_id*2+1,disk_buf);
        for(int j=0;j<ITEM_PER_DISBLK;j++)
        {
            read_buf(disk_buf,&dir,
                    j*(sizeof(struct dir_item)),
                    sizeof(struct dir_item));
            if (dir.valid==1)
            {
                if (dir.type==DIR_TYPE)   
                {
                    strcpy(list[dir_num],dir.name);
                    dir_num++;
                }
                else if (dir.type==FILE_TYPE)  
                {
                    strcpy(list[47-file_num],dir.name);
                    file_num++;
                }           
            }  
        }
    }
    //打印
    for(int i=0;i<dir_num;i++)
    {
        printf("%s/ \n",list[i]);
    }
    for(int i=0;i<file_num;i++)
    {
        printf("%s \n",list[47-i]);
    }
}
//新建项目
void exec_mkdir()
{
    int level = get_path();
    if (level!=1)
    {
        printf("mkdir error\n");
        return;
    }
    //先检查sp_blk还能不能加目录inode
    if (super_block.free_inode_count==0||
            super_block.free_block_count==0)
    {
        printf("no more space\n");
        return;
    }

    dir_item_t dir;
    dir.valid = 1;
    int i,j;//i是在哪一个block_point，j是在磁盘块中第几条
    int disk_blk_part;//指明那个空的dir在第一个磁盘块还是第二个磁盘块
    //再检查当前目录下找一个放目录的地方，还要检查目录或者文件是否同名哦，名字以保存在path[0]中
    get_free_item(&dir,&i,&j,&disk_blk_part,DIR_TYPE,1);
    if (dir.type==ERROR)
    {
        printf("directory %s has already been created\n",path[0]);
        return;
    }
    int free_inode = get_free_inode();
    if (i==6&&dir.valid==1)
    {//当前目录下，目录项满了
        printf("current diretory has no more space\n");
        return;
    }
    else if (i<6&&dir.valid==1)
    {//前面的blk装满了，需要分配一个新的block
        int blk_id = get_free_blk();
        current_path_inode.block_point[i] = blk_id;
        set_block_map(blk_id,1);
        super_block.free_block_count--;
        //修改那个分配数据块的信息
        disk_read_block(blk_id*2,disk_buf);
        super_block.free_inode_count--;
        set_inode_map(free_inode,1);
        //dir信息
        dir.inode_id = free_inode;
        dir.type = DIR_TYPE;
        strcpy(dir.name,path[0]);
        //写入dir信息
        write_buf(disk_buf,&dir,0,sizeof(dir_item_t));
        disk_write_block(blk_id*2,disk_buf);
    }
    else
    {//有一个block没满，此时dir为那个空的目录项
        int blk_id = current_path_inode.block_point[i];
        super_block.free_inode_count--;
        set_inode_map(free_inode,1);
        //dir信息
        dir.inode_id = free_inode;
        dir.type = DIR_TYPE;    
        strcpy(dir.name,path[0]);
        dir.valid = 1;    
        write_buf(disk_buf,&dir,
                    j*sizeof(dir_item_t),
                    sizeof(dir_item_t));
        disk_write_block(blk_id*2+disk_blk_part,disk_buf);   
    }

    //新目录有新inode
    struct inode new;
    new = inode_init(free_inode,DIR_TYPE);
    //加入inode信息
    disk_read_block(inode_to_disk(free_inode),disk_buf);
    write_buf(disk_buf,&new,
                inode_in_disk(free_inode),
                sizeof(struct inode));
    disk_write_block(inode_to_disk(free_inode),disk_buf);
}
//创建文件
void exec_touch()
{
    int level = get_path();
    if (level!=1)
    {
        printf("touch error\n");
        return;
    }
    //先检查sp_blk还能不能加目录inode
    if (super_block.free_inode_count==0||
            super_block.free_block_count==0)
    {
        printf("no more space\n");
        return;
    }
    dir_item_t dir;
    dir.valid = 1;
    int i,j;//i是在哪一个block_point，j是在磁盘块中第几条
    int disk_blk_part;//指明那个空的dir在第一个磁盘块还是第二个磁盘块    
    get_free_item(&dir,&i,&j,&disk_blk_part,FILE_TYPE,1);
    if (dir.type==ERROR)
    {
        printf("file %s has already been created\n",path[0]);
        return;
    }

    int free_inode = get_free_inode();
    if (i==6&&dir.valid==1)
    {//当前目录下，目录项满了
        printf("current diretory has no more space\n");
        return;
    }
    else if (i<6&&dir.valid==1)
    {//前面的blk装满了，需要分配一个新的block
        int blk_id = get_free_blk();
        current_path_inode.block_point[i] = blk_id;
        set_block_map(blk_id,1);
        super_block.free_block_count--;
        //修改那个分配数据块的信息
        disk_read_block(blk_id*2,disk_buf);
        super_block.free_inode_count--;
        
        //dir信息
        dir.inode_id = free_inode;
        dir.type = FILE_TYPE;
        strcpy(dir.name,path[0]);
        write_buf(disk_buf,&dir,0,sizeof(dir_item_t));
        disk_write_block(blk_id*2,disk_buf);       
    }
    else
    {//有一个block没满，此时dir为那个空的目录项
        int blk_id = current_path_inode.block_point[i];
        super_block.free_inode_count--;
        //dir信息
        dir.inode_id = free_inode;
        dir.type = FILE_TYPE;    
        strcpy(dir.name,path[0]);
        dir.valid = 1;    
        write_buf(disk_buf,&dir,
                    j*sizeof(dir_item_t),
                    sizeof(dir_item_t));
        disk_write_block(blk_id*2+disk_blk_part,disk_buf);
    }

    //新文件有新inode
    struct inode new;
    new = inode_init(free_inode,FILE_TYPE);
    set_inode_map(free_inode,1);
    int new_free_blk = get_free_blk();
    new.block_point[0] = new_free_blk;
    set_block_map(new_free_blk,1);
    for(int k=1;k<6;k++)
    {
        new.block_point[k] = 0;
    }

    //加入inode信息
    disk_read_block(inode_to_disk(free_inode),disk_buf);
    write_buf(disk_buf,&new,
                inode_in_disk(free_inode),
                sizeof(struct inode));
    disk_write_block(inode_to_disk(free_inode),disk_buf); 
}

void exec_cp()
{
    int level = get_path();
    if (level!=1)
    {
        printf("mkdir error\n");
        return;
    }
    //先检查sp_blk还能不能加目录inode
    if (super_block.free_inode_count==0||
            super_block.free_block_count==0)
    {
        printf("no more space\n");
        return;
    }
    //检查当前目录下有没有这个文件
    int m,n,l;//file_name_exist要用，后面没用到
    if(file_name_exist(path[0],&m,&n,&l)==-1)
    {
        printf("no such file\n");
        return;
    }
    //给复制的文件取一个新名字
    char new_name[121];
    int k;
    for(k=0;path[0][k]!='\0';k++)
        new_name[k] = path[0][k];
    strcpy(new_name+k,"-copy");
    //检查复制的文件是不是也存在了
    int copy_num = 0;
    while (file_name_exist(new_name,&m,&n,&l)!=-1)
    {
        copy_num++;
        char num[4];
        num[0] = '-';
        itoa(copy_num,num+1,10);
        strcpy(new_name+k+5,num);
    }
    dir_item_t dir;
    dir.valid = 1;
    int i=0,j=0;//i是在哪一个block_point，j是在磁盘块中第几条
    int disk_blk_part;//指明那个空的dir在第一个磁盘块还是第二个磁盘块    
    get_free_item(&dir,&i,&j,&disk_blk_part,FILE_TYPE,0);
    int free_inode = get_free_inode();
    if (i==6&&dir.valid==1)
    {//当前目录下，目录项满了
        printf("current diretory has no more space\n");
        return;
    }
    else if (i<6&&dir.valid==1)
    {//前面的blk装满了，需要分配一个新的block
        int blk_id = get_free_blk();
        current_path_inode.block_point[i] = blk_id;
        set_block_map(blk_id,1);
        super_block.free_block_count--;
        //修改那个分配数据块的信息
        disk_read_block(blk_id*2,disk_buf);
        super_block.free_inode_count--;
        set_inode_map(free_inode,1);
        //dir信息
        dir.inode_id = free_inode;
        dir.type = FILE_TYPE;
        strcpy(dir.name,new_name);
        write_buf(disk_buf,&dir,0,sizeof(dir_item_t));
        disk_write_block(blk_id*2,disk_buf);       
    }
    else
    {//有一个block没满，此时dir为那个空的目录项
        int blk_id = current_path_inode.block_point[i];
        super_block.free_inode_count--;
        set_inode_map(free_inode,1);
        //dir信息
        dir.inode_id = free_inode;
        dir.type = FILE_TYPE;    
        strcpy(dir.name,new_name);
        dir.valid = 1;    
        write_buf(disk_buf,&dir,
                    j*sizeof(dir_item_t),
                    sizeof(dir_item_t));
        disk_write_block(blk_id*2+disk_blk_part,disk_buf);
    }

    //新文件有新inode
    struct inode new;
    new = inode_init(free_inode,FILE_TYPE);
    
    //加入inode信息
    disk_read_block(inode_to_disk(free_inode),disk_buf);
    write_buf(disk_buf,&new,
                inode_in_disk(free_inode),
                sizeof(struct inode));
    disk_write_block(inode_to_disk(free_inode),disk_buf);     

}

void exec_shutdown()
{
    sp_write();
    printf("bye \n");
}

void exec_rm()
{
    //检查指令
    int level = get_path();
    if (level!=1)
    {
        printf("rm error\n");
        return;
    }   
    //检查当前目录下有没有这个文件
    int block_point_idx,item_idx,disk_blk_part;
    if(file_name_exist(path[0],&block_point_idx,&item_idx,&disk_blk_part)==-1)
    {
        printf("no such file\n");
        return;
    }
    disk_read_block(current_path_inode.block_point[block_point_idx]*2+disk_blk_part,disk_buf);
    dir_item_t dir;
    read_buf(disk_buf,&dir,
            item_idx*sizeof(dir_item_t),
            sizeof(dir_item_t));
    if (dir.type!=FILE_TYPE)
    {
        printf("%s can't be remove\n",dir.name);
        return;
    }
    
    int rm_inode_idx = dir.inode_id;
    //修改dir_item
    dir.valid = 0;
    write_buf(disk_buf,&dir,
            item_idx*sizeof(dir_item_t),
            sizeof(dir_item_t));   
    disk_write_block(current_path_inode.block_point[block_point_idx]*2+disk_blk_part,disk_buf);
    //如果这是该块中的最后一项，就需要释放目录空间
    if (item_idx==0)
    {
        current_path_inode.block_point[block_point_idx]=0;
        current_path_inode.size -= sizeof(dir_item_t);
        set_block_map(block_point_idx,0);
    }
    
    //找到删除文件的inode
    int m = inode_to_disk(rm_inode_idx);
    int n = inode_in_disk(rm_inode_idx);
    struct inode rm_inode;
    disk_read_block(m,disk_buf);
    read_buf(disk_buf,&rm_inode,
                n*sizeof(struct inode),
                sizeof(struct inode));
    //接着释放那个文件inode下的block
    for(int k=0;k<6;k++)
    {
        set_block_map(rm_inode.block_point[k],0);
    }
    //接着释放该inode
    set_inode_map(rm_inode_idx,0);
}