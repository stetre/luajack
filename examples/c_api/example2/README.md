
####LuaJack C-API - Example 2

This example enhances Example 1 by adding a client thread (thread.lua) and 
a bidirectional communication channel between the process context and the
thread context.

The communication channel is implemented with a couple of ringbuffers, and in
this example it is used just to exchange pings and pongs between the two contexts.

In a real life application the thread context would typically implement some sort
of GUI, and the bidirectional channel would be used to carry control commands 
(in the thread to process direction) and notifications (in the process to thread
directions).

#####Running the example

Same as Example 1.

