/*
*        ________________           ________                                                    ______  
* ____  ___  /___(_)__  /_______    ___  __/____________ _______ ___________      _________________  /__
* _  / / /  __/_  /__  /__  ___/    __  /_ __  ___/  __ `/_  __ `__ \  _ \_ | /| / /  __ \_  ___/_  //_/
* / /_/ // /_ _  / _  / _(__  )     _  __/ _  /   / /_/ /_  / / / / /  __/_ |/ |/ // /_/ /  /   _  ,<   
* \__,_/ \__/ /_/  /_/  /____/      /_/    /_/    \__,_/ /_/ /_/ /_/\___/____/|__/ \____//_/    /_/|_|  
*                                                                                                       
*/

#pragma once

#include <cstdint>
#include <functional>
#include <memory>

union itc_msg;

namespace UtilsFramework
{
namespace ItcPubSub
{
namespace V1
{
/*! @brief ITC Pub Sub basically holds a list of message numbers registered with the mailbox of current thread.
* + Add our thread's mailbox FD to our local-thread Event Loop FD list to be ready for monitoring incoming messages on this FD.
* + Whenever receiving an ITC message from others to our mailbox with a message number which exists in the list,
* call a respective handler function.
*
* Example usage:
* 
* </code>
*   using namespace UtilsFramework::ItcPubSub::V1;
*
*	int main()
*   	{
*       	... 
*
*       	IItcPubSub& itcPubSub = IItcPubSub::getInstance();
*       	IItcPubSub& eventLoop = IItcPubSub::getInstance();
*
*		// Add your thread's mailbox FD to ItcPubSub where registered messages will be monitored
*		if(itcPubSub.addItcFd(itc_get_fd()) != IItcPubSub::ReturnCode::NORMAL)
*		{
*			itcPubSub.registerMsg(yourMessageNumber0, yourHandlerFunction0);
*			itcPubSub.registerMsg(yourMessageNumber1, yourHandlerFunction1);
*			itcPubSub.registerMsg(yourMessageNumber2, yourHandlerFunction2);
*			...
*
*			// Please be careful, after running the Event Loop for current thread, you will come into an infinite loop,
* 			// start monitoring the mailbox FD and registered messages, and not be able to registerMsg() anymore.
*			eventLoop.run();
*		}
*    
*       return -1;
*   	}
* </code>    
*
*/

class IItcPubSub
{
public:
	enum class ReturnCode
	{
		NORMAL,             /*!< No error */
		ALREADY_EXISTS,     /*!< A FD handler already exists */
		NOT_FOUND,          /*!< A FD handler was not found */
		NOT_THREAD_LOCAL,   /*!< The IItcPubSub instance called was not the thread-local instance */
		INTERNAL_FAULT      /*!< An internal error */
	};

	// First prevent end users from copy/move construtors
	IItcPubSub(const IItcPubSub&)               = delete; // Copy constructor
	IItcPubSub(IItcPubSub&&)                    = delete; // Move constructor
	IItcPubSub& operator=(const IItcPubSub&)    = delete; // Copy assignment constructor
	IItcPubSub& operator=(IItcPubSub&&)         = delete; // Move assignment constructor

	static IItcPubSub& getInstance();

	using MsgHandler = std::function<void(const std::shared_ptr<union itc_msg>& msg)>;

	virtual ReturnCode addItcFd(int fd) = 0;
	virtual ReturnCode registerMsg(uint32_t msgNo, const MsgHandler& msgHandler) = 0;
	virtual ReturnCode deregisterMsg(uint32_t msgNo) = 0;

protected:
	IItcPubSub()  = default;
	~IItcPubSub() = default;

}; // class IItcPubSub

} // namespace V1

} // namespace ItcPubSub

} // namespace UtilsFramework