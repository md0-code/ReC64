/* One end-to-end input check, including a virtual SDL controller, no video. */
#include "keymap.h"
#include "../core/machines/c64.h"
#include "../app/pads.h"
#include "../app/joykeys.h"
#include <SDL.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#define CHECK(x) do{if(!(x)){fprintf(stderr,"line %d: %s (%s)\n",__LINE__,#x,SDL_GetError());return 1;}}while(0)
static int equal(rc_state_writer *a,rc_state_writer *b){return a->size==b->size&&!memcmp(a->data,b->data,a->size);}
int main(void){
    CHECK(SDL_Init(SDL_INIT_TIMER)==0);SDL_SetHint(SDL_HINT_JOYSTICK_ALLOW_BACKGROUND_EVENTS,"1");
    rc_pads pads;CHECK(rc_pads_open(&pads));CHECK(!SDL_WasInit(SDL_INIT_VIDEO));
    const int device=SDL_JoystickAttachVirtual(SDL_JOYSTICK_TYPE_GAMECONTROLLER,SDL_CONTROLLER_AXIS_MAX,SDL_CONTROLLER_BUTTON_MAX,0);CHECK(device>=0);
    SDL_Event event;while(SDL_PollEvent(&event))rc_pads_event(&pads,&event,NULL);
    CHECK(rc_pads_present(&pads,0));SDL_Joystick *virtual=SDL_JoystickOpen(device);CHECK(virtual);
    CHECK(!SDL_JoystickSetVirtualAxis(virtual,SDL_CONTROLLER_AXIS_LEFTX,24000));CHECK(!SDL_JoystickSetVirtualButton(virtual,SDL_CONTROLLER_BUTTON_A,1));SDL_JoystickUpdate();
    CHECK(rc_pads_joystick(&pads,0)==(RC_JOY_RIGHT|RC_JOY_FIRE));int x,y;unsigned buttons;rc_pads_paddles(&pads,0,&x,&y,&buttons);CHECK(x>200&&buttons==1);
    CHECK(!SDL_JoystickDetachVirtual(device));while(SDL_PollEvent(&event))rc_pads_event(&pads,&event,NULL);CHECK(!rc_pads_present(&pads,0)&&!rc_pads_joystick(&pads,0));SDL_JoystickClose(virtual);
    int keys[]={1,2,3,4,5,6};rc_joykeys joy;rc_joykeys_init(&joy,keys,0);CHECK(!rc_joykeys_key(&joy,1,1));CHECK(rc_joykeys_switch(&joy));CHECK(rc_joykeys_key(&joy,1,1)&&rc_joykeys_key(&joy,2,1));CHECK(!rc_joykeys_held(&joy));CHECK(rc_joykeys_key(&joy,2,0)&&rc_joykeys_held(&joy)==RC_JOY_UP);CHECK(!rc_joykeys_switch(&joy)&&!rc_joykeys_held(&joy));
    rc_config c;char error[256];const char *yaml="keyboard: {RUN STOP: F8, F2: F1}\ncontrollers: {joystick1: {type: mouse1351}, joystick2: {type: paddles, device: mouse}}\n";
    CHECK(rc_config_parse(yaml,strlen(yaml),"",&c,error,sizeof error));
    rc_rom_index roms;rc_rom_index_init(&roms);const char *folders[]={RC_ROM_DIR};CHECK(rc_rom_index_scan(&roms,folders,1));
    rc_machine *m=rc_machine_create(&c,&roms,error,sizeof error);CHECK(m&&!strcmp(rc_machine_family(m)->id,"c64"));const uint64_t frame=rc_machine_frame_period(m);CHECK(rc_machine_run(m,frame*200)==RC_RUN_OK);
    rc_keymap map;rc_keymap_init(&map,m,&c);
    /* Host Shift is overridden by symbolic punctuation, including @, which
     * has a dedicated key on the guest and different positions on PC layouts. */
    CHECK(rc_keymap_key(&map,1,"Left Shift",1));CHECK(rc_keymap_text(&map,"PRINT \"@![]\""));CHECK(rc_keymap_key(&map,2,"Return",1));
    while(map.count){CHECK(rc_machine_run(m,frame)==RC_RUN_OK);rc_keymap_frame(&map);}rc_keymap_clear(&map);CHECK(rc_machine_run(m,frame*3)==RC_RUN_OK);
    char text[2048];rc_machine_text(m,text,sizeof text);CHECK(strstr(text,"PRINT \"@![]\"")&&strstr(text,"\n@![]\n"));
    CHECK(rc_keymap_key(&map,3,"F1",1));CHECK(map.held[4]&&map.held[15]);CHECK(rc_keymap_key(&map,4,"Right Shift",1));CHECK(rc_keymap_key(&map,3,"F1",0));CHECK(!map.held[15]&&map.held[52]);CHECK(rc_keymap_key(&map,4,"Right Shift",0));CHECK(rc_keymap_key(&map,5,"F8",1)&&map.held[63]);rc_keymap_clear(&map);
    /* Read the ADC through the actual board and mux, with CPU stopped from
     * changing CIA selects. Buttons appear on the documented CIA bits. */
    CHECK(rc_machine_poke(m,0xdc02,255)&&rc_machine_poke(m,0xdc00,64));
    CHECK(rc_machine_poke(m,0xdc03,0));rc_machine_pointer(m,0,RC_CONTROL_MOUSE1351,7,-9,3);
    uint8_t value;CHECK(rc_machine_peek(m,0xdc01,&value)&&!(value&17));
    rc_machine_destroy(m);
    /* The board fixture gives cycle-granular access without guest keyscan. */
    unsigned options[]={0};rc_machine_setup setup={.family=&rc_c64_family,.model=0,.options=options,.roms=&roms};rc_c64 *board=rc_c64_create(&setup,error,sizeof error);CHECK(board);
    rc_mouse1351_move(&board->bus.mouse[0],7,-9,3);board->bus.control_type[0]=RC_CONTROL_MOUSE1351;
    rc_cia6526_write(&board->cia[0],2,255);rc_cia6526_write(&board->cia[0],0,64);
    for(unsigned i=0;i<512;++i)rc_bus_pot_tick(&board->bus);CHECK(board->bus.pot_value[0]==14&&board->bus.pot_value[1]==18);
    board->bus.control_type[1]=RC_CONTROL_PADDLES;board->bus.paddle[1][0]=33;board->bus.paddle[1][1]=201;rc_cia6526_write(&board->cia[0],0,128);
    for(unsigned i=0;i<512;++i)rc_bus_pot_tick(&board->bus);CHECK(rc_bus_read(&board->bus,0xd419,1)==33&&rc_bus_read(&board->bus,0xd41a,1)==201);CHECK(rc_sid_read(&board->sid,0)==201);
    for(unsigned i=0;i<219;++i)rc_bus_pot_tick(&board->bus);
    rc_state_writer saved;rc_sw_init(&saved);rc_sw_header(&saved,"c64","c64-pal",NULL,0);rc_c64_family.impl->save(board,&saved);CHECK(rc_sw_finish(&saved));
    CHECK(rc_c64_run(board,1234)==RC_RUN_OK);rc_state_writer expected;rc_sw_init(&expected);rc_sw_header(&expected,"c64","c64-pal",NULL,0);rc_c64_family.impl->save(board,&expected);CHECK(rc_sw_finish(&expected));
    rc_state_reader reader;CHECK(rc_sr_open(&reader,saved.data,saved.size,error,sizeof error)&&rc_c64_family.impl->load(board,&reader));CHECK(rc_c64_run(board,1234)==RC_RUN_OK);
    rc_state_writer actual;rc_sw_init(&actual);rc_sw_header(&actual,"c64","c64-pal",NULL,0);rc_c64_family.impl->save(board,&actual);CHECK(rc_sw_finish(&actual)&&equal(&expected,&actual));
    rc_c64_family.impl->destroy(board);rc_sw_free(&saved);rc_sw_free(&expected);rc_sw_free(&actual);rc_rom_index_free(&roms);rc_pads_close(&pads);SDL_Quit();
    puts("Symbolic typing, bindings, controller hotplug, keyboard joystick, 1351, paddles and state OK");return 0;
}
