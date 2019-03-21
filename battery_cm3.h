/*
Data acquisition : backlight off, nothing running except of cpuburn-a53 running on one core, battery reading done every 5 sec with 3 digits precision.
Since the system is powered with a Li-ion battery, the voltage drop at this load (25% of the cpu) is around 0.05v, similar result during emulation.

To avoid usage of float or double, battery voltage is multiply by 1000.

Note: because battery percentage can "bounce" easily due to spike when reading voltage, percentage precision in set to 10% increment only
*/

//int voltage_drop_per_core=30;
int battery_precision=10;

int battery_percentage[12]={
3081,		//0%
3416,		//10%
3522,		//20%
3621,		//30%
3648,		//40%
3672,		//50%
3734,		//60%
3817,		//70%
3888,		//80%
3983,		//90%
4095};	//100%


