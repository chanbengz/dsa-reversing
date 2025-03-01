make $1 LDLIBS=-laccel-config
sudo setcap cap_sys_rawio+ep ./$1
./$1
