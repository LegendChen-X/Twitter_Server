# Twitter_Server
For assignment four, you will write a simple version of a Twitter server. When a new user connects, the server will send them "Welcome to CSC209 Twitter! Enter your username: ". After they input an acceptable name that is not equal to any existing users's username and is not the empty string, they are added to Twitter, and all active users (if any) are alerted to the addition (e.g., "Karen has just joined."). If they input an unacceptable name, they should be notified and asked again to enter their name.

With your server, users can join or leave Twitter at any time. A user leaves by issuing the quit command or exiting/killing `nc`.
New users who have not yet entered their names are added to the head of a linked list of new users (see `add_client`). Users in this list cannot issue commands, and do not receive any announcements about the status of Twitter (e.g., announcements clients connecting, disconnecting, etc.).

When a user has entered their name, they are removed from the new clients list and added to the head of the list of active clients. Once a user becomes active, they can issue commands. The possible commands are:

### `follow username`
When this user follows username, add username to this user's following list and add this user to username's list of followers. Once this user follows username, they are sent any messages that username sends.
A user can follow up to `FOLLOW_LIMIT` users and a user can have up to `FOLLOW_LIMIT` followers. If either of those lists have insufficient space, then this user cannot follow username, and should be notified of that.
If unsuccessful (e.g., username is not an active user), notify the user who issued the command.

### `unfollow username`
If this user unfollows username, remove username from this user's following list, and remove this user from username's list of followers. One this user unfollows username, they are no longer sent any messages that username sends.
If unsuccessful (e.g., username is not an active user), notify the user who issued the command.

### `show`
Displays the previously sent messages of those users this user is following.

### `send <message>`
Send a message that is up to 140 characters long (not including the command send) to all of this user's followers.
You may assume all messages are between 1 and 140 characters.
If a user has already sent MSG_LIMIT messages, notify the user who issued the command and do not send the message.

### `quit`
Close the socket connection and terminate.

If a user issues an invalid command or enters a blank line, tell them "Invalid command".

When a users leaves, they must be removed from all active clients' followers/following lists.

Be prepared for the possibility that a client drops the connection (disconnects) at any time. How does the server tell when a client has dropped a connection? When a client terminates, the socket is closed. This means that when the server tries to write to the socket, the return value of `write` will indicate that there was an error, and the socket is closed. Similarly, if the server tries to read from a closed socket, the return value from the `read` call will indicate if the client end is closed.

As clients connect, disconnect, enter their names, and issue commands, the server should send to `stdout` a brief statement of each activity. (Again the format of these activity statements is up to you.) Remember to use the network newline in your network communication throughout, but not in messages printed on `stdout`.

You are allowed to assume that the client does not "type ahead" -- if you receive multiple lines in a single `read()`, for this assignment you do not need to do what is usually required in working with sockets of storing the excess data for a future input.

However, you can't assume that you get the entire name or command in a single `read()`. For the input of the name or command, if the data you read does not include a network newline, you must get the rest of the input from subsequent calls on `read()`.

The server should continue running even if all clients have disconnected.

## Testing
To use nc, type `nc -C hostname yyyyy` (use lowercase `-c` on Mac), where `hostname` is the full name of the machine on which your server is running, and `yyyyy` is the port on which your server is listening. If you aren't sure which machine your server is running on, you can run `hostname -f` to find out. If you are sure that the server and client are both on the same server, you can use `localhost` in the place of the fully specified host name.

To test if your partial reads work correctly, you can send a partial line (without the network newline) from `nc` by typing `Ctrl-D`.
