/*
mq 0x02

copyright (c) Davide Gironi, 2016

Released under GPLv3.
Please refer to LICENSE file for licensing information.
*/


#ifndef MQ_H_
#define MQ_H_

//functions
extern long mq_getro(long resvalue, double ppm, double scalingfactor, double exponent);
extern double mq_getppm(long resvalue, long ro, double scalingfactor, double exponent, double maxrsro, double minrsro);
extern long mq_getrotemphumd(long resvalue, double ppm, double scalingfactor, double exponent, double actualtemp, double actualhumd, double senreftemp, double senrefhumd, int lookupthsize, double *lookuptht, double *lookupth1, double *lookupth2, double lookupth1humdvalue, double lookupth2humdvalue);
extern double mq_getppmtemphumd(long resvalue, long ro, double scalingfactor, double exponent, double maxrsro, double minrsro, double actualtemp, double actualhumd, double senreftemp, double senrefhumd, int lookupthsize, double *lookuptht, double *lookupth1, double *lookupth2, double lookupth1humdvalue, double lookupth2humdvalue);

#endif
