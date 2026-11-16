#include "desktop.h"
#include "pads.h"
#include "../host/host.h"
#include "rc_version.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
static void usage(void) {
    puts("ReC64 " RC_VERSION "\nUsage: rec64 [--hidden] [--frames N] [--script FILE] [--control stdin|tcp:PORT]\n"
        "             [--screenshot FILE.png] [--audio FILE.wav] [--config FILE]\n"
        "             [--model MODEL] [--list-models] [--list-roms] [--version]\n"
        "F12 opens setup. The test model runs without firmware.");
}
int main(int argc,char **argv) {
    rc_config_set_key_check(rc_host_key_known);
    rc_desktop_options o={0};const char *yaml=NULL,*model=NULL;int list_roms=0;
    char default_yaml[RC_HOST_PATH];
    /* Pipes already belong to a headless run. Never attach a console or alert. */
    rc_config c;rc_config_defaults(&c);char error[512]="";
    for(int i=1;i<argc;++i) {
        const char *arg=argv[i];
        if(!strcmp(arg,"--hidden")) o.hidden=1;
        else if(!strcmp(arg,"--help")||!strcmp(arg,"-h")) { usage();return 0; }
        else if(!strcmp(arg,"--version")) { puts(RC_VERSION);return 0; }
        else if(!strcmp(arg,"--list-models")) {
            for(unsigned f=0;f<rc_family_count();++f) {
                const rc_family *family=rc_family_at(f);
                for(unsigned m=0;m<family->model_count;++m) printf("%s  %s\n",family->models[m].id,family->models[m].name);
            }
            return 0;
        } else if(!strcmp(arg,"--list-roms")) list_roms=1;
        else {
            if(i+1==argc) { fprintf(stderr,"%s needs a value\n",arg);return 2; }
            const char *v=argv[++i];
            if(!strcmp(arg,"--script")) o.script=v;
            else if(!strcmp(arg,"--control")) {
                if(strcmp(v,"stdin")) {
                    char *end=NULL;unsigned long port=0;
                    if(!strncmp(v,"tcp:",4)) port=strtoul(v+4,&end,10);
                    if(!end||end==v+4||*end||port<1||port>65535) { fputs("--control needs stdin or tcp:PORT (1-65535)\n",stderr);return 2; }
                }
                o.control=v;o.hidden=1;
            } else if(!strcmp(arg,"--screenshot")) o.screenshot=v;
            else if(!strcmp(arg,"--audio")) o.audio=v;
            else if(!strcmp(arg,"--config")) yaml=v;
            else if(!strcmp(arg,"--model")) model=v;
            else if(!strcmp(arg,"--frames")) {
                char *end=NULL;o.frames=strtoull(v,&end,10);
                if(!*v||*v=='-'||*end||!o.frames||o.frames>1000000000ull) { fputs("--frames needs a positive count up to 1000000000\n",stderr);return 2; }
            } else { fprintf(stderr,"Unknown option: %s\n",arg);return 2; }
        }
    }
    if(!yaml) {
        if(rc_host_file_exists("rec64.yaml")) yaml="rec64.yaml";
        else {
            char folder[RC_HOST_PATH];
            if(rc_host_program_folder(folder,sizeof folder)) {
                const int n=snprintf(default_yaml,sizeof default_yaml,"%srec64.yaml",folder);
                if(n>0&&(size_t)n<sizeof default_yaml&&rc_host_file_exists(default_yaml)) yaml=default_yaml;
            }
        }
    }
    if(yaml&&!rc_config_load(yaml,&c,error,sizeof error)) { fprintf(stderr,"%s\n",error);return 2; }
    if(model) { snprintf(c.model,sizeof c.model,"%s",model);c.option_count=0;c.media_count=0; }
    if(o.hidden&&!o.frames&&!o.script&&!o.control) { fputs("--hidden needs --frames, --script or --control\n",stderr);return 2; }
    rc_rom_index roms;rc_rom_index_init(&roms);
    char program_roms[RC_HOST_PATH]="",config_roms[RC_HOST_PATH]="",folder[RC_HOST_PATH];
    if(rc_host_program_folder(folder,sizeof folder)) snprintf(program_roms,sizeof program_roms,"%sroms",folder);
    if(yaml) {
        snprintf(folder,sizeof folder,"%s",yaml);
        char *slash=strrchr(folder,'/'),*backslash=strrchr(folder,'\\');
        if(backslash&&(!slash||backslash>slash)) slash=backslash;
        if(slash) slash[1]=0;else folder[0]=0;
        snprintf(config_roms,sizeof config_roms,"%sroms",folder);
    }
    const char *folders[]={"roms",program_roms,config_roms};
    if(!rc_rom_index_scan(&roms,folders,3)) { fputs("Cannot index ROMs\n",stderr);rc_rom_index_free(&roms);return 1; }
    if(list_roms) {
        char buffer[256];for(size_t i=0;i<roms.count;++i) printf("%s: %s\n",roms.files[i].path,rc_rom_file_describe(&roms,&roms.files[i],buffer,sizeof buffer));
        rc_rom_index_free(&roms);return 0;
    }
    rc_machine *m=rc_machine_create(&c,&roms,error,sizeof error);
    const int status=m?rc_desktop_run(&m,&c,&roms,&o,error,sizeof error):1;
    if(status) fprintf(stderr,"%s\n",error);
    rc_machine_destroy(m);rc_rom_index_free(&roms);return status;
}
