/*****************************************************************/
/*                                                               */
/*   CASIO fx-9860G SDK Library                                  */
/*                                                               */
/*   File name : [ProjectName].c                                 */
/*                                                               */
/*   Copyright (c) 2006 CASIO COMPUTER CO., LTD.                 */
/*                                                               */
/*****************************************************************/
#include "fxlib.h"

#include "sound.h"

#define N_FRAMES 6571

//#define DEBUGOUT

#ifdef DEBUGOUT
#include <stdio.h>
void printx(int x, int y, const char *format, unsigned int value)
{
	char str[80];
	sprintf(str, format, value);
	locate(x, y);
	Print((const unsigned char *)str);
}
#endif // DEBUGOUT

//****************************************************************************
//  AddIn_main (Sample program main function)
//
//  param   :   isAppli   : 1 = This application is launched by MAIN MENU.
//                        : 0 = This application is launched by a strip in eACT application.
//
//              OptionNum : Strip number (0~3)
//                         (This parameter is only used when isAppli parameter is 0.)
//
//  retval  :   1 = No error / 0 = Error
//
//****************************************************************************
int AddIn_main(int isAppli, unsigned short OptionNum)
{
    unsigned int key;
    int result;

    Bdisp_AllClr_DDVRAM();

    result = dmasound_init(1920);
    if (result == 0) {
        locate(1, 1);
        Print((const unsigned char *)"Failed dmasound_init()");
        while (1) {
            GetKey(&key);
        }
    }
    if (!video_init()) {
        locate(1, 1);
        Print((const unsigned char *)"Failed video_init()");
        while (1) {
            GetKey(&key);
        }
    }
    if (!audio_init()) {
        locate(1, 1);
        Print((const unsigned char *)"Failed audio_init()");
        while (1) {
            GetKey(&key);
        }
    }

    while (1) {
        int frame;
#ifdef DEBUGOUT
        int n_timeout = 0;
        int n_remain = 0;
#endif //DEBUGOUT
        int r;
        for (frame = 0; frame < N_FRAMES; frame++) {
            // decode audio
            audio_dec();
            // decode video
            makefb();
            // abort playing if EXIT key is pressed
            if ((key = *(uint16_t *)0xa44b0006) != 0) {
                goto on_pressed_exit_key;
            }
            r = dmasound_get_remain();
#ifdef DEBUGOUT
            if (r == 0) n_timeout++;
            n_remain += r;
#endif //DEBUGOUT
            // wait until DMA buffer becomes empty
            
            while (r > 0) {
                r = dmasound_get_remain();
            }
            // fill audio DMA buffer
            dmasound_play();
            // update screen
            draw();
        }
on_pressed_exit_key:
        dma_stop();
#ifdef DEBUGOUT
        printx(1, 1, "frame  = %4d ", frame);
        if (frame > 0) {
            printx(1, 2, "timeout= %4d ", n_timeout - 1);
            printx(1, 3, "remain = %4d ", n_remain / frame);
        }
#endif //DEBUGOUT
        
        while (1) {
            GetKey(&key);
        }
    }

    return 1;
}




//****************************************************************************
//**************                                              ****************
//**************                 Notice!                      ****************
//**************                                              ****************
//**************  Please do not change the following source.  ****************
//**************                                              ****************
//****************************************************************************


#pragma section _BR_Size
unsigned long BR_Size;
#pragma section


#pragma section _TOP

//****************************************************************************
//  InitializeSystem
//
//  param   :   isAppli   : 1 = Application / 0 = eActivity
//              OptionNum : Option Number (only eActivity)
//
//  retval  :   1 = No error / 0 = Error
//
//****************************************************************************
int InitializeSystem(int isAppli, unsigned short OptionNum)
{
    return INIT_ADDIN_APPLICATION(isAppli, OptionNum);
}

#pragma section

