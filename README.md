# Simple TCP server with asynchronous socket calls
Mutithreaded TCP server for automatic control of remote robots. Server is able to navigate several robots at once and implements communication protocol without errors.<br>
Robots authenticate to the server and server directs them to target location in coordinate plane. For testing purposes each robot starts at the random coordinates and its target location is always located in the area inclusively between coordinates -2 and 2 at *x*- and *y*-axes. Somewhere in the target area a secret message is placed, which robot should find, thus all positions of the target area should be searched. <br>

Here are some basic message types server sending to robots:

|Name|Message|Description|
|:---|:----------------|:---------|
|SERVER_CONFIRMATION|<16-bit decimal number>\a\b|Message with confirmation code. Can contain <br />  maximally 5 digits and the termination sequence \a\b|
|SERVER_MOVE|102 MOVE\a\b|Command to move one position forward|
|SERVER_TURN_LEFT|103 TURN LEFT\a\b|Command to turn left|
|SERVER_TURN_RIGHT|104 TURN RIGHT\a\b|Command to turn right|
|SERVER_PICK_UP|105 GET MESSAGE\a\b|Command to pick up the message|
|SERVER_LOGOUT|106 LOGOUT\a\b|Command to terminate the connection <br /> after successfull message discovery
|SERVER_OK|	200 OK\a\b|Positive acknowledgement|
|SERVER_LOGIN_FAILED|300 LOGIN FAILED\a\b|Autentication failed|
|SERVER_SYNTAX_ERROR|301 SYNTAX ERROR\a\b|Incorrect syntax of the message|
|SERVER_LOGIC_ERROR|302 LOGIC ERROR\a\b|Message sent in wrong situation|

Client messages:
|Name|Message|Description|Example|Maximal length|
|:--|:--|:--|:--|:--|
|CLIENT_USERNAME|<user name>\a\b|Message with username. Username can be any sequence of characters except for the pair \a\b.|	Oompa_Loompa\a\b|12|
|CLIENT_CONFIRMATION|<16-bit decimal number>\a\b|Message with confirmation code. Can contain maximally 5 digits and the termination sequence \a\b.|1009\a\b|7|
|CLIENT_OK|OK \<x\> \<y\>\a\b|Confirmation of performed movement, where x and y are the robot coordinates after execution of move command.|OK -3 -1\a\b|12|
|CLIENT_RECHARGING|RECHARGING\a\b|Robot starts charging and stops to respond to messages.| |12|
|CLIENT_FULL_POWER|FULL POWER\a\b|Robot has recharged and accepts commands again.| |12|
|CLIENT_MESSAGE|<text>\a\b|Text of discovered secret message. Can contain any characters except for the termination sequence \a\b.|Haf!\a\b|100|