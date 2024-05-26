// INA226EnumToString.h

#ifndef _INA226ENUMTOSTRING_h
#define _INA226ENUMTOSTRING_h

#if defined(ARDUINO) && ARDUINO >= 100
	#include "arduino.h"
#else
	#include "WProgram.h"
#endif

#include "INA226.h"

const char* INA226EnumTypeEmpty = "";

template<typename T> void PrintINA226EnumType(T a, Stream* OutputStream, bool addLF = true) {
	const char* str = INA226EnumTypeToStr(a);
	if (str[0] != '\0') {
		if (addLF) { OutputStream->println(str); }
		else { OutputStream->print(str); }
	}
	else {
		OutputStream->print(F("unknown (")); OutputStream->print(a); OutputStream->println(F(")"));
	}
}

#define MakeINA226EnumTypeToStrFunc(enumType,strs) \
const char * INA226EnumTypeToStr(enumType enumVal) { \
  if ( enumVal<(sizeof(strs)/sizeof(const char *)) ) { \
    return strs[enumVal]; \
  } else { return INA226EnumTypeEmpty; }\
}

const char* INA226ModeStrs[] = {
	"Power Down",
	"Shunt Voltage, Triggered",
	"Bus Voltage, Triggered",
	"Shunt and Bus, Triggered",
	"ADC Off",
	"Shunt Voltage, Continuous",
	"Bus Voltage, Continuous",
	"Shunt and Bus, Continuous"
};
MakeINA226EnumTypeToStrFunc(ina226_mode_t, INA226ModeStrs);

const char* INA226AveragesStrs[] = {
	"1 sample",
	"4 samples",
	"16 samples",
	"64 samples",
	"128 samples",
	"256 samples",
	"512 samples",
	"1024 samples"
};
MakeINA226EnumTypeToStrFunc(ina226_averages_t, INA226AveragesStrs);

const char* INA226BusConvTimeStrs[] = {
	"140us",
	"204us",
	"332us",
	"588us",
	"1100us",
	"2116us",
	"4156us",
	"8244us"
};
MakeINA226EnumTypeToStrFunc(ina226_busConvTime_t, INA226BusConvTimeStrs);

const char* INA226ShuntConvTimeStrs[] = {
	"140us",
	"204us",
	"332us",
	"588us",
	"1100us",
	"2116us",
	"4156us",
	"8244us"
};
MakeINA226EnumTypeToStrFunc(ina226_shuntConvTime_t, INA226ShuntConvTimeStrs);


#endif

