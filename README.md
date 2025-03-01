# Client-Server TCP-UDP

In this implementation, the server performs most of the "heavy lifting," meaning it sends the processed data, ready for display, to the client. (This seemed to me like the most logical structure.)
Both the server and client use I/O multiplexing to remain in a waiting state until an event occurs on one of the sockets, at which point execution begins.

Server
The server waits to receive TCP connections from clients. When a connection is made, it must verify the client's ID to ensure that no other client is using the same ID. It must reallocate the three dynamic structures if necessary and check if the client has previously connected to the server, in which case it will retain the client's old subscriptions.
If the server receives UDP connections, they are assigned to a unique socket, and it receives messages of up to 1551 bytes, which it will interpret, place in a structure, and send to clients subscribed to the corresponding topic.
If wildcards are used, both the searched element and the topics to which the client is subscribed will be placed in separate arrays of strings (imitating a list of strings). These will then be compared word by word to check if a match can be made.
If the server receives a message from one of the TCP-connected sockets, it could be one of two commands: subscribe or unsubscribe (if another command is received, an error message is displayed informing the user of the two valid commands).

Subscribe: The server checks if the topic is valid and adds it to the client's subscription list. If the topic is valid, a confirmation message is sent to the client. Otherwise, a "not ok" message is sent to display an error.
Unsubscribe: The server ensures that the subscription exists before attempting to remove it. If it doesn't, an error message is sent to the client to inform them.
STDIN: The only valid command is "exit"; any other command will trigger an error message.
Exit: The server sends a signal to all subscribers to stop, then stops all connections and frees the program’s resources.

Client
The client doesn't have many functionalities since the server handles most operations.
It can intercept one of the following messages from STDIN:

Subscribe/Unsubscribe: Sends a message to the server and waits for confirmation to display the corresponding message.
Exit: Closes the socket and ends the program.
Anything else: Displays a message informing the user of valid commands.
If the client receives a message from the server via TCP, it will display the interpreted data on the screen (the data already processed by the server).

