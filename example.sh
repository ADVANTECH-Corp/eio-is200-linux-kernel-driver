#!/bin/bash

################################################################################ 
#
# Demo script for Advantech Embedded Controller
# 
# Copyright (c) Advantech Co., Ltd. All Rights Reserved
#
################################################################################ 

################################################################################ 
######                      Global Value                                  ######
################################################################################
ADV_DEBUG_INFO="0"	# Please change to "0", it only use for debug info print.					
SUSIDEMO4_VERSION=23426

################################################################################ 
######                    Global Functions                                ######
################################################################################
function dbg_print
{
	if [ "$ADV_DEBUG_INFO" == "1" ]; then
		echo -e "DEBUG INFO:[ $* ]"
	fi 
}
 
################################################################################
######                     1) WDT Sub1 Menu                               ######
################################################################################
function WdtSub1Menu
{
	WDT_SYSFS=/sys/class/watchdog

	# check if wdctl exists	
	if [ ! -e "/dev/watchdog" ] ; then
		echo -e "\n No watchdog found \n"
		echo -e "Press ENTER to continue.2 \c"
		read choose_value;
		return 1
	fi

	wdctl > /dev/null 2>&1
	if [ $? -eq 0 ] ;then
		WDT_TOOLS=0
		# find the device name watchdog#
		for wdtN in $(ls -v $WDT_SYSFS)
		do
			WDT_DRV=`wdctl /dev/${wdtN} 2> /dev/null | grep  eiois200`
			if [ "$WDT_DRV" != "" ]; then
				WDT_NAME=${wdtN}
				dbg_print "[$wdtN] name=$WDT_NAME"
				break;
			fi
		done    

		if [ "$WDT_NAME" == "" ]; then
			echo "Can't find eiois200 watchdog"
			echo -e "Press ENTER to continue. \c"
		    read choose_value;
			return 1
		fi

	else
		echo "please install wdctl"
		echo -e "Press ENTER to continue. \c"
		read choose_value;
		return 1
	fi

	while [ -z "$loop" ]
	do
		sync
		clear
		Timeleft=`cat /sys/class/watchdog/$WDT_NAME/timeleft`
		echo "**********************************************"
		echo "**               SUSI4.1 demo               **"
		echo "**********************************************"
		echo ""

		if [ "$Timeleft" != "0" ]; then
			echo System reboots in $Timeleft s
		fi

		echo ""
		echo -e "0) Back to Main menu"
		echo -e "1) Select watchdog timer: [0]"
		echo -e "2) set timeout value: " `cat /sys/class/watchdog/$WDT_NAME/timeout`
		echo -e "3) Start"
		echo -e "4) Trigger"
		echo -e "5) Stop";
		echo ""
		echo -e "Enter your choice: \c" 
		read -t1 -n1 choose_value;
		echo ""	
        
		case $choose_value in
		"0")
		echo V > /dev/${WDT_NAME}
			return 1
			;;
		"1")
			echo "Only a Watch Dog timer."
			echo ""
			echo -e "Press ENTER to continue. \c"
			read -t3 choose_value;
			;;
		"2")
			read -p "Reset time (unit: second): " delay ;

			if [ "$delay" -lt "3277" ] && [ "$delay" -ge "0" ]; then    
				wdctl -s $delay /dev/$WDT_NAME
			fi                         
			;;
		"3")                                                                              
			echo -n 0 > /dev/$WDT_NAME
			echo Watchdog started
			echo -e "Press ENTER to continue. \c"
			read -t3 choose_value;
			;;
		"4")
			echo -n 0 > /dev/$WDT_NAME  
			echo Watchdog triggered
			echo -e "Press ENTER to continue. \c"
			read -t3 choose_value;
			;;
		"5")
			echo -n V > /dev/${WDT_NAME}
			echo Watchdog Stoped
			echo -e "Press ENTER to continue. \c"
			read -t3 choose_value;
			;;
		*)
			;;
		esac
	done
}

################################################################################
######                     2) HWM Sub1 Menu                               ######
################################################################################
function HwmSub1Menu
{
	HWM_SYSFS=/sys/class/hwmon

	while [ -z "$loop" ]
	do
		clear
		# find the hwmon#
		for hwmonN in $(ls -v $HWM_SYSFS)
		do
			if [ -a $HWM_SYSFS/$hwmonN/name ]; then
				HWM_NAME=`cat $HWM_SYSFS/$hwmonN/name`
				if [ "$HWM_NAME" == "eiois200_hwmon" ]; then
					dbg_print "[$hwmonN] name=$HWM_NAME"
					break;
				fi
			fi
		done
		
		echo "**********************************************"
		echo "**               SUSI4.1 demo               **"
		echo "**********************************************"
		echo ""
		echo "Hardware Monitor"
		echo ""
		echo -e "0) Back to Main menu";
		echo -e "1) Voltage";
		echo -e "2) Temperature";
		echo -e "3) Fan speed";
		echo ""
		if [ "$HWM_NAME" != "eiois200_hwmon" ]; then
			echo "Can't find eiois200 hwmon"
			echo -e "Press ENTER to continue. \c"
			read choose_value;
			return 1
		else
			echo -e "Enter your choice: \c"
			read choose_value;
		fi
                                           
		hwm_dev=$HWM_SYSFS/$hwmonN
        
		case $choose_value in
		"0")
			return 1
			;;

		"1")
			for ((i=0 ; i < 20 ; i=i+1))
			do 
				if [ -a $hwm_dev/in${i}_label ]; then        
					echo -ne `cat $hwm_dev/in${i}_label` "\t   : " 
					echo `cat $hwm_dev/in${i}_input` mV 
				fi
			done                                                                 
			
			echo -e "Press ENTER to continue. \c"
			read choose_value;
							 
			continue 
			;;                                                                                                    
            
		"2")
			for ((i=0 ; i < 20 ; i=i+1))
			do 
				if [ -a $hwm_dev/temp${i}_label ]; then
					echo -ne `cat $hwm_dev/temp${i}_label` "\t   : " 
					echo `cat $hwm_dev/temp${i}_input` millidegree Celsius
				fi
			done                                                                 
			echo -e "Press ENTER to continue. \c"
			read choose_value;
						 
			continue 
			;;
            
		"3")
			for ((i=0 ; i < 20 ; i=i+1))
			do 
				if [ -a $hwm_dev/fan${i}_label ]; then
					echo -ne `cat $hwm_dev/fan${i}_label` "\t   : " 
					echo `cat $hwm_dev/fan${i}_input` rpm
				fi
			done                                          
			echo -e "Press ENTER to continue. \c"
			read choose_value;

			continue 
			;;
            
		*)
			continue
			;;
		esac
	done
}

################################################################################
######                     3) Smart Fan Sub1 Menu                         ######
################################################################################
function SmartFanSub1Menu
{
	TZ_SYSFS=/sys/class/thermal
	tz_cnt=0
	TZ_NAME[2]=""    
	tz_idx=0
    
	for tzN in $(ls -v $TZ_SYSFS)
	do
 		if [ -a $TZ_SYSFS/$tzN/type ]; then 
			if [  `cat $TZ_SYSFS/$tzN/type` == "eiois200_fan" ]; then
				TZ_NAME[${tz_cnt}]=$tzN   
				tz_cnt=$((tz_cnt+1))    
 			fi
		fi                                
	done
    
	if [ "$tz_cnt" == "0" ]; then
		echo "Can't find SUSI4 Smart Fan"
		echo -e "Press ENTER to continue. \c"
		read choose_value;
		return 1
	fi

	tz_dev=$TZ_SYSFS/${TZ_NAME[${tz_idx}]}
	hl=$((`cat $tz_dev/trip_point_0_temp` / 1000))  
	ll=$((`cat $tz_dev/trip_point_1_temp` / 1000))
	sl=$((`cat $tz_dev/trip_point_2_temp` / 1000))
	hr=`cat $tz_dev/cdev0/max_state` 
	lr=`cat $tz_dev/cdev1/max_state`
	temp=$((`cat $tz_dev/temp` / 1000))    

	while [ -z "$loop" ]
	do
		clear
		echo "**********************************************"
		echo "**               SUSI4.1 demo               **"
		echo "**********************************************"
		echo ""
		echo "Smart Fan Controller"
		echo ""
		echo -e "0) Back to Main menu"
		echo -ne "1) Select Smart Fan: "
		
		for ((i=0 ; i < $tz_cnt ; i=i+1))
		do 
			if [ $i != 0 ]; then 
				printf "/"
			fi
			
			if [ $i == $tz_idx ]; then        
				printf "[$i]"
			else                 
				printf " $i "
			fi
		done                                                                  
        
        echo 
		echo -e " ) Temperature		:" $temp
		echo -e "3) Low Stop Limit	:" $sl
		echo -e "4) Low Limit		:" $ll
		echo -e "5) High Limit    	:" $hl
		echo -e "6) Max PWM 	  	:" $hr
		echo -e "7) Min PWM 	  	:" $lr
		echo -e "8) Get/Refresh all values"
		echo -e "9) Save and apply setting"
		echo ""
		echo -e "Enter your choice: \c"
		read choose_value;                                    
                                           
		hwm_dev=$HWM_SYSFS/$hwmonN/device/
        
		case $choose_value in
		"0")
			return 1
			;;
		"1")
			((++tz_idx))
			if [ $tz_idx == $tz_cnt ] ; then
				tz_idx=0 
			fi                            
			    
			tz_dev=$TZ_SYSFS/${TZ_NAME[${tz_idx}]}
			hl=$((`cat $tz_dev/trip_point_0_temp` / 1000))  
			ll=$((`cat $tz_dev/trip_point_1_temp` / 1000))
			sl=$((`cat $tz_dev/trip_point_2_temp` / 1000))
			hr=`cat $tz_dev/cdev0/max_state` 
			lr=`cat $tz_dev/cdev1/max_state`
			temp=$((`cat $tz_dev/temp` / 1000))    
                                                             
			continue 
			;;                                                                                                   
            
		"3")
            
			echo -e "Low Stop Limit (0 ~ 255 milli-Celsius): \c" 			
			read tv;
												      
			if [ ${tv} -le 0 ] || [ ${tv} -gt 255 ] ; then  
				echo -e "Invalid value\c"                
				read tv;
			else
				sl=${tv}              
			fi
		     
			continue 
			;;            
		"4")
			echo -e "Low Limit (0 ~ 255 milli-Celsius): \c"			
			read tv;
			    
			if [ "$((${tv} + 0))" != "${tv}" ] || [ ${tv} < 0 ] || [${tv} > 255 ] ; then
				echo -e "Invalid value\c"               
				read tv;
			else 
				ll=${tv} 
			fi
            
			continue 
			;;            
		"5") 
			echo -e "High Stop Limit (0 ~ 255 milli-Celsius): \c"			
			read tv;
                                                                                
			if [ "$((${tv} + 0))" != "${tv}" ] || [ ${tv} < 0 ] || [${tv} > 255 ] ; then
				echo -e "Invalid value\c"                
				read tv;
			else
				hl=${tv} 
			fi
            
			continue 
			;;
		"6")
			echo -e "Max RPM (0 ~ 100 %) : \c"
			read tv;
			if [ "$((${tv} + 0))" != "${tv}" ] || [ ${tv} < 0 ] || [${tv} > 100 ] ; then
				echo -e "Invalid value\c"
				read tv;
			else
				hr=${tv}
			fi
			;;
		"7")
			echo -e "Min RPM (0 ~ 100 %) : \c"
			read tv;
			if [ "$((${tv} + 0))" != "${tv}" ] || [ ${tv} < 0 ] || [${tv} > 100 ] ; then
				echo -e "Invalid value\c"
				read tv;
			else
				lr=${tv} 
			fi
			;;			
		"8")
			tz_dev=$TZ_SYSFS/${TZ_NAME[${tz_idx}]}
			hl=$((`cat $tz_dev/trip_point_0_temp` / 1000))  
			ll=$((`cat $tz_dev/trip_point_1_temp` / 1000))
			sl=$((`cat $tz_dev/trip_point_2_temp` / 1000))
			hr=`cat $tz_dev/cdev0/max_state` 
			lr=`cat $tz_dev/cdev1/max_state`
			temp=$((`cat $tz_dev/temp` / 1000))    

			echo -e "Press ENTER to continue. \c"
			read choose_value;
			;;            
		"9")
		    echo ${hl}000 > $tz_dev/trip_point_0_temp
		    echo ${ll}000 > $tz_dev/trip_point_1_temp
		    echo ${sl}000 > $tz_dev/trip_point_2_temp
		    echo ${hr} > $tz_dev/cdev0/set_max_state
		    echo ${lr} > $tz_dev/cdev1/set_max_state
							    
				echo -e "Press ENTER to continue. \c"
		    read choose_value;
			;;
		*)
			continue
			;;
		esac
	done
}


################################################################################
######                     4) GPIO Sub1 Menu                              ######
################################################################################
function GpioSub1Menu                           
{
	GPIO_SYSFS=/sys/class/gpio
	gpio_sel=0
    
	for GpioN in $(ls -v $GPIO_SYSFS)
	do
		if [ -a $GPIO_SYSFS/$GpioN/label ]; then
			GPIO_NAME=`cat $GPIO_SYSFS/$GpioN/label`
			if [ "$GPIO_NAME" == "gpio_eiois200" ]; then
				# get rid of prefix gpiochip
				gpio_dir=$GPIO_SYSFS/${GpioN}
				ngpio=` cat $GPIO_SYSFS/${GpioN}/ngpio ` 
				GpioN=$(echo $GpioN | sed s/[a-z]//g)                                
				dbg_print "[$GpioN] pin amount=$ngpio"
				break;
			fi
		fi
	done                            
    
        
	while [ -z "$loop" ] 
	do
        gpio_num=$((${GpioN}+${gpio_sel}))
        gpio_path=${GPIO_SYSFS}/gpio${gpio_num}
        echo ${gpio_num} > ${GPIO_SYSFS}/export  
	
        gpio_dir=`cat ${gpio_path}/direction`
        gpio_val=`cat ${gpio_path}/value`
        echo ${gpio_num} > ${GPIO_SYSFS}/unexport
          
        clear
        echo "**********************************************"
		echo "**               SUSI4.1 demo               **"
		echo "**********************************************"
		echo ""
		echo "GPIO"
		echo ""
		echo -e "0) Back to Main menu";
		printf  "1) GPIO Pin	: %d (GPIO%d)\n" $gpio_sel $(($GpioN + $gpio_sel))
		echo -e "2) Type         : [Single Pin]";
		echo -e "3) Direction	: $gpio_dir";
		echo -e "4) Level    	: $gpio_val";
		echo -e "5) Get/Refresh all values"; 
		echo ""
		
		if [ "$GPIO_NAME" != "gpio_eiois200" ]; then
			echo "Can't find eiois200 GPIO"
			echo -e "Press ENTER to continue. \c"                            
			read choose_value;
			return 1
		else
            
			echo -e "Select the item you want to set: \c"
			read choose_value;
		fi

		case $choose_value in
		"0")
			return 1
			;;
		"1") 
			printf "Pin Number (0 ~ %d):" $(($ngpio-1))
			read gpio_sel_t

			if [ $gpio_sel_t -lt $ngpio ] && [ $gpio_sel_t -ge 0 ] ; then 
				gpio_sel=$(($gpio_sel_t)) 
			fi
			
			continue 
			;; 
		"3")
			echo Set Direction:
			read -p "Set Direction (in or out): " gpio_dir 
			echo $gpio_num > ${GPIO_SYSFS}/export  
			echo $gpio_dir > ${gpio_path}/direction   
			echo $gpio_num > ${GPIO_SYSFS}/unexport  
			;; 
		"4")
			if [ $gpio_dir == out ] ; then
				echo Set Direction:
				read -p "Set Value (0 or 1): " gpio_val 
				echo $gpio_num > ${GPIO_SYSFS}/export  
				echo $gpio_val > ${gpio_path}/value     
				echo $gpio_num > ${GPIO_SYSFS}/unexport   
			else 
				read -p "Input cannot set output value " choose_value  
			fi

			continue
			;;
		*)
			continue
			;;
		esac
	done
}

################################################################################
######                     5) VGA Sub1 Menu                               ######
################################################################################
function VgaSub1Menu
{
	VGA_SYSFS=/sys/class/backlight
	VGA_MOD=/sys/module/eiois200_bl/parameters
	
	# find the Vga#
	for VgaN in $(ls -v $VGA_SYSFS | grep eiois200_bl)  
	do
		VGA_NAME=$VgaN
        break ;
	done 

	if [ "$VGA_NAME" == "" ]; then
		echo "Can't find eiois200 VGA"
		echo -e "Press ENTER to continue. \c"
		read choose_value;
		return 1
	fi    
    
	while [ -z "$loop" ]
	do
		clear
        
		bl_path=${VGA_SYSFS}/${VGA_NAME}
		bl_bri=`cat ${bl_path}/brightness`
		bl_bri_invert=`cat ${VGA_MOD}/bri_invert`
		bl_freq=`cat ${VGA_MOD}/bri_freq`
		bl_onoff=`cat ${bl_path}/bl_power`
		bl_onoff_invert=`cat ${VGA_MOD}/bl_power_invert`
		bl_max=`cat ${bl_path}/max_brightness`  

		echo "**********************************************"
		echo "**               SUSI4.1 demo               **"
		echo "**********************************************"
		echo "" 
		echo "VGA: Backlight 1"
		echo ""
		echo -e "0) Back to Main menu";
		echo -e "1) Select Flat Panel   : [0]";
		printf  "2) Brightness value    : %d (0 to %d)\n" $bl_max $bl_bri
		echo -e "3) Brightness freqency : $bl_freq Hz"
		
		if [ $bl_bri_invert == 1 ] ; then 
			  echo -e "4) Brightness invert   : [On]/ Off ";
		else
			  echo -e "4) Brightness invert   :  On /[Off]";
		fi
		
		if [ $bl_onoff == 1 ] ; then 
			  echo -e "5) Backlight           : [On]/ Off ";
		else
			  echo -e "5) Backlight           :  On /[Off]";
		fi

		if [ $bl_onoff_invert == 1 ] ; then 
			  echo -e "6) Backlight invert    : [On]/ Off ";
		else
			  echo -e "6) Backlight invert    :  On /[Off]";
		fi
		
		echo ""
		echo -e "Enter your choice: \c"
		read choose_value;

		case $choose_value in
		"0")
			return 1
			;;
		"1")
			echo "Only a panel."
			echo ""
			echo -e "Press ENTER to continue. \c"
			read choose_value;
			continue
			;;
		"2")
			printf "Input PWM value between 0 to %d: " $bl_max
			read ret

			echo $ret > ${bl_path}/brightness    
			continue 
			;;
		"3")
			printf "Input PWM freqeuency (kHz): "
			read ret

			sudo rmmod eiois200_bl
			sudo modprobe eiois200_bl bri_freq=${ret}
			continue 
			;;
		"4")		
			sudo rmmod eiois200_bl

			if [ $bl_bri_invert == 1 ] ; then 
				sudo modprobe eiois200_bl bri_invert=0
			else
				sudo modprobe eiois200_bl bri_invert=1
			fi

			continue 
			;;
		"5")
			if [ $bl_onoff == 1 ] ; then
				echo 0 > ${bl_path}/bl_power 
			else
				echo 1 > ${bl_path}/bl_power 
			fi
			
			continue
			;;			
		"6")	
			sudo rmmod eiois200_bl

			if [ $bl_onoff_invert == 1 ] ; then 
				sudo modprobe eiois200_bl bl_power_invert=0
			else
				sudo modprobe eiois200_bl bl_power_invert=1
			fi

			continue 
			;;
		*)
			continue
			;;
		esac
	done
}

################################################################################
######                     6-2) SMBus Sub2 Probe:                         ######
################################################################################
function SMBusSub2Probe
{
		echo "Probe:"
		echo "Slave address of existed devices (Hex):"
		echo ""

		i2cdetect -y ${SMBus[$SMBUS_current]}| sed '1 d' | cut -c5-51 | tr '\n' ' '| sed 's/-- //g' | sed 's/  //g'

		echo ""
		echo ""
		echo "These are 7 bits addresses. These addresses may needs shift one bit."
		echo -e "Press ENTER to continue. \c" 
		read probe_value;
}
################################################################################
######                     6-3) SMBus Sub2 Read:                          ######
################################################################################
function SMBusSub2Read
{
	SMBUS_read_adr=0
	SMBUS_read_cmd=0
	SMBUS_read_len=1

	while [ -z "$loop" ]
		do
		clear
		echo "**********************************************"
		echo "**               SUSI4.1 demo               **"
		echo "**********************************************"
		echo ""
		echo "$SMBUS_DEV_NAME"
		echo ""
		echo "Read Data:"
		echo ""
		echo -e "0) Back to SMBus menu"
		printf  "1) Slave Address : %d (0x%X)\n" $SMBUS_read_adr $SMBUS_read_adr
		printf  "2) Command : %d (0x%X)\n" $SMBUS_read_cmd $SMBUS_read_cmd
		printf  "3) Read data length : %d (0x%X)\n" $SMBUS_read_len $SMBUS_read_len
		echo -e "4) Run"
		echo ""
		echo -e "Enter your choice: \c"
		read smbus_read_value;

		case $smbus_read_value in
		"0")
			return 1
			;;
		"1")
			echo -e "Slave Address: \c"
			read read_value;
			if [ -n "$read_value" ]; then
				SMBUS_read_adr=$read_value
			fi
			continue
			;;
		"2")
			echo -e "Command: \c"
			read read_value;
			if [ -n "$read_value" ]; then
				SMBUS_read_cmd=$read_value
			fi
			
			continue
			;;

		"3")
			echo -e "Length: \c"
			read read_value;
			if [ -n "$read_value" ]; then
				SMBUS_read_len=$read_value
			fi
			if [ $SMBUS_read_len -gt 32 ]; then
				SMBUS_read_len=32
			fi
			if [ $SMBUS_read_len -lt 0 ]; then
				SMBUS_read_len=0
			fi
			continue
			;;

		"4")
			dbg_print "SMBUS_read_adr=$SMBUS_read_adr"
			dbg_print "I2C-${SMBus[$SMBUS_current]} Addr=$SMBUS_read_adr"
			dbg_print "SMBUS_read_cmd = [$SMBUS_read_cmd]"
			dbg_print "SMBUS_read_len = [$SMBUS_read_len]"
			echo ""
			echo -e "Data (Hex): \c"
			i2ctransfer -y ${SMBus[$SMBUS_current]} w1@$SMBUS_read_adr $SMBUS_read_cmd r$SMBUS_read_len
			
			echo ""
			echo ""
			echo "Press ENTER to continue."
			read smbus_read_value;
			continue
			;;
		*)
			continue
			;;
		esac
	done
}

################################################################################
######                     6-4) SMBus Sub2 Write:                           ######
################################################################################
function SMBusSub2Write
{
	SMBUS_write_adr=0
	SMBUS_write_cmd=0
	SMBUS_write_value=""
	while [ -z "$loop" ]
		do
		clear
		echo "**********************************************"
		echo "**               SUSI4.1 demo               **"
		echo "**********************************************"
		echo ""
		echo "$SMBUS_DEV_NAME"
		echo ""
		echo "Write Data:"
		echo ""
		echo -e "0) Back to SMBus menu"
		#echo -e "1) Protocol: Byte Data"
		printf "1) Slave Address : %d (0x%X) (7-bit)\n" $SMBUS_write_adr $SMBUS_write_adr
		printf "2) Command : %d (0x%X) (Byte-type)\n" $SMBUS_write_cmd $SMBUS_write_cmd
		printf "3) Length: %d (0x%X)\n" $SMBUS_write_len $SMBUS_write_len
		echo "4) Data (Hex only): $SMBUS_write_value"
		echo -e "5) Run"
		echo ""
		echo -e "Enter your choice: \c"
		read smbus_write;

		case $smbus_write in
		"0")
			return 1
			;;
		"1")
			echo -e "Slave Address: \c"
			read write_value;
			if [ -n "$write_value" ]; then
				SMBUS_write_adr=$write_value
			fi
			continue
			;;
		"2")
			echo -e "Command: \c"
			read write_value;
			if [ -n "$write_value" ]; then
				SMBUS_write_cmd=$write_value
			fi
			if [ $SMBUS_write_cmd -gt 256 ]; then
				SMBUS_write_cmd=256
			fi
			if [ $SMBUS_write_cmd -lt 0 ]; then
				SMBUS_write_cmd=0
			fi
			continue
			;;
		"3")
			echo -e "Input length: \c"
			read write_value;
			if [ -n "$write_value" ]; then
				SMBUS_write_len=$write_value
			fi
			continue
			;;
		"4")
			echo -e "Input a series of HEX value (like 0x01 0x02 0x03 ...) : \c"
			read write_value;
			if [ -n "$write_value" ]; then
				SMBUS_write_value=$write_value
			fi
			continue
			;;
		"5")
			dbg_print "SMBUS_write_adr=$SMBUS_write_adr"
			dbg_print "Command=$SMBUS_write_cmd"
			dbg_print "I2C-${SMBus[$SMBUS_current]} Value=$SMBUS_write_value"
			
			i2ctransfer -y ${SMBus[$SMBUS_current]} \
				w$((SMBUS_write_len+1))@$SMBUS_write_adr $SMBUS_write_cmd $SMBUS_write_value-
				
			if [ $? -eq 0 ]; then
				echo "Write transfer succeed."
			else
				echo "Write transfer failed."
			fi
			echo -e "Press ENTER to continue. \c"
			read smbus_write;
			;;
		*)
			continue
			;;
		esac
	done
}

################################################################################
######                     6) SMBus Sub1 Menu                              ######
################################################################################
function SMBusSub1Menu
{
	SMBUS_SYSFS=/sys/bus/i2c/devices
	SMBUS_tatal=0
	SMBUS_current=0
	# find the SMBus# and save to array
	for SMBusN in $(ls -v $SMBUS_SYSFS)
	do
		if [ -d $SMBUS_SYSFS/$SMBusN ]; then
			SMBUS_NAME=$(cat $SMBUS_SYSFS/$SMBusN/name | grep "eiois200")
			if [ "$SMBUS_NAME" != "" ]; then
				dbg_print "[$SMBusN] name=$SMBUS_NAME"
				SMBus[SMBUS_tatal++]=$(echo $SMBusN | sed 's/i2c-//g')
			fi
		fi
	done

	while [ -z "$loop" ]
	do
		clear
		i2cdetect -l > /dev/null 2>&1
		if [ $? -eq 0 ] ;then
			I2C_TOOLS=0
		else
			I2C_TOOLS=1
		fi
		dbg_print "SMBUS_current=$SMBUS_current"
		dbg_print "SMBUS_tatal=$SMBUS_tatal"
		dbg_print "SMBus array= ${SMBus[@]}"
		SMBUS_DEV_NAME=$(cat $SMBUS_SYSFS/i2c-${SMBus[$SMBUS_current]}/name | sed 's/eiois200 //g')

		echo "**********************************************"
		echo "**               SUSI4.1 demo               **"
		echo "**********************************************"
		echo ""
		echo "$SMBUS_DEV_NAME"
		echo ""
		echo -e "0) Back to Main menu"
		if [ $SMBUS_tatal -eq 2 ]; then
			case $SMBUS_current in
			1) echo -e "1) Select SMBus host:  0 /[1]";;
			*) echo -e "1) Select SMBus host: [0]/ 1 ";;
			esac
		elif [ $SMBUS_tatal -eq 3 ]; then
			case $SMBUS_current in
			1) echo -e "1) Select SMBus host:  0 /[1]/ 2";;
			2) echo -e "1) Select SMBus host:  0 / 1 /[2]";;
			*) echo -e "1) Select SMBus host: [0]/ 1 / 2";;
			esac
		else
			echo -e "1) Select SMBus host: [0]"
		fi
		echo -e "2) Probe"
		echo -e "3) Read"
		echo -e "4) Write"
		echo ""
		if [ $SMBUS_tatal -eq 0 ]; then
			echo "Can't find eiois200 SMBus"
			echo -e "Press ENTER to continue. \c"
			read choose_value;
			return 1
		elif [ $I2C_TOOLS -eq 1 ]; then
			echo "please install i2c-tools"
			echo -e "Press ENTER to continue. \c"
			read choose_value;
			return 1
		else
			echo -e "Enter your choice: \c"
			read choose_value;
		fi

		case $choose_value in
		"0")
			return 1
			;;
		"1")
			let "SMBUS_current += 1"
			dbg_print "SMBUS_current=$SMBUS_current"
			if [ $SMBUS_current -eq $SMBUS_tatal ]; then
				SMBUS_current=0
			fi
			continue
			;;
		"2")
			SMBusSub2Probe
			continue
			;;
		"3")
			SMBusSub2Read
			continue
			;;
		"4")
			SMBusSub2Write
			continue
			;;
		*)
			continue
			;;
		esac
	done
}

################################################################################
######                     7-2) I2C Sub2 Probe:                           ######
################################################################################
function I2CSub2Probe
{
	while [ -z "$loop" ]
		do
		clear
		echo "**********************************************"
		echo "**               SUSI4.1 demo               **"
		echo "**********************************************"
		echo ""
		echo "$I2C_DEV_NAME"
		echo ""
		echo -e "0) Back to I2C menu"
		echo -e "1) Probe 7-bit address"
		echo ""
		echo -e "Enter your choice: \c"
		read probe_value;

		case $probe_value in
		"0")
			return 1
			;;
		"1")
			dbg_print "I2C_current=$I2C_current"
			i2cdetect -y ${I2C[$I2C_current]} | sed '1 d' | cut -c5-51 | tr '\n' ' '| sed 's/-- //g' | sed 's/  //g'

			echo ""
			echo ""
			echo "These are 7 bits addresses. These addresses may needs shift one bit." 
			echo -e "Press ENTER to continue. \c"
			read probe_value;
			;;
		*)
			continue
			;;
		esac
	done
}
################################################################################
######                     7-3) I2C Sub2 Read:                            ######
################################################################################
function I2CSub2Read
{
	I2C_read_adr=0
	I2C_read_cmd=0
	I2C_read_len=1
	
	while [ -z "$loop" ]
		do
		clear
		
		echo "**********************************************"
		echo "**               SUSI4.1 demo               **"
		echo "**********************************************"
		echo ""
		echo "$I2C_DEV_NAME"
		echo ""
		echo "Read Data:"
		echo ""
		echo -e "0) Back to I2C menu"
		printf "1) Slave Address : %d (0x%X) ( (7-bit)\n" $I2C_read_adr $I2C_read_adr
		printf "2) Command : %d (0x%X) (Byte-type)\n" $I2C_read_cmd $I2C_read_cmd
		printf "3) Read Data Length : %d (0x%X)\n" $I2C_read_len $I2C_read_len
		echo -e "4) Run"
		echo ""
		echo -e "Enter your choice: \c"
		
		read i2c_read_value;

		case $i2c_read_value in
		"0")
			return 1
			;;
		"1")
			echo -e "7-bit Slave Address : \c"
			read read_value;
			if [ -n "$read_value" ]; then
				I2C_read_adr=$read_value
			fi
			continue
			;;
		"2")
			echo -e "Byte Command : \c"
			read read_value;
			if [ -n "$read_value" ]; then
				I2C_read_cmd=$read_value
			fi
			continue
			;;
		"3")
			echo -e "Read Length (1 to 32):\c"
			read read_value;
			if [ -n "$read_value" ]; then
				I2C_read_len=$read_value
			fi
			if [ $I2C_read_len -gt 32 ]; then
				I2C_read_len=32
			fi
			if [ $I2C_read_len -lt 0 ]; then
				I2C_read_len=0
			fi
			continue
			;;
		"4")
			dbg_print "I2C_read_adr=$I2C_read_adr"
			dbg_print "first-last=$I2C_read_cmd-$(($I2C_read_cmd+$I2C_read_len))"
			dbg_print "I2C-${I2C[$I2C_current]} Addr=$I2C_read_adr"

			echo -n "Data (Hex): "
			i2ctransfer -y ${I2C[$I2C_current]} \
				w1@$I2C_read_adr $I2C_read_cmd r$I2C_read_len
			#i2cdump -f -y -r $I2C_read_cmd-$(($I2C_read_cmd+$I2C_read_len-1)) ${I2C[$I2C_current]} 0x$I2C_read_adr b | sed '1 d' | cut -c5-51 | tr '\n' ' ' | tr '\t' ' ' | sed 's/^[  ]*//g'
                                                                                     
			echo -e "\n\nPress ENTER to continue. \c"
			read read_value;
            
			continue
			;;
		*)
			continue
			;;
		esac
	done
}

################################################################################
######                     7-4) I2C Sub2 Write:                           ######
################################################################################
function I2CSub2Write
{
	I2C_write_adr=0
	I2C_write_cmd=0
	I2C_write_len=0
	I2C_write_value=""
	while [ -z "$loop" ]
		do
		clear
		echo "**********************************************"
		echo "**               SUSI4.1 demo               **"
		echo "**********************************************"
		echo ""
		echo "$I2C_DEV_NAME"
		echo ""
		echo "Write Data:"
		echo ""
		echo -e "0) Back to I2C menu"
		printf "1) Slave Address: %d (0x%X) (7-bit)\n" $I2C_write_adr $I2C_write_adr
		printf "2) Command: %d (0x%X)\n" $I2C_write_cmd $I2C_write_cmd
		printf "3) Length: %d (0x%X)\n" $I2C_write_len $I2C_write_len
		echo -e "4) Write Data (Hex): $I2C_write_value"
		echo -e "5) Run"
		echo ""
		echo -e "Enter your choice: \c"
		read i2c_write;

		case $i2c_write in
		"0")
			return 1
			;;
		"1")
			echo -e "7-bit Slave Address : \c"
			read write_value;
			if [ -n "$write_value" ]; then
				I2C_write_adr=$write_value
			fi
			continue
			;;
		"2")
			echo -e "Byte Command : \c"
			read write_value;
			if [ -n "$write_value" ]; then
				I2C_write_cmd=$write_value
			fi
			continue
			;;
		"3")
			echo -e "Length: \c"
			read write_value;
			if [ -n "$write_value" ]; then
				I2C_write_len=$write_value
			fi
			continue
			;;
		"4")
			echo -e "Input a series of HEX value (Like 0x01 0x02 0x03 ...): \c"
			read write_value;
			if [ -n "$write_value" ]; then
				I2C_write_value=$write_value
			fi
			continue
			;;
		"5")
			dbg_print "I2C_write_adr=$I2C_write_adr"
			dbg_print "Command=$I2C_write_cmd"
			dbg_print "I2C-${I2C[$I2C_current]} Value=$I2C_write_value"
			#i2cset -f -y ${I2C[$I2C_current]} 0x$I2C_write_adr $I2C_write_cmd 0x$I2C_write_value
			i2ctransfer -y ${I2C[$I2C_current]} w$((I2C_write_len+1))@$I2C_write_adr $I2C_write_cmd $I2C_write_value-

			if [ $? -eq 0 ]; then
				echo "Write transfer succeed."
			else
				echo "Write transfer failed."
			fi
			echo -e "Press ENTER to continue. \c"
			read i2c_write;
			;;
		*)
			continue
			;;
		esac
	done
}

################################################################################
######                     7) I2C Sub1 Menu                               ######
################################################################################
function I2CSub1Menu
{
	I2C_SYSFS=/sys/bus/i2c/devices
	I2C_PARAM=/sys/module/i2c_eiois200/parameters
	I2C_tatal=0
	I2C_current=0
	# find the I2C# and save to array
	for I2CN in $(ls -v $I2C_SYSFS)
	do
		if [ -d $I2C_SYSFS/$I2CN ]; then
			I2C_NAME=$(cat $I2C_SYSFS/$I2CN/name | grep "eiois200")
			if [ "$I2C_NAME" != "" ]; then
				dbg_print "[$I2CN] name=$I2C_NAME"
				I2C[I2C_tatal++]=$(echo $I2CN | sed 's/i2c-//g')
			fi
		fi
	done

	while [ -z "$loop" ]
	do
		clear
		
		i2cdetect -l > /dev/null 2>&1

		if [ $? -eq 0 ] ;then
			I2C_TOOLS=0
		else
			I2C_TOOLS=1
		fi
		dbg_print "I2C_current=$I2C_current"
		dbg_print "I2C_tatal=$I2C_tatal"
		dbg_print "I2C array= ${I2C[@]}"
		I2C_DEV_NAME=$(cat $I2C_SYSFS/i2c-${I2C[$I2C_current]}/name | sed 's/eiois200-//g')
		I2C_freq=`cat ${I2C_PARAM}/${I2C_DEV_NAME: -4}_freq`

		echo "**********************************************"
		echo "**               SUSI4.1 demo               **"
		echo "**********************************************"
		echo ""
		echo "$I2C_DEV_NAME"
		echo ""
		echo -e "0) Back to Main menu"
		if [ $I2C_tatal -eq 2 ]; then
			case $I2C_current in
			1) echo -e "1) Select I2C host:  0 /[1]";;
			*) echo -e "1) Select I2C host: [0]/ 1";;
			esac
		elif [ $I2C_tatal -eq 3 ]; then
			case $I2C_current in
			1) echo -e "1) Select I2C host:  0 /[1]/ 2";;
			2) echo -e "1) Select I2C host:  0 / 1 /[2]";;
			*) echo -e "1) Select I2C host: [0]/ 1 / 2";;
			esac
		else
			echo -e "1) Select I2C host: [0]"
		fi
		echo -e "2) Probe"
		echo -e "3) Read"
		echo -e "4) Write"
#		echo -e "5) WR Combined"
		echo -e "6) Frequency : ${I2C_freq}kHz"

		echo ""
		if [ $I2C_tatal -eq 0 ]; then
			echo "Can't find SUSI I2C"
			echo -e "Press ENTER to continue. \c"
			read choose_value;
			return 1
		elif [ $I2C_TOOLS -eq 1 ]; then
			echo "please install i2c-tools"
			echo -e "Press ENTER to continue. \c"
			read choose_value;
			return 1
		else
			echo -e "Enter your choice: \c"
			read choose_value;
		fi

		case $choose_value in
		"0")
			return 1
			;;
		"1")
			let "I2C_current += 1"
			dbg_print "I2C_current=$I2C_current"
			if [ $I2C_current -eq $I2C_tatal ]; then
				I2C_current=0
			fi
			continue
			;;
		"2")
			I2CSub2Probe
			continue
			;;
		"3")
			I2CSub2Read
			continue
			;;
		"4")
			I2CSub2Write
			continue
			;;
		"5")
			continue
			;;
		"6")
			echo -e "I2C clock Freqency(kHz) : \c"
			read write_value;
			if [ -n "$write_value" ]; then
				rmmod i2c_eiois200
				modprobe i2c_eiois200 ${I2C_DEV_NAME}_freq=${write_value}
			fi		

			continue
			;;
		*)
			continue
			;;
		esac
	done
}

################################################################################
######                     x) Storage Sub1 Menu                           ######
################################################################################
function StorageSub1Menu
{
	while [ -z "$loop" ]
	do
		clear
		echo "**********************************************"
		echo "**               SUSI4.1 demo               **"
		echo "**********************************************"
		echo ""
		echo "Storage 1"
		echo ""
		echo -e "0) Back to Main menu"
		echo -e "1) Select storage device"
		echo -e "2) Read date"
		echo -e "3) Write date"
		echo -e "4) Get/Refresh write protection status: Unlocked"
		echo -e "5) Write protection lock"
		echo ""
		echo -e "Select the item you want to do: \c"
		read choose_value;

		case $choose_value in
		"0")
			return 1
			;;
		"1")
			continue
			;;
		"2")
			continue
			;;
		"3")
			continue
			;;
		"4")
			continue
			;;
		"5")
			continue
			;;
		*)
			continue
			;;
		esac
	done
}

################################################################################
######                     x) Thermal Sub1 Menu                           ######
################################################################################
function ThermalSub1Menu
{
	while [ -z "$loop" ]
	do
		clear
		echo "**********************************************"
		echo "**               SUSI4.1 demo               **"
		echo "**********************************************"
		echo ""
		echo "Thermal Protection: Zone 1"
		echo ""
		echo -e "0) Back to Main menu"
		echo -e "1) Select thermal protection zone"
		echo -e "2) Thermal Source    : Temperature CPU"
		echo -e "3) Event Type        :  None /[Shutdown]/ Throttle / Power OFF"
		echo -e "4) Trigger Temperature: 226 Celsius"
		echo -e "5) Clear Temperature : 60 Celsius"
		echo -e "6) Get/Refresh all values"
		echo -e "7) Save and apply setting"
		echo ""
		echo -e "Select the item you want to set: \c"
		read choose_value;

		case $choose_value in
		"0")
			return 1
			;;
		"1")
			continue
			;;
		"2")
			continue
			;;
		"3")
			continue
			;;
		"4")
			continue
			;;
		"5")
			continue
			;;
		"6")
			continue
			;;
		"7")
			continue
			;;
		*)
			continue
			;;
		esac
	done
}

################################################################################
######                     8) Information Sub1 Menu                       ######
################################################################################
function InfoSub1Menu
{
	UP_TIME=$(uptime | cut -d "p" -f 2 | cut -d "," -f 1)
	DIR=/sys/bus/isa/devices/eiois200_core.0
	while [ -z "$loop" ]
	do
		clear
		echo "**********************************************"
		echo "**               SUSI4.1 demo               **"
		echo "**********************************************"
		echo ""
		echo "Information"
		echo ""
		echo -e "Running time		:$UP_TIME"
		echo -e "Board ID		:" `cat $DIR/board_id`	
		echo -e "Board Manufacturer	:" `cat $DIR/board_manufacturer`	
		echo -e "Board Name		:" `cat $DIR/board_name`	
		echo -e "Board Serial		:" `cat $DIR/board_serial`
		echo -e "Boot Count		:" $((16#`cat $DIR/boot_count`))
		echo -e "Chip Detect		:" `cat $DIR/chip_detect`
		echo -e "Chip ID			:" `cat $DIR/chip_id`
		echo -e "EAPI ID			:" `cat $DIR/eapi_id`
		echo -e "EAPI Rersion		:" `cat $DIR/eapi_version`
		echo -e "Firmware Build		:" `cat $DIR/firmware_build`
		echo -e "Firmware Date		:" `cat $DIR/firmware_date`
		echo -e "Firmware Version	:" `cat $DIR/firmware_version`
		echo -e "Platform Revision	:" `cat $DIR/platform_revision`
		echo -e "Platform Type		:" `cat $DIR/platform_type`			
		echo ""
		echo -e "Press ENTER to continue. \c"
		read choose_value;

		case $choose_value in
		*)
			return 1
			;;
		esac
	done
}

################################################################################
######                     Main Menu                                      ######
################################################################################
function SusiMainMenu
{
	if [ "$USER" != "root" ]; then
		clear
		echo "please run as root"
		echo -e "Press ENTER to exit. \c"
		read choose_value;
		return 0
	fi
	DRV_VERSION=$(modinfo eiois200_core | grep -sw version: | cut -d ":" -f 2 | sed 's/^[[:space:]]*//g')
	while [ -z "$loop" ];
	do
		clear
		echo "**********************************************"
		echo "**               SUSI4.1 demo               **"
		echo "**********************************************"
		echo ""
		echo "Main (demo version : 4.1.$SUSIDEMO4_VERSION.0)"
		echo ""
		echo -e "0) Terminate this program"
		echo -e "1) Watch Dog"
		echo -e "2) HWM"
		echo -e "3) SmartFan"
		echo -e "4) GPIO"
		echo -e "5) VGA"
		echo -e "6) SMBus"
		echo -e "7) I2C"
#		echo -e "x) Storage"
#		echo -e "x) Thermal Protection"
		echo -e "8) Information"
		echo ""
		echo -e "Enter your choice: \c"
		read choose_value;

#		echo "choose_value=$choose_value"

		case $choose_value in
		"0")
			echo "Exit the program..."
			return 0
			;;
		"1")
			WdtSub1Menu
			if [ "$?" -eq "1" ]; then
				continue;
			fi
			break
			;;
		"2")
			HwmSub1Menu
			if [ "$?" -eq "1" ]; then
				continue; 
			fi
			break
			;;
		"3")
			SmartFanSub1Menu
			if [ "$?" -eq "1" ]; then
				continue; 
			fi
			break
			;;
		"4")
			GpioSub1Menu
			if [ "$?" -eq "1" ]; then
				continue;
			fi
			break
			;;
		"5")
			VgaSub1Menu
			if [ "$?" -eq "1" ]; then
				continue;
			fi
			break
			;;
		"6")
			SMBusSub1Menu
			if [ "$?" -eq "1" ]; then
				continue;
			fi
			break
			;;
		"7")
			I2CSub1Menu
			if [ "$?" -eq "1" ]; then
				continue;
			fi
			break
			;;
#		"x")
#			StorageSub1Menu
#			if [ "$?" -eq "1" ]; then
#				continue; 
#			fi
#			break
#			;;
#		"x")
#			ThermalSub1Menu
#			if [ "$?" -eq "1" ]; then
#				continue;
#			fi
#			break
#			;;
		"8")
			InfoSub1Menu
			if [ "$?" -eq "1" ]; then
				continue;
			fi
			break
			;;
		*)
			echo -e "Unknown target platform,return menu...\n"
			continue
			;;
		esac
	done
}

################################################################################
######                     Main Script                                    ######
################################################################################

if [ $# == 0 ]; then
	SusiMainMenu
fi
