# NMEA Battery monitor

## Shunt settings

### Shunt resistance [mΩ]

### Expected max current [A]

### Voltage calibration factor

### Current calibration factor

## Battery settings

### Type

### Capacity

This parameter is used to tell the battery monitor how big the battery is. This setting should already have been done during the initial installation.

The setting is the battery capacity in Amp-hours (Ah).

### charge efficiency [%]

The “Charge Efficiency Factor” compensates for the capacity (Ah) losses during charging. A setting of 100% means that there are no losses.
A charge efficiency of 95% means that 10Ah must be transferred to the battery to get 9.5Ah actually stored in the battery. The charge efficiency of a 
battery depends on battery type, age and usage. The battery monitor takes this phenomenon into account with the charge efficiency factor.

The charge efficiency of a lead acid battery is almost 100% as long as no gas generation takes place. Gassing means that part of the charge current 
is not transformed into chemical energy, which is stored in the plates of the battery, but is used to decompose water into oxygen and hydrogen 
gas (highly explosive!). The energy stored in the plates can be retrieved during the next discharge, whereas the energy used to decompose water is lost. 
Gassing can easily be observed in flooded batteries. Please note that the ‘oxygen only’ end of the charge phase of sealed (VRLA) gel 
and AGM batteries also results in a reduced charge efficiency.

### Minimun SOC [%]

## Battery full detection

### Voltage when full [V]

The battery voltage must be above this voltage level to consider the battery as fully charged. As soon as the battery monitor detects that the 
voltage of the battery has reached this “charged voltage” parameter and the current has dropped below the “tail current” parameter for a certain 
amount of time, the battery monitor will set the state of charge to 100%.

### Tail current [A]
The battery is considered as fully charged once the charge current has dropped to less than this “Tail current” parameter. 
The “Tail current” parameter is expressed as a percentage of the battery capacity.

Note that some battery chargers stop charging when the current drops below a set threshold. In these cases, the tail current must be set 
higher than this threshold.

As soon as the battery monitor detects that the voltage of the battery has reached the set “Charged voltage” parameter and the current has 
dropped below this “Tail current” parameter for a certain amount of time, the battery monitor will set the state of charge to 100%.

### Delay before full [s]

This is the time the "Voltage when full [V]" parameter and the "Tail current [A]" parameter must be met in order to consider the battery fully charged.