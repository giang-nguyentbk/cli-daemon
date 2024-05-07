# cli-daemon
```
This project is divided into 3 parts:

1. shell: this is a custom shell that continously receives command syntax from user terminal via std::cin, validates input arguments and forward them to daemon part.

2. daemon: holds a set of mailbox IDs and its respective registered command headers. When receiving CLID_EXE_CMD_REQ from cli part, search for the corresponding command header and forward CLID_EXE_CMD_FWD to the related mailbox.

3. cmdif: holds a set of command syntax and respective handlers. When receiving CLID_EXE_CMD_FWD, search for the handler based on syntax, carry it out and respond with CLID_EXE_CMD_FWD_REP.



```

# How to test

```bash
# Run itccoord
$ Open 1st terminal
$ ws
$ cd itc-framework/sw/itc/unitTest/itc-main/test-itcgws
$ make rcp

# Run itcgws
$ Open 2nd terminal
$ ws
$ cd itc-framework/sw/itc/unitTest/itc-main/test-itcgws
$ make r2

# Run clid
$ Open 3rd terminal
$ ws
$ cd cli-daemon/sw/clid
$ make run

# Run shell
$ Open 4th terminal
$ ws
$ cd cli-daemon/sw/shell
$ make run

```