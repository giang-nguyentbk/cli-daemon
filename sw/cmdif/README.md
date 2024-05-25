```bash
# CmdTypesIf: define result codes where a cmd is treated as success, fail, or invalid arguments received.

# CmdJobIf: a job containing an array of arguments, string output, cmdName,... Will be given to the consumer threads who register cmdTable. They will decide when the cmd execution is finished, by setting string outputStream (job->getOutputStream() = "Cmd is successfully executed or failed! Results are ...") and calling job->done() finally. 

# CmdTableIf: The consumer threads will register their cmd list (including cmd syntaxes, handlers, and descriptions) to a static cmdTable.

# CmdRegisterIf: When consumer threads call registerCmdHandler(cmdName, cmdHandler), stored the pair in a std::map, and send the cmd list related to the cmdName to cli-daemon. On receiving CMDIF_CMD_EXE_REQ from clid, look up the cmdHandler by cmdName from the std::map and execute cmdHanler which will call an actual cmdHandler associated with a cmd syntax in the registered cmd list in cmdTable. 

```