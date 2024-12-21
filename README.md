# EEVDF Queue 

A simple implementation of an EEVDF queue meant to be used by a shceduler

The queue is not thread safe so if you need to integrate it into an SMP environment you will need to add your own locks to it

It also comes with a simple binary heap implementation that is used by the library.
