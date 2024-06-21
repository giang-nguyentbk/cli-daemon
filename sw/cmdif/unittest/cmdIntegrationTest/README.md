```bash
# Open 5 terminals
## Terminal 1
$ <path-to-sdk>/sysroot/usr/exec/itccoord_so

## Terminal 2
$ <path-to-sdk>/sysroot/usr/exec/itcgws_so -n "/ubuntu/"

## Terminal 3
$ <path-to-sdk>/sysroot/usr/exec/clid_so

## Terminal 4
$ <path-to-sdk>/sysroot/usr/exec/clishell

## Terminal 5
$ cd /home/giangnguyentbk/workspace/cli-daemon/sw/cmdif/unittest/cmdIntegrationTest
$ make clean
$ make
$ make run

# On terminal 4
$[local]>> scan 
$[local]>> connect --idx 1 
$[192.168.x.y]>> abc --help 
$[192.168.x.y]>> abc 
$[192.168.x.y]>> abc 111 
$[192.168.x.y]>> abc 111 222 

```