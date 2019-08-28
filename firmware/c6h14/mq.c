/*
mq 0x02

copyright (c) Davide Gironi, 2016

Released under GPLv3.
Please refer to LICENSE file for licensing information.
*/


#include "mq.h"

#include <stdio.h>
#include <math.h> //include libm

//prevent out of temperature and humidity limits
#define MQ_RSROTEMPHUMDGAINPREVENTOUTOFLIMITS 1

/*
 * get the calibrated Ro based upon read resistance
 * given the know ppm amount of gas, scalingfactor and exponent coefficient for the correlation function
 */
long mq_getro(long resvalue, double ppm, double scalingfactor, double exponent) {
	return (long)((double)resvalue * pow((scalingfactor/ppm), (1/exponent)));
}

/*
 * get the ppm concentration based upon read resistance
 * given the Ro of the gas, scalingfactor and exponent coefficient for the correlation function
 * set maxrsro and mixrsro limits to prevent correlation error
 */
double mq_getppm(long resvalue, long ro, double scalingfactor, double exponent, double maxrsro, double minrsro) {
	double ret = 0;
	double validinterval = resvalue/(double)ro;
	//check valid interval
	if(validinterval<maxrsro && validinterval>minrsro) {
		//perform correlation
		ret = scalingfactor * pow(((double)resvalue/ro), exponent);
	}
	return ret;
}

/*
 * find Rs/Ro ratio given the acutal temp and humd
 * given the dependency curve Ro reference temp and humd, the lookup temperature table, and the lookup Rs/Ro ratio
 * lookupth1 and lookupth2 at humidity value lookupth1humdvalue and lookupth2humdvalue
 */
double mq_rsrotemphumdgain(double actualtemp, double actualhumd, int lookupthsize, double *lookuptht, double *lookupth1, double *lookupth2, double lookupth1humdvalue, double lookupth2humdvalue) {
	double ret = -1;

	int i = 0;
	double rsroth1 = -1;
	double rsroth2 = -1;

	//find lookup temperature point
	while((i < (lookupthsize)) && (actualtemp > lookuptht[i])) {
		i++;
	}
	//check limit max
	if(i == lookupthsize) {
#if MQ_RSROTEMPHUMDGAINPREVENTOUTOFLIMITS == 1
		return -1;
#else
		rsroth1 = lookupth1[i-1];
		rsroth2 = lookupth2[i-1];
#endif
	//check limit min
	} else if(i == 0){
#if MQ_RSROTEMPHUMDGAINPREVENTOUTOFLIMITS == 1
		return -1;
#else
		rsroth1 = lookupth1[i];
		rsroth2 = lookupth2[i];
#endif
	//compute interpolation
	} else {
		//find ratio actualtemp and lookupth1humdvalue
		rsroth1 = (lookupth1[i] - lookupth1[i-1]) / (lookuptht[i] - lookuptht[i-1]) * (actualtemp - lookuptht[i]) + lookupth1[i];
		//find ratio actualtemp and lookupth2humdvalue
		rsroth2 = (lookupth2[i] - lookupth2[i-1]) / (lookuptht[i] - lookuptht[i-1]) * (actualtemp - lookuptht[i]) + lookupth2[i];
	}
	//compute interpolation, find ratio actualtemp and actualhumd
	ret = rsroth1 + (actualhumd - lookupth1humdvalue)*(rsroth2 - rsroth1)/(lookupth2humdvalue-lookupth1humdvalue);

	return ret;
}

/*
 * get the calibrated Ro based upon read resistance
 * given the know ppm amount of gas, scalingfactor and exponent coefficient for the correlation function
 * correlate the Ro to the actualtemp and actualhumd
 * given the sensitivity characteristics curve Ro reference temp and humd, the lookup temperature table, and the lookup Rs/Ro ratio
 * lookupth1 and lookupth2 at humidity value lookupth1humdvalue and lookupth2humdvalue
 */
long mq_getrotemphumd(long resvalue, double ppm, double scalingfactor, double exponent, double actualtemp, double actualhumd, double senreftemp, double senrefhumd, int lookupthsize, double *lookuptht, double *lookupth1, double *lookupth2, double lookupth1humdvalue, double lookupth2humdvalue) {
	long ret = 0;
	//compute ro
	long ro = mq_getro(resvalue, ppm, scalingfactor, exponent);
	//get ratio actualtemp actualhumd over dependence curve
	double m = mq_rsrotemphumdgain(actualtemp, actualhumd, lookupthsize, lookuptht, lookupth1, lookupth2, lookupth1humdvalue, lookupth2humdvalue);
	//get ratio reftemp refhumd over dependence curve
	double n = mq_rsrotemphumdgain(senreftemp, senrefhumd, lookupthsize, lookuptht, lookupth1, lookupth2, lookupth1humdvalue, lookupth2humdvalue);
	//compute correlated ro
	ret = (long)((n/m)*ro);
	return ret;
}

/*
 * get the ppm concentration based upon read resistance
 * given the Ro of the gas, scalingfactor and exponent coefficient for the correlation function
 * set maxrsro and mixrsro limits to prevent correlation error
 * correlate the ppm to the actualtemp and actualhumd
 * given the sensitivity characteristics curve Ro reference temp and humd, the lookup temperature table, and the lookup Rs/Ro ratio
 * lookupth1 and lookupth2 at humidity value lookupth1humdvalue and lookupth2humdvalue
 */
double mq_getppmtemphumd(long resvalue, long ro, double scalingfactor, double exponent, double maxrsro, double minrsro, double actualtemp, double actualhumd, double senreftemp, double senrefhumd, int lookupthsize, double *lookuptht, double *lookupth1, double *lookupth2, double lookupth1humdvalue, double lookupth2humdvalue) {
	double ret = 0;
	//get ratio actualtemp actualhumd over dependence curve
	double t = mq_rsrotemphumdgain(actualtemp, actualhumd, lookupthsize, lookuptht, lookupth1, lookupth2, lookupth1humdvalue, lookupth2humdvalue);
	//get ratio reftemp refhumd over dependence curve
	double q = mq_rsrotemphumdgain(senreftemp, senrefhumd, lookupthsize, lookuptht, lookupth1, lookupth2, lookupth1humdvalue, lookupth2humdvalue);
	//get correlated resvalue
	long resvaluetemphum = (long)((q/t)*resvalue);
	//compute correlated ppm
	ret = mq_getppm(resvaluetemphum, ro, scalingfactor, exponent, maxrsro, minrsro);
	return ret;
}

