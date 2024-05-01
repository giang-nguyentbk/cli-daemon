# cli-daemon
```
This project is divided into 3 parts:

1. shell: this is a custom shell that continously receives command syntax from user terminal via std::cin, validates input arguments and forward them to daemon part.

2. daemon: holds a set of mailbox IDs and its respective registered command headers. When receiving CLID_EXE_CMD_REQ from cli part, search for the corresponding command header and forward CLID_EXE_CMD_FWD to the related mailbox.

3. cmdif: holds a set of command syntax and respective handlers. When receiving CLID_EXE_CMD_FWD, search for the handler based on syntax, carry it out and respond with CLID_EXE_CMD_FWD_REP.



```