/*************************************************************************
 *
*    Used with ICCARM and AARM.
 *
 *    (c) Copyright IAR Systems 2008
 *
 *    File name   : main.c
 *    Description : Main module
 *
 *    History :
 *    1. Date        : 4, August 2008
 *       Author      : Stanimir Bonev
 *       Description : Create
 *
 *  This example project shows how to use the IAR Embedded Workbench for ARM
 * to develop code for the IAR LPC-1788 board. It shows basic use of the I/O,
 * the timer and the interrupt controllers.
 *  It starts by blinking USB Link LED.
 *
 * Jumpers:
 *  EXT/JLINK  - depending of power source
 *  ISP_E      - unfilled
 *  RST_E      - unfilled
 *  BDS_E      - unfilled
 *  C/SC       - SC
 *
 * Note:
 *  After power-up the controller get clock from internal RC oscillator that
 * is unstable and may fail with J-Link auto detect, therefore adaptive clocking
 * should always be used. The adaptive clock can be select from menu:
 *  Project->Options..., section Debugger->J-Link/J-Trace  JTAG Speed - Adaptive.
 *
 *    $Revision: 24636 $
 **************************************************************************/


#include "logo.h"
#include "Cursor.h"
#include "sensor_smb380.h"
#include "sensor_mma7455.h"
#include "lpc_types.h"
#include "system_LPC177x_8x.h"
#include "lpc177x_8x_clkpwr.h"
#include "lpc177x_8x_timer.h"
#include "lpc177x_8x_pinsel.h"
#include "lpc177x_8x_lcd.h"
#include "lpc177x_8x_pwm.h"
#include "lpc177x_8x_adc.h"
#include "bsp.h"
#if (_CURR_USING_BRD == _IAR_OLIMEX_BOARD)
#include "sdram_k4s561632j.h"
#elif (_CURR_USING_BRD == _QVGA_BOARD)
#include "sdram_mt48lc8m32lfb5.h"
#elif (_CURR_USING_BRD == _EA_PA_BOARD)
#include "sdram_is42s32800d.h"
#endif
#include "tsc2046.h"

/** @defgroup LCD_Demo	LCD Demo
 * @ingroup LCD_Examples
 * @{
 */

#if (_CUR_USING_LCD == _RUNNING_LCD_GFT035A320240Y)
#define   LOGO_DISPLAYED                (1)
#define   TCS_USED						(0)
#define ACCEL_SENSOR_USED               (1)

/* LCD Config */
#define LCD_REFRESH_FREQ     (50HZ)
#define LCD_H_SIZE           320
#define LCD_H_PULSE          30
#define LCD_H_FRONT_PORCH    20
#define LCD_H_BACK_PORCH     38
#define LCD_V_SIZE           240
#define LCD_V_PULSE          3
#define LCD_V_FRONT_PORCH    5
#define LCD_V_BACK_PORCH     15
#define LCD_PIX_CLK          (6.5*1000000l)

/* PWM */
#define _PWM_NO_USED    1
#define _PWM_CHANNEL_NO 2
#define _PWM_PORT_NUM   2
#define _PWM_PIN_NUM    1
#define _PWM_PIN_FUNC_NUM 1

#else  /*(_CUR_USING_LCD != _RUNNING_LCD_GFT035A320240Y)*/

#define   LOGO_DISPLAYED                (0)
#define   TCS_USED						(1)
#define   ACCEL_SENSOR_USED               (0)

/* LCD Config */
#define LCD_REFRESH_FREQ     (30HZ)
#define LCD_H_SIZE           240
#define LCD_H_PULSE          2
#define LCD_H_FRONT_PORCH    10
#define LCD_H_BACK_PORCH     28
#define LCD_V_SIZE           320
#define LCD_V_PULSE          2
#define LCD_V_FRONT_PORCH    1
#define LCD_V_BACK_PORCH     2
#define LCD_PIX_CLK          (8200000l)

/* PWM */
#define _PWM_NO_USED    1
#define _PWM_CHANNEL_NO 1
#define _PWM_PORT_NUM   1
#define _PWM_PIN_NUM    18
#define _PWM_PIN_FUNC_NUM 2

/* Touch Screen Config */
#if (TSC2046_CONVERSION_BITS == 8)
#define TOUCH_AD_LEFT    240
#define TOUCH_AD_RIGHT   10
#define TOUCH_AD_TOP     240
#define TOUCH_AD_BOTTOM  16
#else
#define TOUCH_AD_LEFT    3686
#define TOUCH_AD_RIGHT   205
#define TOUCH_AD_TOP     3842
#define TOUCH_AD_BOTTOM  267
#endif

#endif  /*(_CUR_USING_LCD == _RUNNING_LCD_GFT035A320240Y)*/

#define LCD_VRAM_BASE_ADDR_UPPER 	((uint32_t)SDRAM_BASE_ADDR + 0x00100000)
#define LCD_VRAM_BASE_ADDR_LOWER 	(LCD_VRAM_BASE_ADDR_UPPER + 1024*768*4)
#define LCD_CURSOR_BASE_ADDR 	    ((uint32_t)0x20088800)

#if (LOGO_BPP == 24)
#define MAKE_COLOR(red,green,blue)  (blue<<16|green<<8|red)
#elif (LOGO_BPP == 16)
#define MAKE_COLOR(red,green,blue)  (blue << 10 | green << 5 | red)
#elif (LOGO_BPP == 8)
#define MAKE_COLOR(red,green,blue)  (blue << 5 | green << 3 | red)
#else
#define MAKE_COLOR(red,green,blue)  (red)
#endif

#define _BACK_LIGHT_BASE_CLK (1000/2)

uint8_t Smb380Id, Smb380Ver;
#if (_CURR_USING_BRD == _IAR_OLIMEX_BOARD)
#define  AccSensor_Data_t  SMB380_Data_t
#define  AccSensor_Init    SMB380_Init
#define  AccSensor_GetData SMB380_GetData
#else
#define  AccSensor_Data_t  MMA7455_Data_t
#define  AccSensor_Init    MMA7455_Init
#define  AccSensor_GetData MMA7455_GetData
#endif

extern uint8_t * LogoStream;
#if (LOGO_BPP == 2)
extern uint8_t * LogoPalette;
#endif
extern void InitLcdController (void);

Bmp_t LogoPic =
{
  320,
  240,
  LOGO_BPP,
  BMP_BYTES_PP,
  #if (LOGO_BPP == 2)
  (uint8_t *)&LogoPalette,
  #else
  NULL,
  #endif
  (uint8_t *)&LogoStream,
  ( uint8_t *)"Logos picture"
};


void DelayNS(uint32_t dly)
{
	uint32_t i = 0;

	for ( ; dly > 0; dly--)
		for (i = 0; i < 5000; i++);
}

/*************************************************************************
 * Function Name: SetBackLight
 * Parameters: level     Backlight value
 *
 * Return: none
 *
 * Description: Set LCD backlight
 *
 *************************************************************************/
void SetBackLight(uint32_t level)
{
  PWM_MATCHCFG_Type PWMMatchCfgDat;
  PWM_MatchUpdate(_PWM_NO_USED, _PWM_CHANNEL_NO, level, PWM_MATCH_UPDATE_NOW);
  PWMMatchCfgDat.IntOnMatch = DISABLE;
  PWMMatchCfgDat.MatchChannel = _PWM_CHANNEL_NO;
  PWMMatchCfgDat.ResetOnMatch = DISABLE;
  PWMMatchCfgDat.StopOnMatch = DISABLE;
  PWM_ConfigMatch(_PWM_NO_USED, &PWMMatchCfgDat);


  /* Enable PWM Channel Output */
  PWM_ChannelCmd(_PWM_NO_USED, _PWM_CHANNEL_NO, ENABLE);

  /* Reset and Start counter */
  PWM_ResetCounter(_PWM_NO_USED);

  PWM_CounterCmd(_PWM_NO_USED, ENABLE);

  /* Start PWM now */
  PWM_Cmd(_PWM_NO_USED, ENABLE);
}

/*************************************************************************
 * Function Name: GetBacklightVal
 * Parameters: none
 *
 * Return: none
 *
 * Description: Get backlight value from user
 *
 *************************************************************************/
uint32_t GetBacklightVal (void) {
  uint32_t val;
  uint32_t backlight_off, pclk;

  ADC_StartCmd(LPC_ADC, ADC_START_NOW);

  while (!(ADC_ChannelGetStatus(LPC_ADC, BRD_ADC_PREPARED_CHANNEL, ADC_DATA_DONE)));

  val = ADC_ChannelGetData(LPC_ADC, BRD_ADC_PREPARED_CHANNEL);

  val = (val >> 7) & 0x3F;

  pclk = CLKPWR_GetCLK(CLKPWR_CLKTYPE_PER);
  backlight_off = pclk/(_BACK_LIGHT_BASE_CLK*20);
  val =  val* (pclk*9/(_BACK_LIGHT_BASE_CLK*20))/0x3F;

  return backlight_off + val;
}
/*************************************************************************
 * Function Name: lcd_colorbars
 * Parameters: none
 *
 * Return: none
 *
 * Description: Draw color bar on screen
 *
 *************************************************************************/
void lcd_colorbars(void)
{
  LcdPixel_t color = 0;
  UNS_16 curx = 0, cury = 0, ofsx, ofsy;
  uint16_t red,green,blue;
  uint16_t maxclr;
 
  #if (LOGO_BPP == 24)
  maxclr = 256;
  #elif (LOGO_BPP == 16)
  maxclr = 32;
  #else
  maxclr = 4;
  #endif
  ofsx = (LCD_H_SIZE + maxclr - 1)/maxclr;
  ofsy = LCD_V_SIZE/3;
  
  #if (LOGO_BPP >= 8)
  // Red bar
  green = blue = 0;
  for(red = 0; red < maxclr; red++)
  {
      color = MAKE_COLOR(red,green,blue);
      LCD_FillRect (LCD_PANEL_UPPER,curx, curx+ofsx, cury, cury+ofsy, color);
      curx+=ofsx;
      if(curx>= LCD_H_SIZE)
      {
        curx = 0;
        break;
      }
  }
  
   // green bar
  red = blue = 0;
  curx = 0;
  cury += ofsy;
  for(green = 0; green < maxclr; green++)
  {
      color = MAKE_COLOR(red,green,blue);
      LCD_FillRect (LCD_PANEL_UPPER,curx, curx+ofsx, cury, cury+ofsy, color);
      curx+=ofsx;
      if(curx>= LCD_H_SIZE)
      {
        curx = 0;
        break;
      }
  }
  
  // blue bar
  green = blue = 0;
  curx = 0;
  cury += ofsy;
  for(blue = 0; blue < maxclr; blue++)
  {
      color = MAKE_COLOR(red,green,blue);
      LCD_FillRect (LCD_PANEL_UPPER,curx, curx+ofsx, cury, cury+ofsy, color);
      curx+=ofsx;
      if(curx>= LCD_H_SIZE)
      {
        curx = 0;
        break;
      }
  }
 #else
  for(color = 0; color < maxclr; color++)
  {
      LCD_FillRect (LCD_PANEL_UPPER,curx, curx+ofsx, cury, cury+LCD_V_SIZE-1, color);
      curx+=ofsx;
      if(curx>= LCD_H_SIZE)
      {
        curx = 0;
        break;
      }
  }
 #endif    
}

/*************************************************************************
 * Function Name: c_entry
 * Parameters: none
 *
 * Return: none
 *
 * Description: entry
 *
 *************************************************************************/
  void c_entry(void)
{
  uint32_t i, pclk;
#if LOGO_DISPLAYED
  uint32_t xs, ys;
#endif
#if ACCEL_SENSOR_USED
  AccSensor_Data_t XYZT;
#endif
  uint32_t backlight;
  PWM_TIMERCFG_Type PWMCfgDat;
  PWM_MATCHCFG_Type PWMMatchCfgDat;
  LCD_Cursor_Config_Type cursor_config;
  LCD_Config_Type lcd_config;
  int cursor_x = 0, cursor_y = 0;
#if LOGO_DISPLAYED
  uint32_t start_pix_x, start_pix_y, pix_ofs;
#endif
#if TCS_USED
  TSC2046_Init_Type tsc_config;
#endif
  
  /***************/
  /** Initialize ADC */
  /***************/
  PINSEL_ConfigPin (BRD_ADC_PREPARED_CH_PORT,
  					BRD_ADC_PREPARED_CH_PIN,
  					BRD_ADC_PREPARED_CH_FUNC_NO);
  PINSEL_SetAnalogPinMode(BRD_ADC_PREPARED_CH_PORT,BRD_ADC_PREPARED_CH_PIN,ENABLE);

  ADC_Init(LPC_ADC, 400000);
  ADC_IntConfig(LPC_ADC, BRD_ADC_PREPARED_INTR, DISABLE);
  ADC_ChannelCmd(LPC_ADC, BRD_ADC_PREPARED_CHANNEL, ENABLE);

  /***************/
  /** Initialize LCD */
  /***************/
  LCD_Enable (FALSE);
    
  // SDRAM Init	= check right board to avoid linking error
  SDRAMInit();
  
  lcd_config.big_endian_byte = 0;
  lcd_config.big_endian_pixel = 0;
  lcd_config.hConfig.hbp = LCD_H_BACK_PORCH;
  lcd_config.hConfig.hfp = LCD_H_FRONT_PORCH;
  lcd_config.hConfig.hsw = LCD_H_PULSE;
  lcd_config.hConfig.ppl = LCD_H_SIZE;
  lcd_config.vConfig.lpp = LCD_V_SIZE;
  lcd_config.vConfig.vbp = LCD_V_BACK_PORCH;
  lcd_config.vConfig.vfp = LCD_V_FRONT_PORCH;
  lcd_config.vConfig.vsw = LCD_V_PULSE;
  lcd_config.panel_clk   = LCD_PIX_CLK;
  lcd_config.polarity.active_high = 1;
  lcd_config.polarity.cpl = LCD_H_SIZE;
  lcd_config.polarity.invert_hsync = 1;
  lcd_config.polarity.invert_vsync = 1;
  lcd_config.polarity.invert_panel_clock = 1;
  lcd_config.lcd_panel_upper =  LCD_VRAM_BASE_ADDR_UPPER;
  lcd_config.lcd_panel_lower =  LCD_VRAM_BASE_ADDR_LOWER;
  #if (LOGO_BPP == 24)
  lcd_config.lcd_bpp = LCD_BPP_24;
  #elif (LOGO_BPP == 16)
  lcd_config.lcd_bpp = LCD_BPP_16;
  #elif (LOGO_BPP == 2)
  lcd_config.lcd_bpp = LCD_BPP_2;
  #else
  while(1);
  #endif
  lcd_config.lcd_type = LCD_TFT;
  lcd_config.lcd_palette = LogoPic.pPalette;
  lcd_config.lcd_bgr = FALSE;

  LCD_Init (&lcd_config);
  #if ((_CUR_USING_LCD == _RUNNING_LCD_QVGA_TFT) || (_CUR_USING_LCD == _RUNNING_LCD_G240320LTSW))
  InitLcdController();
  #endif

  #if TCS_USED
  tsc_config.ad_left = TOUCH_AD_LEFT;
  tsc_config.ad_right = TOUCH_AD_RIGHT;
  tsc_config.ad_top = TOUCH_AD_TOP;
  tsc_config.ad_bottom = TOUCH_AD_BOTTOM;
  tsc_config.lcd_h_size = LCD_H_SIZE;
  tsc_config.lcd_v_size = LCD_V_SIZE;
  tsc_config.swap_xy = 1;
  InitTSC2046(&tsc_config);
  #endif

  LCD_SetImage(LCD_PANEL_UPPER, NULL);
  LCD_SetImage(LCD_PANEL_LOWER, NULL);
 
   /***************/
  /* Initialize PWM */
  /***************/
  PWMCfgDat.PrescaleOption = PWM_TIMER_PRESCALE_TICKVAL;
  PWMCfgDat.PrescaleValue = 1;
  PWM_Init(_PWM_NO_USED, PWM_MODE_TIMER, (void *) &PWMCfgDat);

  PINSEL_ConfigPin (_PWM_PORT_NUM, _PWM_PIN_NUM, _PWM_PIN_FUNC_NUM);
  PWM_ChannelConfig(_PWM_NO_USED, _PWM_CHANNEL_NO, PWM_CHANNEL_SINGLE_EDGE);

  // Set MR0
  pclk = CLKPWR_GetCLK(CLKPWR_CLKTYPE_PER);
  PWM_MatchUpdate(_PWM_NO_USED, 0,pclk/_BACK_LIGHT_BASE_CLK, PWM_MATCH_UPDATE_NOW);
  PWMMatchCfgDat.IntOnMatch = DISABLE;
  PWMMatchCfgDat.MatchChannel = 0;
  PWMMatchCfgDat.ResetOnMatch = ENABLE;
  PWMMatchCfgDat.StopOnMatch = DISABLE;
  PWM_ConfigMatch(_PWM_NO_USED, &PWMMatchCfgDat);
  // Set backlight
  backlight = GetBacklightVal();
  SetBackLight(backlight);
  
    // Enable LCD
  LCD_Enable (TRUE);
  
#if LOGO_DISPLAYED
  if(LogoPic.H_Size > LCD_H_SIZE)
  {
    start_pix_x = (LogoPic.H_Size - LCD_H_SIZE)/2; 
    xs = 0;
  }
  else
  {
    start_pix_x = 0;
    xs = (LCD_H_SIZE - LogoPic.H_Size)/2;
  }
  
  if(LogoPic.V_Size > LCD_V_SIZE)
  {
     start_pix_y = (LogoPic.V_Size - LCD_V_SIZE)/2;
     ys = 0;
  }
  else 
  {
     ys = (LCD_V_SIZE - LogoPic.V_Size)/2;
     start_pix_y = 0;
  }
  
  pix_ofs = (start_pix_y * LogoPic.H_Size + start_pix_x)*LogoPic.BitsPP/8;
  LogoPic.pPicStream += pix_ofs;
  LCD_LoadPic(LCD_PANEL_UPPER,xs,ys,&LogoPic,0x00);
  
  for(i = 0x1000000; i; i--);
#endif /*LOGO_DISPLAYED*/ 

  // Draw color bars
  lcd_colorbars();
  
  // Draw cursor
  LCD_Cursor_Enable(DISABLE, 0);

  cursor_config.baseaddress = LCD_CURSOR_BASE_ADDR;
  cursor_config.framesync = 1;
  cursor_config.size32 = 0;
  LCD_Cursor_Cfg(&cursor_config);
  LCD_Cursor_SetImage((uint32_t *)Cursor, 0, sizeof(Cursor)/sizeof(uint32_t)) ;

  cursor_x = (LCD_H_SIZE - CURSOR_H_SIZE)/2;
  cursor_y = (LCD_V_SIZE - CURSOR_V_SIZE)/2;
  LCD_Move_Cursor(cursor_x, cursor_y);

  LCD_Cursor_Enable(ENABLE, 0);

  /* Initialize accellation sensor */
#if ACCEL_SENSOR_USED
  AccSensor_Init();
  for(i = 0; i < 0x100000;  i++);
#endif

#if (_CURR_USING_BRD == _IAR_OLIMEX_BOARD)
  SMB380_GetID(&Smb380Id, &Smb380Ver);
#endif
  
  while(1)
  {
    
 	backlight = GetBacklightVal();
    SetBackLight(backlight);

#if TCS_USED || ACCEL_SENSOR_USED

#if TCS_USED
    {
        int16_t tmp_x = -1, tmp_y = -1;
        for(i = 0; i < 0x10000;  i++);
  	    GetTouchCoord((int16_t*)&tmp_x, (int16_t*)&tmp_y);
        if((tmp_x >= 0) && (tmp_y >0))
        {      
          cursor_x = tmp_x - CURSOR_H_SIZE/2;
          cursor_y = tmp_y - CURSOR_V_SIZE/2;
        }
        
    }
#elif ACCEL_SENSOR_USED
    for(i = 0; i < 0x10000;  i++);
    AccSensor_GetData (&XYZT);
    if((XYZT.AccX == 0) && (XYZT.AccY == 0))
      continue; 
       
#if (_CURR_USING_BRD == _IAR_OLIMEX_BOARD)
    cursor_x += XYZT.AccX/512;
    cursor_y += XYZT.AccY/512;
#else
    {
       MMA7455_Orientation_t ori = MMA7455_GetOrientation(&XYZT);
       if(ori & MMA7455_XUP) cursor_x++;
       if (ori & MMA7455_XDOWN) cursor_x--;
       if (ori & MMA7455_YUP) cursor_y++;
       if (ori & MMA7455_YDOWN) cursor_y--;
    }
#endif

#endif  /*TCS_USED*/
    
    if((LCD_H_SIZE - CURSOR_H_SIZE/2) < cursor_x)
    {
      cursor_x = LCD_H_SIZE - CURSOR_H_SIZE/2;
    }

    if(-(CURSOR_H_SIZE/2) > cursor_x)
    {
      cursor_x = -(CURSOR_H_SIZE/2);
    }

    if((LCD_V_SIZE - CURSOR_V_SIZE/2) < cursor_y)
    {
      cursor_y = (LCD_V_SIZE - CURSOR_V_SIZE/2);
    }

    if(-(CURSOR_V_SIZE/2) > cursor_y)
    {
      cursor_y = -(CURSOR_V_SIZE/2);
    }
#else  /* !(TCS_USED || ACCEL_SENSOR_USED)*/ 
    for(i = 0; i < 0x1000000;  i++);
    
    cursor_x += 32;
    if(cursor_x >= (LCD_H_SIZE - CURSOR_H_SIZE))
    {
      cursor_x = 0;
      cursor_y += 32;
      if(cursor_y >(LCD_V_SIZE - CURSOR_V_SIZE))
        cursor_y = 0;
    }
#endif  /*TCS_USED || ACCEL_SENSOR_USED*/

    LCD_Move_Cursor(cursor_x, cursor_y);
  }
}

int main (void)
{
   c_entry();
   return 0;
}

/**
 * @}
 */
