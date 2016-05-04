   SourceForge.net Logo

                                      UMC

   Section: User Manuals (1)
   Updated: MARCH 2005
   Index Download

     ----------------------------------------------------------------------

    

NAME

   umc - universal modeline calculator  

SYNOPSIS

   umc x y refresh [OPTIONS]
   umc x y hclock [OPTIONS]
   umc x y pclock [OPTIONS]  

DESCRIPTION

   umc is a modeline calculator that fully implements the Coordinated Video
   Timing and General Timing Formula spreadsheets published by the Video
   Electronics Standards Association (www.vesa.org). Simply input a desired
   resolution and clock frequency. All clock frequencies must be given in Hz.
   For a full length, mostly accurate modeline tutorial visit
   http://lrmc.sourceforge.net  

OPTIONS

   -d
   --doublescan
           Calculate a doublescanned modeline. Doublescanned modes are useful
           for running low resolutions on high resolution monitors, but they
           can also be used to display low resolutions with a very small
           scanline, popular gaming resolutions like 640x480 and 800x600.
           Doublescanned modes are not supported by many video cards. You
           should only use this option if you know your video card supports
           them.
   -i
   --interlace
           Calculate an interlaced modeline. Interlaced modes are useful for
           running very high resolutions. Interlaced modes are not supported
           by many video cards. You should only use this option if you know
           your video card supports them.
   -m number
   --margin=number
           Margin size as a percentage of the resolution (default = 0). All
           margins are equal percentage wise (i.e. %top = %bottom = %left =
           %right).
   -v number
   --vertical-sync=number
           Number of lines in vertical sync, UMC = 4, GTF = 3, CVT varies by
           aspect ratio as follows:
           4:3 = 4 lines (default)
           16:9 = 5 lines
           16:10 = 6 lines
           5:4 = 7 lines
           15:9 = 7 lines
   -x
   --X11R6
           Print modeline in X11R6 format (default).
   -f
   --fb
           Print modeline in fbset format.
   --gtf
           Use General Timing Formula. To avoid any parameter conflicts
           always specify this option first. UMC's default calculations are
           equivalent to:
           umc x y clock -gtf -f 4 -v 4 [OPTIONS]
   --cvt
           Use Coordinated Video Timing. See -v, --vertical-sync option above
           for the correct number of lines to use in vertical sync. The
           default is 4 lines = 4:3 aspect ratio (i.e. standard PC monitor).
           To avoid any parameter conflicts always specify this option first.
   --rbt
           Use Reduced Blanking Timing. To avoid any parameter conflicts
           always specify this option first.
    

ADVANCED OPTIONS

   -c number
   --character-cell=number
           Number of pixels per character cell (default = 8).
   -p number
   --pclock-step=number
           Pixel clock multiple (UMC default = 0, GTF default = 0, CVT
           default = 0.25).
   -h number
   --horizontal-sync=number
           Horizontal sync as a percentage of total active (default = 8).
   -f number
   --vertical-front-porch=number
           Number of lines in front porch (UMC default = 4, GTF default = 1,
           CVT default = 3).
   -b number
   --vertical-back-porch=number
           Minimum number of lines in vertical back porch (default = 6).
   -y number
   --back-porch-plus-sync=number
           Number of microseconds in vertical sync and back porch (default
           550).
   -w number
   --horizontal-blanking-ticks=number
           Number of pixel clock ticks in horizontal blanking time, RBT only
           (default 160).
   -s number
   --horizontal-sync-ticks=number
           Number of pixel clock ticks in horizontal sync, RBT only (default
           32).
   -t number
   --vertical-blanking-time=number
           Minimum vertical blanking time in microseconds, RBT only (default
           = 460)
   -M number
   --gradient=number
           The value of M used to calculate the horizontal blanking time
           (DEFAULT = 600).
   -C number
   --offset=number
           The value of C used to calculate the horizontal blanking time
           (DEFAULT = 40).
   -K number
   --scaling-factor=number
           The value of K used to calculate the horizontal blanking time
           (DEFAULT = 128).
   -J number
   --scale-factor-weight=number
           The value of J used to calculate the horizontal blanking time
           (DEFAULT = 20).
    

EXAMPLES

    umc 1280 960 72
           Calculate a modeline for a 1280x960 resolution running at a
           refresh rate of 72Hz. Print the modeline in X11 format.
    umc 640 480 60 -d
           Calculate a modeline for a 640x480 resolution running at a refresh
           rate of 60Hz, doublescanned. Print the modeline in X11 format.
    umc 1280 960 31500 -i
           Calculate a modeline for a 1280x960 resolution running at a
           horizontal clock of 31.5kHz, interlaced. Print the modeline in X11
           format.
    umc 1280 960 125000000
           Calculate a modeline for a 1280x960 resolution running at a pixel
           clock of 125mHz. Print the modeline in X11 format.
    

AUTHOR

   Written by Des Jones.  

BUGS THAT AREN'T BUGS

   Doublescanned calculations are not formally laid out in either the GTF or
   CVT worksheets. Though easily added, for consistency purposes the default
   vertical front porch and sync times were changed to be a multiple of 2.
   Consequently, umc calculations differ ever so slightly from VESA's. See
   the --gtf and --cvt options above for unmolested calculations. Only the
   GTF supports horizontal clock and pixel clock driven calculations. Clock
   frequencies must always be given in Hz, never in kHz or mHz.  

BUGS

   The fbset modeline format has never been tested. It may not work. Other
   bugs may be reported at http://sourceforge.net/projects/umc/  

COPYRIGHT

   Copyright 2005 Des Jones.
   This is free software; see the source for copying conditions. There is NO
   warranty; not even for MERCHANTABILITY or FITNESS FOR A PARTICULAR
   PURPOSE.  

SEE ALSO

   lrmc(1), advv(1), gtf(1)

     ----------------------------------------------------------------------

    

Index

   NAME

   SYNOPSIS

   DESCRIPTION

   OPTIONS

   ADVANCED OPTIONS

   EXAMPLES

   AUTHOR

   BUGS THAT AREN'T BUGS

   BUGS

   COPYRIGHT

   SEE ALSO

     ----------------------------------------------------------------------

   This document was created by man2html, using the manual pages.
   Time: 04:13:41 GMT, May 14, 2005
