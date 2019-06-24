/*
Data acquisition : backlight off, nothing running except of cpuburn-a53 running on one core, battery reading done every 5 sec with 3 digits precision.
Since the system is powered with a Li-ion battery, the voltage drop at this load (25 of the cpu) is around 0.05v, similar result during emulation.

To avoid usage of float or double, battery voltage is multiply by 1000.

Note: Following data has been smooth to avoid value crossing
*/


int battery_percentage[102]={
3038,		//0%
3173,		//1%
3244,		//2%
3285,		//3%
3314,		//4%
3339,		//5%
3361,		//6%
3379,		//7%
3395,		//8%
3409,		//9%
3421,		//10%
3433,		//11%
3445,		//12%
3456,		//13%
3468,		//14%
3479,		//15%
3490,		//16%
3502,		//17%
3513,		//18%
3523,		//19%
3534,		//20%
3544,		//21%
3554,		//22%
3564,		//23%
3573,		//24%
3582,		//25%
3590,		//26%
3598,		//27%
3606,		//28%
3614,		//29%
3621,		//30%
3627,		//31%
3633,		//32%
3639,		//33%
3645,		//34%
3650,		//35%
3654,		//36%
3658,		//37%
3662,		//38%
3665,		//39%
3668,		//40%
3671,		//41%
3673,		//42%
3676,		//43%
3678,		//44%
3680,		//45%
3682,		//46%
3684,		//47%
3686,		//48%
3688,		//49%
3690,		//50%
3693,		//51%
3696,		//52%
3698,		//53%
3702,		//54%
3705,		//55%
3709,		//56%
3714,		//57%
3718,		//58%
3724,		//59%
3729,		//60%
3735,		//61%
3741,		//62%
3747,		//63%
3754,		//64%
3761,		//65%
3769,		//66%
3776,		//67%
3784,		//68%
3792,		//69%
3800,		//70%
3809,		//71%
3818,		//72%
3826,		//73%
3835,		//74%
3845,		//75%
3854,		//76%
3863,		//77%
3873,		//78%
3883,		//79%
3893,		//80%
3903,		//81%
3913,		//82%
3923,		//83%
3933,		//84%
3943,		//85%
3953,		//86%
3963,		//87%
3973,		//88%
3983,		//89%
3993,		//90%
4003,		//91%
4013,		//92%
4023,		//93%
4033,		//94%
4043,		//95%
4053,		//96%
4063,		//97%
4073,		//98%
4083,		//99%
4093};	//100%


