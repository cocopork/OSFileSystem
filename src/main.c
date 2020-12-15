#include "global.h"
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>/* 提供类型pid_t的定义*/
#include <wait.h>
int main(){
    disk_init();
    int cmd_code = -1;
    while (1)
    {
        int f = get_cmd(cmd_buf,sizeof(cmd_buf));
        if(f==0)
            continue;
        cmd_code = parse_cmd(cmd_buf);
        if (cmd_code==-1)
        {//无效命令
            continue;
        }
        else
        {//有效命令

            switch (cmd_code)
            {
            case LS:
                exec_ls();
                break;
            case MKDIR:
                exec_mkdir();
                break;
            case TOUCH:
                exec_touch();
                break;
            case CP:
                exec_cp();
                break;
            case SHUTDOWN:
                exec_shutdown();
                return 0;
            case RM:
                exec_rm();
                break;
            default:
                break;
            }

        }
    }
    
}