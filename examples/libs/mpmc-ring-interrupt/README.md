# mpmc-ring-interrupt example

This example demonstrates and tests interrupt-safety of
lib/mpmc-ring.

## What it does

It prepares a message queue managed by mpmc-ring. Two producers put
some messages into the queue. One producer runs in normal context,
and the other in interrupt context (using rtimer). At the same time,
two consumers (in normal and interrupt contexts, respectively) get
messages from the queue and stores them in dedicated memory regions.

                          put             get
       NORMAL (Producer) --+               +--> (Consumer) --> [store]
                           |               |
                           +-> --------- >-+
                               | queue |
                           +-> --------- >-+
                           |               |
    INTERRUPT (Producer) --+               +--> (Consumer) --> [store]

After all activities are finished, it checks the content of the two
message stores. If the stores do not contain the exact messages put
by the producers, it prints error messages.

If the check succeeds, it re-initializes the producers, consumers
and queue, and run the same test again. If the check fails, it
resets.

## Configuration

In project-conf, you can configure number of messages two produers
put to the queue and two consumers get from the queue. You can also
configure speed of the producers and consumers relative to each
other.

## Expectations

This example should run without any error logs even though there are
multiple producers and consumers.

## Author

Toshio Ito <toshio9.ito@toshiba.co.jp>
