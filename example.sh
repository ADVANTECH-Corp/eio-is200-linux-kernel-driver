#!/bin/bash
# SPDX-License-Identifier: GPL-2.0-only
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

################################################################################
######                    Global Functions                                ######
################################################################################
function show_title
{
	clear
	echo "**********************************************"
	echo "**            EIOIS200 Example              **"
	echo "**********************************************"
	echo ""
}

# ternary simulates the ternary operator likes:
# C++ :   A = B == C ? D : E ;
# to be: $A = `ternary "[ $B == $C ]" ?  "$D" :  "$E"`
#                      |<--- $1 --->| $2  $3  $4  $5
function ternary {

	if eval "$1" ; then
		echo -e "$3"
	else
		echo -e "$5"
	fi
}

################################################################################
######                     1) WDT Sub1 Menu                               ######
################################################################################
function WdtSub1Menu
{
	local SYSFS=/sys/class/watchdog
	local wdt
	local sel
	local i
	local delay

	# Need wdctl
	wdctl > /dev/null 2>&1
	if [ $? -ne 0 ]; then
		return `echo -p "please install wdctl"`
	fi

	# find the device name of watchdog#
	for i in $(ls -v $SYSFS); do
		local dev=`wdctl /dev/${i} 2> /dev/null | grep eiois200`
		if [ "$dev" != "" ]; then
			wdt=${i}
		fi
	done

	if [ "$wdt" == "" ]; then
		return `read -p"Can't find eiois200 watchdog"`
	fi

	while :
	do
		local Timeleft=`cat /sys/class/watchdog/$wdt/timeleft`

		# Show Menu
		sync
		show_title

		echo "Watchdog"
		if [ "$Timeleft" != "0" ]; then
			echo -e "\033[41m\033[30m\033[05mSystem reboots in ${Timeleft}(s)\033[0m"
		else
			echo -e
		fi

		echo -e "0) Back to Main menu(Stop watchdog)"
		echo -e "1) set timeout value:" `cat /sys/class/watchdog/$wdt/timeout`
		echo -e "2) Start / Trigger"
		echo -e "3) Stop\n"
		read -t1 -n1 -p "Enter your choice: " sel
		echo -e

		# Processes
		case $sel in
		"0")
			echo V > /dev/${wdt}
			return 1
			;;
		"1")
			read -p"Reset time (unit: second): " delay
			wdctl -s $delay /dev/$wdt
			;;
		"2")
			echo -n 0 > /dev/$wdt
			read -p"Watchdog started..." -t1 -n1
			;;
		"3")
			echo -n V > /dev/${wdt}
			read -p"Watchdog stopped..." -t1 -n1
			;;
		*)
			continue
			;;
		esac
	done
}

################################################################################
######                     2) HWM Sub1 Menu                               ######
################################################################################
function HwmSub1Menu
{
	local SYSFS=/sys/class/hwmon
	local dev
	local sel
	local file
	local hwm_dev=""
	local label

	# find the hwmon#
	for dev in $(ls -v $SYSFS)
	do
		if [ -e $SYSFS/$dev/name ] &&
		   [ `cat $SYSFS/$dev/name` == "eiois200_hwmon" ]; then
			hwm_dev=$SYSFS/$dev
			break
		fi
	done

	if [ $hwm_dev == ""]; then
		return `read -p"Can't find eiois200 hwmon device..."`
	fi

	show_title

	while :
	do
		# Show all hwmon items
		echo -e "\033[4;0H"
		echo -e "Hardware Monitor"

		echo -e "\nVoltage(mV): "
		for file in $hwm_dev/in*input; do
			if [ -f "$file" ]; then
				label=`cat ${file%input}label`
				val=`cat $file`
				printf "\t%s\t: %d.%02d  \n" $label $((val/1000)) $(((val/10) % 100))
			fi
		done

		echo -e "\nTemperature(degree Celsius): "
		for file in $hwm_dev/temp*input; do
			if [ -f "$file" ]; then
				label=`cat ${file%input}label`
				val=`cat $file`
				printf "\t%s\t: %d.%d  \n" $label $((val/1000)) $(((val%1000)/100))
			fi
		done

		echo -e "\nFan speed(rpm): "
		for file in $hwm_dev/fan*input; do
			if [ -f "$file" ]; then
				echo -e "\t`cat ${file%input}label`\t: `cat $file`  "
			fi
		done

		echo -e
		read -t0.016 -n1 -p "Press any key to leave..."
		if [ $? -eq 0 ]; then
			return 0
		fi
	done
}

################################################################################
######                     3) Smart Fan Sub1 Menu                         ######
################################################################################
function SmartFanSub1Menu
{
	local SYSFS=/sys/class/thermal
	local names=()
	local i=0
	local dev

	# Search smart fan
	for dev in $(ls -v $SYSFS); do
 		if [ -e $SYSFS/$dev/type ] &&
		   [ `cat $SYSFS/$dev/type` == "eiois200_fan" ]; then
			names[${#names[@]}]=$dev
		fi
	done

	if [ "${#names[@]}" == "0" ]; then
		return `read -p"Can't find Smart Fan..."`
	fi

	while : ; do
		local tz_dev=$SYSFS/${names[${i}]}
		local mode=`cat $tz_dev/fan_mode`
		local temp=`cat $tz_dev/temp`
		local dev_lst=()
		local sel
		local val

		# Show menu
		for ((j=0 ; j < ${#names[@]} ; j=j+1)); do
			dev_lst+=`ternary "[ $j == $i ]" ? "[${names[j]}]" : " ${names[j]} "`
		done

		show_title

		echo -e "Smart Fan Controller"
		echo -e
		echo -e "0) Back to Main menu"
		echo -e " ) Temperature		:" $((temp/1000)).$(((temp/100)%10))
		echo -e "1) Select Smart Fan	: $dev_lst"
		echo -e "2) Toggle Fan mode	:" $mode
		echo -e "3) Manual PWM		:" `cat $tz_dev/PWM`
		echo -e "4) Low Stop Limit	:" $((`cat $tz_dev/trip_point_2_temp` / 1000))
		echo -e "5) Low Limit		:" $((`cat $tz_dev/trip_point_1_temp` / 1000))
		echo -e "6) High Limit		:" $((`cat $tz_dev/trip_point_0_temp` / 1000))
		echo -e "7) Max PWM		:" `cat $tz_dev/cdev0/max_state`
		echo -e "8) Min PWM		:" `cat $tz_dev/cdev1/max_state`
		echo -e
		read -t0.1 -n1 -p"Enter your choice: " sel
		echo -e

		# Processes
		case $sel in
		"0")
			return 0;;
		"1")
			i=$(((i + 1) % ${#names[@]}));;
		"2")
			# Switch fan operation mode
			case $mode in
			 "Stop")   echo Full   > $tz_dev/fan_mode;;
			 "Full")   echo Manual > $tz_dev/fan_mode;;
			 "Manual") echo Auto   > $tz_dev/fan_mode;;
			 "Auto")   echo Stop   > $tz_dev/fan_mode;;
			esac
			;;
		"3")
			read -p"Enter PWM (0 ~ 100 %): " val
			echo "$val" > "$tz_dev/PWM"
			;;
		"4")
			read -p"Low Stop Limit (0 ~ 255 milli-Celsius): " val
			echo ${val}000 > $tz_dev/trip_point_2_temp
			;;
		"5")
			read -p"Low Limit (0 ~ 255 milli-Celsius): " val
			echo ${val}000 > $tz_dev/trip_point_1_temp
			;;
		"6")
			read -p"High Stop Limit (0 ~ 255 milli-Celsius): " val
			echo ${val}000 > $tz_dev/trip_point_0_temp
			;;
		"7")
			read -p"Max PWM (0 ~ 100 %): " val
			echo ${val} > $tz_dev/cdev0/set_max_state
   			rmmod eiois200_fan
   			modprobe eiois200_fan
			;;
		"8")
			read -p"Min PWM (0 ~ 100 %): " val
			echo ${val} > $tz_dev/cdev1/set_max_state
   			rmmod eiois200_fan
   			modprobe eiois200_fan
			;;
		esac
	done
}

################################################################################
######                     x) Thermal Sub1 Menu                           ######
################################################################################
function ThermalSub1Menu
{
	local ACTS=("Shutdown" "Power OFF" "Throttle")
	local SYSFS=/sys/class/thermal
	local names=()
	local act=0
	local i=0
	local dev

	# Search thermal protect
	for dev in $(ls -v $SYSFS); do
 		if [ -e $SYSFS/$dev/type ] &&
		   [ $(cat "$SYSFS/$dev/type") == "eiois200_thermal" ]; then
			names[${#names[@]}]=$dev
		fi
	done

	if [ "${#names[@]}" == "0" ]; then
		return `read -p"Can't find thermal protect..."`
	fi

	while :
	do
		local fs=${SYSFS}/${names[$i]}
		local trigger_fs=${fs}/trip_point_${act}_temp
		local state_fs=${fs}/cdev${act}/enable
		local trigger=`cat $trigger_fs`
		local state=`cat $state_fs`
		local temp=`cat $fs/temp`

		# Show menu
		show_title

		echo -e "Thermal Protection"
		echo -e
		echo -e "0) Back to Main menu"
		echo -e "1) Protection zone	:" ${names[$i]}
		echo -e " ) Sensor		:" `cat ${fs}/name`
		echo -e " ) Temperature		:" $((temp / 10)).$((temp % 10))
		echo -e " ) Type			:" `cat $fs/trip_point_${act}_type`
		echo -e "2) Event Type		:" ${ACTS[$act]}
		echo -e "3) Trigger Temperature	:" $((trigger / 10)).$((trigger % 10))
		echo -e "4) State		:" $state
		echo -e
		read -t1 -n1 -p"Select the item you want to set: " sel
		echo -e

		# Processes
		case $sel in
		"0")
			return 0
			;;
		"1")
			# Switch to next device
			i=$(( (i + 1) % ${#names[@]} ))
			;;
		"2")
			# Switch to next action
			act=$(( (act + 1) % ${#ACTS[@]} ))
			;;
		"3")
			# Temp unit is 0.1 degree-Celsius
			echo -e "Trigger Temperature (0 ~ 1250 Centi-Celsius): \c"
			read trigger

			echo $trigger > $trigger_fs
			;;
		"4")
			if [ $state == enabled ]; then
				echo disabled > $state_fs
			else
				echo enabled > $state_fs
			fi
		esac
	done
}

################################################################################
######                     4) GPIO Sub1 Menu                              ######
################################################################################
function GpioSub1Menu
{
	local SYSFS=/sys/class/gpio
	local shift=0
	local base=0
	local val
	local sel
	local dir
	local state

	# Search GPIO
	for base in $(ls -v $SYSFS); do
		if [ -e $SYSFS/$base/label ] &&
		   [ `cat $SYSFS/$base/label` == "gpio_eiois200" ]; then
			# get rid of prefix gpiochip
			ngpio=` cat $SYSFS/${base}/ngpio `
			base=$(echo $base | sed s/[a-z]//g)
			break
		fi
	done

	if [[ $base -eq 0 ]]; then
		return `read -p"GPIO device not found..."`
	fi

	while : ; do
		local num=$((${base}+${shift}))
		local path=${SYSFS}/gpio${num}

		echo ${num} > ${SYSFS}/export
		dir=`cat ${path}/direction`
		state=`cat ${path}/value`
		echo ${num} > ${SYSFS}/unexport

		# Show menu
		show_title		
		printf "GPIO\n\n"
		printf "0) Back to Main menu\n"
		printf "1) GPIO Pin	: %d (GPIO%d)\n" $shift $(($base + $shift))
		printf "2) Direction	: $dir\n"
		printf "3) Level    	: $state\n\n"

		read -t1 -n1 -p "Select the item you want to set: " sel
		echo -e

		# Processes
		case $sel in
		"0")
			return 0
			;;
		"1")
			printf "Pin Number (0 ~ %d):" $(($ngpio-1))
			read val

			if [ $val -lt $ngpio ] && [ $val -ge 0 ] ; then
				shift=$(($val))
			fi
			;;
		"2")
			dir=`ternary "[ \"$dir\" == \"in\" ]" ? "out" : "in"`

			echo $num > ${SYSFS}/export
			echo $dir > ${path}/direction
			echo $num > ${SYSFS}/unexport
			;;
		"3")
			state=$((1 - state))
			echo $num > ${SYSFS}/export
			echo $state > ${path}/value
			echo $num > ${SYSFS}/unexport
			;;
		*)
			;;
		esac
	done
}

################################################################################
######                     5) Backlight Sub1 Menu                         ######
################################################################################
function BlSub1Menu
{
	local SYSFS=/sys/class/backlight
	local PARAM=/sys/module/eiois200_bl/parameters
	local i
	local val
	local bl_name

	# find the Backlight
	for i in $(ls -v $SYSFS | grep eiois200_bl); do
		bl_name=$i
		break ;
	done

	if [ "$bl_name" == "" ]; then
		return `read -p"Can't find eiois200 bl"`
	fi

	while :	; do
		local dir=${SYSFS}/${bl_name}
		local bri=`cat ${dir}/brightness`
		local max=`cat ${dir}/max_brightness`
		local bri_invert=`cat ${PARAM}/bri_invert`
		local bri_freq=`cat ${PARAM}/bri_freq`
		local bl_power=`cat ${dir}/bl_power`
		local bl_power_invert=`cat ${PARAM}/bl_power_invert`

		# Show menu
		show_title

		printf "Backlight: $bl_name\n\n"
		printf "0) Back to Main menu\n"
		printf "1) Brightness value	: %d (0 to %d)\n" $bri $max
		printf "2) Brightness frequency\t: $bri_freq Hz\n"
		printf "3) Brightness invert	: %s\n" `ternary "[[ \"$((bri_invert&1))\"      == \"1\" ]]" ? On : Off`
		printf "4) Power		: %s\n" `ternary "[[ \"$bl_power\"        == \"1\" ]]" ? On : Off`
		printf "5) Power invert\t	: %s\n" `ternary "[[ \"$bl_power_invert\" == \"1\" ]]" ? On : Off`
		printf "\n"

		read -n1 -p"Enter your choice: " sel
		echo -e

		# Processes
		case $sel in
		"0")
			return 0
			;;
		"1")
			printf "Input PWM value between 0 to %d: " $max
			read val

			echo $val > ${dir}/brightness
			;;
		"2")
			read -p"Input PWM freqeuency(Hz): " val

			if [ $val -ge 10 ]; then
				rmmod eiois200_bl
				modprobe eiois200_bl bri_freq=${val}
				sleep 0.2
			fi
			;;
		"3")
			rmmod eiois200_bl
			modprobe eiois200_bl bri_invert=$((1 - (bri_invert&1)))
			sleep 0.2
			;;
		"4")
			echo $((1 - bl_power)) > ${dir}/bl_power
			;;
		"5")
			rmmod eiois200_bl
			modprobe eiois200_bl bl_power_invert=$((1 - bl_power_invert))
			sleep 0.2
			;;
		*)
			;;
		esac
	done
}

################################################################################
######                     6-2) SMBus Sub2 Probe:                         ######
################################################################################
function SMBusSub2Probe
{
	echo -e
	echo "Address of existed devices (Hex):" \
	     `i2cdetect -y -a $1 | sed '1 d' | cut -c5-51 | tr '\n' ' '| sed 's/-- //g' | sed 's/  //g'`
	echo -e
	read -n1 -t2 -p"Press ENTER to continue..."
}

################################################################################
######                     6-3) SMBus Sub2 Read:                          ######
################################################################################
function SMBusSub2Read
{
	local host=$1
	local expr=""
	local addr=0
	local cmd=0
	local len=1
	local sel
	local val

	while : ; do
		#Show menu
		show_title

		case $len in
		 0) protocol="Block Read";;
		 1) protocol="Receive Byte";;
		 2) protocol="Read Byte";;
		 3) protocol="Read Word";;
		 *) protocol="Block Read";;
		esac

		printf "SMBus Read Data: i2c-$1\n\n"
		printf "0) Back to smbus menu\n"
		printf "1) Protocol	: $protocol\n"
		printf "2) Address	: %d (0x%X)\n" $addr $addr
		printf "3) Command	: %d (0x%X)\n" $cmd $cmd
		printf "4) Run\n\n"
		read -n1 -p"Enter your choice: " sel
		echo -e

		# Processes
		case $sel in
		"0")
			return 0
			;;
		"1")
			# Different length of protocal
			len=$(( (len + 1) % 4))
			;;
		"2")
			read -p"Address: " val;

			if [ -n "$val" ]; then
				addr=$val
			fi
			;;
		"3")
			read -p"Command: " val;

			if [ -n "$val" ]; then
				cmd=$val
			fi
			;;
		"4")
			# Different expression by protocal
			case $len in
			 0) expr="i2cdump -y -f -a ${host} $addr s";;
			 1) expr="i2cget  -y -f -a ${host} $addr ";;
			 2) expr="i2cget  -y -f -a ${host} $addr $cmd b";;
			 3) expr="i2cget  -y -f -a ${host} $addr $cmd w";;
			esac

			# Execute expression
			echo -e "\n${expr}"
			eval "$expr"
			read -n1 -t2 -p"Read transfer `ternary "[[ $? -eq 0 ]]" ? "Success\n" : "Fail\n"` ..."
			;;
		esac
	done
}

################################################################################
######                     6-4) SMBus Sub2 Write:                           ######
################################################################################
function SMBusSub2Write
{
	local host=$1
	local addr=0
	local data=()
	local expr
	local sel

	while : ; do
		# Show menu
		show_title

		case ${#data[@]} in
		 0) protocol="Quick Write";;
		 1) protocol="Send Byte";;
		 2) protocol="Write Byte";;
		 3) protocol="Write Word";;
		 *) protocol="Block Write";;
		esac

		printf "SMBus Write Data: i2c-$1\n\n"
		printf "0) Back to SMBus menu\n"
		printf " ) Protocol: $protocol\n"
		printf "1) Address	: %d (0x%02X) (7-bit)\n" $addr $addr
		echo -e " ) Command 	: ${data[0]}"
		echo -e " ) length	: ${#data[@]}"
		echo -e "2) Data	: ${data[@]}"
		echo -e "3) Run\n"
		read -n1 -p"Enter your choice: " sel
		echo -e "\n"

		# Processes
		case $sel in
		"0")
			return 0
			;;
		"1")
			read -p"Address: " addr
			;;
		"2")
			read -p"Input a series of HEX value (like 0x01 0x02 0x03 ...): " \
			     -a data
			;;
		"3")
			# Different expression by length
			case ${#data[@]} in
			 0)
				expr="i2cdetect -y -a -q ${host} $addr $addr"

				echo -e "\n" $expr "\n"
				val=`$expr`
				read -p"Write transfer $(ternary "[[ \"\$val\" == *\"--\"* ]]" ? "fail\n" : "Success\n") ..."

				continue
				;;
			 1)	expr="i2cset -y -f -a ${host} $addr ${data[@]}";;
			 2)	expr="i2cset -y -f -a ${host} $addr ${data[@]}";;
			 3)	expr="i2cset -y -f -a ${host} $addr ${data[0]} $((data[1] * 0x100 + data[2])) w";;
			 *)	expr="i2cset -y -f -a ${host} $addr ${data[@]} s";;
			esac

			# Execute expression
			echo -e "\n${expr}"
			eval "${expr}"
			read -n1 -t2 -p"Write transfer `ternary "[[ $? -eq 0 ]]" ? "Success\n" : "Fail\n"` ... "
			;;
		*)
			;;
		esac
	done
}

################################################################################
######                     6) SMBus Sub1 Menu                              ######
################################################################################
function SMBusSub1Menu
{
	local SYSFS=/sys/bus/i2c/devices
	local count=0
	local cur=0
	local hosts=()
	local i

	i2cdetect -l > /dev/null 2>&1
	if [ $? -ne 0 ]; then
		return `read -p "please install i2c-tools"`
	fi

	# find the SMBus# and save to array
	for i in $(ls -v $SYSFS); do
		if [ -d $SYSFS/$i ] &&
		   [ "$(cat $SYSFS/$i/name | grep "eiois200")" != "" ]; then
			hosts[count++]=$(echo $i | sed 's/i2c-//g')
		fi
	done

	if [ $count -eq 0 ]; then
		read -p"No SMBus adapters... "
		return 1
	fi

	while : ; do
		local sel=
		local host=${hosts[$cur]}

		# Show menu
		for ((i=0 ; i < $count ; i=i+1)); do
			sel+=$(ternary "[[ $i -eq $cur ]]" ? "[i2c-${hosts[$i]}]" : " i2c-${hosts[$i]} ")
		done
	
		show_title

		echo -e "SMBus"
		echo -e
		echo -e "0) Back to Main menu"
		echo -e "1) Select SMBus host: $sel"
		echo -e " ) Internal name: `cat $SYSFS/i2c-${hosts[$cur]}/name | sed 's/eiois200 //g'`"
		echo -e "2) Probe"
		echo -e "3) Read"
		echo -e "4) Write\n"
		read -n1 -p "Enter your choice: " sel
		echo -e

		# Switch to sub-menu
		case $sel in
		"0")
			return 0
			;;
		"1")
			cur=$(( (cur + 1) % count))
			;;
		"2")
			SMBusSub2Probe $host
			;;
		"3")
			SMBusSub2Read $host
			;;
		"4")
			SMBusSub2Write $host
			;;
		*)
			;;
		esac
	done
}

################################################################################
######                     7-2) I2C Sub2 Probe:                           ######
################################################################################
function I2CSub2Probe
{
	echo -e "\nAddress of existed devices (Hex):" \
	      `i2cdetect -y $1 | sed '1 d' | cut -c5-51 | tr '\n' ' '| sed 's/-- //g' | sed 's/  //g'`
	echo -e
	read -n1 -t2 -p"Press ENTER to continue..." probe_value
}

################################################################################
######                     7-3) I2C Sub2 Read:                            ######
################################################################################
function I2CSub2Read
{
	local host=$1
	local addr=0
	local cmd=0
	local len=0
	local val
	local sel

	while : ; do
		# Menu
		show_title

		printf "I2C Read Data: i2c-$host\n\n"
		printf "0) Back to I2C menu\n"
		printf "1) Address	 : %d (0x%X) ( (7-bit)\n" $addr $addr
		printf "2) Command	 : %d (0x%X) (Byte-type)\n" $cmd $cmd
		printf "3) Length	 : %d (0x%X)\n" $len $len
		printf "4) Run\n\n"
		read -n1 -p"Enter your choice: " sel
		echo -e "\n"

		# Processes
		case $sel in
		"0")
			return 0
			;;
		"1")
			read -p"Enter 7-bit Address: " val
			if [ -n "$val" ]; then
				addr=$val
			fi
			;;
		"2")
			read -p"Byte Command : " val
			if [ -n "$val" ]; then
				cmd=$val
			fi
			;;
		"3")
			read -p"Read Length: " val

			if [ -n "$val" ]; then
				len=$val
			fi
			;;
		"4")
			echo -e "i2ctransfer -f -a -y $host w1@$addr $cmd r$len"
			i2ctransfer -f -a -y $host w1@$addr $cmd r$len
			read -n1 -t2 -p "I2C Read transfer `ternary "[ $? -eq 0 ]" ? "Success" : "Fail"`..."
			;;
		*)
			;;
		esac
	done
}

################################################################################
######                     7-4) I2C Sub2 Write:                           ######
################################################################################
function I2CSub2Write
{
	local host=$1
	local addr=0
	local data=()

	while : ; do
		local val
		local sel

		# Show Menu
		show_title

		printf "I2C Write Data: i2c-$host \n\n"
		printf "0) Back to I2C menu\n"
		printf "1) Address	: %d (0x%X) (7-bit)\n" $addr $addr
		printf " ) Command	: ${data[0]}\n"
		printf " ) Length	: ${#data[@]}\n"
		echo -e "2) Write Data	: ${data[@]}"
		printf "3) Run\n\n"
		read -n1 -p"Enter your choice: " sel;
		echo -e "\n"

		# Processes
		case $sel in
		"0")
			return 1
			;;
		"1")
			read -p"7-bit Address: " addr
			;;
		"2")
			read -p"Input a series of HEX value (Like 0x01 0x02 0x03 ...): " -a data
			;;
		"3")
			echo i2ctransfer -f -a -y $host w$((${#data[@]}))@$addr ${data[@]}
			i2ctransfer -f -a -y $host w$((${#data[@]}))@$addr ${data[@]}
			read -n1 -t2 -p "I2C Write transfer `ternary "[ $? -eq 0 ]" ? "Success" : "Fail"`..."
			;;
		esac
	done
}

################################################################################
######                     7-4) I2C Sub2 Write Read combine:              ######
################################################################################
function I2CSub2WriteRead
{
	local host=$1
	local addr=0
	local len=0
	local data=()
	local sel
	local val
	local expr=""

	while : ; do
		# Show menu
		show_title

		printf "I2C Write Read Combine: i2c-$host\n\n"
		printf "0) Back to I2C menu\n"
		printf "1) Address	: %d (0x%X) (7-bit)\n" $addr $addr
		printf " ) Command	: ${data[0]}\n"
		printf " ) Write Length	: %d (0x%X)\n" ${#data[@]} ${#data[@]}
		echo   "2) Write Data	: ${data[@]} "
		printf "3) Read Length	: $len\n"
		printf "4) Run\n\n"
		read -n1 -p"Enter your choice: " sel
		echo -e "\n"

		# Processes
		case $sel in
		"0")
			return 1
			;;
		"1")
			echo -e "7-bit Address : \c"
			read val
			if [ -n "$val" ]; then
				addr=$val
			fi
			;;
		"2")
			echo -e p"Input a series of HEX value (Like 0x01 0x02 0x03 ...): \c"
			read -a data
			;;
		"3")
			echo -e "Read Length: \c"
			read val
			if [ -n "$val" ]; then
				len=$val
			fi
			;;
		"4")
			# Different expression by length
			if [ ${#data[@]} -eq 0 ]; then
				expr="i2ctransfer -f -a -y $host r${len}@$addr"
			elif [ $len -eq 0 ]; then
				expr="i2ctransfer -f -a -y $host w${#data[@]}@$addr ${data[@]}"
			else
				expr="i2ctransfer -f -a -y $host w${#data[@]}@$addr ${data[@]} r$len"
			fi

			# Execute expression
			echo -e "$expr"
			eval "$expr"
			read -n1 -t2 -p "I2C Write Read Combined transfer `ternary "[ $? -eq 0 ]" ? "Success" : "Fail"`..."
			;;
		*)
			;;
		esac
	done
}

################################################################################
######                     7) I2C Sub1 Menu                               ######
################################################################################
function I2CSub1Menu
{
	local SYSFS=/sys/bus/i2c/devices
	local PARAM=/sys/module/i2c_eiois200/parameters
	local i=0
	local hosts=()
	local val

	i2cdetect -l > /dev/null 2>&1
	if [ $? -ne 0 ]; then
		return `read -p"Please install i2c-tools"`
	fi

	# find the I2C# and save to array
	for I2CN in $(ls -v $SYSFS); do
		if [ -d $SYSFS/$I2CN ] &&
		   [ "$(cat $SYSFS/$I2CN/name | grep "eiois200")" != "" ]; then
			hosts[${#hosts[@]}]=$(echo $I2CN | sed 's/i2c-//g')
		fi
	done

	if [[ ${#hosts[@]} -eq 0 ]]; then
		return `read -p"No I2C adapter..."`
	fi

	while : ; do
		local name=$(cat $SYSFS/i2c-${hosts[$i]}/name | sed 's/eiois200-//g')
		local i2c_freq=`cat ${PARAM}/${name: -4}_freq`
		local lst=
		local sel=0

		# Shoe menu
		for ((j=0 ; j < ${#hosts[@]} ; j++)); do
			lst+=`ternary "[ $i == $j ]" ? "[i2c${hosts[j]}]" : " i2c${hosts[j]} "`
		done

		show_title

		echo -e "I2C"
		echo -e
		echo -e "0) Back to Main menu"
		echo -e "1) Select host: $lst"
		echo -e " ) Internal name: eiois200-$name"
		echo -e "2) Probe"
		echo -e "3) Read"
		echo -e "4) Write"
		echo -e "5) WR Combined"
		echo -e "6) Frequency : ${i2c_freq}kHz\n"
		read -n1 -p"Enter your choice: " sel
		echo -e

		# Switch to sub-menu
		case $sel in
		"0")
			return 0
			;;
		"1")
			i=$(( (i + 1) % ${#hosts[@]}))
			;;
		"2")
			I2CSub2Probe ${hosts[$i]}
			;;
		"3")
			I2CSub2Read ${hosts[$i]}
			;;
		"4")
			I2CSub2Write ${hosts[$i]}
			;;
		"5")
			I2CSub2WriteRead ${hosts[$i]}
			;;
		"6")
			echo -e "I2C clock Frequency(kHz): \c"
			read val
			if [ -n "$val" ]; then
				rmmod i2c_eiois200
				modprobe i2c_eiois200 ${name}_freq=${val}
			fi
			;;
		esac
	done
}

################################################################################
######                     8) Information Sub1 Menu                       ######
################################################################################
function InfoSub1Menu
{
	local DIR=/sys/bus/isa/devices/eiois200_core.0
	local up_time=$(uptime | cut -d "p" -f 2 | cut -d "," -f 1)
	local eapi_ver=`cat $DIR/eapi_version 2> /dev/null`
	local fw_ver=`cat $DIR/firmware_version 2> /dev/null`

	show_title

	echo -e "Information"
	echo -e
	echo -e "Up time			:" $up_time
	echo -e "Board ID		:" `cat $DIR/board_id 2> /dev/null`
	echo -e "Board Manufacturer	:" `cat $DIR/board_manufacturer 2> /dev/null`
	echo -e "Board Name		:" `cat $DIR/board_name 2> /dev/null`
	echo -e "Board Serial		:" `cat $DIR/board_serial 2> /dev/null`
	echo -e "Boot Count		:" `cat $DIR/boot_count 2> /dev/null`
	echo -e "Chip Detect		:" `cat $DIR/chip_detect 2> /dev/null`
	echo -e "Chip ID			:" `cat $DIR/chip_id 2> /dev/null`
	echo -e "EAPI ID			:" `cat $DIR/eapi_id 2> /dev/null`
	printf  "EAPI Version		: %u.%u\n" ${eapi_ver:2:2} ${eapi_ver:4:2}
	echo -e "Firmware Build		:" `cat $DIR/firmware_build 2> /dev/null`
	echo -e "Firmware Date		:" `cat $DIR/firmware_date 2> /dev/null`
	echo -e "Firmware Name		:" `cat $DIR/firmware_name 2> /dev/null`
	printf  "Firmware Version	: %u.%u.%u\n" 0x${fw_ver:8:2} 0x${fw_ver:6:2} 0x${fw_ver:2:4}
	echo -e "Platform Revision	:" `cat $DIR/platform_revision 2> /dev/null`
	echo -e "Platform Type		:" `cat $DIR/platform_type 2> /dev/null`
	echo -e "Pnp ID			:" `cat $DIR/pnp_id 2> /dev/null`
	echo -e "Running time (hour)	:" `cat $DIR/powerup_hour 2> /dev/null`
	echo -e
	read -n1 -p"Press ENTER to continue..."
}

################################################################################
######                     Main Menu                                      ######
################################################################################
function MainMenu
{
	local DRIVERS=( eiois200_core eiois200_wdt eiois200_hwmon eiois200_fan eiois200_thermal eiois200_bl gpio_eiois200 i2c_eiois200 )
	local loads=()

	# Must super user
	if [ "$USER" != "root" ]; then
		return `read -p"please run as root..."`
	fi

	# Load drivers
	for ((i=0; i < ${#DRIVERS[@]}; i=i+1)); do
		if [ "$(lsmod | grep ${DRIVERS[i]})" == "" ]; then
			loads[${#loads[@]}]=${DRIVERS[i]}
			modprobe ${DRIVERS[i]}
		fi
	done

	# Main menu
	while :
	do
		show_title

		echo -e "Main (EIOIS200 driver demo script)"
		echo -e
		echo -e "0) Terminate this program"
		echo -e "1) Watch Dog"
		echo -e "2) HWM"
		echo -e "3) SmartFan"
		echo -e "4) Thermal Protection"
		echo -e "5) GPIO"
		echo -e "6) Backlight"
		echo -e "7) SMBus"
		echo -e "8) I2C"
		echo -e "9) Information\n"
		read -n1 -p"Enter your choice: " sel
		echo -e

		# Switch to sub-menu by selection
		case $sel in
		 0) break;;
		 1) WdtSub1Menu;;
		 2) HwmSub1Menu;;
		 3) SmartFanSub1Menu;;
		 4) ThermalSub1Menu;;
		 5) GpioSub1Menu;;
		 6) BlSub1Menu;;
		 7) SMBusSub1Menu;;
		 8) I2CSub1Menu;;
		 9) InfoSub1Menu;;
		esac
	done
	
	# Remove drivers
	for ((i=${#loads[@]}; i > 0 ; i--)); do
		rmmod ${loads[i-1]}
	done
}

# Start from here
MainMenu

