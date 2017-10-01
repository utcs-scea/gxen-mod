#!/bin/bash

modprobe xen-pciback

#echo unbind
#echo 0000:03:00.0 > /sys/bus/pci/devices/0000\:03\:00.0/driver/unbind
#echo 0000:03:00.1 > /sys/bus/pci/devices/0000\:03\:00.1/driver/unbind

#echo pciback new_slot
#echo 0000:03:00.0 > /sys/bus/pci/drivers/pciback/new_slot
#echo 0000:03:00.1 > /sys/bus/pci/drivers/pciback/new_slot
#echo pciback/slots=`cat /sys/bus/pci/drivers/pciback/slots`

#echo pciback bind
#echo 0000:03:00.0 > /sys/bus/pci/drivers/pciback/bind
#echo 0000:03:00.1 > /sys/bus/pci/drivers/pciback/bind

xl pci-assignable-add 03:00.0
xl pci-assignable-add 03:00.1
