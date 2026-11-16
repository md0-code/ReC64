/* Windows GUI startup: no console allocation; preserve redirected I/O. */
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <shellapi.h>
#include <stdio.h>
#include <stdlib.h>

int main(int argc,char **argv);

int WINAPI WinMain(HINSTANCE instance,HINSTANCE previous,LPSTR command,int show){
    (void)instance;(void)previous;(void)command;(void)show;
    int count=0,result=2;
    LPWSTR *wide=CommandLineToArgvW(GetCommandLineW(),&count);
    if(!wide)return result;
    char **args=calloc((size_t)count+1,sizeof *args);
    if(!args){LocalFree(wide);return result;}
    for(int i=0;i<count;++i){
        const int bytes=WideCharToMultiByte(CP_UTF8,WC_ERR_INVALID_CHARS,wide[i],-1,NULL,0,NULL,NULL);
        if(bytes<=0||(args[i]=malloc((size_t)bytes))==NULL||
           !WideCharToMultiByte(CP_UTF8,WC_ERR_INVALID_CHARS,wide[i],-1,args[i],bytes,NULL,NULL))goto done;
    }
    result=main(count,args);
done:
    for(int i=0;i<count;++i)free(args[i]);
    free(args);LocalFree(wide);return result;
}
