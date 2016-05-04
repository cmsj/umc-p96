/***************************************************************************
 *   Copyright (C) 2005 by Desmond Colin Jones                             *
 *   http://umc.sourceforge.net/                                           *
 *                                                                         *
 *   gcc umc.c -o umc -lm -Wall                                            *
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 *   This program is distributed in the hope that it will be useful,       *
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of        *
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the         *
 *   GNU General Public License for more details.                          *
 *                                                                         *
 *   You should have received a copy of the GNU General Public License     *
 *   along with this program; if not, write to the                         *
 *   Free Software Foundation, Inc.,                                       *
 *   59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.             *
 ***************************************************************************/


#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <string.h>

/* The name by which this program was run. */
char *program_name;

/* Nonzero if a non-fatal error has occurred.  */
static int exit_status = EXIT_SUCCESS;

/* define a return variable of type modeline */
typedef struct __UMC_MODELINE
{
  /* pixel clock Hz */
  double PClock;

  /* horizontal modeline variables */
  int HRes;           //horizontal resolution
  int HSyncStart;     //horizontal sync start
  int HSyncEnd;       //horizontal sync end
  int HTotal;         //horizontal total

  /* vertical modeline variables */
  int VRes;           //vertical resolution
  int VSyncStart;     //vertical sync start
  int VSyncEnd;       //vertical sync end
  int VTotal;         //vertical total

  /* parameters */
  int Interlace;      //boolean (> 0 = yes)
  int Doublescan;     //boolean (> 0 = yes)
  int HSyncPolarity;  //boolean (low voltage, high voltage > 0)
  int VSyncPolarity;  //boolean (low voltage, high voltage > 0)

  /* linked list pointers */
  struct __UMC_MODELINE *next;
  struct __UMC_MODELINE *previous;
} UMC_MODELINE;

/* define an input variable for display timings */
typedef struct __UMC_DISPLAY
{
  /* pixel clock parameters */
  double CharacterCell;         //horizontal character cell granularity
  double PClockStep;            //pixel clock stepping

  /* horizontal blanking time parameters */
  double HSyncPercent;          //horizontal sync width as % of line period
  double M;                     //M gradient (%/kHz)
  double C;                     //C offset (%)
  double K;                     //K blanking time scaling factor
  double J;                     //J scaling factor weighting

  /* vertical blanking time parameters */
  double VFrontPorch;           //minimum number of lines for front porch
  double VBackPorchPlusSync;    //minimum time for vertical sync+back porch
  double VSyncWidth;            //minimum number of lines for vertical sync
  double VBackPorch;            //minimum number of lines for back porch

  /* configuration parameters */
  double Margin;                //top/bottom MARGIN size as % of height

  /* reduced blanking parameters */
  double HBlankingTicks;        //number of clock ticks for horizontal blanking
  double HSyncTicks;            //number of clock ticks for horizontal sync
  double VBlankingTime;         //minimum vertical blanking time
} UMC_DISPLAY;

UMC_DISPLAY UMC_GTF = {
  8.0,    //horizontal character cell granularity
  0.0,    //pixel clock stepping
  8.0,    //horizontal sync width% of line period
  600.0,  //M gradient (%/kHz)
  40.0,   //C offset (%)
  128.0,  //K blanking time scaling factor
  20.0,   //J scaling factor weighting
  1.0,    //number of lines for front porch
  550.0,  //time for vertical sync+back porch
  3.0,    //number of lines for vertical sync
  6.0,    //number of lines for back porch
  0.0,    //top/bottom MARGIN size as % of height
  160.0,  //clock ticks for horizontal blanking
  32.0,   //clock ticks for horizontal sync
  460.0,  //minimum vertical blanking time
};

UMC_DISPLAY UMC_CVT = {
  8.0,    //horizontal character cell granularity
  0.25,    //pixel clock stepping
  8.0,    //horizontal sync width% of line period
  600.0,  //M gradient (%/kHz)
  40.0,   //C offset (%)
  128.0,  //K blanking time scaling factor
  20.0,   //J scaling factor weighting
  3.0,    //number of lines for front porch
  550.0,  //time for vertical sync+back porch
  4.0,    //number of lines for vertical sync
  6.0,    //number of lines for back porch
  0.0,    //top/bottom MARGIN size as % of height
  160.0,  //clock ticks for horizontal blanking
  32.0,   //clock ticks for horizontal sync
  460.0,  //minimum vertical blanking time
};


/* define a round function to keep the source ANSI C 89 compliant */
double round(double input)
{
  return ((double) ((int) (input + 0.5)));
}

double floor(double input)
{
  return ((double) ((int) input));
}

double ceil(double input)
{
  return ((double) ((int) (input + 1.0)));
}

void usage (int status)
{
  if (status != EXIT_SUCCESS)
    fprintf (stderr,"Try `%s --help' for more information.\n",
             program_name);
  else
  {
    printf("Usage: %s x y refresh [OPTIONS]\n", program_name);
    printf("Compute a modeline given a resolution and refresh rate.\n\n\
        -i, --interlace      calculate an interlaced mode\n\
        -d, --doublescan     calculate a doublescanned mode\n\
        -m, --margin         margin percentage\n\
        -x, --X11R6          print X11R6 modeline format (default)\n\
        -f, --fb             print fb modeline format\n\
        --help\n\
        --version\n\n");
  }
  exit (status);
} // usage()


UMC_MODELINE *reduced_blanking_timing(double HRes, double VRes,
                 double RefreshRate, UMC_DISPLAY Display, double Flags)
{
  /* define Clock variables */
  double PClock = 0;

  /* horizontal calculation variables */
  double HActive = 0;
  double HSyncWidth = 0;
  double HBackPorch = 0;
  double HTotal = 0;
  double HorizontalPeriodEstimate = 0;

  /* vertical calculation variables */
  double Interlace = 0;
  double VBlanking = 0;
  double VBackPorch = 0;
  double VTotal = 0;

  /* Margin variables */
  double TopMargin = 0;
  double BottomMargin = 0;
  double RightMargin = 0;
  double LeftMargin = 0;

  /* get a return variable */
  UMC_MODELINE *Modeline = (UMC_MODELINE*) malloc(sizeof(UMC_MODELINE));


  /* perform a very, very small sainity check */
  if (Modeline == NULL)
  {
    fprintf(stderr, "Error:  not enough memory available.\n");
    return (NULL);
  }
  if (
      (HRes < 1) ||
      (VRes < 1) ||
      (RefreshRate < 1)
     )
  {
    fprintf(stderr, "Error: mode parameters invalid.\n");
    return (NULL);
  }


  /* initialize a few Modeline variables to their default values */
  Modeline->Doublescan = 0;
  Modeline->Interlace = 0;
  Modeline->next = NULL;
  Modeline->previous = NULL;


  /* round character cell granularity to nearest integer*/
  Display.CharacterCell = floor(Display.CharacterCell);

  if (Display.CharacterCell < 1)
  {
    fprintf(stderr, "Error:  character cell less than 1 pixel.\n");
  }


  /* round number of lines in front porch to nearest integer */
  if (Flags < 0) // if doublescan mode
  {
    Display.VFrontPorch = round(Display.VFrontPorch / 2.0) * 2.0;
  }
  else
  {
    Display.VFrontPorch = floor(Display.VFrontPorch);
  }

  if (Display.VFrontPorch < 0)
  {
    fprintf(stderr, "Error:  vertical sync start less than 0 lines.\n");
  }


  /* round number of lines in vsync width to nearest integer */
  if (Flags < 0) // if doublescan mode
  {
    Display.VSyncWidth = round(Display.VSyncWidth / 2.0) * 2.0;
  }
  else
  {
    Display.VSyncWidth = floor(Display.VSyncWidth);
  }

  if (Display.VSyncWidth < 1)
  {
    fprintf(stderr, "Error:  vertical sync width less than 1 line.\n");
  }


  /* round number of lines in back porch to nearest integer */
  if (Flags < 0) // if doublescan mode
  {
    Display.VBackPorch= round(Display.VBackPorch / 2.0) * 2.0;
  }
  else
  {
    Display.VBackPorch = floor(Display.VBackPorch);
  }

  if (Display.VBackPorch < 1)
  {
    fprintf(stderr, "Error:  vertical sync width less than 1 line.\n");
  }


  /* number of lines per field rounded to nearest integer */
  if (Flags > 0)  // if interlace mode
  {
    VRes = floor(VRes / 2.0);
  }
  else if (Flags < 0)  // if doublescan mode
  {
    VRes = floor(VRes) * 2;
  }
  else
  {
    VRes = floor(VRes);
  }


  /* round horizontal resolution to lowest character cell multiple */
  HRes = floor(HRes / Display.CharacterCell) * Display.CharacterCell;


  /* round horizontal blanking to lowest character cell multiple */
  Display.HBlankingTicks =
      floor(Display.HBlankingTicks / Display.CharacterCell) *
      Display.CharacterCell;


  /* round horizontal sync to lowest character cell multiple */
  Display.HSyncTicks =
      floor(Display.HSyncTicks / Display.CharacterCell) *
      Display.CharacterCell;


  /* calculate margins */
  if (Display.Margin > 0)
  {
    /* calculate top and bottom margins */
    TopMargin = BottomMargin = floor((Display.Margin / 100.0) * VRes);


    /* calculate left and right margins */
    LeftMargin = RightMargin =
        floor(HRes * (Display.Margin / 100.0) / Display.CharacterCell) *
        Display.CharacterCell;
  }


  /* calculate total number of active pixels per line */
  HActive = HRes + LeftMargin + RightMargin;


  /* set interlace variable if interlace mode */
  if (Flags > 0)  // if interlace mode
  {
    Interlace = 0.5;
  }
  else
  {
    Interlace = 0;
  }


  /* calculate field refresh rate */
  if (Flags > 0)  // if interlace mode
  {
    RefreshRate = RefreshRate * 2.0;
  }


  /* estimate horizontal period */
  HorizontalPeriodEstimate =
      ((1000000.0 / RefreshRate) - Display.VBlankingTime) /
      (VRes + TopMargin + BottomMargin);


  /* calculate number of lines in vertical sync and back porch */
  VBlanking = Display.VBlankingTime / HorizontalPeriodEstimate;

  if (Flags < 0) // if doublescan mode
  {
    VBlanking = ceil(VBlanking / 2.0) * 2.0;
  }
  else // default
  {
    VBlanking = ceil(VBlanking);
  }

  if (VBlanking <
      Display.VFrontPorch + Display.VSyncWidth + Display.VBackPorch)
  {
    VBlanking =
        Display.VFrontPorch + Display.VSyncWidth + Display.VBackPorch;
  }


  /* calculate number of lines in back porch */
  VBackPorch = VBlanking - Display.VSyncWidth - Display.VFrontPorch;


  /* calculate total number of lines */
  VTotal = VRes + TopMargin + BottomMargin + VBlanking + Interlace;


  /* calculate total number of pixels per line */
  HTotal = HActive + Display.HBlankingTicks;


  /* calculate pixel clock */
  PClock = RefreshRate * VTotal * HTotal / 1000000.0;
  if (Display.PClockStep > 0)
  {
    PClock = floor(PClock / Display.PClockStep) * Display.PClockStep;
  }


  /* calculate horizontal sync width in pixels */
  HSyncWidth = Display.HSyncTicks;


  /* calculate horizontal back porch in pixels */
  HBackPorch = Display.HBlankingTicks / 2.0;


  /* output modeline calculations */
  if (Flags > 0)  // if interlace mode
  {
    Modeline->Interlace = 1;
    Modeline->PClock = PClock;
    Modeline->HRes = (int) HRes;
    Modeline->HSyncStart = (int) (HTotal - HBackPorch - HSyncWidth);
    Modeline->HSyncEnd = (int) (HTotal - HBackPorch);
    Modeline->HTotal = (int) HTotal;
    Modeline->VRes = (int) (VRes * 2.0);
    Modeline->VSyncStart =
        (int) ((VTotal - VBackPorch - Display.VSyncWidth) * 2.0);
    Modeline->VSyncEnd = (int) ((VTotal - VBackPorch) * 2.0);
    Modeline->VTotal = (int) (VTotal * 2.0);
  }
  else if (Flags < 0)  // if doublescan mode
  {
    Modeline->Doublescan = 1;
    Modeline->PClock = PClock;
    Modeline->HRes = (int) HRes;
    Modeline->HSyncStart = (int) (HTotal - HBackPorch - HSyncWidth);
    Modeline->HSyncEnd = (int) (HTotal - HBackPorch);
    Modeline->HTotal = (int) HTotal;
    Modeline->VRes = (int) (VRes / 2.0);
    Modeline->VSyncStart =
        (int) ((VTotal - VBackPorch - Display.VSyncWidth) / 2.0);
    Modeline->VSyncEnd = (int) ((VTotal - VBackPorch) / 2.0);
    Modeline->VTotal = (int) (VTotal / 2.0);
  }
  else
  {
    Modeline->PClock = PClock;
    Modeline->HRes = (int) HRes;
    Modeline->HSyncStart = (int) (HTotal - HBackPorch - HSyncWidth);
    Modeline->HSyncEnd = (int) (HTotal - HBackPorch);
    Modeline->HTotal = (int) HTotal;
    Modeline->VRes = (int) VRes;
    Modeline->VSyncStart = (int) (VTotal - VBackPorch - Display.VSyncWidth);
    Modeline->VSyncEnd = (int) (VTotal - VBackPorch);
    Modeline->VTotal = (int) VTotal;
  }

  Modeline->HSyncPolarity = 1;
  Modeline->VSyncPolarity = 0;


  return (Modeline);
} // reduced_blanking_timing()


UMC_MODELINE *coordinated_video_timing(double HRes, double VRes,
                 double RefreshRate, UMC_DISPLAY Display, double Flags)
{
  /* define Clock variables */
  double PClock = 0;

  /* horizontal calculation variables */
  double HActive = 0;
  double HSyncWidth = 0;
  double HBackPorch = 0;
  double HTotal = 0;
  double IdealDutyCycle = 0;
  double HorizontalPeriodEstimate = 0;
  double HorizontalBlankingPixels = 0;

  /* vertical calculation variables */
  double Interlace = 0;
  double VSyncPlusBackPorch = 0;
  double VBackPorch = 0;
  double VTotal = 0;

  /* Margin variables */
  double TopMargin = 0;
  double BottomMargin = 0;
  double RightMargin = 0;
  double LeftMargin = 0;

  /* get a return variable */
  UMC_MODELINE *Modeline = (UMC_MODELINE*) malloc(sizeof(UMC_MODELINE));


  /* perform a very, very small sainity check */
  if (Modeline == NULL)
  {
    fprintf(stderr, "Error:  not enough memory available.\n");
    return (NULL);
  }
  if (
      (HRes < 1) ||
      (VRes < 1) ||
      (RefreshRate < 1)
     )
  {
    fprintf(stderr, "Error: mode parameters invalid.\n");
    return (NULL);
  }

  /* initialize a few Modeline variables to their default values */
  Modeline->Doublescan = 0;
  Modeline->Interlace = 0;
  Modeline->next = NULL;
  Modeline->previous = NULL;


  /* round character cell granularity to nearest integer*/
  Display.CharacterCell = floor(Display.CharacterCell);

  if (Display.CharacterCell < 1)
  {
    fprintf(stderr, "Error:  character cell less than 1 pixel.\n");
  }


  /* round number of lines in front porch to nearest integer */
  if (Flags < 0) // if doublescan mode
  {
    Display.VFrontPorch = round(Display.VFrontPorch / 2.0) * 2.0;
  }
  else
  {
    Display.VFrontPorch = floor(Display.VFrontPorch);
  }

  if (Display.VFrontPorch < 0)
  {
    fprintf(stderr, "Error:  vertical sync start less than 0 lines.\n");
  }


  /* round number of lines in vsync width to nearest integer */
  if (Flags < 0) // if doublescan mode
  {
    Display.VSyncWidth = round(Display.VSyncWidth / 2.0) * 2.0;
  }
  else
  {
    Display.VSyncWidth = floor(Display.VSyncWidth);
  }

  if (Display.VSyncWidth < 1)
  {
    fprintf(stderr, "Error:  vertical sync width less than 1 line.\n");
  }


  /* round number of lines in back porch to nearest integer */
  if (Flags < 0) // if doublescan mode
  {
    Display.VBackPorch= round(Display.VBackPorch / 2.0) * 2.0;
  }
  else
  {
    Display.VBackPorch = floor(Display.VBackPorch);
  }

  if (Display.VBackPorch < 1)
  {
    fprintf(stderr, "Error:  vertical sync width less than 1 line.\n");
  }


  /* Calculate M, incorporating the scaling factor */
  if (Display.K == 0)
  {
    Display.K = 00.1;
  }
  Display.M = (Display.K / 256.0) * Display.M;


  /* Calculate C, incorporating the scaling factor */
  Display.C = ((Display.C - Display.J) * Display.K / 256.0) + Display.J;


  /* number of lines per field rounded to nearest integer */
  if (Flags > 0)  // if interlace mode
  {
    VRes = floor(VRes / 2.0);
  }
  else if (Flags < 0)  // if doublescan mode
  {
    VRes = floor(VRes) * 2;
  }
  else
  {
    VRes = floor(VRes);
  }


  /* number of pixels per line rouned to nearest character cell */
  HRes = floor(HRes / Display.CharacterCell) * Display.CharacterCell;


  /* calculate margins */
  if (Display.Margin > 0)
  {
    /* calculate top and bottom margins */
    TopMargin = BottomMargin = floor((Display.Margin / 100.0) * VRes);


    /* calculate left and right margins */
    LeftMargin = RightMargin =
        floor(HRes * (Display.Margin / 100.0) / Display.CharacterCell) *
        Display.CharacterCell;
  }


  /* calculate total number of active pixels per line */
  HActive = HRes + LeftMargin + RightMargin;


  /* set interlace variable if interlace mode */
  if (Flags > 0)  // if interlace mode
  {
    Interlace = 0.5;
  }
  else
  {
    Interlace = 0;
  }


  /* calculate field refresh rate */
  if (Flags > 0)  // if interlace mode
  {
    RefreshRate = RefreshRate * 2.0;
  }


  /* estimate horizontal period */
  HorizontalPeriodEstimate =
      ((1.0 / RefreshRate) - Display.VBackPorchPlusSync / 1000000.0) /
      (VRes + TopMargin + BottomMargin + Display.VFrontPorch + Interlace) *
      1000000.0;


  /* calculate number of lines in vertical sync and back porch */
  VSyncPlusBackPorch = Display.VBackPorchPlusSync / HorizontalPeriodEstimate;

  if (Flags < 0) // if doublescan mode
  {
    VSyncPlusBackPorch = ceil(VSyncPlusBackPorch / 2.0) * 2.0;
  }
  else // default
  {
    VSyncPlusBackPorch = ceil(VSyncPlusBackPorch);
  }

  if (VSyncPlusBackPorch < Display.VSyncWidth + Display.VBackPorch)
  {
    VSyncPlusBackPorch = Display.VSyncWidth + Display.VBackPorch;
  }


  /* calculate number of lines in back porch */
  VBackPorch = VSyncPlusBackPorch - Display.VSyncWidth;


  /* calculate total number of lines */
  VTotal =
      VRes + TopMargin + BottomMargin +
      Display.VFrontPorch + VSyncPlusBackPorch + Interlace;


  /* calculate ideal duty cycle */
  IdealDutyCycle =
      Display.C - (Display.M * HorizontalPeriodEstimate / 1000.0);


  /* calculate horizontal blanking time in pixels */
  if (IdealDutyCycle < 20)
  {
    HorizontalBlankingPixels =
        floor((HActive * 20.0) / (100.0 - 20.0) /
        (2.0 * Display.CharacterCell)) * 2.0 * Display.CharacterCell;
  }
  else
  {
    HorizontalBlankingPixels =
        floor(HActive * IdealDutyCycle / (100.0 - IdealDutyCycle) /
        (2.0 * Display.CharacterCell)) * 2.0 * Display.CharacterCell;
  }


  /* calculate total number of pixels per line */
  HTotal = HActive + HorizontalBlankingPixels;


  /* calculate pixel clock */
  if (Display.PClockStep > 0)
  {
    PClock =
        floor(HTotal / HorizontalPeriodEstimate / Display.PClockStep) *
        Display.PClockStep;
  }
  else
  {
    PClock = HTotal / HorizontalPeriodEstimate;
  }


  /* calculate horizontal sync width in pixels */
  HSyncWidth =
      round(Display.HSyncPercent / 100.0 * HTotal / Display.CharacterCell) *
      Display.CharacterCell;


  /* calculate horizontal back porch in pixels */
  HBackPorch = HorizontalBlankingPixels / 2.0;


  /* output modeline calculations */
  if (Flags > 0)  // if interlace mode
  {
    Modeline->Interlace = 1;
    Modeline->PClock = PClock;
    Modeline->HRes = (int) HRes;
    Modeline->HSyncStart = (int) (HTotal - HBackPorch - HSyncWidth);
    Modeline->HSyncEnd = (int) (HTotal - HBackPorch);
    Modeline->HTotal = (int) HTotal;
    Modeline->VRes = (int) (VRes * 2.0);
    Modeline->VSyncStart = (int) ((VTotal - VSyncPlusBackPorch) * 2.0);
    Modeline->VSyncEnd = (int) ((VTotal - VBackPorch) * 2.0);
    Modeline->VTotal = (int) (VTotal * 2.0);
  }
  else if (Flags < 0)  // if doublescan mode
  {
    Modeline->Doublescan = 1;
    Modeline->PClock = PClock;
    Modeline->HRes = (int) HRes;
    Modeline->HSyncStart = (int) (HTotal - HBackPorch - HSyncWidth);
    Modeline->HSyncEnd = (int) (HTotal - HBackPorch);
    Modeline->HTotal = (int) HTotal;
    Modeline->VRes = (int) (VRes / 2.0);
    Modeline->VSyncStart = (int) ((VTotal - VSyncPlusBackPorch) / 2.0);
    Modeline->VSyncEnd = (int) ((VTotal - VBackPorch) / 2.0);
    Modeline->VTotal = (int) (VTotal / 2.0);
  }
  else
  {
    Modeline->PClock = PClock;
    Modeline->HRes = (int) HRes;
    Modeline->HSyncStart = (int) (HTotal - HBackPorch - HSyncWidth);
    Modeline->HSyncEnd = (int) (HTotal - HBackPorch);
    Modeline->HTotal = (int) HTotal;
    Modeline->VRes = (int) VRes;
    Modeline->VSyncStart = (int) (VTotal - VSyncPlusBackPorch);
    Modeline->VSyncEnd = (int) (VTotal - VBackPorch);
    Modeline->VTotal = (int) VTotal;
  }

  Modeline->HSyncPolarity = 0;
  Modeline->VSyncPolarity = 1;

  return (Modeline);
} // coordinated_video_timing()


UMC_MODELINE *general_timing_formula(double HRes, double VRes, double Clock,
                                     UMC_DISPLAY Display, double Flags)
{
  /* define Clock variables */
  double RefreshRate = 0;
  double RefreshRateEstimate = 0;
  double HClock = 0;
  double PClock = 0;

  /* horizontal calculation variables */
  double HActive = 0;
  double HSyncWidth = 0;
  double HBackPorch = 0;
  double HTotal = 0;
  double IdealDutyCycle = 0;
  double HorizontalPeriodEstimate = 0;
  double IdealHorizontalPeriod = 0;
  double HorizontalPeriod = 0;
  double HorizontalBlankingPixels = 0;

  /* vertical calculation variables */
  double Interlace = 0;
  double VSyncPlusBackPorch = 0;
  double VBackPorch = 0;
  double VTotal = 0;


  /* Margin variables */
  double TopMargin = 0;
  double BottomMargin = 0;
  double RightMargin = 0;
  double LeftMargin = 0;


  /* get a return variable */
  UMC_MODELINE *Modeline = (UMC_MODELINE*) malloc(sizeof(UMC_MODELINE));


  /* perform a very, very small sainity check */
  if (Modeline == NULL)
  {
    fprintf(stderr, "Error:  not enough memory available.\n");
    return (NULL);
  }
  if (
      (HRes < 1) ||
      (VRes < 1) ||
      (Clock < 1)
     )
  {
    fprintf(stderr, "Error: mode parameters invalid.\n");
    return (NULL);
  }


  /* initialize a few Modeline variables to their default values */
  Modeline->Doublescan = 0;
  Modeline->Interlace = 0;
  Modeline->next = NULL;
  Modeline->previous = NULL;


  /* round character cell granularity to nearest integer*/
  Display.CharacterCell = round(Display.CharacterCell);

  if (Display.CharacterCell < 1)
  {
    fprintf(stderr, "Error:  character cell less than 1 pixel.\n");
  }


  /* round number of lines in front porch to nearest integer */
  if (Flags < 0) // if doublescan mode
  {
    Display.VFrontPorch = round(Display.VFrontPorch / 2.0) * 2.0;
  }
  else
  {
    Display.VFrontPorch = floor(Display.VFrontPorch);
  }

  if (Display.VFrontPorch < 0)
  {
    fprintf(stderr, "Error:  vertical sync start less than 0 lines.\n");
  }


  /* round number of lines in vsync width to nearest integer */
  if (Flags < 0) // if doublescan mode
  {
    Display.VSyncWidth = round(Display.VSyncWidth / 2.0) * 2.0;
  }
  else
  {
    Display.VSyncWidth = round(Display.VSyncWidth);
  }

  if (Display.VSyncWidth < 1)
  {
    fprintf(stderr, "Error:  vertical sync width less than 1 line.\n");
  }


  /* round number of lines in back porch to nearest integer */
  if (Flags < 0) // if doublescan mode
  {
    Display.VBackPorch= round(Display.VBackPorch / 2.0) * 2.0;
  }
  else
  {
    Display.VBackPorch = round(Display.VBackPorch);
  }

  if (Display.VBackPorch < 1)
  {
    fprintf(stderr, "Error:  vertical sync width less than 1 line.\n");
  }


  /* Calculate M, incorporating the scaling factor */
  if (Display.K == 0)
  {
    Display.K = 00.1;
  }
  Display.M = (Display.K / 256.0) * Display.M;


  /* Calculate C, incorporating the scaling factor */
  Display.C = ((Display.C - Display.J) * Display.K / 256.0) + Display.J;


  /* number of lines per field rounded to nearest integer */
  if (Flags > 0)  // if interlace mode
  {
    VRes = round(VRes / 2.0);
  }
  else if (Flags < 0)  // if doublescan mode
  {
    VRes = round(VRes) * 2;
  }
  else
  {
    VRes = round(VRes);
  }


  /* number of pixels per line rouned to nearest character cell */
  HRes = round(HRes / Display.CharacterCell) * Display.CharacterCell;


  /* calculate margins */
  if (Display.Margin > 0)
  {
    /* calculate top and bottom margins */
    TopMargin = BottomMargin = round((Display.Margin / 100.0) * VRes);


    /* calculate left and right margins */
    LeftMargin = RightMargin =
        round(HRes * (Display.Margin / 100.0) / Display.CharacterCell) *
        Display.CharacterCell;
  }


  /* calculate total number of active pixels per line */
  HActive = HRes + LeftMargin + RightMargin;


  /* set interlace variable if interlace mode */
  if (Flags > 0)  // if interlace mode
  {
    Interlace = 0.5;
  }
  else
  {
    Interlace = 0;
  }


/***************************************************************************
 *                                                                         *
 *   Refresh rate driven calculations.                                     *
 *                                                                         *
 ***************************************************************************/
  if (Clock < 1000)
  {
    RefreshRate = Clock;


    /* calculate field refresh rate */
    if (Flags > 0)  // if interlace mode
    {
      RefreshRate = RefreshRate * 2.0;
    }


    /* estimate horizontal period */
    HorizontalPeriodEstimate =
        ((1.0 / RefreshRate) - Display.VBackPorchPlusSync / 1000000.0) /
        (VRes + TopMargin + BottomMargin + Display.VFrontPorch + Interlace) *
        1000000.0;


    /* calculate number of lines in vertical sync and back porch */
    VSyncPlusBackPorch =
        Display.VBackPorchPlusSync / HorizontalPeriodEstimate;

    if (Flags < 0) // if doublescan mode
    {
      VSyncPlusBackPorch = round(VSyncPlusBackPorch / 2.0) * 2.0;
    }
    else // default
    {
      VSyncPlusBackPorch = round(VSyncPlusBackPorch);
    }

/***************************************************************************
*                                                                          *
*   The following reality check was not a part of the gtf worksheet but    *
*   it should have been, which is why I included it.                       *
*                                                                          *
****************************************************************************/
    if (VSyncPlusBackPorch < Display.VSyncWidth + Display.VBackPorch)
    {
      VSyncPlusBackPorch = Display.VSyncWidth + Display.VBackPorch;
    }


    /* calculate number of lines in back porch */
    VBackPorch = VSyncPlusBackPorch - Display.VSyncWidth;


    /* calculate total number of lines */
    VTotal =
        VRes + TopMargin + BottomMargin +
        Display.VFrontPorch + VSyncPlusBackPorch + Interlace;


    /* estimate field refresh rate*/
    RefreshRateEstimate =
        1.0 / HorizontalPeriodEstimate / VTotal * 1000000.0;


    /* calculate horizontal period */
    HorizontalPeriod =
        HorizontalPeriodEstimate / (RefreshRate / RefreshRateEstimate);


    /* calculate ideal duty cycle */
    IdealDutyCycle = Display.C - (Display.M * HorizontalPeriod / 1000.0);


    /* calculate horizontal blanking time in pixels */
    if (IdealDutyCycle < 20)
    {
      HorizontalBlankingPixels =
          floor((HActive * 20.0) / (100.0 - 20.0) /
          (2.0 * Display.CharacterCell)) * 2.0 * Display.CharacterCell;
    }
    else
    {
      HorizontalBlankingPixels =
          round(HActive * IdealDutyCycle / (100.0 - IdealDutyCycle) /
          (2.0 * Display.CharacterCell)) * 2.0 * Display.CharacterCell;
    }


    /* calculate total number of pixels per line */
    HTotal = HActive + HorizontalBlankingPixels;


    /* calculate pixel clock */
    PClock = HTotal / HorizontalPeriod;
    if (Display.PClockStep > 0)
    {
      PClock = floor(PClock / Display.PClockStep) * Display.PClockStep;
    }
  }


/***************************************************************************
 *                                                                         *
 *   Horizontal clock driven calculations.                                 *
 *                                                                         *
 ***************************************************************************/
  else if (Clock < 100000)
  {
    HClock = Clock / 1000.0;

    /* calculate number of lines in vertical sync and back porch */
    VSyncPlusBackPorch =
        Display.VBackPorchPlusSync * HClock / 1000.0;

    if (Flags < 0) // if doublescan mode
    {
      VSyncPlusBackPorch = round(VSyncPlusBackPorch / 2.0) * 2.0;
    }
    else // default
    {
      VSyncPlusBackPorch = round(VSyncPlusBackPorch);
    }

/****************************************************************************
 *                                                                          *
 *   The following reality check was not a part of the gtf worksheet but    *
 *   it should have been, which is why I included it.                       *
 *                                                                          *
 ****************************************************************************/
    if (VSyncPlusBackPorch < Display.VSyncWidth + Display.VBackPorch)
    {
      VSyncPlusBackPorch = Display.VSyncWidth + Display.VBackPorch;
    }


    /* calculate number of lines in back porch */
    VBackPorch = VSyncPlusBackPorch - Display.VSyncWidth;


    /* calculate total number of lines */
    VTotal =
        VRes + TopMargin + BottomMargin +
        Display.VFrontPorch + VSyncPlusBackPorch + Interlace;


    /* calculate ideal duty cycle */
    IdealDutyCycle = Display.C - (Display.M / HClock);


    /* calculate horizontal blanking time in pixels */
    if (IdealDutyCycle < 20)
    {
      HorizontalBlankingPixels =
          floor((HActive * 20.0) / (100.0 - 20.0) /
          (2.0 * Display.CharacterCell)) * 2.0 * Display.CharacterCell;
    }
    else
    {
      HorizontalBlankingPixels =
          round(HActive * IdealDutyCycle / (100.0 - IdealDutyCycle) /
          (2.0 * Display.CharacterCell)) * 2.0 * Display.CharacterCell;
    }


    /* calculate total number of pixels per line */
    HTotal = HActive + HorizontalBlankingPixels;


    /* calculate pixel clock */
    PClock = HTotal * HClock / 1000.0;
    if (Display.PClockStep > 0)
    {
      PClock = floor(PClock / Display.PClockStep) * Display.PClockStep;
    }

  }


/***************************************************************************
 *                                                                         *
 *   Pixel clock driven calculations.                                      *
 *                                                                         *
 ***************************************************************************/
  else
  {
    PClock = Clock / 1000000.0;
    if (Display.PClockStep > 0)
    {
      PClock = floor(PClock / Display.PClockStep) * Display.PClockStep;
    }


    /* calculate ideal horizontal period */
    IdealHorizontalPeriod =
        ((Display.C - 100) + sqrt(((100 - Display.C) * (100 - Display.C)) +
        (0.4 * Display.M * (HActive + LeftMargin + RightMargin) / PClock))) /
        2.0 / Display.M * 1000.0;


    /* calculate ideal duty cycle */
    IdealDutyCycle = Display.C - (Display.M * IdealHorizontalPeriod / 1000);


    /* calculate horizontal blanking time in pixels */
    if (IdealDutyCycle < 20)
    {
      HorizontalBlankingPixels =
          floor((HActive * 20.0) / (100.0 - 20.0) /
          (2.0 * Display.CharacterCell)) * 2.0 * Display.CharacterCell;
    }
    else
    {
      HorizontalBlankingPixels =
          round(HActive * IdealDutyCycle / (100.0 - IdealDutyCycle) /
          (2.0 * Display.CharacterCell)) * 2.0 * Display.CharacterCell;
    }


    /* calculate total number of pixels per line */
    HTotal = HActive + HorizontalBlankingPixels;


    /* calculate horizontal clock */
    HClock = PClock / HTotal * 1000.0;


    /* calculate number of lines in vertical sync and back porch */
    VSyncPlusBackPorch =
        Display.VBackPorchPlusSync * HClock / 1000.0;

    if (Flags < 0) // if doublescan mode
    {
      VSyncPlusBackPorch = round(VSyncPlusBackPorch / 2.0) * 2.0;
    }
    else // default
    {
      VSyncPlusBackPorch = round(VSyncPlusBackPorch);
    }

/****************************************************************************
 *                                                                          *
 *   The following reality check was not a part of the gtf worksheet but    *
 *   it should have been, which is why I included it.                       *
 *                                                                          *
 ****************************************************************************/
    if (VSyncPlusBackPorch < Display.VSyncWidth + Display.VBackPorch)
    {
      VSyncPlusBackPorch = Display.VSyncWidth + Display.VBackPorch;
    }


    /* calculate number of lines in back porch */
    VBackPorch = VSyncPlusBackPorch - Display.VSyncWidth;


    /* calculate total number of lines */
    VTotal =
        VRes + TopMargin + BottomMargin +
        Display.VFrontPorch + VSyncPlusBackPorch + Interlace;
  }


  /* calculate horizontal sync width in pixels */
  HSyncWidth =
      round(Display.HSyncPercent / 100.0 * HTotal / Display.CharacterCell) *
      Display.CharacterCell;


  /* calculate horizontal back porch in pixels */
  HBackPorch = HorizontalBlankingPixels / 2.0;


  /* output modeline calculations */
  if (Flags > 0)  // if interlace mode
  {
    Modeline->Interlace = 1;
    Modeline->PClock = PClock;
    Modeline->HRes = (int) HRes;
    Modeline->HSyncStart = (int) (HTotal - HBackPorch - HSyncWidth);
    Modeline->HSyncEnd = (int) (HTotal - HBackPorch);
    Modeline->HTotal = (int) HTotal;
    Modeline->VRes = (int) (VRes * 2.0);
    Modeline->VSyncStart = (int) ((VTotal - VSyncPlusBackPorch) * 2.0);
    Modeline->VSyncEnd = (int) ((VTotal - VBackPorch) * 2.0);
    Modeline->VTotal = (int) (VTotal * 2.0);
  }
  else if (Flags < 0)  // if doublescan mode
  {
    Modeline->Doublescan = 1;
    Modeline->PClock = PClock;
    Modeline->HRes = (int) HRes;
    Modeline->HSyncStart = (int) (HTotal - HBackPorch - HSyncWidth);
    Modeline->HSyncEnd = (int) (HTotal - HBackPorch);
    Modeline->HTotal = (int) HTotal;
    Modeline->VRes = (int) (VRes / 2.0);
    Modeline->VSyncStart = (int) ((VTotal - VSyncPlusBackPorch) / 2.0);
    Modeline->VSyncEnd = (int) ((VTotal - VBackPorch) / 2.0);
    Modeline->VTotal = (int) (VTotal / 2.0);
  }
  else
  {
    Modeline->PClock = PClock;
    Modeline->HRes = (int) HRes;
    Modeline->HSyncStart = (int) (HTotal - HBackPorch - HSyncWidth);
    Modeline->HSyncEnd = (int) (HTotal - HBackPorch);
    Modeline->HTotal = (int) HTotal;
    Modeline->VRes = (int) VRes;
    Modeline->VSyncStart = (int) (VTotal - VSyncPlusBackPorch);
    Modeline->VSyncEnd = (int) (VTotal - VBackPorch);
    Modeline->VTotal = (int) VTotal;
  }

  Modeline->HSyncPolarity = 0;
  Modeline->VSyncPolarity = 1;

  return (Modeline);
} //general_timing_formula()


void print_fb_modeline(UMC_MODELINE *Modeline)
{
  double PClock = 0.0;
  double VClock = 0.0;
  double HClock = 0.0;
  char Doublescan[16] = "";
  char Interlace[16] = "";
  PClock = Modeline->PClock * 1000000.0;
  if (Modeline->Doublescan == 1)
  {
    strcpy(Doublescan, " doublescan");
    VClock = PClock / (Modeline->HTotal * 2.0 * Modeline->VTotal);
    HClock = VClock * Modeline->VTotal * 2.0 / 1000.0;
  }
  else if (Modeline->Interlace == 1)
  {
    strcpy(Interlace, " interlace");
    VClock = PClock / (Modeline->HTotal * 0.5 * Modeline->VTotal) * 0.5;
    HClock = 2 * VClock * Modeline->VTotal * 0.5 / 1000.0;
  }
  else
  {
    VClock = PClock / (Modeline->HTotal * Modeline->VTotal);
    HClock = VClock * Modeline->VTotal / 1000.0;
  }

  printf("\n# %dx%dx%.2f @ %.3fkHz%s%s\n",
         Modeline->HRes, Modeline->VRes, VClock, HClock,
         Doublescan, Interlace);

  printf("mode \"%dx%dx%.2f\"\n",
         Modeline->HRes, Modeline->VRes, VClock);

  printf("    geometry %d %d %d %d %d\n",
         Modeline->HRes, Modeline->VRes,
         Modeline->HRes, Modeline->VRes, 16);

  printf("    timings %d %d %d %d %d %d %d\n",
         (int) round(1000000.0/Modeline->PClock),
         Modeline->HTotal - Modeline->HSyncEnd,
         Modeline->HSyncStart - Modeline->HRes,
         Modeline->VTotal - Modeline->VSyncEnd,
         Modeline->VSyncStart - Modeline->VRes,
         Modeline->HSyncEnd - Modeline->HSyncStart,
         Modeline->VSyncEnd - Modeline->VSyncStart);
  if (Modeline->HSyncPolarity > 0)
  {
    printf("    hsync high\n");
  }
  else
  {
    printf("    hsync low\n");
  }

  if (Modeline->VSyncPolarity > 0)
  {
    printf("    vsync high\n");
  }
  else
  {
    printf("    vsync low\n");
  }

  if (Modeline->Doublescan == 1)
  {
    printf("    double true\n");
  }
  else if (Modeline->Interlace == 1)
  {
    printf("    laced true\n");
  }
  printf("endmode\n");
} // print_fb_modeline()

void print_modeline(UMC_MODELINE *Modeline)
{
  double PClock = 0.0;
  double VClock = 0.0;
  double HClock = 0.0;
  char HSyncPolarity[8] = "";
  char VSyncPolarity[8] = "";
  char Doublescan[16] = "";
  char Interlace[16] = "";
  PClock = Modeline->PClock * 1000000.0;
  if (Modeline->Doublescan == 1)
  {
    strcpy(Doublescan, " doublescan");
    VClock = PClock / (Modeline->HTotal * 2.0 * Modeline->VTotal);
    HClock = VClock * Modeline->VTotal * 2.0 / 1000.0;
  }
  else if (Modeline->Interlace == 1)
  {
    strcpy(Interlace, " interlace");
    VClock = PClock / (Modeline->HTotal * 0.5 * Modeline->VTotal) * 0.5;
    HClock = 2 * VClock * Modeline->VTotal * 0.5 / 1000.0;
  }
  else
  {
    VClock = PClock / (Modeline->HTotal * Modeline->VTotal);
    HClock = VClock * Modeline->VTotal / 1000.0;
  }

  if (Modeline->HSyncPolarity > 0)
  {
    strcpy(HSyncPolarity, "+HSync");
  }
  else
  {
    strcpy(HSyncPolarity, "-HSync");
  }

  if (Modeline->VSyncPolarity > 0)
  {
    strcpy(VSyncPolarity, "+VSync");
  }
  else
  {
    strcpy(VSyncPolarity, "-VSync");
  }

  printf("\n    # %dx%dx%.2f @ %.3fkHz\n",
         Modeline->HRes, Modeline->VRes, VClock, HClock);

  printf ("    Modeline \"%dx%dx%.2f\"  %f  %d %d %d %d"
      "  %d %d %d %d  %s %s%s%s\n",
  Modeline->HRes, Modeline->VRes, VClock, Modeline->PClock,
  Modeline->HRes, Modeline->HSyncStart, Modeline->HSyncEnd, Modeline->HTotal,
  Modeline->VRes, Modeline->VSyncStart, Modeline->VSyncEnd, Modeline->VTotal,
  HSyncPolarity, VSyncPolarity, Doublescan, Interlace);
}

int main(int argc, char *argv[])
{

  /* initialize variables */
  int i = 4;                 //counter for *argv[] loop
  int Calculator = 0;        //which calculator to use
  int PrintFormat = 0;       //which print format to use
  double HRes = 0.0;         //input: horizontal resolution
  double VRes = 0.0;         //input: vertical resolution
  double Clock = 0.0;        //input: clock in Hz
  double Flags = 0.0;        //input: doublescan, interlace flag
  double VFrontPorch = -1.0;  //vertical front porch
  double VSyncWidth = -1.0;   //vertical sync width


  /* declare and initialize input display */
  UMC_DISPLAY Display = UMC_CVT;
  Display.VFrontPorch = 4.0;  //make vertical front porch doublescan friendly
  Display.PClockStep = 0.0;   //gtf default


  /* declare and initialize UMC_MODELINE return pointer */
  UMC_MODELINE *Modeline = NULL;


  /* calling program */
  program_name = argv[0];


  /* parse command line options */
  if (
      (argc > 1) &&
      ((strcmp (argv[1], "-h") == 0) || (strcmp (argv[1], "--help") == 0))
     )
  {
    usage(EXIT_SUCCESS);
  }
  else if (
           (argc > 1) &&
           (strcmp (argv[1], "--version") == 0)
          )
  {
    printf("Universal Modeline Calculator 0.2\n\n");
    printf("Copyright (C) 2005 Desmond Colin Jones.\n");
    printf("This is free software; see the source for copying conditions. ");
    printf("There is NO\nwarranty; not even for MERCHANTABILITY or FITNESS");
    printf(" FOR A PARTICULAR PURPOSE.\n");
    exit (EXIT_SUCCESS);
  }
  else if (
           (argc < 4) ||
           (atof (argv[1]) < 1) ||
           (atof (argv[2]) < 1) ||
           (atof (argv[3]) < 1)
          )
  {
    usage(EXIT_FAILURE);
  }
  else
  {
    HRes = atof (argv[1]);
    VRes = atof (argv[2]);
    Clock = atof (argv[3]);
  }
  while (i < argc)
  {
    if ((strcmp (argv[i], "-i") == 0) ||
         (strcmp (argv[i], "--interlace") == 0))
    {
      Flags = 1;
    }
    else if ((strcmp (argv[i], "-d") == 0) ||
              (strcmp (argv[i], "--doublescan") == 0))
    {
      Flags = -1;
    }
    else if ((strcmp (argv[i], "-fb") == 0) ||
              (strcmp (argv[i], "--fbset") == 0))
    {
      PrintFormat = 1;
    }
    else if ((strcmp (argv[i], "-x") == 0) ||
              (strcmp (argv[i], "--X11R6") == 0))
    {
      PrintFormat = 0;
    }
    else if ((strcmp (argv[i], "-gtf") == 0) ||
              (strcmp (argv[i], "--gtf") == 0))
    {
      Display.VFrontPorch = 1;  //restore vertical front porch to default
      Display.VSyncWidth = 3;   //restore vertical sync width to default
    }
    else if ((strcmp (argv[i], "-cvt") == 0) ||
              (strcmp (argv[i], "--cvt") == 0))
    {
      Calculator = 1;
      Display.VFrontPorch = 3;    //restore vertical front porch to default
      Display.PClockStep = 0.25;  //restore pixel clock step to default
    }
    else if ((strcmp (argv[i], "-rbt") == 0) ||
              (strcmp (argv[i], "--rbt") == 0))
    {
      Calculator = 2;
      Display.VFrontPorch = 3;    //restore vertical front porch to default
      Display.PClockStep = 0.25;  //restore pixel clock step to default
    }
    else if (
             (strcmp (argv[i], "-m") == 0) &&
             (i + 1 < argc)
            )
    {
      ++i;
      Display.Margin = atof (argv[i]);
      if (Display.Margin < 0)
      {
        fprintf(stderr, "%.2f is not a valid margin percentage ",
                Display.Margin);
        fprintf(stderr, "Error %d\n", EXIT_FAILURE);
        exit_status = EXIT_FAILURE;
      }
    }
    else if ((strncmp (argv[i], "--margin=", 9) == 0) &&
              (argv[i][9] != '\0'))
    {
      Display.Margin = atof (argv[i] + 9);
      if (Display.Margin < 0)
      {
        fprintf(stderr, "%.2f is not a valid margin percentage ",
                Display.Margin);
        fprintf(stderr, "Error %d\n", EXIT_FAILURE);
        exit_status = EXIT_FAILURE;
      }
    }
    else if (
             (strcmp (argv[i], "-c") == 0) &&
             (i + 1 < argc)
            )
    {
      ++i;
      Display.CharacterCell = atof (argv[i]);
      if (Display.CharacterCell < 1)
      {
        fprintf(stderr, "%.2f character cell must be 1 or greater ",
                Display.CharacterCell);
        fprintf(stderr, "Error %d\n", EXIT_FAILURE);
        exit_status = EXIT_FAILURE;
      }
    }
    else if ((strncmp (argv[i], "--character-cell=", 17) == 0) &&
              (argv[i][17] != '\0'))
    {
      Display.CharacterCell = atof (argv[i] + 17);
      if (Display.CharacterCell < 1)
      {
        fprintf(stderr, "%.2f character cell must be 1 or greater ",
                Display.CharacterCell);
        fprintf(stderr, "Error %d\n", EXIT_FAILURE);
        exit_status = EXIT_FAILURE;
      }
    }
    else if (
             (strcmp (argv[i], "-p") == 0) &&
             (i + 1 < argc)
            )
    {
      ++i;
      Display.PClockStep = atof (argv[i]);
      if (Display.PClockStep < 0)
      {
        fprintf(stderr, "%.2f pixel clock step must be positive ",
                Display.PClockStep);
        fprintf(stderr, "Error %d\n", EXIT_FAILURE);
        exit_status = EXIT_FAILURE;
      }
    }
    else if ((strncmp (argv[i], "--pclock-step=", 14) == 0) &&
              (argv[i][14] != '\0'))
    {
      Display.PClockStep = atof (argv[i] + 14);
      if (Display.PClockStep < 0)
      {
        fprintf(stderr, "%.2f pixel clock step must be positive ",
                Display.PClockStep);
        fprintf(stderr, "Error %d\n", EXIT_FAILURE);
        exit_status = EXIT_FAILURE;
      }
    }
    else if (
             (strcmp (argv[i], "-h") == 0) &&
             (i + 1 < argc)
            )
    {
      ++i;
      Display.HSyncPercent = atof (argv[i]);
      if ((Display.HSyncPercent < 0) || (Display.HSyncPercent > 100))
      {
        fprintf(stderr, "%.2f is not a valid percentage ",
                Display.HSyncPercent);
        fprintf(stderr, "Error %d\n", EXIT_FAILURE);
        exit_status = EXIT_FAILURE;
      }
    }
    else if ((strncmp (argv[i], "--horizontal-sync=", 18) == 0) &&
              (argv[i][18] != '\0'))
    {
      Display.HSyncPercent = atof (argv[i] + 18);
      if ((Display.HSyncPercent < 0) || (Display.HSyncPercent > 100))
      {
        fprintf(stderr, "%.2f is not a valid percentage ",
                Display.HSyncPercent);
        fprintf(stderr, "Error %d\n", EXIT_FAILURE);
        exit_status = EXIT_FAILURE;
      }
    }
    else if (
             (strcmp (argv[i], "-f") == 0) &&
             (i + 1 < argc)
            )
    {
      ++i;
      VFrontPorch = atof (argv[i]);
      if (VFrontPorch < 0)
      {
        fprintf(stderr, "%.2f front porch must be positive ",
                VFrontPorch);
        fprintf(stderr, "Error %d\n", EXIT_FAILURE);
        exit_status = EXIT_FAILURE;
      }
    }
    else if ((strncmp (argv[i], "--vertical-front-porch=", 23) == 0) &&
              (argv[i][23] != '\0'))
    {
      VFrontPorch = atof (argv[i] + 23);
      if (VFrontPorch < 0)
      {
        fprintf(stderr, "%.2f front porch must be positive ",
                VFrontPorch);
        fprintf(stderr, "Error %d\n", EXIT_FAILURE);
        exit_status = EXIT_FAILURE;
      }
    }
    else if (
             (strcmp (argv[i], "-v") == 0) &&
             (i + 1 < argc)
            )
    {
      ++i;
      VSyncWidth = atof (argv[i]);
      if (VSyncWidth < 0)
      {
        fprintf(stderr, "%.2f sync width must be positive ",
                VSyncWidth);
        fprintf(stderr, "Error %d\n", EXIT_FAILURE);
        exit_status = EXIT_FAILURE;
      }
    }
    else if ((strncmp (argv[i], "--vertical-sync=", 16) == 0) &&
              (argv[i][16] != '\0'))
    {
      VSyncWidth = atof (argv[i] + 16);
      if (VSyncWidth < 0)
      {
        fprintf(stderr, "%.2f sync width must be positive ",
                VSyncWidth);
        fprintf(stderr, "Error %d\n", EXIT_FAILURE);
        exit_status = EXIT_FAILURE;
      }
    }
    else if (
             (strcmp (argv[i], "-b") == 0) &&
             (i + 1 < argc)
            )
    {
      ++i;
      Display.VBackPorch = atof (argv[i]);
      if (Display.VBackPorch < 0)
      {
        fprintf(stderr, "%.2f back porch must be postive ",
                Display.VBackPorch);
        fprintf(stderr, "Error %d\n", EXIT_FAILURE);
        exit_status = EXIT_FAILURE;
      }
    }
    else if ((strncmp (argv[i], "--vertical-back-porch=", 22) == 0) &&
              (argv[i][22] != '\0'))
    {
      Display.VBackPorch = atof (argv[i] + 22);
      if (Display.VBackPorch < 0)
      {
        fprintf(stderr, "%.2f back porch must be postive ",
                Display.VBackPorch);
        fprintf(stderr, "Error %d\n", EXIT_FAILURE);
        exit_status = EXIT_FAILURE;
      }
    }
    else if (
             (strcmp (argv[i], "-y") == 0) &&
             (i + 1 < argc)
            )
    {
      ++i;
      Display.VBackPorchPlusSync = atof (argv[i]);
      if (Display.VBackPorchPlusSync < 0)
      {
        fprintf(stderr, "%.2f back porch plus sync must be positive ",
                Display.VBackPorchPlusSync);
        fprintf(stderr, "Error %d\n", EXIT_FAILURE);
        exit_status = EXIT_FAILURE;
      }
    }
    else if ((strncmp (argv[i], "--back-porch-plus-sync=", 23) == 0) &&
              (argv[i][23] != '\0'))
    {
      Display.VBackPorchPlusSync = atof (argv[i] + 23);
      if (Display.VBackPorchPlusSync < 0)
      {
        fprintf(stderr, "%.2f back porch plus sync must be positive ",
                Display.VBackPorchPlusSync);
        fprintf(stderr, "Error %d\n", EXIT_FAILURE);
        exit_status = EXIT_FAILURE;
      }
    }
    else if (
             (strcmp (argv[i], "-w") == 0) &&
             (i + 1 < argc)
            )
    {
      ++i;
      Display.HBlankingTicks = atof (argv[i]);
      if (Display.HBlankingTicks < 0)
      {
        fprintf(stderr, "%.2f horizontal blanking time must be positive ",
                Display.HBlankingTicks);
        fprintf(stderr, "Error %d\n", EXIT_FAILURE);
        exit_status = EXIT_FAILURE;
      }
    }
    else if ((strncmp (argv[i], "--horizontal-blanking-ticks=", 28) == 0) &&
              (argv[i][28] != '\0'))
    {
      Display.HBlankingTicks = atof (argv[i] + 28);
      if (Display.HBlankingTicks < 0)
      {
        fprintf(stderr, "%.2f horizontal blanking time must be positive ",
                Display.HBlankingTicks);
        fprintf(stderr, "Error %d\n", EXIT_FAILURE);
        exit_status = EXIT_FAILURE;
      }
    }
    else if (
             (strcmp (argv[i], "-s") == 0) &&
             (i + 1 < argc)
            )
    {
      ++i;
      Display.HSyncTicks = atof (argv[i]);
      if (Display.HSyncTicks < 0)
      {
        fprintf(stderr, "%.2f horizontal sync time must be positive ",
                Display.HSyncTicks);
        fprintf(stderr, "Error %d\n", EXIT_FAILURE);
        exit_status = EXIT_FAILURE;
      }
    }
    else if ((strncmp (argv[i], "--horizontal-sync-ticks=", 24) == 0) &&
              (argv[i][24] != '\0'))
    {
      Display.HSyncTicks = atof (argv[i] + 24);
      if (Display.HSyncTicks < 0)
      {
        fprintf(stderr, "%.2f horizontal sync time must be positive ",
                Display.HSyncTicks);
        fprintf(stderr, "Error %d\n", EXIT_FAILURE);
        exit_status = EXIT_FAILURE;
      }
    }
    else if (
             (strcmp (argv[i], "-t") == 0) &&
             (i + 1 < argc)
            )
    {
      ++i;
      Display.VBlankingTime = atof (argv[i]);
      if (Display.VBlankingTime < 0)
      {
        fprintf(stderr, "%.2f vertical blanking time must be positive ",
                Display.VBlankingTime);
        fprintf(stderr, "Error %d\n", EXIT_FAILURE);
        exit_status = EXIT_FAILURE;
      }
    }
    else if ((strncmp (argv[i], "--vertical-blanking-time=", 25) == 0) &&
              (argv[i][25] != '\0'))
    {
      Display.VBlankingTime = atof (argv[i] + 25);
      if (Display.VBlankingTime < 0)
      {
        fprintf(stderr, "%.2f vertical blanking time must be positive ",
                Display.VBlankingTime);
        fprintf(stderr, "Error %d\n", EXIT_FAILURE);
        exit_status = EXIT_FAILURE;
      }
    }
    else if (
             (strcmp (argv[i], "-M") == 0) &&
             (i + 1 < argc)
            )
    {
      ++i;
      Display.M = atof (argv[i]);
      if (Display.M < 0)
      {
        fprintf(stderr, "%.2f gradient must be positive ",
                Display.M);
        fprintf(stderr, "Error %d\n", EXIT_FAILURE);
        exit_status = EXIT_FAILURE;
      }
    }
    else if ((strncmp (argv[i], "--gradient=", 11) == 0) &&
              (argv[i][11] != '\0'))
    {
      Display.M = atof (argv[i] + 11);
      if (Display.M < 0)
      {
        fprintf(stderr, "%.2f gradient must be positive ",
                Display.M);
        fprintf(stderr, "Error %d\n", EXIT_FAILURE);
        exit_status = EXIT_FAILURE;
      }
    }
    else if (
             (strcmp (argv[i], "-C") == 0) &&
             (i + 1 < argc)
            )
    {
      ++i;
      Display.C = atof (argv[i]);
      if (Display.C < 0)
      {
        fprintf(stderr, "%.2f offset must be 0 or higher ",
                Display.C);
        fprintf(stderr, "Error %d\n", EXIT_FAILURE);
        exit_status = EXIT_FAILURE;
      }
    }
    else if ((strncmp (argv[i], "--offset=", 9) == 0) &&
              (argv[i][9] != '\0'))
    {
      Display.C = atof (argv[i] + 9);
      if (Display.C < 0)
      {
        fprintf(stderr, "%.2f offset must be 0 or higher ",
                Display.C);
        fprintf(stderr, "Error %d\n", EXIT_FAILURE);
        exit_status = EXIT_FAILURE;
      }
    }
    else if (
             (strcmp (argv[i], "-K") == 0) &&
             (i + 1 < argc)
            )
    {
      ++i;
      Display.K = atof (argv[i]);
      if (Display.K < 0)
      {
        fprintf(stderr, "%.2f scaling factor must be 0 or higher ",
                Display.K);
        fprintf(stderr, "Error %d\n", EXIT_FAILURE);
        exit_status = EXIT_FAILURE;
      }
    }
    else if ((strncmp (argv[i], "--scaling-factor=", 17) == 0) &&
              (argv[i][17] != '\0'))
    {
      Display.K = atof (argv[i] + 17);
      if (Display.K < 0)
      {
        fprintf(stderr, "%.2f scaling factor must be 0 or higher ",
                Display.K);
        fprintf(stderr, "Error %d\n", EXIT_FAILURE);
        exit_status = EXIT_FAILURE;
      }
    }
    else if (
             (strcmp (argv[i], "-J") == 0) &&
             (i + 1 < argc)
            )
    {
      ++i;
      Display.J = atof (argv[i]);
      if (Display.J < 0)
      {
        fprintf(stderr, "%.2f scaling factor weight must be 0 or higher ",
                Display.J);
        fprintf(stderr, "Error %d\n", EXIT_FAILURE);
        exit_status = EXIT_FAILURE;
      }
    }
    else if ((strncmp (argv[i], "--scale-factor-weight=", 22) == 0) &&
              (argv[i][22] != '\0'))
    {
      Display.J = atof (argv[i] + 22);
      if (Display.J < 0)
      {
        fprintf(stderr, "%.2f scaling factor weight must be 0 or higher ",
                Display.J);
        fprintf(stderr, "Error %d\n", EXIT_FAILURE);
        exit_status = EXIT_FAILURE;
      }
    }
    else
    {
      exit_status = EXIT_FAILURE;
    }
    ++i;
  }

  /* calculate modeline */
  if (exit_status != EXIT_FAILURE)
  {
    if (VFrontPorch >= 0)
    {
      Display.VFrontPorch = VFrontPorch;
    }
    if (VSyncWidth >= 0)
    {
      Display.VSyncWidth = VSyncWidth;
    }

    if (Calculator == 1)
    {
      Modeline = coordinated_video_timing(HRes, VRes, Clock, Display, Flags);
    }
    else if (Calculator == 2)
    {
      Modeline = reduced_blanking_timing(HRes, VRes, Clock, Display, Flags);
    }
    else
    {
      Modeline = general_timing_formula(HRes, VRes, Clock, Display, Flags);
    }

    if (PrintFormat == 1)
    {
      print_fb_modeline(Modeline);
    }
    else
    {
      print_modeline(Modeline);
    }
  }
  else
  {
    usage(EXIT_FAILURE);
  }

  return(exit_status);
}

