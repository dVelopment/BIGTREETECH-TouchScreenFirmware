#include "Printing.h"
#include "includes.h"

//1title, ITEM_PER_PAGE item(icon + label) 
MENUITEMS printingItems = {
//  title
LABEL_BACKGROUND,
// icon                       label
 {{ICON_PAUSE,                LABEL_PAUSE},
  {ICON_BACKGROUND,           LABEL_BACKGROUND},
  {ICON_BACKGROUND,           LABEL_BACKGROUND},
  {ICON_STOP,                 LABEL_STOP},
  {ICON_HEAT,                 LABEL_HEAT},
  {ICON_PERCENTAGE,           LABEL_PERCENTAGE},
  {ICON_BABYSTEP,             LABEL_BABYSTEP},
  {ICON_MORE,                 LABEL_MORE},}
};

const ITEM itemIsPause[2] = {
// icon                       label
  {ICON_PAUSE,                LABEL_PAUSE},
  {ICON_RESUME,               LABEL_RESUME},
};


static PRINTING infoPrinting;

static u32     update_time = M27_REFRESH * 100;
#ifdef ONBOARD_SD_SUPPORT
static bool    update_waiting = false;
#else
static bool    update_waiting = M27_WATCH_OTHER_SOURCES;
#endif


//
bool isPrinting(void)
{
  return infoPrinting.printing;
}

//
bool isPause(void)
{
  return infoPrinting.pause;
}

//
void setPrintingTime(u32 RTtime)
{
  if(RTtime%100 == 0)
  {
    if(isPrinting() && !isPause())
    {
      infoPrinting.time++;      
    }
  }
}

//
void setPrintSize(u32 size)
{
  infoPrinting.size = size;
}

//
void setPrintCur(u32 cur)
{
  infoPrinting.cur = cur;
}

u8 getPrintProgress(void)
{
  return infoPrinting.progress;
}
u32 getPrintTime(void)
{
  return infoPrinting.time;
}

void printSetUpdateWaiting(bool isWaiting)
{
  update_waiting = isWaiting;
}


void startGcodeExecute(void)
{    

}

void endGcodeExecute(void)
{
  mustStoreCmd("G90\n");
  mustStoreCmd("G92 E0\n");
  for(TOOL i = BED; i < HEATER_NUM; i++)
  {
    mustStoreCmd("%s S0\n", heatCmd[i]);  
  }
  for(u8 i = 0; i < FAN_NUM; i++)
  {
    mustStoreCmd("%s S0\n", fanCmd[i]);  
  }
  mustStoreCmd("T0\n");
  mustStoreCmd("M18\n");
}


void menuBeforePrinting(void)
{
  long size = 0;
  switch (infoFile.source)
  {
    case BOARD_SD: // GCode from file on ONBOARD SD      
      size = request_M23(infoFile.title+5);

      //  if( powerFailedCreate(infoFile.title)==false)
      //  {
      //
      //  }	  // FIXME: Powerfail resume is not yet supported for ONBOARD_SD. Need more work.

      if(size == 0)
      {
        infoMenu.cur--;		
        return;
      }

      infoPrinting.size  = size;

      //    if(powerFailedExist())
      //    {
      request_M24(0);
      //    }
      //    else
      //    {
      //      request_M24(infoBreakPoint.offset);
      //    }
      printSetUpdateWaiting(true);

      #ifdef M27_AUTOREPORT
        request_M27(M27_REFRESH); 
      #else
        request_M27(0); 
      #endif

      infoHost.printing=true; // Global lock info on printer is busy in printing.

      break;
      
    case TFT_UDISK:
    case TFT_SD: // GCode from file on TFT SD
      if(f_open(&infoPrinting.file,infoFile.title, FA_OPEN_EXISTING | FA_READ)!=FR_OK)
      {		
        infoMenu.cur--;		
        return ;
      }
      if( powerFailedCreate(infoFile.title)==false)
      {

      }
      powerFailedlSeek(&infoPrinting.file);

      infoPrinting.size  = f_size(&infoPrinting.file);
      infoPrinting.cur   = infoPrinting.file.fptr;

      startGcodeExecute();
      break;
  }
  infoPrinting.printing = true;
  infoMenu.menu[infoMenu.cur] = menuPrinting;
}


void resumeToPause(bool is_pause)
{
  if(infoMenu.menu[infoMenu.cur] != menuPrinting) return;
  printingItems.items[KEY_ICON_0] = itemIsPause[is_pause];
  menuDrawItem(&itemIsPause[is_pause],0);
}

bool setPrintPause(bool is_pause)
{
  static bool pauseLock = false;
  if(pauseLock)                      return false;
  if(!isPrinting())                  return false;
  if(infoPrinting.pause == is_pause) return false;

  pauseLock = true;
  switch (infoFile.source)
  {
    case BOARD_SD:
      infoPrinting.pause = is_pause;    
      if (is_pause){
        request_M25();
      } else {
        request_M24(0);
      }
      break;
      
    case TFT_UDISK:
    case TFT_SD:
      while (infoCmd.count != 0) {loopProcess();}

      bool isCoorRelative = coorGetRelative();
      bool isExtrudeRelative = eGetRelative();
      static COORDINATE tmp;
      
      infoPrinting.pause = is_pause;
      if(infoPrinting.pause)
      {
        //restore status before pause
        coordinateGetAll(&tmp);
        if (isCoorRelative == true)     mustStoreCmd("G90\n");
        if (isExtrudeRelative == true)  mustStoreCmd("M82\n");
        
        if (heatGetCurrentTemp(heatGetCurrentToolNozzle()) > PREVENT_COLD_EXTRUSION_MINTEMP)
          mustStoreCmd("G1 E%.5f F%d\n", tmp.axis[E_AXIS] - NOZZLE_PAUSE_RETRACT_LENGTH, NOZZLE_PAUSE_E_FEEDRATE);
        if (coordinateIsClear())
        {
          mustStoreCmd("G1 Z%.3f F%d\n", tmp.axis[Z_AXIS] + NOZZLE_PAUSE_Z_RAISE, NOZZLE_PAUSE_Z_FEEDRATE);
          mustStoreCmd("G1 X%d Y%d F%d\n", NOZZLE_PAUSE_X_POSITION, NOZZLE_PAUSE_Y_POSITION, NOZZLE_PAUSE_XY_FEEDRATE);
        }
        
        if (isCoorRelative == true)     mustStoreCmd("G91\n");
        if (isExtrudeRelative == true)  mustStoreCmd("M83\n");
      }
      else
      {
        if (isCoorRelative == true)     mustStoreCmd("G90\n");
        if (isExtrudeRelative == true)  mustStoreCmd("M82\n");
        
        if (coordinateIsClear())
        {
          mustStoreCmd("G1 X%.3f Y%.3f F%d\n", tmp.axis[X_AXIS], tmp.axis[Y_AXIS], NOZZLE_PAUSE_XY_FEEDRATE);
          mustStoreCmd("G1 Z%.3f F%d\n", tmp.axis[Z_AXIS], NOZZLE_PAUSE_Z_FEEDRATE);
        }
        if(heatGetCurrentTemp(heatGetCurrentToolNozzle()) > PREVENT_COLD_EXTRUSION_MINTEMP)
          mustStoreCmd("G1 E%.5f F%d\n", tmp.axis[E_AXIS] + NOZZLE_PAUSE_PURGE_LENGTH, NOZZLE_PAUSE_E_FEEDRATE);
        mustStoreCmd("G92 E%.5f\n", tmp.axis[E_AXIS]);
        mustStoreCmd("G90 F%d\n", tmp.feedrate);
        
        if (isCoorRelative == true)     mustStoreCmd("G91\n");
        if (isExtrudeRelative == true)  mustStoreCmd("M83\n");
      }
      break;
  }
  resumeToPause(is_pause);
  pauseLock = false;
  return true;
}

const GUI_RECT progressRect = {1*SPACE_X_PER_ICON, 0*ICON_HEIGHT+0*SPACE_Y+TITLE_END_Y + ICON_HEIGHT/4,
                               3*SPACE_X_PER_ICON, 0*ICON_HEIGHT+0*SPACE_Y+TITLE_END_Y + ICON_HEIGHT*3/4};

#define BED_X  (progressRect.x1 - 9 * BYTE_WIDTH)
#define TEMP_Y (progressRect.y1 + 3)
#define TIME_Y (TEMP_Y + 1 * BYTE_HEIGHT + 3)
void reValueNozzle(void)
{
  GUI_DispString(progressRect.x0, TEMP_Y, (u8* )heatDisplayID[heatGetCurrentToolNozzle()], 1);
  GUI_DispDec(progressRect.x0+BYTE_WIDTH*3, TEMP_Y, heatGetCurrentTemp(heatGetCurrentToolNozzle()), 3, 1, RIGHT);
  GUI_DispDec(progressRect.x0+BYTE_WIDTH*7, TEMP_Y, heatGetTargetTemp(heatGetCurrentToolNozzle()),  3, 1, LEFT);
}

void reValueBed(void)
{
  GUI_DispDec(BED_X + 2 * BYTE_WIDTH, TEMP_Y, heatGetCurrentTemp(BED), 3, 1, RIGHT);
  GUI_DispDec(BED_X + 6 * BYTE_WIDTH, TEMP_Y, heatGetTargetTemp(BED),  3, 1, LEFT);
}

void reDrawTime(void)
{
  u8  hour = infoPrinting.time/3600,
      min = infoPrinting.time%3600/60,
      sec = infoPrinting.time%60;

  GUI_DispChar(progressRect.x0 + 2 * BYTE_WIDTH, TIME_Y, hour/10%10+'0',1);
  GUI_DispChar(progressRect.x0 + 3 * BYTE_WIDTH, TIME_Y, hour%10+'0', 1);
  GUI_DispChar(progressRect.x0 + 5 * BYTE_WIDTH, TIME_Y, min/10+'0',1);
  GUI_DispChar(progressRect.x0 + 6 * BYTE_WIDTH, TIME_Y, min%10+'0',1);
  GUI_DispChar(progressRect.x0 + 8 * BYTE_WIDTH, TIME_Y, sec/10+'0',1);
  GUI_DispChar(progressRect.x0 + 9 * BYTE_WIDTH, TIME_Y, sec%10+'0',1);
}

void reDrawProgress(u8 progress)
{	  
  char buf[5];
  u16 progressX = map(progress, 0, 100, progressRect.x0, progressRect.x1);
  GUI_FillRectColor(progressRect.x0, progressRect.y0, progressX, progressRect.y1,BLUE);
  GUI_FillRectColor(progressX, progressRect.y0, progressRect.x1, progressRect.y1,GRAY);
  my_sprintf(buf, "%d%%", progress);
  GUI_DispStringInPrect(&progressRect, (u8 *)buf, 0);                         
}

extern SCROLL   titleScroll;
extern GUI_RECT titleRect;

//only return gcode file name except path
//for example:"SD:/test/123.gcode"
//only return "123.gcode"
u8 *getCurGcodeName(char *path)
{
  int i=strlen(path);
  for(; path[i]!='/'&& i>0; i--)
  {}
  return (u8* )(&path[i+1]);
}

void printingDrawPage(void)
{
  menuDrawPage(&printingItems);
  //	Scroll_CreatePara(&titleScroll, infoFile.title,&titleRect);  //
  GUI_DispLenString(titleRect.x0, titleRect.y0, 
                    getCurGcodeName(infoFile.title), 1, 
                   (titleRect.x1 - titleRect.x0) );
  // printed time
  GUI_DispString(progressRect.x0, TIME_Y, (u8* )"T:", 0);
  GUI_DispChar(progressRect.x0+BYTE_WIDTH*4, TIME_Y, ':', 0);
  GUI_DispChar(progressRect.x0+BYTE_WIDTH*7, TIME_Y, ':', 0);
  // nozzle temperature 
  GUI_DispString(progressRect.x0+BYTE_WIDTH*2, TEMP_Y,(u8* )":",0);
  GUI_DispChar(progressRect.x0+BYTE_WIDTH*6, TEMP_Y,'/',0);
  // hotbed temperature
  GUI_DispString(BED_X, TEMP_Y,(u8* )"B:",0);
  GUI_DispChar(BED_X+BYTE_WIDTH*5, TEMP_Y,'/',0);
  reDrawProgress(infoPrinting.progress);
  reValueNozzle();
  reValueBed();
  reDrawTime();
}


void menuPrinting(void)	
{
  KEY_VALUES  key_num = KEY_IDLE;		
  u32         time = 0;
  HEATER      nowHeat; 
  memset(&nowHeat, 0, sizeof(HEATER));
  
  printingItems.items[KEY_ICON_0] = itemIsPause[infoPrinting.pause];
  printingDrawPage();

  while(infoMenu.menu[infoMenu.cur] == menuPrinting)
  {		
//    Scroll_DispString(&titleScroll,1,LEFT); //Scroll display file name will take too many CPU cycles

    if( infoPrinting.size != 0)
    {
      if(infoPrinting.progress!=limitValue(0,(uint64_t)infoPrinting.cur*100/infoPrinting.size,100))
      {
        infoPrinting.progress=limitValue(0,(uint64_t)infoPrinting.cur*100/infoPrinting.size,100);
        reDrawProgress(infoPrinting.progress);
      }	
    }
    else
    {
      if(infoPrinting.progress != 100)
      {
        infoPrinting.progress = 100;
        reDrawProgress(infoPrinting.progress);
      }	
    }            

    if (nowHeat.T[heatGetCurrentToolNozzle()].current != heatGetCurrentTemp(heatGetCurrentToolNozzle()) 
     || nowHeat.T[heatGetCurrentToolNozzle()].target != heatGetTargetTemp(heatGetCurrentToolNozzle()))
    {
      nowHeat.T[heatGetCurrentToolNozzle()].current = heatGetCurrentTemp(heatGetCurrentToolNozzle());
      nowHeat.T[heatGetCurrentToolNozzle()].target = heatGetTargetTemp(heatGetCurrentToolNozzle());
      reValueNozzle();	
    }
    if (nowHeat.T[BED].current != heatGetCurrentTemp(BED) 
     || nowHeat.T[BED].target != heatGetTargetTemp(BED))
    {
      nowHeat.T[BED].current = heatGetCurrentTemp(BED);
      nowHeat.T[BED].target = heatGetTargetTemp(BED);
      reValueBed();	
    }
    
    if(time!=infoPrinting.time)
    {
      time=infoPrinting.time;
      reDrawTime();
    }

    key_num = menuKeyGetValue();
    switch(key_num)
    {
      case KEY_ICON_0:
        setPrintPause(!isPause());
        break;
      
      case KEY_ICON_3:
        if(isPrinting())				
          infoMenu.menu[++infoMenu.cur]=menuStopPrinting;	
        else
        {
          exitPrinting();
          infoMenu.cur--;
        }					
        break;
        
      case KEY_ICON_4:
        infoMenu.menu[++infoMenu.cur] = menuHeat;
        break;
      
      case KEY_ICON_5:
        infoMenu.menu[++infoMenu.cur] = menuSpeed;
        break;
      
      case KEY_ICON_6:
        infoMenu.menu[++infoMenu.cur] = menuBabyStep;
        break;
      
      case KEY_ICON_7:
        infoMenu.menu[++infoMenu.cur] = menuMore;
        break;
      
      default :break;
    }                
    loopProcess();
  }	
}

void exitPrinting(void)
{	
  memset(&infoPrinting,0,sizeof(PRINTING));
  ExitDir();	
}

void endPrinting(void)
{  
  switch (infoFile.source)
  {
    case BOARD_SD:
      printSetUpdateWaiting(M27_WATCH_OTHER_SOURCES);
      break;
    
    case TFT_UDISK:
    case TFT_SD:
      f_close(&infoPrinting.file);	
      break;
  }
  infoPrinting.printing = infoPrinting.pause = false;
  powerFailedClose();
  powerFailedDelete();  
  endGcodeExecute();
}


void completePrinting(void)
{
  endPrinting();  
  if(infoSettings.auto_off) // Auto shut down after printing
    mustStoreCmd("M81\n");
}

void haltPrinting(void)
{
  switch (infoFile.source)
  {
    case BOARD_SD:
      request_M524();
      break;
  
    case TFT_UDISK:
    case TFT_SD:
      clearCmdQueue();	
      break;
  }

  heatClearIsWaiting();
  
  mustStoreCmd("G0 Z%d F3000\n", limitValue(0, (int)coordinateGetAxisTarget(Z_AXIS) + 10, Z_MAX_POS));
  mustStoreCmd("G28 X0 Y0\n");

  endPrinting();
  exitPrinting();
}

void menuStopPrinting(void)
{
  u16 key_num = IDLE_TOUCH;

  popupDrawPage(bottomDoubleBtn, textSelect(LABEL_WARNING), textSelect(LABEL_STOP_PRINT), textSelect(LABEL_CONFIRM), textSelect(LABEL_CANNEL));
 
  while(infoMenu.menu[infoMenu.cur] == menuStopPrinting)
  {
    key_num = KEY_GetValue(2, doubleBtnRect);
    switch(key_num)
    {
      case KEY_POPUP_CONFIRM:
        haltPrinting();
        infoMenu.cur-=2;
      break;

      case KEY_POPUP_CANCEL:
        infoMenu.cur--;
        break;		
    }
    loopProcess();
  }
}

// get gcode command from sd card
void getGcodeFromFile(void)
{	
  bool    sd_comment_mode = false;
  bool    sd_comment_space = true;
  char    sd_char;
  u8      sd_count = 0;
  UINT    br = 0;

  if(isPrinting()==false || infoFile.source == BOARD_SD)  return;
  
  powerFailedCache(infoPrinting.file.fptr);
  
  if(heatHasWaiting() || infoCmd.count || infoPrinting.pause )  return;
  
  if(moveCacheToCmd() == true) return;

  for(;infoPrinting.cur < infoPrinting.size;)
  {
    if(f_read(&infoPrinting.file, &sd_char, 1, &br)!=FR_OK) break;

    infoPrinting.cur++;

    //Gcode
    if (sd_char == '\n' )         //'\n' is end flag for per command
    {
      sd_comment_mode = false;   //for new command
      sd_comment_space= true;
      if(sd_count!=0)
      {
        infoCmd.queue[infoCmd.index_w].gcode[sd_count++] = '\n'; 
        infoCmd.queue[infoCmd.index_w].gcode[sd_count] = 0; //terminate string
        infoCmd.queue[infoCmd.index_w].src = SERIAL_PORT;
        sd_count = 0; //clear buffer
        infoCmd.index_w = (infoCmd.index_w + 1) % CMD_MAX_LIST;
        infoCmd.count++;	
        break;			
      }
    }
    else if (sd_count >= CMD_MAX_CHAR - 2) {	}   //when the command length beyond the maximum, ignore the following bytes
    else 
    {
      if (sd_char == ';')             //';' is comment out flag
        sd_comment_mode = true;
      else 
      {
        if(sd_comment_space && (sd_char== 'G'||sd_char == 'M'||sd_char == 'T'))  //ignore ' ' space bytes
          sd_comment_space = false;
        if (!sd_comment_mode && !sd_comment_space && sd_char != '\r')  //normal gcode
          infoCmd.queue[infoCmd.index_w].gcode[sd_count++] = sd_char;
      }
    }
  }

  if((infoPrinting.cur>=infoPrinting.size) && isPrinting())  // end of .gcode file
  {
    completePrinting();
  }
}

void loopCheckPrinting(void)
{
  static u32  nowTime=0;

  do
  {  /* WAIT FOR M27	*/
    if(update_waiting == true)                {nowTime=OS_GetTime();break;}
    if(OS_GetTime()<nowTime+update_time)       break;

    if(storeCmd("M27\n")==false)               break;

    nowTime=OS_GetTime();
    update_waiting=true;
  }while(0);
}




