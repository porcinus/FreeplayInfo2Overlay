#!/usr/bin/env bash

create_service_files(){
	echo "Creating service files"
	cp "$OVLPATH/service/base.service" "$OVLPATH/service/$OVLSERVICE.service"
	cp "$OVLPATH/service/base.sh" "$OVLPATH/service/$OVLSERVICE.sh"
	
	sed -i "s|OVLPATH|$OVLPATH|g;s|OVLSERVICE|$OVLSERVICE|g;s|OVLPIN|$OVLPIN|g;s|OVLREVPIN|$OVLREVPIN|g;" "$OVLPATH/service/$OVLSERVICE.service"
	sed -i "s|OVLPATH|$OVLPATH|g;s|OVLSERVICE|$OVLSERVICE|g;s|OVLPIN|$OVLPIN|g;s|OVLREVPIN|$OVLREVPIN|g;" "$OVLPATH/service/$OVLSERVICE.sh"
	if [ $OVLREVPIN == true ]; then
    sed -i "s|-pin|-reverselogic -pin|g;" "$OVLPATH/service/$OVLSERVICE.sh"
	fi
}

update_config(){
	echo "Update config file"
	rm "$OVLPATH/freeplay_overlay_config.txt"
	echo "OVLPATH=\"$OVLPATH\"
OVLSERVICE=\"$OVLSERVICE\"
OVLPIN=\"$OVLPIN\"
OVLREVPIN=$OVLREVPIN" > "$OVLPATH/freeplay_overlay_config.txt"
	
}


install_service(){
	remove_service
	create_service_files
	echo "Install service"
	cp "$OVLPATH/service/$OVLSERVICE.service" "/lib/systemd/system/$OVLSERVICE.service"
	systemctl enable "$OVLSERVICE.service"
	systemctl start "$OVLSERVICE.service"
	#sleep 5
}


remove_service(){
	echo "Remove service"
	systemctl stop "$OVLSERVICE.service"
	systemctl disable "$OVLSERVICE.service"
	#sleep 5
}


read_input(){
	returned_input=$(sudo $OVLPATH/gpio-input-detect)
	returned_status=$?
	returned_input=$(($returned_input + 0)) #convert to int
	#echo $returned_input
	
	if [ $returned_input != -1 ] && [ $returned_status == 0 ] ; then
		if [ $returned_input -lt 0 ]; then
			OVLREVPIN=true
			returned_input=$(($returned_input * -1))
		else
			OVLREVPIN=false
		fi
		OVLPIN=$returned_input
		update_config
	fi
}


show_menu(){
	choice=$(dialog --clear --title "Freeplay Overlay Configuration" --nocancel --menu "Current GPIO input : $OVLPIN" 11 50 4 \
	E "Enable" \
	D "Disable" \
	I "Set input to display the overlay" \
	Q "Exit without changes" 2>&1 >/dev/tty)


	case "$choice" in
		E) install_service
			 show_menu;;
		D) remove_service
			 show_menu;;
		I) read_input
			 show_menu;;
		Q) printf "\033c"
			 exit 0;;
	esac
}






#https://stackoverflow.com/questions/630372/determine-the-path-of-the-executing-bash-script
OVLPATH=`dirname "$0"`
OVLPATH=`( cd "$OVLPATH" && pwd )`


g++ "$OVLPATH/gpio-input-detect.cpp" -o "$OVLPATH/gpio-input-detect" -lpthread -lwiringPi
if [ ! -e "$OVLPATH/gpio-input-detect" ]; then
    echo "Failed to compile required program..."
    
    exit 1;
fi


if [ ! -e "$OVLPATH/freeplay_overlay_config.txt" ]; then
	echo "Default config not exist, Creating a new one."

	echo "OVLPATH=\"$OVLPATH\"
OVLSERVICE=\"freeplayinfo2overlay\"
OVLPIN=\"20\"
OVLREVPIN=true" > "$OVLPATH/freeplay_overlay_config.txt"
fi

source "$OVLPATH/freeplay_overlay_config.txt" #import vars


#echo $OVLPATH
#echo $OVLSERVICE
#echo $OVLPIN
#echo $OVLREVPIN

#rm "$OVLPATH/service/$OVLSERVICE.service"

if [ ! -e "$OVLPATH/service/$OVLSERVICE.service" ]; then
	echo "Default service not exist"
	create_service_files
fi

show_menu
