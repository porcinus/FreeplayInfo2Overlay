sudo g++ info2png.cpp -o info2png -lgd -lpng -lz -lfreetype -lm
#sudo g++ png2fb16.cpp -o png2fb16 -lgd -lpng -lz -lm
sudo g++ nns-overlay-deamon.cpp -o nns-overlay-deamon -lwiringPi
sudo gcc img2dispmanx.c -o img2dispmanx -lpng -ljpeg -L/opt/vc/lib/ -lbcm_host -lm -I./ -I/opt/vc/include/ -I/opt/vc/include/interface/vcos/pthreads -I/opt/vc/include/interface/vmcs_host/linux 
